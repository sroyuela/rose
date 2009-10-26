#!/usr/bin/pl -q -t main -f
% -*- prolog -*-

%-----------------------------------------------------------------------
% transform.pl
%
% About
% -----
%
% Use a rule-based system to externally transform flow facts embedded into
% C++ souce code in the form of annotations
%
% Authors
% -------
%
% Copyright (C) 2007, 2009 Adrian Prantl
%
% License
% -------
%
% This program is free software; you can redistribute it and/or modify
% it under the terms of the GNU General Public License as published by
% the Free Software Foundation; version 2 of the License.
% 
% This program is distributed in the hope that it will be useful,
% but WITHOUT ANY WARRANTY; without even the implied warranty of
% MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
% GNU General Public License for more details.
%
%-----------------------------------------------------------------------

:- prolog_load_context(directory, CurDir),
   asserta(library_directory(CurDir)),
   (getenv('TERMITE_LIB', TermitePath)
   ; (print_message(error, 'Please set the environment variable TERMITE_LIB'),
      halt(1))
   ),
   asserta(library_directory(TermitePath)).

:- use_module([library(clpfd),
	       library(utils)]).

% direct replacement
replace(Term, Term, WithTerm, WithTerm) :- !.
% substring replacement
% this is not exactly elegant - replace substrings only if rhs is a string
replace(Term, SubTerm, WithTerm, Result) :- 
  atom(Term), atom(SubTerm), string(WithTerm),
  sub_string(Term, Start, Length, After, SubTerm),
  TailStart is Start+Length,
  sub_string(Term, TailStart, After, 0, Tail),
  (Start == 0 -> 
   R1 = WithTerm ; 
   (sub_string(Term, 0, Start, _, Head),
    string_concat(Head, WithTerm, R1))
  ), 
  string_concat(R1, Tail, R2),
  string_to_atom(R2, Result),
  !.

replace(Term, SubTerm, WithTerm, Result) :-
  bin_op(Term, F, A, B), !,
  replace(A, SubTerm, WithTerm, A1),
  replace(B, SubTerm, WithTerm, B1),
  bin_op(Result, F, A1, B1).
replace(Term, SubTerm, WithTerm, Result) :-
  un_op(Term, F, A), !,
  replace(A, SubTerm, WithTerm, A1),
  un_op(Result, F, A1).
replace(Term, _, _, Term) :-
  atom(Term), !.
replace(Term, _, _, Term) :-
  number(Term), !.
replace(Term, _, _, Term) :-
  string(Term), !.
replace(_,_,_,_) :- writeln('FIXME').

% Marker renaming
rename_markers(Annots, Annots_renamed) :-
  select(annotation(M_int, wcet_marker(M_usr)), Annots, Annots1), !,
  write('Replacing '), write(M_usr), write(' with '), write(M_int), writeln('.'), 
  rename_marker(M_usr, M_int, Annots1, Annots2),
  rename_markers(Annots2, Annots_renamed).
rename_markers(A, A).

rename_marker(_, _, [], []).
rename_marker(M_user, M_internal, [A|As], [AT|ATs]) :-
  %write('    '), writeln(A),  write('  ->'),
  replace(A, M_user, M_internal, AT),
  %write(AT), writeln('\n'),
  rename_marker(M_user, M_internal, As, ATs).

%-----------------------------------------------------------------------
% Simplification of Terms
%-----------------------------------------------------------------------
associative(+).
associative(-).
associative(*).

commutative(+).
commutative(*).
commutative(=).

% op(Int, X) -> op(X, Int)
simplify(Term, TermS) :-
  bin_op(Term, F, A, B),
  commutative(F),
  number(A),
  \+ number(B),
  bin_op(TermS, F, B, A).


% X*Int =< Y -> X =< Y/Int
simplify(Term, TermS) :-
  bin_op(Term, =<, A, B),
  bin_op(A, *, X, I),
  number(I),
  bin_op(B1, /, B, I),
  bin_op(TermS, =<, X, B1).

% O1(O2(A, I1), I2) -> O2(A, I3) 
simplify(Term, TermS) :-
  bin_op(Term, O1, X, I2), number(I2),
  bin_op(X,    O2, A, I1), number(I1),
  associative(O2), 

  bin_op(Y, O1, I1, I2),
  simplify(Y, I3), number(I3),
  bin_op(TermS, O2, A, I3).

simplify(-A, C) :- number(A), C is -A.
simplify(A+B, C) :- number(A), number(B), C is A+B.
simplify(A-B, C) :- number(A), number(B), C is A-B.
simplify(A*B, C) :- number(A), number(B), C is A*B.
simplify(A/B, C) :- number(A), number(B), C is A/B.

simplify(A*1, A).
simplify(A/1, A).

simplify(T, T).

% To avoid cluttering the rules with recursion, 
% we have this as an extra predicate

simplify_all(Term, TermS) :-
  simplify(Term, Term1),
  bin_op(Term1, F, A, B),
  simplify_all(A, A1),
  simplify_all(B, B1),
  bin_op(TermS, F, A1, B1).
simplify_all(Term, TermS) :-
  simplify(Term, Term1),
  un_op(Term1, F, A),
  simplify_all(A, A1),
  un_op(TermS, F, A1).  
simplify_all(Term, TermS) :-
  simplify(Term, TermS).


simplify_term(Term, TermS) :-
  simplify_all(Term, Simpler),
  Term \= Simpler, !,
  simplify_term(Simpler, TermS).
simplify_term(Term, Term).

%-----------------------------------------------------------------------
% rule base
%-----------------------------------------------------------------------

nested_in(M1, M2) :- string_concat(M2, _, M1).

% loop unrolling
% --------------
unrolled(M, K, annotation(M, wcet_loopbound(Bound)), 
              [annotation(M, wcet_loopbound(New))]) :-
  New is ceiling(Bound/K).

unrolled(M_Loop, K, annotation(M_Annot, wcet_restriction(Term)), NewAnnots) :- 
  replace(Term, M_Loop, M_Loop*K, Term1), 
  (nested_in(M_Annot, M_Loop) -> 
    list_from_to(1, K, Ns),
    sub_string(M_Annot, 0, _, 2, S1), % Workaround until ROSE will embrace {} unrolled blocks
    string_to_atom(S1, M_AnnotBase),

    maplist(unroll_clone(M_AnnotBase, M_Annot, Term1, Ns), Ns, NewAnnots) ;
    NewAnnots = []).

unroll_clone(M_AnnotBase, M_Annot, RestrTerm, Ns, N, NewAnnot) :-
  foldl1([M_AnnotBase, '_', N], string_concat, S1), string_to_atom(S1, M_clone), % Pos. of Restr
  sumterm(M_AnnotBase, Ns, M_sum), % New RestrTerm
  replace(RestrTerm, M_Annot, M_sum, RestrTerm1),    
  NewAnnot = annotation(M_clone, wcet_restriction(RestrTerm1)).

% Construct a term <Sum> that is 'Base_N1+Base_N2+...+Base_Nn'
sumterm(_, [], []) :- trace.
sumterm(Base, [N], Sum) :-
  foldl1([Base, '_', N], string_concat, S),
  string_to_atom(S, Sum).
sumterm(Base, [N|Ns], Sum) :-
  sumterm(Base, Ns, Sums),
  foldl1([Base, '_', N], string_concat, S),
  string_to_atom(S, A),
  Sum = A+Sums.


% loop blocking
% --------------
blocked(M, K, N, annotation(M, wcet_loopbound(Bound)), 
             [annotation(M, wcet_loopbound(New)),
              annotation(Mblk, wcet_loopbound(K))]) :-
  New is ceiling(Bound/K),
  repeat_string('_1', N, S1),
  string_concat(M, S1, S2), string_to_atom(S2, Mblk).

% this rule moves annotations to their new locations deeper in the tree
blocked(M_blk, _, _, annotation(Mann, wcet_restriction(Term)), 
                    [annotation(Mnew, wcet_restriction(Term1))]) :-
  string_concat(M_blk, '_1', M_newroot),
  replace(Mann, M_blk, M_newroot, Mnew),
  replace(Term, M_blk, M_newroot, Term1).

%more complicated: block CNT  LOOP -> assertz(blocked(inner)))

% loop fusion
% --------------
fused(M_orig, M_fused, annotation(M_orig,  Annot), 
                      [annotation(M_fused, Annot)]).

%-----------------------------------------------------------------------
%
%-----------------------------------------------------------------------

apply_transformations([], Annotations, Annotations).
apply_transformations([T|Ts], Annotations, Result) :-
  write('Processing '), write(T), writeln(':'),
  apply_transformation(T, Annotations, AnnotationsT),
  apply_transformations(Ts, AnnotationsT, Result).

apply_transformation(_, [], []).
apply_transformation(T, [A|As], ATs) :-
  write('    '), writeln(A),
  write('  ->'),
  apply(T, A, ATs_trans),
  write(ATs_trans), nl,
  maplist(simplify_term, ATs_trans, ATs_sim), !, 
  ((ATs_trans \= ATs_sim) -> write('  ->'), write(ATs_sim), nl ; true), 
  nl,
  apply_transformation(T, As, ATs_rem),
  append(ATs_sim, ATs_rem, ATs).

apply(Transformation, Annot, NewAnnots) :- 
  bin_op(Transformation, F, Mt, K),
  call(F, Mt, K, Annot, NewAnnots).
apply(Transformation, Annot, NewAnnots) :- 
  tern_op(Transformation, F, Mt, K, N),
  call(F, Mt, K, N, Annot, NewAnnots).
apply(_, A, [A]).
  
%-----------------------------------------------------------------------
% MAIN
%-----------------------------------------------------------------------

transform(InputAnnotation, InputTrace, OutputAnnotations) :-
  open(InputAnnotation, read, _, [alias(rstrm)]),
  read_term(rstrm,Annotations,[double_quotes(string)]),
  close(rstrm),

  open(InputTrace, read, _, [alias(rstrm)]),
  read_term(rstrm,Transformations,[double_quotes(string)]),
  close(rstrm),

  rename_markers(Annotations, Annotations1),   
  apply_transformations(Transformations, Annotations1, R),
  sort(R, R1),

  open(OutputAnnotations, write, _, [alias(wstrm)]),
  write_term(wstrm,R1,[quoted(true),double_quotes(string)]),
  write(wstrm, '.\n'),
  close(wstrm).

main :-
  current_prolog_flag(argv, Argv), 
  append(_, [--|Args], Argv),
  Args = [A1, A2, A3],
  catch(transform(A1, A2, A3), E, (print_message(error, E), fail)),
  halt.

main :-
  writeln('Usage: transform.pl InputAnnotation, InputTrace, OutputAnnotations'),
  halt(1).
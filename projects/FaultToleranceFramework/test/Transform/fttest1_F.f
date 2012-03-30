       PROGRAM main
! Fortran testcase #1 for FT module of ROSE
          IMPLICIT NONE

          REAL :: A
          REAL, DIMENSION(10) :: X, Y, Z
! Initialize X & Y
          call random_seed
          call random_number(A)
          call random_number(X)
          call random_number(Y)
          
!$resiliency
          Z = A*X+Y

          PRINT *, "A: ", A 
          PRINT *, "X: ", X 
          PRINT *, "Y: ", Y 
          PRINT *, "Z: ", Z 
       END

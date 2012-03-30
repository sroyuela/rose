#ifndef UACOMMON_HDR
#define UACOMMON_HDRE

#include <string>

using namespace std;

namespace BA {
     //Type definitions
       typedef unsigned long int RegisterRep;

     //Exception class
          struct BAException : public std::exception {
               string s;
               BAException(string ss) : s(ss) {}
               virtual ~BAException() throw() {}
               const char *what() const throw() {return s.c_str();}
          };
};

#endif

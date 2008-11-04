/*


  This header file is included as a wrapper to get asf
  to compile with g++
*/

#ifndef COMPAT_HPP
#define COMPAT_HPP

#include <string.h>
#include <ctype.h>

// Explicit?
//char *ustrrchr(unsigned char * s, int c ) { return strrchr((const char*)s,c); }

//int ustrlen(unsigned char* s) { return strlen((const char*)s); }
#define ustrlen(s) strlen((const char*)s)

// int ustrcmp(const unsigned char* l,const char*r) { return strcmp((const char*)l,r); }
#define ustrcmp(l,r) strcmp((const char*)l,r)

// char* ustrcpy(unsigned char*l,char*r) { return strcpy((char*)l,r); }
#define ustrcpy(l,r) strcpy((char*)l,r)

#endif // COMPAT_HPP

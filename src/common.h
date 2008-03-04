#ifndef COMMON_H
#define COMMON_H

/*
	defines core types & macros
*/

typedef unsigned char u08;
typedef unsigned short u16;
typedef unsigned long u32;
typedef unsigned __int64 u64;

typedef signed char i08;
typedef signed short i16;
typedef signed long i32;
typedef signed __int64 i64;

/* on win32, hal declarations are function pointers.
   on arm9 target, hal declarations are just normal functions. */

/*#ifdef WIN32
#define HAL(f)	(* f)
#else
#define HAL(f)	f
#endif	*/

#endif

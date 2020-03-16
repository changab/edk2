/** @file
  CRT wrapper head functions for jansson system call.

 Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
 (C) Copyright 2020 Hewlett Packard Enterprise Development LP<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __JANSSON_CRT_LIB_SUPPORT_H__
#define __JANSSON_CRT_LIB_SUPPORT_H__

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>

#define MAX_STRING_SIZE  0x10000000

/** Minimum value for an object of type long long int. **/
#define LLONG_MIN   (-9223372036854775807LL - 1LL)  // -(2^63 - 1)

/** Maximum value for an object of type long long int. **/
#define LLONG_MAX   9223372036854775807LL // 2^63 - 1

//
// OpenSSL relies on explicit configuration for word size in crypto/bn,
// but we want it to be automatically inferred from the target. So we
// bypass what's in <openssl/opensslconf.h> for OPENSSL_SYS_UEFI, and
// define our own here.
//
#ifdef CONFIG_HEADER_BN_H
#error CONFIG_HEADER_BN_H already defined
#endif

#define CONFIG_HEADER_BN_H

#if defined(MDE_CPU_X64) || defined(MDE_CPU_AARCH64) || defined(MDE_CPU_IA64)
//
// With GCC we would normally use SIXTY_FOUR_BIT_LONG, but MSVC needs
// SIXTY_FOUR_BIT, because 'long' is 32-bit and only 'long long' is
// 64-bit. Since using 'long long' works fine on GCC too, just do that.
//
#define SIXTY_FOUR_BIT
#elif defined(MDE_CPU_IA32) || defined(MDE_CPU_ARM) || defined(MDE_CPU_EBC)
#define THIRTY_TWO_BIT
#else
#error Unknown target architecture
#endif

//
// Map all va_xxxx elements to VA_xxx defined in MdePkg/Include/Base.h
//
#if !defined(__CC_ARM) // if va_list is not already defined
#define va_list   VA_LIST
#define va_arg    VA_ARG
#define va_start  VA_START
#define va_end    VA_END
#else // __CC_ARM
#define va_start(Marker, Parameter)   __va_start(Marker, Parameter)
#define va_arg(Marker, TYPE)          __va_arg(Marker, TYPE)
#define va_end(Marker)                ((void)0)
#endif

//
// Definitions for global constants used by CRT library routines
//
#define EINVAL       22               /* Invalid argument */
#define INT_MAX      0x7FFFFFFF       /* Maximum (signed) int value */
#define LONG_MAX     0X7FFFFFFFL      /* max value for a long */
#define LONG_MIN     (-LONG_MAX-1)    /* min value for a long */
#define ULONG_MAX    0xFFFFFFFF       /* Maximum unsigned long value */
#define CHAR_BIT     8                /* Number of bits in a char */

/** Maximum value for an object of type unsigned long long int. **/
#define ULLONG_MAX  0xFFFFFFFFFFFFFFFFULL // 2^64 - 1
/** Maximum value for an object of type unsigned char. **/
#define UCHAR_MAX   255  // 2^8 - 1


//
// Basic types mapping
//
typedef UINTN          size_t;
typedef INTN           ssize_t;
typedef INT32          time_t;
typedef UINT8          __uint8_t;
typedef UINT8          sa_family_t;
typedef UINT32         uid_t;
typedef UINT32         gid_t;
typedef INT32          int32_t;
typedef UINT32         uint32_t;
typedef UINT16         uint16_t;
typedef UINT8          uint8_t;
typedef enum {false, true} bool;

//
// File operations are not required for EFI building,
// so FILE is mapped to VOID * to pass build
//
typedef VOID  *FILE;

//
// Structures Definitions
//
struct tm {
  int   tm_sec;     /* seconds after the minute [0-60] */
  int   tm_min;     /* minutes after the hour [0-59] */
  int   tm_hour;    /* hours since midnight [0-23] */
  int   tm_mday;    /* day of the month [1-31] */
  int   tm_mon;     /* months since January [0-11] */
  int   tm_year;    /* years since 1900 */
  int   tm_wday;    /* days since Sunday [0-6] */
  int   tm_yday;    /* days since January 1 [0-365] */
  int   tm_isdst;   /* Daylight Savings Time flag */
  long  tm_gmtoff;  /* offset from CUT in seconds */
  char  *tm_zone;   /* timezone abbreviation */
};

struct sockaddr {
  __uint8_t    sa_len;       /* total length */
  sa_family_t  sa_family;    /* address family */
  char         sa_data[14];  /* actually longer; address value */
};

//
// Global variables
//
extern int  errno;
extern FILE *stderr;

//
// Function prototypes of CRT Library routines
//
void           *malloc     (size_t);
void           *realloc    (void *, size_t);
void           *calloc     (size_t Num, size_t Size);
void           free        (void *);
void           *memset     (void *, int, size_t);
int            memcmp      (const void *, const void *, size_t);
int            isdigit     (int);
int            isspace     (int);
int            tolower     (int);
int            isupper     (int);
int            isxdigit    (int);
int            isalnum     (int);
void           *memcpy     (void *, const void *, size_t);
void           *memset     (void *, int, size_t);
void           *memchr     (const void *, int, size_t);
int            memcmp      (const void *, const void *, size_t);
void           *memmove    (void *, const void *, size_t);
int            strcmp      (const char *, const char *);
int            strncmp     (const char *, const char *, size_t);
char           *strcpy     (char *, const char *);
size_t         strlen      (const char *);
char           *strcat     (char *, const char *);
char           *strchr     (const char *, int);
int            strcasecmp  (const char *, const char *);
int            strncasecmp (const char *, const char *, size_t);
char           *strncpy    (char *, size_t, const char *, size_t);
int            strncmp     (const char *, const char *, size_t);
char           *strrchr    (const char *, int);
unsigned long  strtoul     (const char *, char **, int);
char *         strstr      (const char *s1 , const char *s2);
long           strtol      (const char *, char **, int);
char           *strerror   (int);
size_t         strspn      (const char *, const char *);
char *         strdup      (const char *str);
char *         strpbrk     (const char *s1, const char *s2);
unsigned long long strtoull(const char * nptr, char ** endptr, int base);
long long      strtoll     (const char * nptr, char ** endptr, int base);
long           strtol      (const char * nptr, char ** endptr, int base);
size_t         strcspn     (const char *, const char *);
int            printf      (const char *, ...);
int            sscanf      (const char *, const char *, ...);
FILE           *fopen      (const char *, const char *);
size_t         fread       (void *, size_t, size_t, FILE *);
size_t         fwrite      (const void *, size_t, size_t, FILE *);
int            fclose      (FILE *);
int            fprintf     (FILE *, const char *, ...);
uid_t          getuid      (void);
uid_t          geteuid     (void);
gid_t          getgid      (void);
gid_t          getegid     (void);
void           qsort       (void *, size_t, size_t, int (*)(const void *, const void *));
char           *getenv     (const char *);
#if defined(__GNUC__) && (__GNUC__ >= 2)
void           abort       (void) __attribute__((__noreturn__));
#else
void           abort       (void);
#endif
int            toupper     (int);
int            Digit2Val   (int);
time_t         time        (time_t *);

//
// Macros that directly map functions to BaseLib, BaseMemoryLib, and DebugLib functions
//
#define strcmp                            AsciiStrCmp
#define memcpy(dest,source,count)         CopyMem(dest,source,(UINTN)(count))
#define memset(dest,ch,count)             SetMem(dest,(UINTN)(count),(UINT8)(ch))
#define memchr(buf,ch,count)              ScanMem8(buf,(UINTN)(count),(UINT8)ch)
#define memcmp(buf1,buf2,count)           (int)(CompareMem(buf1,buf2,(UINTN)(count)))
#define memmove(dest,source,count)        CopyMem(dest,source,(UINTN)(count))
#define strlen(str)                       (size_t)(AsciiStrnLenS(str,MAX_STRING_SIZE))
#define strcpy(strDest,strSource)         AsciiStrCpyS(strDest,(strlen(strSource)+1),strSource)
#define strncpy(strDest,strSource,count)  AsciiStrnCpyS(strDest,(UINTN)count,strSource,(UINTN)count)
#define strncpys(strDest, DestLen, strSource,count)  AsciiStrnCpyS(strDest,DestLen,strSource,(UINTN)count)
#define strcat(strDest,strSource)         AsciiStrCatS(strDest,(strlen(strSource)+strlen(strDest)+1),strSource)
#define strchr(str,ch)                    ScanMem8((VOID *)(str),AsciiStrSize(str),(UINT8)ch)
#define strncmp(string1,string2,count)    (int)(AsciiStrnCmp(string1,string2,(UINTN)(count)))
#define strcasecmp(str1,str2)             (int)AsciiStriCmp(str1,str2)
#define strstr(s1,s2)                     AsciiStrStr(s1,s2)
#define sprintf(buf,...)                  AsciiSPrint(buf,MAX_STRING_SIZE,__VA_ARGS__)
#define snprintf(buf,len,...)             AsciiSPrint(buf,len,__VA_ARGS__)
#define vsnprintf(buf,len,format,marker)  AsciiVSPrint((buf),(len),(format),(marker))
#define assert(expression)
//#define offsetof(type,member)             OFFSET_OF(type,member)
#define atoi(nptr)                        AsciiStrDecimalToUintn(nptr)
#define fabs(x) (((x)<0.0)?(-x):(x))
#define offsetof(type,member)             OFFSET_OF(type,member)

/* Determine if an integer represents character that is a valid double character */
int isdchar (int c);

#define NO_CZMQ 1
#define EOF (-1)

typedef UINT32  curl_off_t;

extern int  errno;

#define ERANGE   34                /* 34   Result too large */

#endif

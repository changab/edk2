/** @file
  CRT wrapper functions for Jansson system call.

  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
 (C) Copyright 2020 Hewlett Packard Enterprise Development LP<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <CrtSupport/JanssonCrtLibSupport.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

int  errno = 0;

/* Determine if a particular character is an alphanumeric character */
int isalnum (int c)
{
  //
  // <alnum> ::= [0-9] | [a-z] | [A-Z]
  //
  return ((('0' <= (c)) && ((c) <= '9')) ||
          (('a' <= (c)) && ((c) <= 'z')) ||
          (('A' <= (c)) && ((c) <= 'Z')));
}

/* Determine if a particular character is a digital character */
int isdchar (int c)
{
  //
  // [0-9] | [e +-.]
  //
  return ((('0' <= (c)) && ((c) <= '9')) ||
          (c == 'e') || (c == 'E') ||
          (c == '+') || (c == '-') ||
          (c == '.'));
}

/* Determine if a particular character is a space character */
int isspace (int c)
{
  //
  // <space> ::= [ ]
  //
  return ((c) == ' ') || ((c) == '\t') || ((c) == '\r') || ((c) == '\n') || ((c) == '\v')  || ((c) == '\f');
}

/* Allocates memory blocks */
void *malloc (size_t size)
{
  return AllocatePool ((UINTN) size);
}

/* De-allocates or frees a memory block */
void free (void *ptr)
{
  //
  // In Standard C, free() handles a null pointer argument transparently. This
  // is not true of FreePool() below, so protect it.
  //
  if (ptr != NULL) {
    FreePool (ptr);
  }
}

/** NetBSD Compatibility Function strdup creates a duplicate copy of a string. **/
char * strdup(const char *str)
{
  size_t len;
  char *copy;

  len = strlen(str) + 1;
  if ((copy = malloc(len)) == NULL)
    return (NULL);
  memcpy(copy, str, len);
  return (copy);
}

/** The toupper function converts a lowercase letter to a corresponding
    uppercase letter.

    @param[in]    c   The character to be converted.

    @return   If the argument is a character for which islower is true and
              there are one or more corresponding characters, as specified by
              the current locale, for which isupper is true, the toupper
              function returns one of the corresponding characters (always the
              same one for any given locale); otherwise, the argument is
              returned unchanged.
**/
int
toupper(
  IN  int c
  )
{
  if ( (c >= 'a') && (c <= 'z') ) {
    c = c - ('a' - 'A');
  }
  return c;
}

int
Digit2Val( int c)
{
  if (((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z'))) {  /* If c is one of [A-Za-z]... */
    c = toupper(c) - 7;   // Adjust so 'A' is ('9' + 1)
  }
  return c - '0';   // Value returned is between 0 and 35, inclusive.
}


/** The strtoll function converts the initial portion of the string pointed to
    by nptr to long long int representation.

    See the description for strtol for more information.

  @return   The strtoll function returns the converted value, if any. If no
            conversion could be performed, zero is returned. If the correct
            value is outside the range of representable values, LLONG_MIN or
            LLONG_MAX is returned (according to the sign of the value, if any),
            and the value of the macro ERANGE is stored in errno.
**/
long long
strtoll(const char * nptr, char ** endptr, int base)
{
  const char *pEnd;
  long long   Result = 0;
  long long   Previous;
  int         temp;
  BOOLEAN     Negative = FALSE;

  pEnd = nptr;

  if((base < 0) || (base == 1) || (base > 36)) {
    if(endptr != NULL) {
    *endptr = NULL;
    }
    return 0;
  }
  // Skip leading spaces.
  while(isspace(*nptr))   ++nptr;

  // Process Subject sequence: optional sign followed by digits.
  if(*nptr == '+') {
    Negative = FALSE;
    ++nptr;
  }
  else if(*nptr == '-') {
    Negative = TRUE;
    ++nptr;
  }

  if(*nptr == '0') {  /* Might be Octal or Hex */
    if(toupper(nptr[1]) == 'X') {   /* Looks like Hex */
      if((base == 0) || (base == 16)) {
        nptr += 2;  /* Skip the "0X"      */
        base = 16;  /* In case base was 0 */
      }
    }
    else {    /* Looks like Octal */
      if((base == 0) || (base == 8)) {
        ++nptr;     /* Skip the leading "0" */
        base = 8;   /* In case base was 0   */
      }
    }
  }
  if(base == 0) {   /* If still zero then must be decimal */
    base = 10;
  }
  if(*nptr  == '0') {
    for( ; *nptr == '0'; ++nptr);  /* Skip any remaining leading zeros */
    pEnd = nptr;
  }

  while( isalnum(*nptr) && ((temp = Digit2Val(*nptr)) < base)) {
    Previous = Result;
    Result = MultS64x64 (Result, base) + (long long int)temp;
    if( Result <= Previous) {   // Detect Overflow
      if(Negative) {
        Result = LLONG_MIN;
      }
      else {
        Result = LLONG_MAX;
      }
      Negative = FALSE;
      errno = ERANGE;
      break;
    }
    pEnd = ++nptr;
  }
  if(Negative) {
    Result = -Result;
  }

  // Save pointer to final sequence
  if(endptr != NULL) {
    *endptr = (char *)pEnd;
  }
  return Result;
}

/** The strtol, strtoll, strtoul, and strtoull functions convert the initial
    portion of the string pointed to by nptr to long int, long long int,
    unsigned long int, and unsigned long long int representation, respectively.
    First, they decompose the input string into three parts: an initial,
    possibly empty, sequence of white-space characters (as specified by the
    isspace function), a subject sequence resembling an integer represented in
    some radix determined by the value of base, and a final string of one or
    more unrecognized characters, including the terminating null character of
    the input string. Then, they attempt to convert the subject sequence to an
    integer, and return the result.

    If the value of base is zero, the expected form of the subject sequence is
    that of an integer constant, optionally preceded
    by a plus or minus sign, but not including an integer suffix. If the value
    of base is between 2 and 36 (inclusive), the expected form of the subject
    sequence is a sequence of letters and digits representing an integer with
    the radix specified by base, optionally preceded by a plus or minus sign,
    but not including an integer suffix. The letters from a (or A) through z
    (or Z) are ascribed the values 10 through 35; only letters and digits whose
    ascribed values are less than that of base are permitted. If the value of
    base is 16, the characters 0x or 0X may optionally precede the sequence of
    letters and digits, following the sign if present.

    The subject sequence is defined as the longest initial subsequence of the
    input string, starting with the first non-white-space character, that is of
    the expected form. The subject sequence contains no characters if the input
    string is empty or consists entirely of white space, or if the first
    non-white-space character is other than a sign or a permissible letter or digit.

    If the subject sequence has the expected form and the value of base is
    zero, the sequence of characters starting with the first digit is
    interpreted as an integer constant. If the subject sequence has the
    expected form and the value of base is between 2 and 36, it is used as the
    base for conversion, ascribing to each letter its value as given above. If
    the subject sequence begins with a minus sign, the value resulting from the
    conversion is negated (in the return type). A pointer to the final string
    is stored in the object pointed to by endptr, provided that endptr is
    not a null pointer.

    In other than the "C" locale, additional locale-specific subject sequence
    forms may be accepted.

    If the subject sequence is empty or does not have the expected form, no
    conversion is performed; the value of nptr is stored in the object pointed
    to by endptr, provided that endptr is not a null pointer.

  @return   The strtol, strtoll, strtoul, and strtoull functions return the
            converted value, if any. If no conversion could be performed, zero
            is returned. If the correct value is outside the range of
            representable values, LONG_MIN, LONG_MAX, LLONG_MIN, LLONG_MAX,
            ULONG_MAX, or ULLONG_MAX is returned (according to the return type
            and sign of the value, if any), and the value of the macro ERANGE
            is stored in errno.
**/
long
strtol(const char * nptr, char ** endptr, int base)
{
  const char *pEnd;
  long        Result = 0;
  long        Previous;
  int         temp;
  BOOLEAN     Negative = FALSE;

  pEnd = nptr;

  if((base < 0) || (base == 1) || (base > 36)) {
    if(endptr != NULL) {
    *endptr = NULL;
    }
    return 0;
  }
  // Skip leading spaces.
  while(isspace(*nptr))   ++nptr;

  // Process Subject sequence: optional sign followed by digits.
  if(*nptr == '+') {
    Negative = FALSE;
    ++nptr;
  }
  else if(*nptr == '-') {
    Negative = TRUE;
    ++nptr;
  }

  if(*nptr == '0') {  /* Might be Octal or Hex */
    if(toupper(nptr[1]) == 'X') {   /* Looks like Hex */
      if((base == 0) || (base == 16)) {
        nptr += 2;  /* Skip the "0X"      */
        base = 16;  /* In case base was 0 */
      }
    }
    else {    /* Looks like Octal */
      if((base == 0) || (base == 8)) {
        ++nptr;     /* Skip the leading "0" */
        base = 8;   /* In case base was 0   */
      }
    }
  }
  if(base == 0) {   /* If still zero then must be decimal */
    base = 10;
  }
  if(*nptr  == '0') {
    for( ; *nptr == '0'; ++nptr);  /* Skip any remaining leading zeros */
    pEnd = nptr;
  }

  while( isalnum(*nptr) && ((temp = Digit2Val(*nptr)) < base)) {
    Previous = Result;
    Result = (Result * base) + (long int)temp;
    if( Result <= Previous) {   // Detect Overflow
      if(Negative) {
        Result = LONG_MIN;
      }
      else {
        Result = LONG_MAX;
      }
      Negative = FALSE;
      errno = ERANGE;
      break;
    }
    pEnd = ++nptr;
  }
  if(Negative) {
    Result = -Result;
  }

  // Save pointer to final sequence
  if(endptr != NULL) {
    *endptr = (char *)pEnd;
  }
  return Result;
}

/** The strtoull function converts the initial portion of the string pointed to
    by nptr to unsigned long long int representation.

    See the description for strtol for more information.

  @return   The strtoull function returns the converted value, if any. If no
            conversion could be performed, zero is returned. If the correct
            value is outside the range of representable values, ULLONG_MAX is
            returned and the value of the macro ERANGE is stored in errno.
**/
unsigned long long
strtoull(const char * nptr, char ** endptr, int base)
{
  const char           *pEnd;
  unsigned long long    Result = 0;
  unsigned long long    Previous;
  int                   temp;

  pEnd = nptr;

  if((base < 0) || (base == 1) || (base > 36)) {
    if(endptr != NULL) {
    *endptr = NULL;
    }
    return 0;
  }
  // Skip leading spaces.
  while(isspace(*nptr))   ++nptr;

  // Process Subject sequence: optional + sign followed by digits.
  if(*nptr == '+') {
    ++nptr;
  }

  if(*nptr == '0') {  /* Might be Octal or Hex */
    if(toupper(nptr[1]) == 'X') {   /* Looks like Hex */
      if((base == 0) || (base == 16)) {
        nptr += 2;  /* Skip the "0X"      */
        base = 16;  /* In case base was 0 */
      }
    }
    else {    /* Looks like Octal */
      if((base == 0) || (base == 8)) {
        ++nptr;     /* Skip the leading "0" */
        base = 8;   /* In case base was 0   */
      }
    }
  }
  if(base == 0) {   /* If still zero then must be decimal */
    base = 10;
  }
  if(*nptr  == '0') {
    for( ; *nptr == '0'; ++nptr);  /* Skip any remaining leading zeros */
    pEnd = nptr;
  }

  while( isalnum(*nptr) && ((temp = Digit2Val(*nptr)) < base)) {
    Previous = Result;
    Result = DivU64x32 (Result, base) + (unsigned long long)temp;
    if( Result < Previous)  {   // If we overflowed
      Result = ULLONG_MAX;
      errno = ERANGE;
      break;
    }
    pEnd = ++nptr;
  }

  // Save pointer to final sequence
  if(endptr != NULL) {
    *endptr = (char *)pEnd;
  }
  return Result;
}


void *
calloc(size_t Num, size_t Size)
{
  void       *RetVal;
  size_t      NumSize;

  NumSize = Num * Size;
  RetVal  = NULL;
  if (NumSize != 0) {
  RetVal = malloc(NumSize);
  if( RetVal != NULL) {
    (VOID)ZeroMem( RetVal, NumSize);
  }
  }
  DEBUG((DEBUG_POOL, "0x%p = calloc(%d, %d)\n", RetVal, Num, Size));

  return RetVal;
}

static UINT8  BitMask[] = {
  0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80
  };

#define WHICH8(c)     ((unsigned char)(c) >> 3)
#define WHICH_BIT(c)  (BitMask[((c) & 0x7)])
#define BITMAP64      ((UINT64 *)bitmap)

static
void
BuildBitmap(unsigned char * bitmap, const char *s2, int n)
{
  unsigned char bit;
  int           index;

  // Initialize bitmap.  Bit 0 is always 1 which corresponds to '\0'
  for (BITMAP64[0] = index = 1; index < n; index++)
    BITMAP64[index] = 0;

  // Set bits in bitmap corresponding to the characters in s2
  for (; *s2 != '\0'; s2++) {
    index = WHICH8(*s2);
    bit = WHICH_BIT(*s2);
    bitmap[index] = bitmap[index] | bit;
  }
}

/** The strpbrk function locates the first occurrence in the string pointed to
    by s1 of any character from the string pointed to by s2.

    @return   The strpbrk function returns a pointer to the character, or a
              null pointer if no character from s2 occurs in s1.
**/
char *
strpbrk(const char *s1, const char *s2)
{
  UINT8 bitmap[ (((UCHAR_MAX + 1) / CHAR_BIT) + (CHAR_BIT - 1)) & ~7U];
  UINT8 bit;
  int index;

  BuildBitmap( bitmap, s2, sizeof(bitmap) / sizeof(UINT64));

  for( ; *s1 != '\0'; ++s1) {
    index = WHICH8(*s1);
    bit = WHICH_BIT(*s1);
    if( (bitmap[index] & bit) != 0) {
      return (char *)s1;
    }
  }
  return NULL;
}

//
//  The arrays give the cumulative number of days up to the first of the
//  month number used as the index (1 -> 12) for regular and leap years.
//  The value at index 13 is for the whole year.
//
UINTN CumulativeDays[2][14] = {
  {
    0,
    0,
    31,
    31 + 28,
    31 + 28 + 31,
    31 + 28 + 31 + 30,
    31 + 28 + 31 + 30 + 31,
    31 + 28 + 31 + 30 + 31 + 30,
    31 + 28 + 31 + 30 + 31 + 30 + 31,
    31 + 28 + 31 + 30 + 31 + 30 + 31 + 31,
    31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30,
    31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31,
    31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30,
    31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30 + 31
  },
  {
    0,
    0,
    31,
    31 + 29,
    31 + 29 + 31,
    31 + 29 + 31 + 30,
    31 + 29 + 31 + 30 + 31,
    31 + 29 + 31 + 30 + 31 + 30,
    31 + 29 + 31 + 30 + 31 + 30 + 31,
    31 + 29 + 31 + 30 + 31 + 30 + 31 + 31,
    31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30,
    31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31,
    31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30,
    31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30 + 31
  }
};

#define IsLeap(y)   (((y) % 4) == 0 && (((y) % 100) != 0 || ((y) % 400) == 0))
#define SECSPERMIN  (60)
#define SECSPERHOUR (60 * 60)
#define SECSPERDAY  (24 * SECSPERHOUR)

/* Get the system time as seconds elapsed since midnight, January 1, 1970. */
//INTN time(
//  INTN *timer
//  )
time_t time (time_t *timer)
{
  EFI_TIME  Time;
  time_t    CalTime;
  UINTN     Year;

  //
  // Get the current time and date information
  //
  gRT->GetTime (&Time, NULL);

  //
  // Years Handling
  // UTime should now be set to 00:00:00 on Jan 1 of the current year.
  //
  for (Year = 1970, CalTime = 0; Year != Time.Year; Year++) {
    CalTime = CalTime + (time_t)(CumulativeDays[IsLeap(Year)][13] * SECSPERDAY);
  }

  //
  // Add in number of seconds for current Month, Day, Hour, Minute, Seconds, and TimeZone adjustment
  //
  CalTime = CalTime +
            (time_t)((Time.TimeZone != EFI_UNSPECIFIED_TIMEZONE) ? (Time.TimeZone * 60) : 0) +
            (time_t)(CumulativeDays[IsLeap(Time.Year)][Time.Month] * SECSPERDAY) +
            (time_t)(((Time.Day > 0) ? Time.Day - 1 : 0) * SECSPERDAY) +
            (time_t)(Time.Hour * SECSPERHOUR) +
            (time_t)(Time.Minute * 60) +
            (time_t)Time.Second;

  if (timer != NULL) {
    *timer = CalTime;
  }

  return CalTime;
}

typedef
int
(*SORT_COMPARE)(
  IN  VOID  *Buffer1,
  IN  VOID  *Buffer2
  );

//
// Duplicated from EDKII BaseSortLib for qsort() wrapper
//
STATIC
VOID
QuickSortWorker (
  IN OUT    VOID          *BufferToSort,
  IN CONST  UINTN         Count,
  IN CONST  UINTN         ElementSize,
  IN        SORT_COMPARE  CompareFunction,
  IN        VOID          *Buffer
  )
{
  VOID        *Pivot;
  UINTN       LoopCount;
  UINTN       NextSwapLocation;

  ASSERT(BufferToSort    != NULL);
  ASSERT(CompareFunction != NULL);
  ASSERT(Buffer          != NULL);

  if (Count < 2 || ElementSize  < 1) {
    return;
  }

  NextSwapLocation = 0;

  //
  // Pick a pivot (we choose last element)
  //
  Pivot = ((UINT8 *)BufferToSort + ((Count - 1) * ElementSize));

  //
  // Now get the pivot such that all on "left" are below it
  // and everything "right" are above it
  //
  for (LoopCount = 0; LoopCount < Count - 1;  LoopCount++)
  {
    //
    // If the element is less than the pivot
    //
    if (CompareFunction ((VOID *)((UINT8 *)BufferToSort + ((LoopCount) * ElementSize)), Pivot) <= 0) {
      //
      // Swap
      //
      CopyMem (Buffer, (UINT8 *)BufferToSort + (NextSwapLocation * ElementSize), ElementSize);
      CopyMem ((UINT8 *)BufferToSort + (NextSwapLocation * ElementSize), (UINT8 *)BufferToSort + ((LoopCount) * ElementSize), ElementSize);
      CopyMem ((UINT8 *)BufferToSort + ((LoopCount) * ElementSize), Buffer, ElementSize);

      //
      // Increment NextSwapLocation
      //
      NextSwapLocation++;
    }
  }
  //
  // Swap pivot to it's final position (NextSwapLocaiton)
  //
  CopyMem (Buffer, Pivot, ElementSize);
  CopyMem (Pivot, (UINT8 *)BufferToSort + (NextSwapLocation * ElementSize), ElementSize);
  CopyMem ((UINT8 *)BufferToSort + (NextSwapLocation * ElementSize), Buffer, ElementSize);

  //
  // Now recurse on 2 paritial lists.  Neither of these will have the 'pivot' element.
  // IE list is sorted left half, pivot element, sorted right half...
  //
  QuickSortWorker (
    BufferToSort,
    NextSwapLocation,
    ElementSize,
    CompareFunction,
    Buffer
    );

  QuickSortWorker (
    (UINT8 *)BufferToSort + (NextSwapLocation + 1) * ElementSize,
    Count - NextSwapLocation - 1,
    ElementSize,
    CompareFunction,
    Buffer
    );

  return;
}

/** Performs a quick sort **/
void qsort (void *base, size_t num, size_t width, int (*compare)(const void *, const void *))
{
  VOID  *Buffer;

  ASSERT (base    != NULL);
  ASSERT (compare != NULL);

  //
  // Use CRT-style malloc to cover BS and RT memory allocation.
  //
  Buffer = malloc (width);
  ASSERT (Buffer != NULL);

  //
  // Re-use PerformQuickSort() function Implementation in EDKII BaseSortLib.
  //
  QuickSortWorker (base, (UINTN)num, (UINTN)width, (SORT_COMPARE)compare, Buffer);

  free (Buffer);
  return;
}

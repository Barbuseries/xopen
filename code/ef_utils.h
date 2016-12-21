#ifndef EF_UTILS_H
#define EF_UTILS_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Types
typedef int8_t		i8;
typedef uint8_t		u8;

typedef int16_t		i16;
typedef uint16_t	u16;

typedef int32_t		i32;
typedef uint32_t	u32;

typedef int64_t		i64;
typedef uint64_t	u64;

typedef float		r32;
typedef double		r64;

typedef int			b32;

// Utilities
#define JOIN_HELPER(x, y) x ## y
#define JOIN(x, y)        JOIN_HELPER(x, y)
#define JOIN3(x, y, z)    JOIN(JOIN(x, y), z)

#define STRINGIFY(x)    #x
#define TO_STRING(x)	STRINGIFY(x)

#define SWAP(type, a, b) do						\
	{											\
		type	ef__temp = a;					\
		a				 = b;					\
		b				 = ef__temp;			\
	}while (0)

#define CLAMP(x, min, max) ((x) < (min)) ? (min) : (x) > (max) ? (max) : (x)
#define MIN(a, b) ((a) < (b)) ? (a) : (b)
#define MAX(a, b) ((a) > (b)) ? (a) : (b)

#define OFFSET_OF(type, f) (size_t) &((type *)0)->f

// Array
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define INDEX_OF(it, arr)	((size_t) ((it) - (arr)))

#define FOR_EACH(type, it, arr) for(type* it = arr;	INDEX_OF(it, arr) < ARRAY_SIZE(arr); ++it)
#define FOR_EACH_IT(type, arr)  FOR_EACH(type, it, arr)

#define FOR_INDEX(index, arr) for(size_t index = 0; index < ARRAY_SIZE(arr); ++index)
#define FOR_I(arr)	          FOR_INDEX(i, arr)
#define FOR_J(arr)	          FOR_INDEX(j, arr)

#define PRINT_N_ARRAY(format, interFormat, arr, n) do					\
	{																	\
		for (int ef__prarr_i = 0; ef__prarr_i < (n - 1); ++ef__prarr_i)	\
		{																\
			printf(format, arr[ef__prarr_i]);							\
			printf("%s", interFormat);									\
		}																\
																		\
		if (n)															\
		{																\
			printf(format, arr[n - 1]);									\
		}																\
																		\
	}while (0)

#define PRINT_ARRAY(format, interformat, arr) PRINT_N_ARRAY(format, interformat, arr, ARRAY_SIZE(arr))


// TODO: Change both functions below to be in O(n).

// NOTE: Copy src rotated rotationCount times to the right into dest.
//       If rotationCount is 0, just copy src into dest (if they are not the same).
// IMPORTANT: Do not forget to either use strlen or to count one less
// when calling this on a nul-terminated string.
static void rotateRightArray(void *dest, void *src, size_t itemSize, size_t itemCount,
					  size_t rotationCount = 1)
{
	if (rotationCount >= itemCount)
	{
		rotationCount %= itemCount;
	}

	if ((rotationCount == 0) && (src != dest))
	{
		memcpy(dest, src, itemSize * itemCount);
	}

	if (rotationCount > 0)
	{	
		u8 tmp[itemSize];
		memcpy(tmp, src + itemSize * (itemCount - 1), 1);

		memcpy(dest + itemSize, src, itemSize * (itemCount - 1));
		memcpy(dest, tmp, 1);
		--rotationCount;
	}

	src = dest;
	while (rotationCount > 0)
	{
		u8 tmp[itemSize];
		memcpy(tmp, src + itemSize * (itemCount - 1), 1);

		memcpy(dest + itemSize, src, itemSize * (itemCount - 1));
		memcpy(dest, tmp, 1);
		--rotationCount;
	}
}

// NOTE: Copy src rotated rotationCount times to the left into dest.
//       If rotationCount is 0, just copy src into dest (if they are not the same).
// IMPORTANT: Do not forget to either use strlen or to count one less
// when calling this on a nul-terminated string.
static void rotateLeftArray(void *dest, void *src, size_t itemSize, size_t itemCount,
					 size_t rotationCount = 1)
{
	if (rotationCount >= itemCount)
	{
		rotationCount %= itemCount;
	}
	
	if ((rotationCount == 0) && (src != dest))
	{
		memcpy(dest, src, itemSize * itemCount);
	}

	if (rotationCount > 0)
	{
		u8 tmp[itemSize];
		memcpy(tmp, src, 1);

		memcpy(dest, src + itemSize, itemSize * (itemCount - 1));
		memcpy(dest + itemSize * (itemCount - 1), tmp, 1);
		--rotationCount;
	}

	src = dest;
	while (rotationCount > 0)
	{
		u8 tmp[itemSize];
		memcpy(tmp, src, 1);

		memcpy(dest, src + itemSize, itemSize * (itemCount - 1));
		memcpy(dest + itemSize * (itemCount - 1), tmp, 1);
		--rotationCount;
	}
}

// Debug
#if EF_DEBUG
#define ASSERT(expression)	do										\
	{																\
		if(!(expression))											\
		{															\
			fprintf(stderr, "ASSERT FAILED : '%s', %s line %d.\n",	\
					#expression, __FILE__, __LINE__);				\
			*(int *)NULL = 0;										\
		}															\
	}while (0)
	
#else
#define ASSERT(expression)
#endif

#endif /* EF_UTILS_H */

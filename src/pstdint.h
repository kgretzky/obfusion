#pragma once

#if (defined _MSC_VER) && (_MSC_VER <= 1300)
// Otherwise define required types.
typedef signed char			int8_t;
typedef signed short		int16_t;
typedef signed int			int32_t;
typedef signed long long int	int64_t;
typedef unsigned char		uint8_t;
typedef unsigned short		uint16_t;
typedef unsigned		uint32_t;
typedef unsigned long long	uint64_t;
#else
#include <stdint.h>
#endif

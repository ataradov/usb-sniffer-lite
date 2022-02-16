// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2022, Alex Taradov <alex@taradov.com>. All rights reserved.

#ifndef _UTILS_H_
#define _UTILS_H_

/*- Includes ----------------------------------------------------------------*/
#include <stdint.h>

/*- Definitions -------------------------------------------------------------*/
#define PACK           __attribute__((packed))
#define WEAK           __attribute__((weak))
#define INLINE         static inline __attribute__((always_inline))
#define LIMIT(a, b)    (((int)(a) > (int)(b)) ? (int)(b) : (int)(a))

/*- Prototypes --------------------------------------------------------------*/
void hw_divmod_u32(uint32_t dividend, uint32_t divisor, uint32_t *quotient, uint32_t *remainder);
void hw_divmod_s32(int32_t dividend, int32_t divisor, int32_t *quotient, int32_t *remainder);
void format_hex(char *buf, uint32_t v, int size);
void format_dec(char *buf, uint32_t v, int size);

#endif // _UTILS_H_

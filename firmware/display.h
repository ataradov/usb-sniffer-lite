// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2022, Alex Taradov <alex@taradov.com>. All rights reserved.

#ifndef _DISPLAY_H_
#define _DISPLAY_H_

/*- Includes ----------------------------------------------------------------*/
#include <stdint.h>

/*- Prototypes --------------------------------------------------------------*/
void display_putc(char c);
void display_puts(const char *s);
void display_puthex(uint32_t v, int size);
void display_putdec(uint32_t v, int size);

void display_buffer(void);

#endif // _DISPLAY_H_

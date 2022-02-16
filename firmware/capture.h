// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2022, Alex Taradov <alex@taradov.com>. All rights reserved.

#ifndef _CAPTURE_H_
#define _CAPTURE_H_

/*- Includes ----------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/*- Definitions -------------------------------------------------------------*/
#define CAPTURE_ERROR_STUFF    (1 << 31)
#define CAPTURE_ERROR_CRC      (1 << 30)
#define CAPTURE_ERROR_PID      (1 << 29)
#define CAPTURE_ERROR_SYNC     (1 << 28)
#define CAPTURE_ERROR_NBIT     (1 << 27)
#define CAPTURE_ERROR_SIZE     (1 << 26)
#define CAPTURE_RESET          (1 << 25)
#define CAPTURE_LS_SOF         (1 << 24)
#define CAPTURE_MAY_FOLD       (1 << 23)

#define CAPTURE_ERROR_MASK     (CAPTURE_ERROR_STUFF | CAPTURE_ERROR_CRC | \
    CAPTURE_ERROR_PID | CAPTURE_ERROR_SYNC | CAPTURE_ERROR_NBIT | CAPTURE_ERROR_SIZE)

#define CAPTURE_SIZE_MASK      0xffff

/*- Types -------------------------------------------------------------------*/
typedef struct
{
  bool     fs;
  bool     trigger;
  int      limit;
  int      count;
  int      errors;
  int      resets;
  int      frames;
  int      folded;
} buffer_info_t;

/*- Prototypes --------------------------------------------------------------*/
void capture_init(void);
void capture_command(int cmd);

#endif // _CAPTURE_H_

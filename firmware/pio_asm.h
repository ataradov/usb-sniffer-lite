// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021, Alex Taradov <alex@taradov.com>. All rights reserved.

#ifndef _PIO_ASM_H_
#define _PIO_ASM_H_

/*- Defines -----------------------------------------------------------------*/
#define OP_JMP               (0 << 13)
#define OP_WAIT              (1 << 13)
#define OP_IN                (2 << 13)
#define OP_OUT               (3 << 13)
#define OP_PUSH              (4 << 13) | (0 << 7)
#define OP_PULL              (4 << 13) | (1 << 7)
#define OP_MOV               (5 << 13)
#define OP_IRQ               (6 << 13)
#define OP_SET               (7 << 13)

#define OP_NOP               OP_MOV | MOV_DST_Y | MOV_SRC_Y

#define OP_DELAY(x)          ((x) << 8)

#define OP_SIDE1(x)          ((x) << 12)
#define OP_SIDE2(x)          ((x) << 11)
#define OP_SIDE3(x)          ((x) << 10)
#define OP_SIDE4(x)          ((x) << 9)
#define OP_SIDE5(x)          ((x) << 8)

#define OP_SIDE1_EN(x)       (1 << 12) | ((x) << 11)
#define OP_SIDE2_EN(x)       (1 << 12) | ((x) << 10)
#define OP_SIDE3_EN(x)       (1 << 12) | ((x) << 9)
#define OP_SIDE4_EN(x)       (1 << 12) | ((x) << 8)

#define JMP_COND_ALWAYS      (0 << 5) // (no condition): Always
#define JMP_COND_X_ZERO      (1 << 5) // !X: scratch X zero
#define JMP_COND_X_NZ_PD     (2 << 5) // X--: scratch X non-zero, post-decrement
#define JMP_COND_Y_ZERO      (3 << 5) // !Y: scratch Y zero
#define JMP_COND_Y_NZ_PD     (4 << 5) // Y--: scratch Y non-zero, post-decrement
#define JMP_COND_X_NE_Y      (5 << 5) // X!=Y: scratch X not equal scratch Y
#define JMP_COND_PIN         (6 << 5) // PIN: branch on input pin
#define JMP_COND_N_OSRE      (7 << 5) // !OSRE: output shift register not empty

#define JMP_ADDR(x)          ((x) << 0)

#define WAIT_POL_0           (0 << 7) // wait for a 0
#define WAIT_POL_1           (1 << 7) // wait for a 1

#define WAIT_SRC_GPIO        (0 << 5) // System GPIO input selected by Index.
#define WAIT_SRC_PIN         (1 << 5) // Input pin selected by Index.
#define WAIT_SRC_IRQ         (2 << 5) // PIO IRQ flag selected by Index.

#define WAIT_INDEX(x)        ((x) << 0)

#define IN_SRC_PINS          (0 << 5) // PINS
#define IN_SRC_X             (1 << 5) // X (scratch register X)
#define IN_SRC_Y             (2 << 5) // Y (scratch register Y)
#define IN_SRC_NULL          (3 << 5) // NULL (all zeroes)
#define IN_SRC_ISR           (6 << 5) // ISR
#define IN_SRC_OSR           (7 << 5) // OSR

#define IN_CNT(x)            (((x) == 32) ? 0 : (x))

#define OUT_DST_PINS         (0 << 5) // PINS
#define OUT_DST_X            (1 << 5) // X (scratch register X)
#define OUT_DST_Y            (2 << 5) // Y (scratch register Y)
#define OUT_DST_NULL         (3 << 5) // NULL (discard data)
#define OUT_DST_PINDIRS      (4 << 5) // PINDIRS
#define OUT_DST_PC           (5 << 5) // PC
#define OUT_DST_ISR          (6 << 5) // ISR (also sets ISR shift counter to Bit count)
#define OUT_DST_EXEC         (7 << 5) // EXEC (Execute OSR shift data as instruction)

#define OUT_CNT(x)           (((x) == 32) ? 0 : (x))

#define PUSH_IF_FULL         (1 << 6) // If 1, do nothing unless the total input shift count has reached its threshold, SHIFTCTRL_PUSH_THRESH.
#define PUSH_BLOCK           (1 << 5) // If 1, stall execution if RX FIFO is full.

#define PULL_IF_EMPTY        (1 << 6) // If 1, do nothing unless the total output shift count has reached its threshold, SHIFTCTRL_PULL_THRESH.
#define PULL_BLOCK           (1 << 5) // If 1, stall if TX FIFO is empty. If 0, pulling from an empty FIFO copies scratch X to OSR.

#define MOV_DST_PINS         (0 << 5) // PINS (Uses same pin mapping as OUT)
#define MOV_DST_X            (1 << 5) // X (Scratch register X)
#define MOV_DST_Y            (2 << 5) // Y (Scratch register Y)
#define MOV_DST_EXEC         (4 << 5) // EXEC (Execute data as instruction)
#define MOV_DST_PC           (5 << 5) // PC
#define MOV_DST_ISR          (6 << 5) // ISR (Input shift counter is reset to 0 by this operation, i.e. empty)
#define MOV_DST_OSR          (7 << 5) // OSR (Output shift counter is reset to 0 by this operation, i.e. full)

#define MOV_OP_NONE          (0 << 3) // None
#define MOV_OP_INVERT        (1 << 3) // Invert (bitwise complement)
#define MOV_OP_BIT_REV       (2 << 3) // Bit-reverse

#define MOV_SRC_PINS         (0 << 0) // PINS (Uses same pin mapping as IN)
#define MOV_SRC_X            (1 << 0) // X
#define MOV_SRC_Y            (2 << 0) // Y
#define MOV_SRC_NULL         (3 << 0) // NULL
#define MOV_SRC_STATUS       (5 << 0) // STATUS
#define MOV_SRC_ISR          (6 << 0) // ISR
#define MOV_SRC_OSR          (7 << 0) // OSR

#define IRQ_CLEAR            (1 << 6) // if 1, clear the flag selected by Index, instead of raising it. If Clear is set, the Wait bit has no effect.
#define IRQ_WAIT             (1 << 5) // if 1, halt until the raised flag is lowered again, e.g. if a system interrupt handler has acknowledged the flag.

#define IRQ_INDEX(x)         ((x) << 0)

#define SET_DST_PINS         (0 << 5) // PINS
#define SET_DST_X            (1 << 5) // X (scratch register X) 5 LSBs are set to Data, all others cleared to 0.
#define SET_DST_Y            (2 << 5) // Y (scratch register Y) 5 LSBs are set to Data, all others cleared to 0.
#define SET_DST_PINDIRS      (4 << 5) // PINDIRS

#define SET_DATA(x)          ((x) << 0)

#endif // _PIO_ASM_H_



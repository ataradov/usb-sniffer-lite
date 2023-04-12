// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2022, Alex Taradov <alex@taradov.com>. All rights reserved.

/*- Includes ----------------------------------------------------------------*/
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "rp2040.h"
#include "display.h"
#include "capture.h"
#include "globals.h"
#include "utils.h"

/*- Definitions -------------------------------------------------------------*/
#define ERROR_DATA_SIZE_LIMIT  16
#define MAX_PACKET_DELTA       10000 // us

/*- Variables ---------------------------------------------------------------*/
static uint32_t g_ref_time;
static uint32_t g_prev_time;
static bool g_check_delta;
static bool g_folding;
static int g_fold_count;
static int g_display_ptr;

/*- Implementations ---------------------------------------------------------*/

//-----------------------------------------------------------------------------
void display_putc(char c)
{
  while (0 == (SIO->FIFO_ST & SIO_FIFO_ST_RDY_Msk));
  SIO->FIFO_WR = c;
}

//-----------------------------------------------------------------------------
void display_puts(const char *s)
{
  while (*s)
    display_putc(*s++);
}

//-----------------------------------------------------------------------------
void display_puthex(uint32_t v, int size)
{
  char buf[16];
  format_hex(buf, v, size);
  display_puts(buf);
}

//-----------------------------------------------------------------------------
void display_putdec(uint32_t v, int size)
{
  char buf[16];
  format_dec(buf, v, size);
  display_puts(buf);
}

//-----------------------------------------------------------------------------
static void print_errors(uint32_t flags, uint8_t *data, int size)
{
  flags &= CAPTURE_ERROR_MASK;

  display_puts("ERROR [");

  while (flags)
  {
    int bit = (flags & ~(flags-1));

    if (bit == CAPTURE_ERROR_STUFF)
      display_puts("STUFF");
    else if (bit == CAPTURE_ERROR_CRC)
      display_puts("CRC");
    else if (bit == CAPTURE_ERROR_PID)
      display_puts("PID");
    else if (bit == CAPTURE_ERROR_SYNC)
      display_puts("SYNC");
    else if (bit == CAPTURE_ERROR_NBIT)
      display_puts("NBIT");
    else if (bit == CAPTURE_ERROR_SIZE)
      display_puts("SIZE");

    flags &= ~bit;

    if (flags)
      display_puts(", ");
  }

  display_puts("]: ");

  if (size > 0)
  {
    display_puts("SYNC = 0x");
    display_puthex(data[0], 2);
    display_puts(", ");
  }

  if (size > 1)
  {
    display_puts("PID = 0x");
    display_puthex(data[1], 2);
    display_puts(", ");
  }

  if (size > 2)
  {
    bool limited = false;

    display_puts("DATA: ");

    if (size > ERROR_DATA_SIZE_LIMIT)
    {
      size = ERROR_DATA_SIZE_LIMIT;
      limited = true;
    }

    for (int i = 2; i < size; i++)
    {
      display_puthex(data[i], 2);
      display_putc(' ');
    }

    if (limited)
      display_puts("...");
  }

  display_puts("\r\n");
}

//-----------------------------------------------------------------------------
static void print_sof(uint8_t *data)
{
  int frame = ((data[3] << 8) | data[2]) & 0x7ff;
  display_puts("SOF #");
  display_putdec(frame, 0);
  display_puts("\r\n");
}

//-----------------------------------------------------------------------------
static void print_handshake(char *pid)
{
  display_puts(pid);
  display_puts("\r\n");
}

//-----------------------------------------------------------------------------
static void print_in_out_setup(char *pid, uint8_t *data)
{
  int v = (data[3] << 8) | data[2];
  int addr = v & 0x7f;
  int ep = (v >> 7) & 0xf;

  display_puts(pid);
  display_puts(": 0x");
  display_puthex(addr, 2);
  display_puts("/");
  display_puthex(ep, 1);
  display_puts("\r\n");
}

//-----------------------------------------------------------------------------
static void print_split(uint8_t *data)
{
  int addr = data[2] & 0x7f;
  int sc   = (data[2] >> 7) & 1;
  int port = data[3] & 0x7f;
  int s    = (data[3] >> 7) & 1;
  int e    = data[4] & 1;
  int et   = (data[4] >> 1) & 3;

  display_puts("SPLIT: HubAddr=0x");
  display_puthex(addr, 2);
  display_puts(", SC=");
  display_puthex(sc, 1);
  display_puts(", Port=");
  display_puthex(port, 2);
  display_puts(", S=");
  display_puthex(s, 1);
  display_puts(", E=");
  display_puthex(e, 1);
  display_puts(", ET=");
  display_puthex(et, 1);
  display_puts("\r\n");
}

//-----------------------------------------------------------------------------
static void print_simple(char *text)
{
  display_puts(text);
  display_puts("\r\n");
}

//-----------------------------------------------------------------------------
static void print_data(char *pid, uint8_t *data, int size)
{
  size -= 4;

  display_puts(pid);

  if (size == 0)
  {
    display_puts(": ZLP\r\n");
  }
  else
  {
    int limited = size;

    if (g_display_data == DisplayData_None)
      limited = 0;
    else if (g_display_data == DisplayData_Limit16)
      limited = LIMIT(size, 16);
    else if (g_display_data == DisplayData_Limit64)
      limited = LIMIT(size, 64);

    display_puts(" (");
    display_putdec(size, 0);
    display_puts("): ");

    for (int j = 0; j < limited; j++)
    {
      display_puthex(data[j+2], 2);
      display_putc(' ');
    }

    if (limited < size)
      display_puts("...");

    display_puts("\r\n");
  }
}

//-----------------------------------------------------------------------------
static void print_g_fold_count(int count)
{
  display_puts("   ... : Folded ");

  if (count == 1)
  {
    display_puts("1 frame");
  }
  else
  {
    display_putdec(count, 0);
    display_puts(" frames");
  }

  display_puts("\r\n");
}

//-----------------------------------------------------------------------------
static void print_reset(void)
{
  display_puts("--- RESET ---\r\n");
}

//-----------------------------------------------------------------------------
static void print_ls_sof(void)
{
  display_puts("LS SOF\r\n");
}

//-----------------------------------------------------------------------------
static void print_time(int time)
{
  display_putdec(time, 6);
  display_puts(" : ");
}

//-----------------------------------------------------------------------------
static bool print_packet(void)
{
  int flags = g_buffer[g_display_ptr];
  int time  = g_buffer[g_display_ptr+1];
  int ftime = time - g_ref_time;
  int delta = time - g_prev_time;
  int size  = flags & CAPTURE_SIZE_MASK;
  uint8_t *payload = (uint8_t *)&g_buffer[g_display_ptr+2];
  int pid = payload[1] & 0x0f;

  if (g_check_delta && delta > MAX_PACKET_DELTA)
  {
    display_puts("Time delta between packets is too large, possible buffer corruption.\r\n");
    return false;
  }

  g_display_ptr += (((size+3)/4) + 2);

  g_prev_time = time;
  g_check_delta = true;

  if (flags & CAPTURE_LS_SOF)
    pid = Pid_Sof;

  if ((g_display_time == DisplayTime_SOF && pid == Pid_Sof) || (g_display_time == DisplayTime_Previous))
    g_ref_time = time;

  if (g_folding)
  {
    if (pid != Pid_Sof)
      return true;

    if (flags & CAPTURE_MAY_FOLD)
    {
      g_fold_count++;
      return true;
    }

    print_g_fold_count(g_fold_count);
    g_folding = false;
  }

  if (flags & CAPTURE_MAY_FOLD && g_display_fold == DisplayFold_Enabled)
  {
    g_folding = true;
    g_fold_count = 1;
    return true;
  }

  print_time(ftime);

  if (flags & CAPTURE_RESET)
  {
    print_reset();

    if (g_display_time == DisplayTime_Reset)
      g_ref_time = time;

    g_check_delta = false;

    return true;
  }

  if (flags & CAPTURE_LS_SOF)
  {
    print_ls_sof();
    return true;
  }

  if (flags & CAPTURE_ERROR_MASK)
  {
    print_errors(flags, payload, size);
    return true;
  }

  if (pid == Pid_Sof)
    print_sof(payload);
  else if (pid == Pid_In)
    print_in_out_setup("IN", payload);
  else if (pid == Pid_Out)
    print_in_out_setup("OUT", payload);
  else if (pid == Pid_Setup)
    print_in_out_setup("SETUP", payload);

  else if (pid == Pid_Ack)
    print_handshake("ACK");
  else if (pid == Pid_Nak)
    print_handshake("NAK");
  else if (pid == Pid_Stall)
    print_handshake("STALL");
  else if (pid == Pid_Nyet)
    print_handshake("NYET");

  else if (pid == Pid_Data0)
    print_data("DATA0", payload, size);
  else if (pid == Pid_Data1)
    print_data("DATA1", payload, size);
  else if (pid == Pid_Data2)
    print_data("DATA2", payload, size);
  else if (pid == Pid_MData)
    print_data("MDATA", payload, size);

  else if (pid == Pid_Ping)
    print_simple("PING");
  else if (pid == Pid_PreErr)
    print_simple("PRE/ERR");
  else if (pid == Pid_Split)
    print_split(payload);
  else if (pid == Pid_Reserved)
    print_simple("RESERVED");

  return true;
}

//-----------------------------------------------------------------------------
void display_value(int value, char *name)
{
  display_putdec(value, 0);
  display_putc(' ');
  display_puts(name);

  if (value != 1)
    display_putc('s');
}

//-----------------------------------------------------------------------------
void display_buffer(void)
{
  if (g_buffer_info.count == 0)
  {
    display_puts("\r\nCapture buffer is empty\r\n");
    return;
  }

  display_puts("\r\nCapture buffer:\r\n");

  g_ref_time    = g_buffer[1];
  g_prev_time   = g_buffer[1];
  g_folding     = false;
  g_check_delta = true;
  g_fold_count  = 0;
  g_display_ptr = 0;

  for (int i = 0; i < g_buffer_info.count; i++)
  {
    if (!print_packet())
      break;
  }

  if (g_folding && g_fold_count)
    print_g_fold_count(g_fold_count);

  display_puts("\r\n");
  display_puts("Total: ");
  display_value(g_buffer_info.errors, "error");
  display_puts(", ");
  display_value(g_buffer_info.resets, "bus reset");
  display_puts(", ");
  display_value(g_buffer_info.count, g_buffer_info.fs ? "FS packet" : "LS packet");
  display_puts(", ");
  display_value(g_buffer_info.frames, "frame");
  display_puts(", ");
  display_value(g_buffer_info.folded, "empty frame");
  display_puts("\r\n\r\n");
}

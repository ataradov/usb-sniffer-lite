// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2022, Alex Taradov <alex@taradov.com>. All rights reserved.

#ifndef _GLOBALS_H_
#define _GLOBALS_H_

/*- Includes ----------------------------------------------------------------*/
#include <stdint.h>

/*- Definitions -------------------------------------------------------------*/
#define BUFFER_SIZE            ((232*1024) / (int)sizeof(uint32_t))

enum
{
  Pid_Reserved = 0,

  Pid_Out      = 1,
  Pid_In       = 9,
  Pid_Sof      = 5,
  Pid_Setup    = 13,

  Pid_Data0    = 3,
  Pid_Data1    = 11,
  Pid_Data2    = 7,
  Pid_MData    = 15,

  Pid_Ack      = 2,
  Pid_Nak      = 10,
  Pid_Stall    = 14,
  Pid_Nyet     = 6,

  Pid_PreErr   = 12,
  Pid_Split    = 8,
  Pid_Ping     = 4,
};

enum
{
  CaptureSpeed_Low,
  CaptureSpeed_Full,
  CaptureSpeedCount,
};

enum
{
  CaptureTrigger_Enabled,
  CaptureTrigger_Disabled,
  CaptureTriggerCount,
};

enum
{
  CaptureLimit_100,
  CaptureLimit_200,
  CaptureLimit_500,
  CaptureLimit_1000,
  CaptureLimit_2000,
  CaptureLimit_5000,
  CaptureLimit_10000,
  CaptureLimit_Unlimited,
  CaptureLimitCount,
};

enum
{
  DisplayTime_First,
  DisplayTime_Previous,
  DisplayTime_SOF,
  DisplayTime_Reset,
  DisplayTimeCount,
};

enum
{
  DisplayData_None,
  DisplayData_Limit16,
  DisplayData_Limit64,
  DisplayData_Full,
  DisplayDataCount,
};

enum
{
  DisplayFold_Enabled,
  DisplayFold_Disabled,
  DisplayFoldCount,
};

/*- Variables ---------------------------------------------------------------*/
extern uint32_t g_buffer[BUFFER_SIZE];
extern buffer_info_t g_buffer_info;

extern int g_capture_speed;
extern int g_capture_trigger;
extern int g_capture_limit;
extern int g_display_time;
extern int g_display_data;
extern int g_display_fold;

/*- Prototypes --------------------------------------------------------------*/
void set_error(bool error);

#endif // _GLOBALS_H_

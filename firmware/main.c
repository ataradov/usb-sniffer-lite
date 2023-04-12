// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2022, Alex Taradov <alex@taradov.com>. All rights reserved.

/*- Includes ----------------------------------------------------------------*/
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "rp2040.h"
#include "hal_gpio.h"
#include "capture.h"
#include "globals.h"
#include "utils.h"
#include "usb.h"

/*- Definitions -------------------------------------------------------------*/
#define F_REF      12000000
#define F_CPU      120000000
#define F_PER      120000000
#define F_RTC      (F_REF / 256)
#define F_TICK     1000000

#define USB_BUFFER_SIZE    64
#define VCP_TIMEOUT        10000 // us
#define STATUS_TIMEOUT     500000 // us

HAL_GPIO_PIN(LED_O, 0, 25, sio_25)
HAL_GPIO_PIN(LED_R, 0, 26, sio_26)

/*- Variables ---------------------------------------------------------------*/
static uint8_t app_recv_buffer[USB_BUFFER_SIZE];
static uint8_t app_send_buffer[USB_BUFFER_SIZE];
static int app_send_buffer_ptr = 0;
static bool app_send_zlp = false;
static bool app_send_pending = false;
static bool app_recv_pending = false;
static bool app_vcp_open = false;

/*- Implementations ---------------------------------------------------------*/

//-----------------------------------------------------------------------------
static void sys_init(void)
{
  // Enable XOSC
  XOSC->CTRL     = (XOSC_CTRL_FREQ_RANGE_1_15MHZ << XOSC_CTRL_FREQ_RANGE_Pos);
  XOSC->STARTUP  = 47; // ~1 ms @ 12 MHz
  XOSC_SET->CTRL = (XOSC_CTRL_ENABLE_ENABLE << XOSC_CTRL_ENABLE_Pos);
  while (0 == (XOSC->STATUS & XOSC_STATUS_STABLE_Msk));

  // Setup SYS PLL for 12 MHz * 40 / 4 / 1 = 120 MHz
  RESETS_CLR->RESET = RESETS_RESET_pll_sys_Msk;
  while (0 == RESETS->RESET_DONE_b.pll_sys);

  PLL_SYS->CS = (1 << PLL_SYS_CS_REFDIV_Pos);
  PLL_SYS->FBDIV_INT = 40;
  PLL_SYS->PRIM = (4 << PLL_SYS_PRIM_POSTDIV1_Pos) | (1 << PLL_SYS_PRIM_POSTDIV2_Pos);

  PLL_SYS_CLR->PWR = PLL_SYS_PWR_VCOPD_Msk | PLL_SYS_PWR_PD_Msk;
  while (0 == PLL_SYS->CS_b.LOCK);

  PLL_SYS_CLR->PWR = PLL_SYS_PWR_POSTDIVPD_Msk;

  // Setup USB PLL for 12 MHz * 36 / 3 / 3 = 48 MHz
  RESETS_CLR->RESET = RESETS_RESET_pll_usb_Msk;
  while (0 == RESETS->RESET_DONE_b.pll_usb);

  PLL_USB->CS = (1 << PLL_SYS_CS_REFDIV_Pos);
  PLL_USB->FBDIV_INT = 36;
  PLL_USB->PRIM = (3 << PLL_SYS_PRIM_POSTDIV1_Pos) | (3 << PLL_SYS_PRIM_POSTDIV2_Pos);

  PLL_USB_CLR->PWR = PLL_SYS_PWR_VCOPD_Msk | PLL_SYS_PWR_PD_Msk;
  while (0 == PLL_USB->CS_b.LOCK);

  PLL_USB_CLR->PWR = PLL_SYS_PWR_POSTDIVPD_Msk;

  // Switch clocks to their final socurces
  CLOCKS->CLK_REF_CTRL = (CLOCKS_CLK_REF_CTRL_SRC_xosc_clksrc << CLOCKS_CLK_REF_CTRL_SRC_Pos);

  CLOCKS->CLK_SYS_CTRL = (CLOCKS_CLK_SYS_CTRL_AUXSRC_clksrc_pll_sys << CLOCKS_CLK_SYS_CTRL_AUXSRC_Pos);
  CLOCKS_SET->CLK_SYS_CTRL = (CLOCKS_CLK_SYS_CTRL_SRC_clksrc_clk_sys_aux << CLOCKS_CLK_SYS_CTRL_SRC_Pos);

  CLOCKS->CLK_PERI_CTRL = CLOCKS_CLK_PERI_CTRL_ENABLE_Msk |
      (CLOCKS_CLK_PERI_CTRL_AUXSRC_clk_sys << CLOCKS_CLK_PERI_CTRL_AUXSRC_Pos);

  CLOCKS->CLK_USB_CTRL = CLOCKS_CLK_USB_CTRL_ENABLE_Msk |
      (CLOCKS_CLK_USB_CTRL_AUXSRC_clksrc_pll_usb << CLOCKS_CLK_USB_CTRL_AUXSRC_Pos);

  CLOCKS->CLK_ADC_CTRL = CLOCKS_CLK_ADC_CTRL_ENABLE_Msk |
      (CLOCKS_CLK_ADC_CTRL_AUXSRC_clksrc_pll_usb << CLOCKS_CLK_ADC_CTRL_AUXSRC_Pos);

  CLOCKS->CLK_RTC_DIV = (256 << CLOCKS_CLK_RTC_DIV_INT_Pos); // 12MHz / 256 = 46875 Hz
  CLOCKS->CLK_RTC_CTRL = CLOCKS_CLK_RTC_CTRL_ENABLE_Msk |
      (CLOCKS_CLK_RTC_CTRL_AUXSRC_xosc_clksrc << CLOCKS_CLK_ADC_CTRL_AUXSRC_Pos);

  // Configure 1 us tick for watchdog and timer
  WATCHDOG->TICK = ((F_REF/F_TICK) << WATCHDOG_TICK_CYCLES_Pos) | WATCHDOG_TICK_ENABLE_Msk;

  // Enable GPIOs
  RESETS_CLR->RESET = RESETS_RESET_io_bank0_Msk | RESETS_RESET_pads_bank0_Msk;
  while (0 == RESETS->RESET_DONE_b.io_bank0 || 0 == RESETS->RESET_DONE_b.pads_bank0);
}

//-----------------------------------------------------------------------------
static void serial_number_init(void)
{
  volatile uint8_t *uid = (volatile uint8_t *)0x20041f01;
  uint32_t sn = 5381;

  for (int i = 0; i < 16; i++)
    sn = ((sn << 5) + sn) ^ uid[i];

  for (int i = 0; i < 8; i++)
    usb_serial_number[i] = "0123456789ABCDEF"[(sn >> (i * 4)) & 0xf];

  usb_serial_number[8] = 0;
}

//-----------------------------------------------------------------------------
static void timer_init(void)
{
  RESETS_CLR->RESET = RESETS_RESET_timer_Msk;
  while (0 == RESETS->RESET_DONE_b.timer);

  TIMER->ALARM0 = TIMER->TIMELR + STATUS_TIMEOUT;
}

//-----------------------------------------------------------------------------
static void status_timer_task(void)
{
  if (TIMER->INTR & TIMER_INTR_ALARM_0_Msk)
  {
    TIMER->INTR = TIMER_INTR_ALARM_0_Msk;
    TIMER->ALARM0 = TIMER->TIMELR + STATUS_TIMEOUT;
    HAL_GPIO_LED_O_toggle();
  }
}

//-----------------------------------------------------------------------------
static void reset_vcp_timeout(void)
{
  TIMER->ALARM1 = TIMER->TIMELR + VCP_TIMEOUT;
}

//-----------------------------------------------------------------------------
static bool vcp_timeout(void)
{
  if (TIMER->INTR & TIMER_INTR_ALARM_1_Msk)
  {
    TIMER->INTR = TIMER_INTR_ALARM_1_Msk;
    return true;
  }
  return false;
}

//-----------------------------------------------------------------------------
static void send_buffer(void)
{
  usb_cdc_send(app_send_buffer, app_send_buffer_ptr);

  app_send_zlp = (USB_BUFFER_SIZE == app_send_buffer_ptr);
  app_send_pending = true;
  app_send_buffer_ptr = 0;
}

//-----------------------------------------------------------------------------
void usb_cdc_send_callback(void)
{
  app_send_pending = false;
}

//-----------------------------------------------------------------------------
static void display_task(void)
{
  if (!app_vcp_open)
  {
    while (SIO->FIFO_ST & SIO_FIFO_ST_VLD_Msk)
      (void)SIO->FIFO_RD;
    return;
  }

  if (app_send_pending)
    return;

  while (SIO->FIFO_ST & SIO_FIFO_ST_VLD_Msk)
  {
    app_send_buffer[app_send_buffer_ptr++] = SIO->FIFO_RD;

    reset_vcp_timeout();

    if (USB_BUFFER_SIZE == app_send_buffer_ptr)
    {
      send_buffer();
      break;
    }
  }
}

//-----------------------------------------------------------------------------
static void vcp_timer_task(void)
{
  if (vcp_timeout())
  {
    if (app_send_zlp || app_send_buffer_ptr)
      send_buffer();
  }
}

//-----------------------------------------------------------------------------
void usb_cdc_line_coding_updated(usb_cdc_line_coding_t *line_coding)
{
  (void)line_coding;
}

//-----------------------------------------------------------------------------
void usb_cdc_control_line_state_update(int line_state)
{
  app_vcp_open = (line_state & USB_CDC_CTRL_SIGNAL_DTE_PRESENT);

  if (!app_vcp_open)
    return;

  app_send_buffer_ptr = 0;

  capture_command('h');

  if (!app_recv_pending)
  {
    app_recv_pending = true;
    usb_cdc_recv(app_recv_buffer, sizeof(app_recv_buffer));
  }
}

//-----------------------------------------------------------------------------
static char lower(char c)
{
  if ('A' <= c && c <= 'Z')
    return c - ('A' - 'a');
  return c;
}

//-----------------------------------------------------------------------------
void usb_cdc_recv_callback(int size)
{
  app_recv_pending = false;

  if (!app_vcp_open)
    return;

  for (int i = 0; i < size; i++)
    capture_command(lower(app_recv_buffer[i]));

  app_recv_pending = true;
  usb_cdc_recv(app_recv_buffer, sizeof(app_recv_buffer));
}

//-----------------------------------------------------------------------------
bool usb_class_handle_request(usb_request_t *request)
{
  if (usb_cdc_handle_request(request))
    return true;
  else
    return false;
}

//-----------------------------------------------------------------------------
void usb_configuration_callback(int config)
{
  (void)config;
}

//-----------------------------------------------------------------------------
void set_error(bool error)
{
  HAL_GPIO_LED_R_write(error);
}

//-----------------------------------------------------------------------------
int main(void)
{
  sys_init();
  timer_init();
  usb_init();
  usb_cdc_init();
  serial_number_init();
  capture_init();

  HAL_GPIO_LED_O_out();
  HAL_GPIO_LED_O_clr();

  HAL_GPIO_LED_R_out();
  HAL_GPIO_LED_R_clr();

  while (1)
  {
    usb_task();
    display_task();
    vcp_timer_task();
    status_timer_task();
  }

  return 0;
}

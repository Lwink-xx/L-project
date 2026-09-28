/******************************************************************************
 * Copyright 2020-2026 The RT-Thread Development Team. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#ifndef __DRV_CANFD_H__
#define __DRV_CANFD_H__

#include <rtthread.h>
#include <rtdevice.h>
#include <rthw.h>

#include "board.h"
#include "cy_canfd.h"
#include "cy_sysint.h"

#ifdef RT_USING_CAN

struct ifx_canfd_config
{
    const char *name;
    CANFD_Type *base;
    uint32_t channel;
    uint32_t channel_mask;
    uint16_t mram_delay_us;
    IRQn_Type irq;
    cy_stc_sysint_t *irq_cfg;
    cy_israddress isr;
    const cy_stc_canfd_config_t *canfd_config;
    cy_stc_canfd_test_mode_t test_mode;
    bool enable_brs;
    uint8_t tx_buffer_index;
};

struct ifx_canfd
{
    struct rt_can_device can_dev;
    struct ifx_canfd_config *config;

    cy_stc_canfd_context_t context;
    uint32_t irq_mask;

    volatile bool rx_pending;
    bool rx_fifo_msg;
    uint8_t rx_buf_fifo_num;
    uint32_t rx_id;
    uint8_t rx_len;
    cy_en_canfd_rtr_t rx_rtr;
    cy_en_canfd_xtd_t rx_xtd;
    uint8_t rx_data[64];

    cy_stc_canfd_t0_t tx_t0;
    cy_stc_canfd_t1_t tx_t1;
    uint32_t tx_data[CY_CANFD_MESSAGE_DATA_BUFFER_SIZE];
    cy_stc_canfd_tx_buffer_t tx_buffer;

    cy_canfd_tx_msg_func_ptr_t user_tx_cb;
    cy_canfd_rx_msg_func_ptr_t user_rx_cb;
    cy_canfd_error_func_ptr_t user_err_cb;

    cy_canfd_tx_msg_func_ptr_t tx_cb;
    cy_canfd_rx_msg_func_ptr_t rx_cb;
    cy_canfd_error_func_ptr_t err_cb;
};

int rt_hw_can_init(void);

#endif /* RT_USING_CAN */

#endif /* __DRV_CANFD_H__ */

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

#ifndef __CANFD_CONFIG_H__
#define __CANFD_CONFIG_H__

#include <rtthread.h>
#include "board.h"
#include "cy_canfd.h"
#include "cy_sysint.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef BSP_USING_CANFD0
extern const cy_stc_canfd_config_t CYBSP_CAN_FD_CH_0_config;

static cy_stc_sysint_t CANFD0_IRQ_cfg =
{
    .intrSrc = canfd_0_interrupts0_0_IRQn,
    .intrPriority = 3u,
};
#endif

#ifdef BSP_USING_CANFD1
extern const cy_stc_canfd_config_t CANFD1_config;

static cy_stc_sysint_t CANFD1_IRQ_cfg =
{
    .intrSrc = canfd_1_interrupts0_0_IRQn,
    .intrPriority = 3u,
};
#endif

/* CANFD0 */
#if defined(BSP_USING_CANFD0)
#ifndef CANFD0_CONFIG
#define CANFD0_CONFIG                                    \
    {                                                    \
        .name = "canfd0",                                \
        .base = CANFD0,                                  \
        .channel = 0u,                                   \
        .channel_mask = (1u << 0),                       \
        .mram_delay_us = 6u,                             \
        .irq = canfd_0_interrupts0_0_IRQn,               \
        .irq_cfg = &CANFD0_IRQ_cfg,                      \
        .isr = RT_NULL,                                  \
        .canfd_config = &CYBSP_CAN_FD_CH_0_config,       \
        .test_mode = CY_CANFD_TEST_MODE_DISABLE,         \
        .enable_brs = true,                              \
        .tx_buffer_index = 0u,                           \
    }
#endif
#endif /* BSP_USING_CANFD0 */

/* CANFD1 */
#if defined(BSP_USING_CANFD1)
#ifndef CANFD1_CONFIG
#define CANFD1_CONFIG                                    \
    {                                                    \
        .name = "canfd1",                                \
        .base = CANFD1,                                  \
        .channel = 0u,                                   \
        .channel_mask = (1u << 0),                       \
        .mram_delay_us = 6u,                             \
        .irq = canfd_1_interrupts0_0_IRQn,               \
        .irq_cfg = &CANFD1_IRQ_cfg,                      \
        .isr = RT_NULL,                                  \
        .canfd_config = &CANFD1_config,                  \
        .test_mode = CY_CANFD_TEST_MODE_DISABLE,         \
        .enable_brs = true,                              \
        .tx_buffer_index = 0u,                           \
    }
#endif
#endif /* BSP_USING_CANFD1 */

#ifdef __cplusplus
}
#endif

#endif /* __CANFD_CONFIG_H__ */

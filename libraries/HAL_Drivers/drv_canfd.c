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

#include "drv_canfd.h"
#include "canfd_config.h"

#ifdef RT_USING_CAN

enum
{
#ifdef BSP_USING_CANFD0
    CANFD0_INDEX,
#endif
#ifdef BSP_USING_CANFD1
    CANFD1_INDEX,
#endif
};

static struct ifx_canfd_config can_config[] =
{
#ifdef BSP_USING_CANFD0
    CANFD0_CONFIG,
#endif
#ifdef BSP_USING_CANFD1
    CANFD1_CONFIG,
#endif
};

static struct ifx_canfd can_obj[sizeof(can_config) / sizeof(can_config[0])] = {0};

#define IFX_CANFD_MAX_DATA_LEN      (64U)

#define IFX_CANFD_ERR_MASK (CY_CANFD_RX_FIFO_0_WATERMARK_REACHED | \
                            CY_CANFD_RX_FIFO_0_FULL | \
                            CY_CANFD_RX_FIFO_0_MSG_LOST | \
                            CY_CANFD_RX_FIFO_1_WATERMARK_REACHED | \
                            CY_CANFD_RX_FIFO_1_FULL | \
                            CY_CANFD_RX_FIFO_1_MSG_LOST | \
                            CY_CANFD_TX_FIFO_1_WATERMARK_REACHED | \
                            CY_CANFD_TX_FIFO_1_FULL | \
                            CY_CANFD_TX_FIFO_1_MSG_LOST | \
                            CY_CANFD_TIMESTAMP_WRAPAROUND | \
                            CY_CANFD_MRAM_ACCESS_FAILURE | \
                            CY_CANFD_TIMEOUT_OCCURRED | \
                            CY_CANFD_BIT_ERROR_CORRECTED | \
                            CY_CANFD_BIT_ERROR_UNCORRECTED | \
                            CY_CANFD_ERROR_LOG_OVERFLOW | \
                            CY_CANFD_ERROR_PASSIVE | \
                            CY_CANFD_WARNING_STATUS | \
                            CY_CANFD_BUS_OFF_STATUS | \
                            CY_CANFD_WATCHDOG_INTERRUPT | \
                            CY_CANFD_PROTOCOL_ERROR_ARB_PHASE | \
                            CY_CANFD_PROTOCOL_ERROR_DATA_PHASE | \
                            CY_CANFD_ACCESS_RESERVED_ADDR)

static uint8_t ifx_canfd_dlc_to_len(uint8_t dlc)
{
    switch (dlc)
    {
    case 0: return 0;
    case 1: return 1;
    case 2: return 2;
    case 3: return 3;
    case 4: return 4;
    case 5: return 5;
    case 6: return 6;
    case 7: return 7;
    case 8: return 8;
    case 9: return 12;
    case 10: return 16;
    case 11: return 20;
    case 12: return 24;
    case 13: return 32;
    case 14: return 48;
    case 15: return 64;
    default: return 0;
    }
}

static uint8_t ifx_canfd_len_to_dlc(uint8_t len)
{
    if (len <= 8)
    {
        return len;
    }
    if (len <= 12)
    {
        return 9;
    }
    if (len <= 16)
    {
        return 10;
    }
    if (len <= 20)
    {
        return 11;
    }
    if (len <= 24)
    {
        return 12;
    }
    if (len <= 32)
    {
        return 13;
    }
    if (len <= 48)
    {
        return 14;
    }
    return 15;
}

static void ifx_canfd_irq_handler(struct ifx_canfd *can)
{
    Cy_CANFD_IrqHandler(can->config->base, can->config->channel, &can->context);
}

static void ifx_canfd_tx_callback(struct ifx_canfd *can)
{
    rt_hw_can_isr(&can->can_dev, RT_CAN_EVENT_TX_DONE | ((rt_uint32_t)can->config->tx_buffer_index << 8));
}

static void ifx_canfd_rx_callback(struct ifx_canfd *can, bool rxFIFOMsg, uint8_t msgBufOrRxFIFONum,
                                  cy_stc_canfd_rx_buffer_t *rxBuffer)
{
    uint8_t data_len;

    can->rx_fifo_msg = rxFIFOMsg;
    can->rx_buf_fifo_num = msgBufOrRxFIFONum;
    can->rx_id = rxBuffer->r0_f->id;
    can->rx_rtr = rxBuffer->r0_f->rtr;
    can->rx_xtd = rxBuffer->r0_f->xtd;

    data_len = ifx_canfd_dlc_to_len((uint8_t)rxBuffer->r1_f->dlc);
    if (data_len > IFX_CANFD_MAX_DATA_LEN)
    {
        data_len = IFX_CANFD_MAX_DATA_LEN;
    }
    can->rx_len = data_len;

    rt_memset(can->rx_data, 0, sizeof(can->rx_data));
    rt_memcpy(can->rx_data, (uint8_t *)rxBuffer->data_area_f, data_len);
    can->rx_pending = true;

    rt_hw_can_isr(&can->can_dev, RT_CAN_EVENT_RX_IND | ((rt_uint32_t)msgBufOrRxFIFONum << 8));

}

static void ifx_canfd_error_callback(struct ifx_canfd *can, uint32_t errorMask)
{

    Cy_CANFD_ClearInterrupt(can->config->base, can->config->channel, errorMask & IFX_CANFD_ERR_MASK);

}

#ifdef BSP_USING_CANFD0
void canfd0_isr_callback(void)
{
    rt_interrupt_enter();
    ifx_canfd_irq_handler(&can_obj[CANFD0_INDEX]);
    rt_interrupt_leave();
}

static void canfd0_tx_callback(void)
{
    ifx_canfd_tx_callback(&can_obj[CANFD0_INDEX]);
}

void canfd0_rx_callback(bool rxFIFOMsg, uint8_t msgBufOrRxFIFONum, cy_stc_canfd_rx_buffer_t *rxBuffer)
{
    ifx_canfd_rx_callback(&can_obj[CANFD0_INDEX], rxFIFOMsg, msgBufOrRxFIFONum, rxBuffer);
}

static void canfd0_error_callback(uint32_t errorMask)
{
    ifx_canfd_error_callback(&can_obj[CANFD0_INDEX], errorMask);
}
#endif

#ifdef BSP_USING_CANFD1
void canfd1_isr_callback(void)
{
    rt_interrupt_enter();
    ifx_canfd_irq_handler(&can_obj[CANFD1_INDEX]);
    rt_interrupt_leave();
}

static void canfd1_tx_callback(void)
{
    ifx_canfd_tx_callback(&can_obj[CANFD1_INDEX]);
}

void canfd1_rx_callback(bool rxFIFOMsg, uint8_t msgBufOrRxFIFONum, cy_stc_canfd_rx_buffer_t *rxBuffer)
{
    ifx_canfd_rx_callback(&can_obj[CANFD1_INDEX], rxFIFOMsg, msgBufOrRxFIFONum, rxBuffer);
}

static void canfd1_error_callback(uint32_t errorMask)
{
    ifx_canfd_error_callback(&can_obj[CANFD1_INDEX], errorMask);
}
#endif

static void ifx_canfd_set_interrupt_mask(struct ifx_canfd *can)
{
    Cy_CANFD_SetInterruptMask(can->config->base, can->config->channel, can->irq_mask);
    Cy_CANFD_SetInterruptLine(can->config->base, can->config->channel, CY_CANFD_INTERRUPT_LINE_0_EN);
    Cy_CANFD_EnableInterruptLine(can->config->base, can->config->channel, CY_CANFD_INTERRUPT_LINE_0_EN);
}

static rt_err_t ifx_canfd_configure(struct rt_can_device *can_dev, struct can_configure *cfg)
{
    cy_en_canfd_status_t status;
    struct ifx_canfd *can;

    RT_ASSERT(can_dev != RT_NULL);
    RT_ASSERT(cfg != RT_NULL);

    can = rt_container_of(can_dev, struct ifx_canfd, can_dev);
    RT_ASSERT(can != RT_NULL);
    RT_ASSERT(can->config != RT_NULL);
    RT_ASSERT(can->config->canfd_config != RT_NULL);

    can->user_tx_cb = can->config->canfd_config->txCallback;
    can->user_rx_cb = can->config->canfd_config->rxCallback;
    can->user_err_cb = can->config->canfd_config->errorCallback;

    Cy_CANFD_EnableMRAM(can->config->base, can->config->channel_mask, can->config->mram_delay_us);

    status = Cy_CANFD_Init(can->config->base, can->config->channel, can->config->canfd_config, &can->context);
    if (status != CY_CANFD_SUCCESS)
    {
        return -RT_ERROR;
    }

    can->context.canFDInterruptHandling.canFDTxInterruptFunction = can->tx_cb;
    can->context.canFDInterruptHandling.canFDRxInterruptFunction = can->rx_cb;
    can->context.canFDInterruptHandling.canFDErrorInterruptFunction = can->err_cb;

    Cy_CANFD_ConfigChangesEnable(can->config->base, can->config->channel);
    Cy_CANFD_TestModeConfig(can->config->base, can->config->channel, can->config->test_mode);
    Cy_CANFD_ConfigChangesDisable(can->config->base, can->config->channel);

    can->tx_buffer.t0_f = &can->tx_t0;
    can->tx_buffer.t1_f = &can->tx_t1;
    can->tx_buffer.data_area_f = can->tx_data;

    can->rx_pending = false;

    return RT_EOK;
}

static rt_err_t ifx_canfd_control(struct rt_can_device *can_dev, int cmd, void *arg)
{
    struct ifx_canfd *can;
    rt_uint32_t argval;

    RT_ASSERT(can_dev != RT_NULL);
    can = rt_container_of(can_dev, struct ifx_canfd, can_dev);
    RT_ASSERT(can != RT_NULL);
    RT_ASSERT(can->config != RT_NULL);

    switch (cmd)
    {
    case RT_DEVICE_CTRL_CLR_INT:
        argval = (rt_uint32_t)arg;
        if (argval & RT_DEVICE_FLAG_INT_RX)
        {
            can->irq_mask &= ~(CY_CANFD_RX_BUFFER_NEW_MESSAGE | CY_CANFD_RX_FIFO_0_NEW_MESSAGE | CY_CANFD_RX_FIFO_1_NEW_MESSAGE);
        }
        if (argval & RT_DEVICE_FLAG_INT_TX)
        {
            can->irq_mask &= ~(CY_CANFD_TRANSMISSION_COMPLETE | CY_CANFD_TRANSMISSION_CANCEL_FINISHED);
        }
        if (argval & RT_DEVICE_CAN_INT_ERR)
        {
            can->irq_mask &= ~IFX_CANFD_ERR_MASK;
        }
        ifx_canfd_set_interrupt_mask(can);
        NVIC_ClearPendingIRQ(can->config->irq);
        break;

    case RT_DEVICE_CTRL_SET_INT:
        argval = (rt_uint32_t)arg;
        if (argval & RT_DEVICE_FLAG_INT_RX)
        {
            can->irq_mask |= (CY_CANFD_RX_BUFFER_NEW_MESSAGE | CY_CANFD_RX_FIFO_0_NEW_MESSAGE | CY_CANFD_RX_FIFO_1_NEW_MESSAGE);
        }
        if (argval & RT_DEVICE_FLAG_INT_TX)
        {
            can->irq_mask |= (CY_CANFD_TRANSMISSION_COMPLETE | CY_CANFD_TRANSMISSION_CANCEL_FINISHED);
        }
        if (argval & RT_DEVICE_CAN_INT_ERR)
        {
            can->irq_mask |= IFX_CANFD_ERR_MASK;
        }
        ifx_canfd_set_interrupt_mask(can);

        if ((can->config->irq_cfg == RT_NULL) || (can->config->isr == RT_NULL))
        {
            return -RT_ERROR;
        }
        Cy_SysInt_Init(can->config->irq_cfg, can->config->isr);
        NVIC_EnableIRQ(can->config->irq);
        break;

    case RT_CAN_CMD_SET_MODE:
        argval = (rt_uint32_t)arg;
        if (argval == RT_CAN_MODE_NORMAL)
        {
            Cy_CANFD_ConfigChangesEnable(can->config->base, can->config->channel);
            Cy_CANFD_TestModeConfig(can->config->base, can->config->channel, CY_CANFD_TEST_MODE_DISABLE);
            Cy_CANFD_ConfigChangesDisable(can->config->base, can->config->channel);
        }
        else if (argval == RT_CAN_MODE_LISTEN)
        {
            Cy_CANFD_ConfigChangesEnable(can->config->base, can->config->channel);
            Cy_CANFD_TestModeConfig(can->config->base, can->config->channel, CY_CANFD_TEST_MODE_BUS_MONITORING);
            Cy_CANFD_ConfigChangesDisable(can->config->base, can->config->channel);
        }
        else if (argval == RT_CAN_MODE_LOOPBACK)
        {
            Cy_CANFD_ConfigChangesEnable(can->config->base, can->config->channel);
            Cy_CANFD_TestModeConfig(can->config->base, can->config->channel, CY_CANFD_TEST_MODE_INTERNAL_LOOP_BACK);
            Cy_CANFD_ConfigChangesDisable(can->config->base, can->config->channel);
        }
        else
        {
            return -RT_ERROR;
        }
        break;

    case RT_CAN_CMD_SET_BAUD:
        return -RT_ERROR;

    case RT_CAN_CMD_GET_STATUS:
    {
        struct rt_can_status *status = (struct rt_can_status *)arg;
        if (status != RT_NULL)
        {
            rt_memset(status, 0, sizeof(*status));
            status->errcode = (rt_uint32_t)Cy_CANFD_GetLastError(can->config->base, can->config->channel);
        }
        break;
    }

    default:
        return -RT_ERROR;
    }

    return RT_EOK;
}

static rt_ssize_t ifx_canfd_sendmsg(struct rt_can_device *can_dev, const void *buf, rt_uint32_t boxno)
{
    struct ifx_canfd *can;
    struct rt_can_msg *msg_rt = (struct rt_can_msg *)buf;
    cy_en_canfd_status_t status;
    uint8_t data_len;

    RT_ASSERT(can_dev != RT_NULL);
    RT_ASSERT(buf != RT_NULL);

    can = rt_container_of(can_dev, struct ifx_canfd, can_dev);
    RT_ASSERT(can != RT_NULL);

    if (boxno != can->config->tx_buffer_index)
    {
        boxno = can->config->tx_buffer_index;
    }

    data_len = msg_rt->len;
    if (data_len > IFX_CANFD_MAX_DATA_LEN)
    {
        return -RT_ERROR;
    }

    if ((!can->config->canfd_config->canFDMode) && (data_len > 8))
    {
        return -RT_ERROR;
    }

    can->tx_t0.id = msg_rt->id;
    can->tx_t0.rtr = msg_rt->rtr ? CY_CANFD_RTR_REMOTE_FRAME : CY_CANFD_RTR_DATA_FRAME;
    can->tx_t0.xtd = msg_rt->ide ? CY_CANFD_XTD_EXTENDED_ID : CY_CANFD_XTD_STANDARD_ID;
    can->tx_t0.esi = CY_CANFD_ESI_ERROR_ACTIVE;

    can->tx_t1.dlc = ifx_canfd_len_to_dlc(data_len);
    can->tx_t1.brs = can->config->enable_brs;
    can->tx_t1.fdf = can->config->canfd_config->canFDMode ? CY_CANFD_FDF_CAN_FD_FRAME : CY_CANFD_FDF_STANDARD_FRAME;
    can->tx_t1.efc = false;
    can->tx_t1.mm = 0;

    rt_memset(can->tx_data, 0, sizeof(can->tx_data));
    rt_memcpy((uint8_t *)can->tx_data, msg_rt->data, data_len);

    status = Cy_CANFD_UpdateAndTransmitMsgBuffer(can->config->base, can->config->channel,
                                                 &can->tx_buffer, (uint8_t)boxno, &can->context);
    if (status != CY_CANFD_SUCCESS)
    {
        return -RT_ERROR;
    }

    return RT_EOK;
}

static rt_ssize_t ifx_canfd_recvmsg(struct rt_can_device *can_dev, void *buf, rt_uint32_t boxno)
{
    struct ifx_canfd *can;
    struct rt_can_msg *msg_rt = (struct rt_can_msg *)buf;
    rt_uint8_t copy_len;

    RT_ASSERT(can_dev != RT_NULL);
    RT_ASSERT(buf != RT_NULL);

    can = rt_container_of(can_dev, struct ifx_canfd, can_dev);
    RT_ASSERT(can != RT_NULL);

    if (!can->rx_pending)
    {
        return 0;
    }

    if (boxno != can->rx_buf_fifo_num)
    {
        return 0;
    }

    msg_rt->id = can->rx_id;
    msg_rt->ide = (can->rx_xtd == CY_CANFD_XTD_EXTENDED_ID) ? 1 : 0;
    msg_rt->rtr = (can->rx_rtr == CY_CANFD_RTR_REMOTE_FRAME) ? 1 : 0;
    msg_rt->rsv = RT_NULL;
    msg_rt->len = can->rx_len;
    msg_rt->priv = boxno;
    msg_rt->hdr_index = RT_NULL;

    copy_len = can->rx_len;
    if (copy_len > sizeof(msg_rt->data))
    {
        copy_len = sizeof(msg_rt->data);
        msg_rt->len = copy_len;
    }

    rt_memcpy(msg_rt->data, can->rx_data, copy_len);
    can->rx_pending = false;

    return sizeof(struct rt_can_msg);
}

static const struct rt_can_ops ifx_can_ops =
{
    .configure = ifx_canfd_configure,
    .control = ifx_canfd_control,
    .sendmsg = ifx_canfd_sendmsg,
    .recvmsg = ifx_canfd_recvmsg
};

static void ifx_canfd_get_config(void)
{
    struct can_configure config = CANDEFAULTCONFIG;

    for (rt_size_t i = 0; i < (sizeof(can_obj) / sizeof(can_obj[0])); i++)
    {
        can_obj[i].can_dev.config = config;
        can_obj[i].can_dev.config.msgboxsz = 64;
        can_obj[i].can_dev.config.sndboxnumber = 1;
        can_obj[i].can_dev.config.ticks = 50;
    }
}

int rt_hw_can_init(void)
{
    rt_err_t result = RT_EOK;
    rt_size_t obj_num = sizeof(can_obj) / sizeof(struct ifx_canfd);

    ifx_canfd_get_config();

    for (rt_size_t i = 0; i < obj_num; i++)
    {
        can_obj[i].config = &can_config[i];
        can_obj[i].can_dev.ops = &ifx_can_ops;

#ifdef BSP_USING_CANFD0
        if (i == CANFD0_INDEX)
        {
            can_obj[i].tx_cb = canfd0_tx_callback;
            can_obj[i].rx_cb = canfd0_rx_callback;
            can_obj[i].err_cb = canfd0_error_callback;
            can_obj[i].config->isr = canfd0_isr_callback;
        }
#endif

#ifdef BSP_USING_CANFD1
        if (i == CANFD1_INDEX)
        {
            can_obj[i].tx_cb = canfd1_tx_callback;
            can_obj[i].rx_cb = canfd1_rx_callback;
            can_obj[i].err_cb = canfd1_error_callback;
            can_obj[i].config->isr = canfd1_isr_callback;
        }
#endif

        result = rt_hw_can_register(&can_obj[i].can_dev, can_obj[i].config->name, can_obj[i].can_dev.ops, RT_NULL);
        RT_ASSERT(result == RT_EOK);
    }

    return result;
}
INIT_PREV_EXPORT(rt_hw_can_init);

#endif /* RT_USING_CAN */

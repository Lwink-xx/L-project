#include "ds18b20.h"

#define DBG_TAG     "ds18b20"
#define DBG_LVL     DBG_INFO
#include <rtdbg.h>

/* 最近一次读到的温度（0.01 摄氏度） */
static volatile rt_int32_t g_last_temp = DS18B20_TEMP_INVALID;

/* ---------------- 1-Wire 底层时序 ---------------- */

static void wire_output_low(void)
{
    rt_pin_mode(DS18B20_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(DS18B20_PIN, PIN_LOW);
}

static void wire_release(void)
{
    /* 释放总线：切为输入，靠外部上拉电阻拉高 */
    rt_pin_mode(DS18B20_PIN, PIN_MODE_INPUT);
}

static rt_uint8_t wire_read_level(void)
{
    return rt_pin_read(DS18B20_PIN);
}

/* 复位脉冲，返回 1 表示检测到从机存在 */
static rt_uint8_t wire_reset(void)
{
    rt_base_t level;
    rt_uint8_t presence;

    level = rt_hw_interrupt_disable();

    wire_output_low();
    rt_hw_us_delay(500);
    wire_release();
    rt_hw_us_delay(70);

    presence = (wire_read_level() == PIN_LOW) ? 1 : 0;

    rt_hw_interrupt_enable(level);
    rt_hw_us_delay(430);

    return presence;
}

static void wire_write_byte(rt_uint8_t byte)
{
    rt_base_t level;
    rt_uint8_t i;

    level = rt_hw_interrupt_disable();

    for (i = 0; i < 8; i++) {
        wire_output_low();
        if (byte & 0x01) {
            rt_hw_us_delay(6);
            wire_release();
            rt_hw_us_delay(64);
        } else {
            rt_hw_us_delay(60);
            wire_release();
            rt_hw_us_delay(10);
        }
        byte >>= 1;
    }

    rt_hw_interrupt_enable(level);
}

static rt_uint8_t wire_read_byte(void)
{
    rt_base_t level;
    rt_uint8_t byte = 0;
    rt_uint8_t i;

    level = rt_hw_interrupt_disable();

    for (i = 0; i < 8; i++) {
        wire_output_low();
        rt_hw_us_delay(3);
        wire_release();
        rt_hw_us_delay(10);

        byte >>= 1;
        if (wire_read_level() == PIN_HIGH) {
            byte |= 0x80;
        }
        rt_hw_us_delay(53);
    }

    rt_hw_interrupt_enable(level);
    return byte;
}

/* ---------------- DS18B20 操作 ---------------- */

/* Dallas/Maxim CRC8（多项式 X^8+X^5+X^4+1 = 0x8C） */
static rt_uint8_t ds18b20_crc8(const rt_uint8_t *data, rt_uint8_t len)
{
    rt_uint8_t crc = 0;
    rt_uint8_t i, j;

    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (j = 0; j < 8; j++) {
            if (crc & 0x01) {
                crc = (crc >> 1) ^ 0x8C;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

void ds18b20_init(void)
{
    /* 释放总线，让外部上拉把线拉高 */
    wire_release();
}

rt_int32_t ds18b20_read_temp(void)
{
    rt_uint8_t buf[9];
    rt_int16_t raw;
    rt_int32_t temp;
    int i;

    if (!wire_reset()) {
        return DS18B20_TEMP_INVALID;
    }
    wire_write_byte(0xCC);      /* Skip ROM */
    wire_write_byte(0x44);      /* Convert T */

    rt_thread_mdelay(750);      /* 12 位精度转换需 750ms */

    if (!wire_reset()) {
        return DS18B20_TEMP_INVALID;
    }
    wire_write_byte(0xCC);
    wire_write_byte(0xBE);      /* Read Scratchpad */

    /* 读完整 9 字节：前 8 字节数据 + 1 字节 CRC */
    for (i = 0; i < 9; i++) {
        buf[i] = wire_read_byte();
    }

    /* CRC 校验：如果总线悬空或通信错误，这一步会过滤掉大部分脏数据 */
    if (ds18b20_crc8(buf, 8) != buf[8]) {
        return DS18B20_TEMP_INVALID;
    }

    raw = (rt_int16_t)((buf[1] << 8) | buf[0]);

    /* 过滤异常值 */
    if (raw == 0x0000) {
        /* 全 0：通常是无传感器 / 总线悬空 */
        return DS18B20_TEMP_INVALID;
    }
    if (raw == 0x0550) {
        /* 85.0℃：DS18B20 上电默认值，说明 Convert T 未真正执行 */
        return DS18B20_TEMP_INVALID;
    }

    /* 原始值单位 1/16 ℃，转为 0.01 ℃ */
    temp = (rt_int32_t)raw * 100 / 16;

    return temp;
}

rt_int32_t ds18b20_get_last(void)
{
    return g_last_temp;
}

/* ---------------- 后台采集线程 ---------------- */

#define DS18B20_THREAD_STACK    2048
#define DS18B20_THREAD_PRIO     15
#define DS18B20_THREAD_TICK     10
#define DS18B20_INTERVAL_MS     2000

static void ds18b20_thread_entry(void *param)
{
    (void)param;

    ds18b20_init();
    LOG_I("DS18B20 thread start, PIN = %d", DS18B20_PIN);

    while (1) {
        rt_int32_t t = ds18b20_read_temp();
        g_last_temp = t;

        if (t == DS18B20_TEMP_INVALID) {
            LOG_W("read error");
        } else {
            LOG_I("temp = %d.%02d C", t / 100, (t % 100 + 100) % 100);
        }

        rt_thread_mdelay(DS18B20_INTERVAL_MS);
    }
}

int ds18b20_start_collector(void)
{
    rt_thread_t tid;

    tid = rt_thread_create("ds18b20", ds18b20_thread_entry, RT_NULL,
                           DS18B20_THREAD_STACK,
                           DS18B20_THREAD_PRIO,
                           DS18B20_THREAD_TICK);
    if (tid == RT_NULL) {
        LOG_E("failed to create ds18b20 thread");
        return -RT_ERROR;
    }

    rt_thread_startup(tid);
    return RT_EOK;
}



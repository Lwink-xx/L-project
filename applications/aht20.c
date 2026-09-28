#include "aht20.h"
#include <board.h>

#define DBG_TAG     "aht20"
#define DBG_LVL     DBG_INFO
#include <rtdbg.h>

/* AHT20 物理挂的引脚：SCL=P9.3(74), SDA=P9.2(75) */
#define AHT20_SCL_PIN       GET_PIN(9, 3)
#define AHT20_SDA_PIN       GET_PIN(9, 2)

#define AHT20_ADDR          0x38

#define AHT20_CMD_INIT      0xBE
#define AHT20_CMD_TRIGGER   0xAC
#define AHT20_CMD_SOFT_RESET 0xBA

#define AHT20_STATUS_BUSY   0x80
#define AHT20_STATUS_CALI   0x08

static volatile rt_int32_t g_last_temp = AHT20_TEMP_INVALID;
static volatile rt_uint16_t g_last_humi = AHT20_HUMI_INVALID;
static rt_bool_t g_inited = RT_FALSE;

/* ---------- bit-bang I2C 时序 ---------- */

#define I2C_DELAY_US    3

static void i2c_delay(void)
{
    rt_hw_us_delay(I2C_DELAY_US);
}

static void scl_h(void) { rt_pin_write(AHT20_SCL_PIN, PIN_HIGH); }
static void scl_l(void) { rt_pin_write(AHT20_SCL_PIN, PIN_LOW);  }
static void sda_h(void) { rt_pin_write(AHT20_SDA_PIN, PIN_HIGH); }
static void sda_l(void) { rt_pin_write(AHT20_SDA_PIN, PIN_LOW);  }
static int  sda_r(void) { return rt_pin_read(AHT20_SDA_PIN); }

static void i2c_start(void)
{
    sda_h(); scl_h(); i2c_delay();
    sda_l(); i2c_delay();
    scl_l(); i2c_delay();
}

static void i2c_stop(void)
{
    sda_l(); i2c_delay();
    scl_h(); i2c_delay();
    sda_h(); i2c_delay();
}

/* 返回 0=收到 ACK，1=NACK */
static int i2c_write_byte(rt_uint8_t b)
{
    int i, ack;

    for (i = 0; i < 8; i++) {
        if (b & 0x80) sda_h(); else sda_l();
        i2c_delay();
        scl_h(); i2c_delay();
        scl_l(); i2c_delay();
        b <<= 1;
    }
    /* 第 9 拍：主机释放 SDA，读从机 ACK */
    sda_h(); i2c_delay();
    scl_h(); i2c_delay();
    ack = sda_r();
    scl_l(); i2c_delay();
    return ack ? 1 : 0;
}

/* ack=1 主机发 ACK，ack=0 主机发 NACK */
static rt_uint8_t i2c_read_byte(int ack)
{
    rt_uint8_t b = 0;
    int i;

    sda_h();
    for (i = 0; i < 8; i++) {
        b <<= 1;
        scl_h(); i2c_delay();
        if (sda_r()) b |= 1;
        scl_l(); i2c_delay();
    }
    if (ack) sda_l(); else sda_h();
    i2c_delay();
    scl_h(); i2c_delay();
    scl_l(); i2c_delay();
    sda_h();
    return b;
}

/* ---------- AHT20 命令 ---------- */

static rt_err_t aht20_send(const rt_uint8_t *data, rt_uint8_t len)
{
    rt_uint8_t i;
    i2c_start();
    if (i2c_write_byte((AHT20_ADDR << 1) | 0)) { i2c_stop(); return -RT_ERROR; }
    for (i = 0; i < len; i++) {
        if (i2c_write_byte(data[i])) { i2c_stop(); return -RT_ERROR; }
    }
    i2c_stop();
    return RT_EOK;
}

static rt_err_t aht20_recv(rt_uint8_t *data, rt_uint8_t len)
{
    rt_uint8_t i;
    i2c_start();
    if (i2c_write_byte((AHT20_ADDR << 1) | 1)) { i2c_stop(); return -RT_ERROR; }
    for (i = 0; i < len; i++) {
        data[i] = i2c_read_byte(i < len - 1 ? 1 : 0);
    }
    i2c_stop();
    return RT_EOK;
}

/* ---------- 对外 API ---------- */

rt_err_t aht20_init(void)
{
    rt_uint8_t cmd[3];
    rt_uint8_t status = 0;

    /* 引脚配置为开漏输出 */
    rt_pin_mode(AHT20_SCL_PIN, PIN_MODE_OUTPUT_OD);
    rt_pin_mode(AHT20_SDA_PIN, PIN_MODE_OUTPUT_OD);
    rt_pin_write(AHT20_SCL_PIN, PIN_HIGH);
    rt_pin_write(AHT20_SDA_PIN, PIN_HIGH);
    rt_thread_mdelay(20);

    /* 软复位 */
    cmd[0] = AHT20_CMD_SOFT_RESET;
    (void)aht20_send(cmd, 1);
    rt_thread_mdelay(20);

    /* 初始化命令 0xBE 0x08 0x00 */
    cmd[0] = AHT20_CMD_INIT;
    cmd[1] = 0x08;
    cmd[2] = 0x00;
    if (aht20_send(cmd, 3) != RT_EOK) {
        LOG_E("init cmd failed");
        return -RT_ERROR;
    }
    rt_thread_mdelay(10);

    /* 读状态，检查校准位 */
    if (aht20_recv(&status, 1) != RT_EOK) {
        LOG_E("read status failed");
        return -RT_ERROR;
    }

    if ((status & AHT20_STATUS_CALI) == 0) {
        LOG_W("not calibrated (status=0x%02x)", status);
        return -RT_ERROR;
    }

    g_inited = RT_TRUE;
    LOG_I("AHT20 init OK");
    return RT_EOK;
}

rt_err_t aht20_read(rt_int32_t *temp_x100, rt_uint16_t *humi_x10)
{
    rt_uint8_t cmd[3] = { AHT20_CMD_TRIGGER, 0x33, 0x00 };
    rt_uint8_t data[7];
    rt_uint32_t raw_humi, raw_temp;
    rt_uint8_t retry;

    if (!g_inited || temp_x100 == RT_NULL || humi_x10 == RT_NULL) {
        return -RT_EINVAL;
    }

    if (aht20_send(cmd, 3) != RT_EOK) {
        return -RT_ERROR;
    }

    for (retry = 0; retry < 10; retry++) {
        rt_thread_mdelay(10);
        if (aht20_recv(data, sizeof(data)) != RT_EOK) {
            return -RT_ERROR;
        }
        if ((data[0] & AHT20_STATUS_BUSY) == 0) break;
    }
    if (retry >= 10) return -RT_ETIMEOUT;

    raw_humi = ((rt_uint32_t)data[1] << 12) | ((rt_uint32_t)data[2] << 4)
               | ((rt_uint32_t)data[3] >> 4);
    raw_temp = (((rt_uint32_t)data[3] & 0x0F) << 16)
               | ((rt_uint32_t)data[4] << 8) | data[5];

    *humi_x10 = (rt_uint16_t)((raw_humi * 1000UL + 524288UL) >> 20);
    *temp_x100 = (rt_int32_t)(((rt_int64_t)raw_temp * 20000LL + 524288LL) >> 20) - 5000;

    return RT_EOK;
}

void aht20_get_last(rt_int32_t *temp_x100, rt_uint16_t *humi_x10)
{
    if (temp_x100) *temp_x100 = g_last_temp;
    if (humi_x10)  *humi_x10  = g_last_humi;
}

/* ---------- 后台采集线程 ---------- */

#define AHT20_THREAD_STACK      2048
#define AHT20_THREAD_PRIO       16
#define AHT20_THREAD_TICK       10
#define AHT20_INTERVAL_MS       2000

static void aht20_thread_entry(void *param)
{
    (void)param;

    if (aht20_init() != RT_EOK) {
        LOG_E("init failed");
        return;
    }

    while (1) {
        rt_int32_t t;
        rt_uint16_t h;

        if (aht20_read(&t, &h) == RT_EOK) {
            g_last_temp = t;
            g_last_humi = h;
            LOG_I("T=%d.%02d C, H=%d.%01d %%RH",
                  t / 100, (t % 100 + 100) % 100,
                  h / 10, h % 10);
        } else {
            LOG_W("read failed");
        }

        rt_thread_mdelay(AHT20_INTERVAL_MS);
    }
}

int aht20_start_collector(void)
{
    rt_thread_t tid;

    tid = rt_thread_create("aht20", aht20_thread_entry, RT_NULL,
                           AHT20_THREAD_STACK,
                           AHT20_THREAD_PRIO,
                           AHT20_THREAD_TICK);
    if (tid == RT_NULL) {
        LOG_E("failed to create thread");
        return -RT_ERROR;
    }
    rt_thread_startup(tid);
    return RT_EOK;
}


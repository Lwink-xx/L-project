#ifndef __AHT20_H__
#define __AHT20_H__

#include <rtthread.h>
#include <rtdevice.h>

/* 温度/湿度无效值 */
#define AHT20_TEMP_INVALID      (-99999)    /* 0.01 ℃ */
#define AHT20_HUMI_INVALID      (0xFFFF)    /* 0.1 %RH */

/* 初始化：自动在 i2c0 和 i2c1 上探测 AHT20 */
/* 成功返回 RT_EOK，失败返回负值 */
rt_err_t aht20_init(void);

/* 读取温湿度。
 * temp_x100: 温度，单位 0.01 ℃
 * humi_x10 : 湿度，单位 0.1 %RH
 * 成功返回 RT_EOK，失败返回负值
 */
rt_err_t aht20_read(rt_int32_t *temp_x100, rt_uint16_t *humi_x10);

/* 启动后台采集线程（每 2 秒更新一次缓存） */
int aht20_start_collector(void);

/* 获取最近一次的温湿度。未采集到返回无效值 */
void aht20_get_last(rt_int32_t *temp_x100, rt_uint16_t *humi_x10);

#endif /* __AHT20_H__ */


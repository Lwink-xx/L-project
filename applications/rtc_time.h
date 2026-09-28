#ifndef __RTC_TIME_H__
#define __RTC_TIME_H__

#include <rtthread.h>
#include <rtdevice.h>

/* 初始化 RTC（打开设备）。成功返回 RT_EOK */
rt_err_t rtc_time_init(void);

/* 获取当前时间，格式化为 "YYYY-MM-DD HH:MM:SS"
 * buf: 输出缓冲区
 * size: 缓冲区大小，建议至少 24
 * 成功返回 RT_EOK，失败返回负值（缓冲区会被填 "--"）
 */
rt_err_t rtc_time_get_str(char *buf, rt_size_t size);

/* 获取 Unix 时间戳。失败返回 -1 */
rt_int64_t rtc_time_get_timestamp(void);

/* 设置 RTC 时间。参数为 Unix 时间戳 */
rt_err_t rtc_time_set(rt_int64_t timestamp);


#endif /* __RTC_TIME_H__ */



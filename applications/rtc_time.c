#include "rtc_time.h"
#include <time.h>
#include <rtdevice.h>

#define DBG_TAG     "rtc"
#define DBG_LVL     DBG_INFO
#include <rtdbg.h>

static rt_device_t g_rtc_dev = RT_NULL;

rt_err_t rtc_time_init(void)
{
    if (g_rtc_dev != RT_NULL) {
        return RT_EOK;
    }

    g_rtc_dev = rt_device_find("rtc");
    if (g_rtc_dev == RT_NULL) {
        LOG_E("rtc device not found");
        return -RT_ENOSYS;
    }

    if (rt_device_open(g_rtc_dev, RT_DEVICE_OFLAG_RDWR) != RT_EOK) {
        LOG_E("failed to open rtc");
        g_rtc_dev = RT_NULL;
        return -RT_ERROR;
    }

    LOG_I("rtc initialized");
    return RT_EOK;
}

rt_int64_t rtc_time_get_timestamp(void)
{
    time_t now = 0;

    if (g_rtc_dev == RT_NULL) {
        return -1;
    }

    if (rt_device_control(g_rtc_dev, RT_DEVICE_CTRL_RTC_GET_TIME, &now) != RT_EOK) {
        return -1;
    }

    return (rt_int64_t)now;
}

rt_err_t rtc_time_get_str(char *buf, rt_size_t size)
{
    time_t now;
    struct tm tm_now;

    if (buf == RT_NULL || size < 20) {
        return -RT_EINVAL;
    }

    if (g_rtc_dev == RT_NULL) {
        rt_snprintf(buf, size, "----/--/-- --:--:--");
        return -RT_ERROR;
    }

    if (rt_device_control(g_rtc_dev, RT_DEVICE_CTRL_RTC_GET_TIME, &now) != RT_EOK) {
        rt_snprintf(buf, size, "----/--/-- --:--:--");
        return -RT_ERROR;
    }

    if (localtime_r(&now, &tm_now) == RT_NULL) {
        rt_snprintf(buf, size, "----/--/-- --:--:--");
        return -RT_ERROR;
    }

    rt_snprintf(buf, size, "%04d-%02d-%02d %02d:%02d:%02d",
                tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
                tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);

    return RT_EOK;
}

rt_err_t rtc_time_set(rt_int64_t timestamp)
{
    time_t t = (time_t)timestamp;

    if (g_rtc_dev == RT_NULL) {
        return -RT_ERROR;
    }

    if (rt_device_control(g_rtc_dev, RT_DEVICE_CTRL_RTC_SET_TIME, &t) != RT_EOK) {
        return -RT_ERROR;
    }

    return RT_EOK;
}


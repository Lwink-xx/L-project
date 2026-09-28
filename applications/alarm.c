#include "alarm.h"
#include "ds18b20.h"
#include "aht20.h"

/* 预警余量：距离阈值 3.0℃ 以内算预警 */
#define WARN_MARGIN_X100    300

typedef struct {
    rt_int32_t low;
    rt_int32_t high;
    alarm_level_t last;
} alarm_ch_t;

/* 默认阈值：环境 20~40℃；粮堆 25~40℃ */
static alarm_ch_t g_env   = { 2000, 4000, ALARM_NORMAL };
static alarm_ch_t g_grain = { 2500, 4000, ALARM_NORMAL };

static alarm_level_t check_ch(alarm_ch_t *ch, rt_int32_t t_x100)
{
    if (t_x100 == -9999999) {
        return ALARM_NORMAL;    /* 无效值不报警 */
    }
    if (t_x100 < ch->low || t_x100 > ch->high) {
        return ALARM_CRITICAL;
    }
    if (t_x100 < ch->low + WARN_MARGIN_X100 ||
        t_x100 > ch->high - WARN_MARGIN_X100) {
        return ALARM_WARNING;
    }
    return ALARM_NORMAL;
}

void alarm_init(void)
{
    g_env.last = ALARM_NORMAL;
    g_grain.last = ALARM_NORMAL;
}

void alarm_set_env_range(rt_int32_t low_x100, rt_int32_t high_x100)
{
    g_env.low = low_x100;
    g_env.high = high_x100;
}

void alarm_set_grain_range(rt_int32_t low_x100, rt_int32_t high_x100)
{
    g_grain.low = low_x100;
    g_grain.high = high_x100;
}

alarm_level_t alarm_check_env(rt_int32_t t_x100)
{
    if (t_x100 == -99999) {   /* AHT20_TEMP_INVALID */
        g_env.last = ALARM_NORMAL;
        return ALARM_NORMAL;
    }

    g_env.last = check_ch(&g_env, t_x100);
    return g_env.last;
}


alarm_level_t alarm_check_grain(rt_int32_t t_x100)
{
    /* DS18B20 无效值（比如传感器未接）不参与报警判断 */
    if (t_x100 == -9999999) {
        g_grain.last = ALARM_NORMAL;
        return ALARM_NORMAL;
    }

    g_grain.last = check_ch(&g_grain, t_x100);
    return g_grain.last;
}


alarm_level_t alarm_get_env_level(void)   { return g_env.last; }
alarm_level_t alarm_get_grain_level(void) { return g_grain.last; }

alarm_level_t alarm_get_combined_level(void)
{
    if (g_env.last == ALARM_CRITICAL || g_grain.last == ALARM_CRITICAL)
        return ALARM_CRITICAL;
    if (g_env.last == ALARM_WARNING || g_grain.last == ALARM_WARNING)
        return ALARM_WARNING;
    return ALARM_NORMAL;
}


rt_int32_t alarm_get_env_low(void)   { return g_env.low;   }
rt_int32_t alarm_get_env_high(void)  { return g_env.high;  }
rt_int32_t alarm_get_grain_low(void) { return g_grain.low; }
rt_int32_t alarm_get_grain_high(void){ return g_grain.high;}




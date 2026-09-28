#ifndef __ALARM_H__
#define __ALARM_H__

#include <rtthread.h>

typedef enum {
    ALARM_NORMAL = 0,
    ALARM_WARNING,       /* 接近阈值 */
    ALARM_CRITICAL,      /* 超出阈值 */
} alarm_level_t;

/* 初始化 */
void alarm_init(void);

/* 设置阈值（单位：0.01℃） */
void alarm_set_env_range(rt_int32_t low_x100, rt_int32_t high_x100);
void alarm_set_grain_range(rt_int32_t low_x100, rt_int32_t high_x100);

/* 检查温度，更新内部状态并返回级别 */
alarm_level_t alarm_check_env(rt_int32_t temp_x100);
alarm_level_t alarm_check_grain(rt_int32_t temp_x100);

/* 获取最近一次检查的级别 */
alarm_level_t alarm_get_env_level(void);
alarm_level_t alarm_get_grain_level(void);

/* 综合级别（取最严重的那个） */
alarm_level_t alarm_get_combined_level(void);


/* 获取当前阈值 */
rt_int32_t alarm_get_env_low(void);
rt_int32_t alarm_get_env_high(void);
rt_int32_t alarm_get_grain_low(void);
rt_int32_t alarm_get_grain_high(void);





#endif /* __ALARM_H__ */



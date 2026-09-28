#ifndef __SETTINGS_H__
#define __SETTINGS_H__

#include <rtthread.h>

/* 初始化：加载配置（如果没有则用默认值） */
void settings_init(void);

/* 保存 4 个阈值到 Flash */
rt_err_t settings_save(void);

/* 从 Flash 加载 4 个阈值。成功返回 RT_EOK */
rt_err_t settings_load(void);

/* 只更新时间戳到配置（Set Time 成功后调用） */
rt_err_t settings_save_time(rt_int64_t timestamp);

/* 检查 RTC 是否为默认假时间，如果是则用配置里的时间戳恢复 */
void settings_restore_time(void);


#endif /* __SETTINGS_H__ */


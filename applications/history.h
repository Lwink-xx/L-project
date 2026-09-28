#ifndef __HISTORY_H__
#define __HISTORY_H__

#include <rtthread.h>

#define HISTORY_MAX     10

typedef struct {
    rt_int64_t  timestamp;    /* Unix 时间戳 */
    rt_int32_t  grain_temp;   /* 0.01 ℃，无效值 DS18B20_TEMP_INVALID */
    rt_int32_t  env_temp;     /* 0.01 ℃，无效值 AHT20_TEMP_INVALID */
    rt_uint16_t env_humi;     /* 0.1 %RH */
} history_record_t;

/* 初始化（清空） */
void history_init(void);

/* 追加一条记录（自动覆盖最旧的） */
void history_push(const history_record_t *rec);

/* 返回已记录条数（最大 HISTORY_MAX） */
int history_count(void);

/* 读取一条记录。
 * index = 0 是最新一条，index = 1 是倒数第二条，依此类推。
 * 成功返回 RT_TRUE，越界返回 RT_FALSE
 */
rt_bool_t history_get(int index, history_record_t *out);

#endif /* __HISTORY_H__ */



/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-09-19     19722       the first version
 */
#ifndef __DS18B20_H__
#define __DS18B20_H__

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>

/* 数据引脚。如需更换引脚，改这一行即可 */
#ifndef DS18B20_PIN
#define DS18B20_PIN             GET_PIN(16, 28)   /* 引脚 40 */
#endif

/* 温度无效值（0.01 摄氏度） */
#define DS18B20_TEMP_INVALID    (-9999999)

/* 初始化 GPIO（调用一次即可） */
void ds18b20_init(void);

/* 读取温度，返回 0.01 摄氏度。失败返回 DS18B20_TEMP_INVALID */
rt_int32_t ds18b20_read_temp(void);

/* 启动后台采集线程（每 2 秒采集一次，更新内部缓存） */
int ds18b20_start_collector(void);

/* 获取最近一次温度值（0.01 摄氏度）。未启动或未采集过返回 DS18B20_TEMP_INVALID */
rt_int32_t ds18b20_get_last(void);

#endif /* __DS18B20_H__ */



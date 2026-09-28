#ifndef __CSV_LOGGER_H__
#define __CSV_LOGGER_H__

#include <rtthread.h>

/* 启动 CSV 记录线程（每 5 分钟自动写一条） */
int csv_logger_start(void);

/* 立即写一条记录（用于按键手动触发） */
void csv_logger_write_now(void);

/* 获取当前日志文件路径，如 /flash/logs/20260920.csv */
const char *csv_logger_current_file(void);

#endif /* __CSV_LOGGER_H__ */


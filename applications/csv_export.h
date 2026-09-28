#ifndef __CSV_EXPORT_H__
#define __CSV_EXPORT_H__

#include <rtthread.h>

/* 通过 UART2 导出指定 CSV 文件到 PC。
 * 返回导出的行数（>=0），失败返回负数。
 */
int csv_export_file(const char *path);

#endif /* __CSV_EXPORT_H__ */


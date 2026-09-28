#include "csv_export.h"
#include "csv_logger.h"

#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define DBG_TAG     "csv.exp"
#define DBG_LVL     DBG_INFO
#include <rtdbg.h>

#define EXPORT_LINE_MAX     160

int csv_export_file(const char *path)
{
    int fd;
    char rbuf[256];
    char line[EXPORT_LINE_MAX];
    int line_len = 0;
    int line_count = 0;
    int n;
    int i;

    if (path == RT_NULL) {
        return -RT_EINVAL;
    }

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        rt_kprintf("<<<CSV_ERROR: cannot open %s>>>\n", path);
        LOG_W("cannot open %s", path);
        return -RT_ERROR;
    }

    /* 开始标记 */
    rt_kprintf("<<<CSV_BEGIN>>>\n");
    rt_thread_mdelay(50);

    while ((n = read(fd, rbuf, sizeof(rbuf))) > 0) {
        for (i = 0; i < n; i++) {
            char c = rbuf[i];

            if (c == '\n') {
                if (line_len > 0) {
                    line[line_len] = '\0';
                    rt_kprintf("<<<CSV_LINE>>>%s\n", line);
                    line_count++;
                }
                line_len = 0;

                /* 每 20 行让出一次 CPU，避免串口缓冲阻塞 */
                if ((line_count % 20) == 0 && line_count > 0) {
                    rt_thread_mdelay(20);
                }
            } else if (c != '\r') {
                if (line_len < EXPORT_LINE_MAX - 1) {
                    line[line_len++] = c;
                }
            }
        }
    }

    /* 处理文件末尾无换行的最后一行 */
    if (line_len > 0) {
        line[line_len] = '\0';
        rt_kprintf("<<<CSV_LINE>>>%s\n", line);
        line_count++;
    }

    close(fd);

    /* 结束标记 */
    rt_kprintf("<<<CSV_END:%d>>>\n", line_count);
    LOG_I("exported %d lines from %s", line_count, path);

    return line_count;
}


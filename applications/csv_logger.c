#include "csv_logger.h"
#include "rtc_time.h"
#include "ds18b20.h"
#include "aht20.h"
#include "alarm.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#define DBG_TAG     "csv"
#define DBG_LVL     DBG_INFO
#include <rtdbg.h>

#define CSV_DIR             "/flash/logs"
#define CSV_INTERVAL_MS     (5 * 60 * 1000)   /* 5 分钟 */
#define CSV_THREAD_STACK    2048
#define CSV_THREAD_PRIO     20
#define CSV_THREAD_TICK     20

static char g_current_path[64] = "";

/* 确保 /flash/logs 目录存在 */
static void ensure_dir(void)
{
    struct stat st;

    if (stat(CSV_DIR, &st) == 0) {
        return;     /* 已存在 */
    }

    if (mkdir(CSV_DIR, 0) != 0) {
        LOG_W("mkdir %s failed", CSV_DIR);
    } else {
        LOG_I("created %s", CSV_DIR);
    }
}

/* 生成文件路径 /flash/logs/YYYYMMDD.csv */
static void build_path(char *buf, rt_size_t size, const struct tm *tm_now)
{
    rt_snprintf(buf, size, "%s/%04d%02d%02d.csv",
                CSV_DIR,
                tm_now->tm_year + 1900,
                tm_now->tm_mon + 1,
                tm_now->tm_mday);
}

/* 温度格式化，无效值输出 "NA" */
static void fmt_temp_csv(char *buf, rt_size_t size, rt_int32_t t_x100, rt_int32_t invalid)
{
    if (t_x100 == invalid) {
        rt_snprintf(buf, size, "NA");
    } else {
        int whole = (int)(t_x100 / 100);
        int frac  = (int)((t_x100 % 100 + 100) % 100);
        rt_snprintf(buf, size, "%d.%02d", whole, frac);
    }
}

void csv_logger_write_now(void)
{
    time_t now_t;
    struct tm tm_now;
    char path[64];
    char line[160];
    char grain_s[12];
    char env_s[12];
    int fd;
    int n;
    rt_int32_t grain, env_t;
    rt_uint16_t env_h;
    alarm_level_t lv;
    const char *lv_str;

    now_t = (time_t)rtc_time_get_timestamp();
    if (now_t <= 0) {
        LOG_W("no valid time");
        return;
    }

    ensure_dir();

    localtime_r(&now_t, &tm_now);
    build_path(path, sizeof(path), &tm_now);

    /* 采集数据 */
    grain = ds18b20_get_last();
    aht20_get_last(&env_t, &env_h);
    lv = alarm_get_combined_level();

    lv_str = (lv == ALARM_CRITICAL) ? "ALARM" :
             (lv == ALARM_WARNING)  ? "WARN"  : "NORMAL";

    fmt_temp_csv(grain_s, sizeof(grain_s), grain, DS18B20_TEMP_INVALID);
    fmt_temp_csv(env_s, sizeof(env_s), env_t, AHT20_TEMP_INVALID);

    n = rt_snprintf(line, sizeof(line),
                    "%04d-%02d-%02d %02d:%02d:%02d,%s,%s,%d.%d,%s\n",
                    tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
                    tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec,
                    grain_s, env_s,
                    env_h / 10, env_h % 10,
                    lv_str);

    fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0);
    if (fd < 0) {
        LOG_W("open %s failed", path);
        return;
    }

    if (write(fd, line, n) != n) {
        LOG_W("write %s failed", path);
    }
    close(fd);

    /* 记录当前路径 */
    rt_strncpy(g_current_path, path, sizeof(g_current_path) - 1);

    /* 去掉末尾换行打印 */
    if (n > 0 && line[n - 1] == '\n') line[n - 1] = '\0';
    LOG_I("%s", line);
}

const char *csv_logger_current_file(void)
{
    return g_current_path;
}

static void csv_thread_entry(void *param)
{
    (void)param;

    /* 首次启动后等一会儿再写第一条 */
    rt_thread_mdelay(10000);

    while (1) {
        csv_logger_write_now();
        rt_thread_mdelay(CSV_INTERVAL_MS);
    }
}

int csv_logger_start(void)
{
    rt_thread_t tid;

    tid = rt_thread_create("csv", csv_thread_entry, RT_NULL,
                           CSV_THREAD_STACK, CSV_THREAD_PRIO, CSV_THREAD_TICK);
    if (tid == RT_NULL) {
        LOG_E("failed to create thread");
        return -RT_ERROR;
    }

    rt_thread_startup(tid);
    return RT_EOK;
}


#include "settings.h"
#include "alarm.h"
#include "rtc_time.h"


#include <fcntl.h>
#include <unistd.h>

#define DBG_TAG     "settings"
#define DBG_LVL     DBG_INFO
#include <rtdbg.h>

#define SETTINGS_PATH       "/flash/settings.bin"
#define SETTINGS_MAGIC      0x47545053UL   /* "GTPS" */
#define SETTINGS_VERSION    1U

typedef struct {
    rt_uint32_t magic;
    rt_uint32_t version;
    rt_int32_t  env_low;
    rt_int32_t  env_high;
    rt_int32_t  grain_low;
    rt_int32_t  grain_high;
    rt_int64_t  last_saved_time;    /* ← 新增 */
    rt_uint32_t checksum;
} settings_blob_t;

static rt_uint32_t calc_checksum(const settings_blob_t *s)
{
    return s->magic ^ s->version
         ^ (rt_uint32_t)s->env_low ^ (rt_uint32_t)s->env_high
         ^ (rt_uint32_t)s->grain_low ^ (rt_uint32_t)s->grain_high
         ^ (rt_uint32_t)(s->last_saved_time & 0xFFFFFFFFULL)
         ^ (rt_uint32_t)((s->last_saved_time >> 32) & 0xFFFFFFFFULL);
}



rt_err_t settings_save(void)
{
    settings_blob_t s;
    int fd;
    int written;

    s.magic      = SETTINGS_MAGIC;
    s.version    = SETTINGS_VERSION;
    s.env_low    = alarm_get_env_low();
    s.env_high   = alarm_get_env_high();
    s.grain_low  = alarm_get_grain_low();
    s.grain_high = alarm_get_grain_high();
    s.last_saved_time = rtc_time_get_timestamp();    /* ← 新增 */
    s.checksum   = calc_checksum(&s);

    fd = open(SETTINGS_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0);
    if (fd < 0) {
        LOG_E("open %s failed: %d", SETTINGS_PATH, fd);
        return -RT_ERROR;
    }

    written = write(fd, &s, sizeof(s));
    close(fd);

    if (written != (int)sizeof(s)) {
        LOG_E("write failed: %d", written);
        return -RT_ERROR;
    }

    LOG_I("settings saved (env %d-%d, grain %d-%d)",
          (int)s.env_low, (int)s.env_high,
          (int)s.grain_low, (int)s.grain_high);
    return RT_EOK;
}

rt_err_t settings_load(void)
{
    settings_blob_t s;
    int fd;
    int n;

    fd = open(SETTINGS_PATH, O_RDONLY, 0);
    if (fd < 0) {
        LOG_I("no settings file, use defaults");
        return -RT_ENOSYS;
    }

    n = read(fd, &s, sizeof(s));
    close(fd);

    if (n != (int)sizeof(s)) {
        LOG_W("settings file size mismatch: %d", n);
        return -RT_ERROR;
    }

    if (s.magic != SETTINGS_MAGIC || s.version != SETTINGS_VERSION) {
        LOG_W("settings magic/version mismatch");
        return -RT_ERROR;
    }

    if (s.checksum != calc_checksum(&s)) {
        LOG_W("settings checksum mismatch");
        return -RT_ERROR;
    }

    alarm_set_env_range(s.env_low, s.env_high);
    alarm_set_grain_range(s.grain_low, s.grain_high);

    LOG_I("settings loaded (env %d-%d, grain %d-%d)",
          (int)s.env_low, (int)s.env_high,
          (int)s.grain_low, (int)s.grain_high);
    return RT_EOK;
}

void settings_init(void)
{
    /* 先设置默认值（alarm_init 里做的） */
    alarm_init();

    /* 尝试从 Flash 加载，加载失败则保留默认值 */
    settings_load();

    settings_restore_time();    /* ← 新增 */

}

rt_err_t settings_save_time(rt_int64_t timestamp)
{
    settings_blob_t s;
    int fd;
    int written;

    s.magic      = SETTINGS_MAGIC;
    s.version    = SETTINGS_VERSION;
    s.env_low    = alarm_get_env_low();
    s.env_high   = alarm_get_env_high();
    s.grain_low  = alarm_get_grain_low();
    s.grain_high = alarm_get_grain_high();
    s.last_saved_time = timestamp;
    s.checksum   = calc_checksum(&s);

    fd = open(SETTINGS_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0);
    if (fd < 0) {
        LOG_E("open %s failed: %d", SETTINGS_PATH, fd);
        return -RT_ERROR;
    }

    written = write(fd, &s, sizeof(s));
    close(fd);

    if (written != (int)sizeof(s)) {
        LOG_E("write failed: %d", written);
        return -RT_ERROR;
    }

    LOG_I("time saved: %ld", (long)timestamp);
    return RT_EOK;
}

void settings_restore_time(void)
{
    settings_blob_t s;
    int fd;
    int n;
    rt_int64_t current;

    /* RTC 时间在 2025-01-01 之后，说明是正常时间，不动 */
    current = rtc_time_get_timestamp();
    if (current >= 1735660800LL) {
        return;
    }

    fd = open(SETTINGS_PATH, O_RDONLY, 0);
    if (fd < 0) {
        return;
    }
    n = read(fd, &s, sizeof(s));
    close(fd);

    if (n != (int)sizeof(s)) return;
    if (s.magic != SETTINGS_MAGIC || s.version != SETTINGS_VERSION) return;
    if (s.checksum != calc_checksum(&s)) return;
    if (s.last_saved_time < 1735660800LL) return;

    if (rtc_time_set(s.last_saved_time) == RT_EOK) {
        LOG_I("RTC restored from settings: %ld", (long)s.last_saved_time);
    }
}


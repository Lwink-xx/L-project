#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <lv_rt_thread_conf.h>
#include "vg_lite.h"
#include "vg_lite_platform.h"
#include "lv_port_disp.h"
#include "rtconfig.h"

#include "ds18b20.h"
#include "aht20.h"
#include "rtc_time.h"
#include "history.h"
#include "alarm.h"
#include "settings.h"
#include "csv_logger.h"
#include "csv_export.h"

#define LED_PIN_G               GET_PIN(16, 6)

/* 阈值调整步长：0.1℃ */
#define THRESHOLD_STEP_X100     100
#define THRESHOLD_MIN_X100      1000   /* 10.0℃ */
#define THRESHOLD_MAX_X100      5000   /* 50.0℃ */

/* ---------------- UI 变量 ---------------- */

static lv_obj_t *g_main_screen;
static lv_obj_t *g_history_screen;
static lv_obj_t *g_settings_screen;
static lv_obj_t *g_settime_screen;

static lv_obj_t *g_alarm_bar;
static lv_obj_t *g_time_label;
static lv_obj_t *g_temp_label;
static lv_obj_t *g_env_label;
static lv_obj_t *g_history_table;

static lv_obj_t *g_threshold_label[4];

static lv_obj_t *g_settime_label[6];
static int g_edit_time[6];   /* 年 月 日 时 分 秒 */

/* ---------------- 前向声明 ---------------- */

static void back_btn_cb(lv_event_t *e);
static void history_btn_cb(lv_event_t *e);
static void settings_btn_cb(lv_event_t *e);
static void settime_btn_cb(lv_event_t *e);
static void savelog_btn_cb(lv_event_t *e);
static void export_csv_btn_cb(lv_event_t *e);      /* ← 新增 */

/* ---------------- 工具函数 ---------------- */

static void fmt_temp(char *buf, rt_size_t size, rt_int32_t t_x100, rt_int32_t invalid)
{
    if (t_x100 == invalid) {
        rt_snprintf(buf, size, "----");
    } else {
        int whole = (int)(t_x100 / 100);
        int frac  = (int)((t_x100 % 100 + 100) % 100);
        rt_snprintf(buf, size, "%d.%02d", whole, frac);
    }
}

static void fmt_time_short(char *buf, rt_size_t size, rt_int64_t ts)
{
    time_t t = (time_t)ts;
    struct tm tm_now;

    if (ts < 0 || localtime_r(&t, &tm_now) == RT_NULL) {
        rt_snprintf(buf, size, "--:--:--");
        return;
    }

    rt_snprintf(buf, size, "%02d:%02d:%02d",
                tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
}

/* ============================================================
 *  主界面
 * ============================================================ */

static void main_screen_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    g_main_screen = scr;

    /* 顶部报警条 */
    g_alarm_bar = lv_label_create(scr);
    lv_label_set_text(g_alarm_bar, "  Status: Normal  ");
    lv_obj_set_style_text_font(g_alarm_bar, &lv_font_montserrat_24, 0);
    lv_obj_set_style_bg_color(g_alarm_bar, lv_color_hex(0x00AA00), 0);
    lv_obj_set_style_bg_opa(g_alarm_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(g_alarm_bar, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_pad_all(g_alarm_bar, 10, 0);
    lv_obj_align(g_alarm_bar, LV_ALIGN_TOP_MID, 0, 5);

    /* 时间 */
    g_time_label = lv_label_create(scr);
    lv_label_set_text(g_time_label, "----/--/-- --:--:--");
    lv_obj_set_style_text_font(g_time_label, &lv_font_montserrat_24, 0);
    lv_obj_align(g_time_label, LV_ALIGN_TOP_MID, 0, 70);

    /* 标题 */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Grain Temp Probe");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 130);

    /* 粮堆温度 */
    g_temp_label = lv_label_create(scr);
    lv_label_set_text(g_temp_label, "Temp: ---- C");
    lv_obj_set_style_text_font(g_temp_label, &lv_font_montserrat_24, 0);
    lv_obj_align(g_temp_label, LV_ALIGN_CENTER, 0, -20);

    /* 环境 */
    g_env_label = lv_label_create(scr);
    lv_label_set_text(g_env_label, "Env: ---- C / ---- %RH");
    lv_obj_set_style_text_font(g_env_label, &lv_font_montserrat_24, 0);
    lv_obj_align(g_env_label, LV_ALIGN_CENTER, 0, 60);

    /* 底部按钮区：History | Settings | Save Log */
    lv_obj_t *btn_row = lv_obj_create(scr);
    lv_obj_remove_style_all(btn_row);
    lv_obj_set_size(btn_row, 460, 70);
    lv_obj_align(btn_row, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *b1 = lv_button_create(btn_row);
    lv_obj_set_size(b1, 140, 60);
    lv_obj_add_event_cb(b1, history_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l1 = lv_label_create(b1);
    lv_label_set_text(l1, "History");
    lv_obj_center(l1);

    lv_obj_t *b2 = lv_button_create(btn_row);
    lv_obj_set_size(b2, 140, 60);
    lv_obj_add_event_cb(b2, settings_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l2 = lv_label_create(b2);
    lv_label_set_text(l2, "Settings");
    lv_obj_center(l2);

    lv_obj_t *b3 = lv_button_create(btn_row);
    lv_obj_set_size(b3, 140, 60);
    lv_obj_add_event_cb(b3, savelog_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l3 = lv_label_create(b3);
    lv_label_set_text(l3, "Save Log");
    lv_obj_center(l3);
}

/* ============================================================
 *  历史界面
 * ============================================================ */

static void history_refresh_table(void)
{
    int n, i;

    if (g_history_table == RT_NULL) return;

    n = history_count();

    lv_table_set_column_count(g_history_table, 4);
    lv_table_set_row_count(g_history_table, HISTORY_MAX + 1);

    lv_table_set_cell_value(g_history_table, 0, 0, "Time");
    lv_table_set_cell_value(g_history_table, 0, 1, "Grain");
    lv_table_set_cell_value(g_history_table, 0, 2, "Env");
    lv_table_set_cell_value(g_history_table, 0, 3, "Humi");

    for (i = 0; i < HISTORY_MAX; i++) {
        int row = i + 1;
        history_record_t rec;
        char buf[32];

        if (i < n && history_get(i, &rec)) {
            fmt_time_short(buf, sizeof(buf), rec.timestamp);
            lv_table_set_cell_value(g_history_table, row, 0, buf);

            fmt_temp(buf, sizeof(buf), rec.grain_temp, DS18B20_TEMP_INVALID);
            lv_table_set_cell_value(g_history_table, row, 1, buf);

            fmt_temp(buf, sizeof(buf), rec.env_temp, AHT20_TEMP_INVALID);
            lv_table_set_cell_value(g_history_table, row, 2, buf);

            rt_snprintf(buf, sizeof(buf), "%d.%d",
                        rec.env_humi / 10, rec.env_humi % 10);
            lv_table_set_cell_value(g_history_table, row, 3, buf);
        } else {
            lv_table_set_cell_value(g_history_table, row, 0, "--:--:--");
            lv_table_set_cell_value(g_history_table, row, 1, "----");
            lv_table_set_cell_value(g_history_table, row, 2, "----");
            lv_table_set_cell_value(g_history_table, row, 3, "----");
        }
    }
}

static void history_screen_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    g_history_screen = scr;

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "History (Last 10)");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    g_history_table = lv_table_create(scr);
    lv_obj_set_size(g_history_table, 480, 620);
    lv_obj_align(g_history_table, LV_ALIGN_TOP_MID, 0, 70);
    lv_table_set_column_width(g_history_table, 0, 130);
    lv_table_set_column_width(g_history_table, 1, 110);
    lv_table_set_column_width(g_history_table, 2, 110);
    lv_table_set_column_width(g_history_table, 3, 100);

    lv_obj_t *back = lv_button_create(scr);
    lv_obj_set_size(back, 180, 60);
    lv_obj_align(back, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_add_event_cb(back, back_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *bl = lv_label_create(back);
    lv_label_set_text(bl, "Back");
    lv_obj_center(bl);
}

/* ============================================================
 *  时间设置界面
 * ============================================================ */

static void update_settime_labels(void)
{
    char buf[32];
    const char *names[6] = { "Year", "Month", "Day",
                             "Hour", "Min", "Sec" };

    for (int i = 0; i < 6; i++) {
        if (g_settime_label[i]) {
            rt_snprintf(buf, sizeof(buf), "%s: %d", names[i], g_edit_time[i]);
            lv_label_set_text(g_settime_label[i], buf);
        }
    }
}

static void settime_adjust_cb(lv_event_t *e)
{
    intptr_t arg = (intptr_t)lv_event_get_user_data(e);
    int which = (int)(arg >> 1);
    int up = (int)(arg & 1);
    int step = up ? 1 : -1;

    switch (which) {
    case 0:
        g_edit_time[0] += step;
        if (g_edit_time[0] < 2020) g_edit_time[0] = 2099;
        if (g_edit_time[0] > 2099) g_edit_time[0] = 2020;
        break;
    case 1:
        g_edit_time[1] += step;
        if (g_edit_time[1] < 1)  g_edit_time[1] = 12;
        if (g_edit_time[1] > 12) g_edit_time[1] = 1;
        break;
    case 2:
        g_edit_time[2] += step;
        if (g_edit_time[2] < 1)  g_edit_time[2] = 31;
        if (g_edit_time[2] > 31) g_edit_time[2] = 1;
        break;
    case 3:
        g_edit_time[3] += step;
        if (g_edit_time[3] < 0)  g_edit_time[3] = 23;
        if (g_edit_time[3] > 23) g_edit_time[3] = 0;
        break;
    case 4:
        g_edit_time[4] += step;
        if (g_edit_time[4] < 0)  g_edit_time[4] = 59;
        if (g_edit_time[4] > 59) g_edit_time[4] = 0;
        break;
    case 5:
        g_edit_time[5] += step;
        if (g_edit_time[5] < 0)  g_edit_time[5] = 59;
        if (g_edit_time[5] > 59) g_edit_time[5] = 0;
        break;
    }

    update_settime_labels();
}

static void settime_save_cb(lv_event_t *e)
{
    struct tm tm_val;
    time_t t;

    (void)e;

    rt_memset(&tm_val, 0, sizeof(tm_val));
    tm_val.tm_year = g_edit_time[0] - 1900;
    tm_val.tm_mon  = g_edit_time[1] - 1;
    tm_val.tm_mday = g_edit_time[2];
    tm_val.tm_hour = g_edit_time[3];
    tm_val.tm_min  = g_edit_time[4];
    tm_val.tm_sec  = g_edit_time[5];
    tm_val.tm_isdst = -1;

    t = mktime(&tm_val);
    if (t <= 0) {
        rt_kprintf("Set time failed: mktime error\n");
        return;
    }

    if (rtc_time_set((rt_int64_t)t) != RT_EOK) {
        rt_kprintf("Set time failed: rtc write error\n");
        return;
    }
    settings_save_time((rt_int64_t)t);

    rt_kprintf("Time set to: %04d-%02d-%02d %02d:%02d:%02d\n",
               g_edit_time[0], g_edit_time[1], g_edit_time[2],
               g_edit_time[3], g_edit_time[4], g_edit_time[5]);

    lv_screen_load(g_settings_screen);
}

static void settime_btn_cb(lv_event_t *e)
{
    time_t t = (time_t)rtc_time_get_timestamp();
    struct tm tm_now;

    (void)e;

    if (t > 0 && localtime_r(&t, &tm_now) != RT_NULL) {
        g_edit_time[0] = tm_now.tm_year + 1900;
        g_edit_time[1] = tm_now.tm_mon + 1;
        g_edit_time[2] = tm_now.tm_mday;
        g_edit_time[3] = tm_now.tm_hour;
        g_edit_time[4] = tm_now.tm_min;
        g_edit_time[5] = tm_now.tm_sec;
    } else {
        g_edit_time[0] = 2026;
        g_edit_time[1] = 9;
        g_edit_time[2] = 20;
        g_edit_time[3] = 12;
        g_edit_time[4] = 0;
        g_edit_time[5] = 0;
    }

    update_settime_labels();
    lv_screen_load(g_settime_screen);
}

static void create_settime_row(lv_obj_t *parent, int index)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, 460, 70);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *bm = lv_button_create(row);
    lv_obj_set_size(bm, 60, 60);
    lv_obj_add_event_cb(bm, settime_adjust_cb, LV_EVENT_CLICKED,
                        (void *)(intptr_t)((index << 1) | 0));
    lv_obj_t *lm = lv_label_create(bm);
    lv_label_set_text(lm, "-");
    lv_obj_set_style_text_font(lm, &lv_font_montserrat_24, 0);
    lv_obj_center(lm);

    g_settime_label[index] = lv_label_create(row);
    lv_obj_set_style_text_font(g_settime_label[index],
                               &lv_font_montserrat_24, 0);
    lv_label_set_text(g_settime_label[index], "...");

    lv_obj_t *bp = lv_button_create(row);
    lv_obj_set_size(bp, 60, 60);
    lv_obj_add_event_cb(bp, settime_adjust_cb, LV_EVENT_CLICKED,
                        (void *)(intptr_t)((index << 1) | 1));
    lv_obj_t *lp = lv_label_create(bp);
    lv_label_set_text(lp, "+");
    lv_obj_set_style_text_font(lp, &lv_font_montserrat_24, 0);
    lv_obj_center(lp);
}

static void settime_screen_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    g_settime_screen = scr;

    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(scr, 15, 0);
    lv_obj_set_style_pad_row(scr, 5, 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Set Time");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);

    for (int i = 0; i < 6; i++) {
        create_settime_row(scr, i);
    }

    lv_obj_t *btn_row = lv_obj_create(scr);
    lv_obj_remove_style_all(btn_row);
    lv_obj_set_size(btn_row, 460, 70);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *bs = lv_button_create(btn_row);
    lv_obj_set_size(bs, 180, 60);
    lv_obj_add_event_cb(bs, settime_save_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *ls = lv_label_create(bs);
    lv_label_set_text(ls, "Save");
    lv_obj_center(ls);

    lv_obj_t *bb = lv_button_create(btn_row);
    lv_obj_set_size(bb, 180, 60);
    lv_obj_add_event_cb(bb, back_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lb = lv_label_create(bb);
    lv_label_set_text(lb, "Back");
    lv_obj_center(lb);
}

/* ============================================================
 *  设置界面（报警阈值）
 * ============================================================ */

static void update_threshold_labels(void)
{
    char buf[32];

    if (g_threshold_label[0]) {
        fmt_temp(buf, sizeof(buf), alarm_get_env_low(), -1);
        lv_label_set_text_fmt(g_threshold_label[0], "Env Low : %s C", buf);
    }
    if (g_threshold_label[1]) {
        fmt_temp(buf, sizeof(buf), alarm_get_env_high(), -1);
        lv_label_set_text_fmt(g_threshold_label[1], "Env High: %s C", buf);
    }
    if (g_threshold_label[2]) {
        fmt_temp(buf, sizeof(buf), alarm_get_grain_low(), -1);
        lv_label_set_text_fmt(g_threshold_label[2], "Grain Low : %s C", buf);
    }
    if (g_threshold_label[3]) {
        fmt_temp(buf, sizeof(buf), alarm_get_grain_high(), -1);
        lv_label_set_text_fmt(g_threshold_label[3], "Grain High: %s C", buf);
    }
}

static void threshold_adjust_cb(lv_event_t *e)
{
    intptr_t arg = (intptr_t)lv_event_get_user_data(e);
    int which = (int)(arg >> 1);
    int up = (int)(arg & 1);
    rt_int32_t step = up ? THRESHOLD_STEP_X100 : -THRESHOLD_STEP_X100;
    rt_int32_t low, high;

    switch (which) {
    case 0:
        low = alarm_get_env_low() + step;
        high = alarm_get_env_high();
        if (low >= THRESHOLD_MIN_X100 && low <= high - 100)
            alarm_set_env_range(low, high);
        break;
    case 1:
        low = alarm_get_env_low();
        high = alarm_get_env_high() + step;
        if (high <= THRESHOLD_MAX_X100 && high >= low + 100)
            alarm_set_env_range(low, high);
        break;
    case 2:
        low = alarm_get_grain_low() + step;
        high = alarm_get_grain_high();
        if (low >= THRESHOLD_MIN_X100 && low <= high - 100)
            alarm_set_grain_range(low, high);
        break;
    case 3:
        low = alarm_get_grain_low();
        high = alarm_get_grain_high() + step;
        if (high <= THRESHOLD_MAX_X100 && high >= low + 100)
            alarm_set_grain_range(low, high);
        break;
    }

    settings_save();
    update_threshold_labels();
}

static void create_threshold_row(lv_obj_t *parent, int index)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, 460, 70);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    g_threshold_label[index] = lv_label_create(row);
    lv_obj_set_style_text_font(g_threshold_label[index],
                               &lv_font_montserrat_24, 0);
    lv_label_set_text(g_threshold_label[index], "...");

    lv_obj_t *btn_minus = lv_button_create(row);
    lv_obj_set_size(btn_minus, 60, 60);
    lv_obj_add_event_cb(btn_minus, threshold_adjust_cb, LV_EVENT_CLICKED,
                        (void *)(intptr_t)((index << 1) | 0));
    lv_obj_t *lm = lv_label_create(btn_minus);
    lv_label_set_text(lm, "-");
    lv_obj_set_style_text_font(lm, &lv_font_montserrat_24, 0);
    lv_obj_center(lm);

    lv_obj_t *btn_plus = lv_button_create(row);
    lv_obj_set_size(btn_plus, 60, 60);
    lv_obj_add_event_cb(btn_plus, threshold_adjust_cb, LV_EVENT_CLICKED,
                        (void *)((intptr_t)(index << 1) | 1));
    lv_obj_t *lp = lv_label_create(btn_plus);
    lv_label_set_text(lp, "+");
    lv_obj_set_style_text_font(lp, &lv_font_montserrat_24, 0);
    lv_obj_center(lp);
}

static void settings_screen_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    g_settings_screen = scr;

    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(scr, 15, 0);
    lv_obj_set_style_pad_row(scr, 5, 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Alarm Thresholds");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);

    create_threshold_row(scr, 0);
    create_threshold_row(scr, 1);
    create_threshold_row(scr, 2);
    create_threshold_row(scr, 3);

    /* Set Time 按钮 */
    lv_obj_t *btn_time = lv_button_create(scr);
    lv_obj_set_size(btn_time, 200, 55);
    lv_obj_add_event_cb(btn_time, settime_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lt = lv_label_create(btn_time);
    lv_label_set_text(lt, "Set Time");
    lv_obj_center(lt);

    /* Export CSV 按钮 —— 新增 */
    lv_obj_t *btn_exp = lv_button_create(scr);
    lv_obj_set_size(btn_exp, 200, 55);
    lv_obj_add_event_cb(btn_exp, export_csv_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *le = lv_label_create(btn_exp);
    lv_label_set_text(le, "Export CSV");
    lv_obj_center(le);

    /* Back 按钮 */
    lv_obj_t *back = lv_button_create(scr);
    lv_obj_set_size(back, 180, 55);
    lv_obj_add_event_cb(back, back_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *bl = lv_label_create(back);
    lv_label_set_text(bl, "Back");
    lv_obj_center(bl);

    update_threshold_labels();
}

/* ============================================================
 *  按钮回调
 * ============================================================ */

static void back_btn_cb(lv_event_t *e)
{
    (void)e;
    lv_screen_load(g_main_screen);
}

static void history_btn_cb(lv_event_t *e)
{
    (void)e;
    history_refresh_table();
    lv_screen_load(g_history_screen);
}

static void settings_btn_cb(lv_event_t *e)
{
    (void)e;
    update_threshold_labels();
    lv_screen_load(g_settings_screen);
}

static void savelog_btn_cb(lv_event_t *e)
{
    static rt_tick_t last_tick = 0;
    rt_tick_t now = rt_tick_get();

    (void)e;

    /* 3 秒冷却 */
    if (last_tick != 0 &&
        (now - last_tick) < rt_tick_from_millisecond(3000)) {
        rt_kprintf("Save Log: too frequent, ignored\n");
        return;
    }

    last_tick = now;
    csv_logger_write_now();
}

/* Export CSV 按钮回调 —— 新增 */
static void export_csv_btn_cb(lv_event_t *e)
{
    const char *path;

    (void)e;

    path = csv_logger_current_file();
    if (path == RT_NULL || path[0] == '\0') {
        rt_kprintf("<<<CSV_ERROR: no file>>>\n");
        return;
    }

    rt_kprintf("Export: %s\n", path);
    csv_export_file(path);
}

/* ============================================================
 *  定时器
 * ============================================================ */

static void ui_update_timer_cb(lv_timer_t *timer)
{
    rt_int32_t t, env_t;
    rt_uint16_t env_h;
    char time_str[24];
    char buf[32];
    alarm_level_t grain_lv, env_lv, combined;

    (void)timer;

    if (rtc_time_get_str(time_str, sizeof(time_str)) == RT_EOK) {
        lv_label_set_text(g_time_label, time_str);
    }

    t = ds18b20_get_last();
    fmt_temp(buf, sizeof(buf), t, DS18B20_TEMP_INVALID);
    lv_label_set_text_fmt(g_temp_label, "Temp: %s C", buf);
    grain_lv = alarm_check_grain(t);

    aht20_get_last(&env_t, &env_h);
    if (env_t == AHT20_TEMP_INVALID || env_h == AHT20_HUMI_INVALID) {
        lv_label_set_text(g_env_label, "Env: ---- C / ---- %RH");
        env_lv = ALARM_NORMAL;
    } else {
        lv_label_set_text_fmt(g_env_label, "Env: %d.%02d C / %d.%d %%RH",
                              (int)(env_t / 100),
                              (int)((env_t % 100 + 100) % 100),
                              (int)(env_h / 10), (int)(env_h % 10));
        env_lv = alarm_check_env(env_t);
    }

    combined = alarm_get_combined_level();
    if (combined == ALARM_CRITICAL) {
        lv_label_set_text(g_alarm_bar, "  !! ALARM: TEMP OUT OF RANGE !!  ");
        lv_obj_set_style_bg_color(g_alarm_bar, lv_color_hex(0xDD0000), 0);
    } else if (combined == ALARM_WARNING) {
        lv_label_set_text(g_alarm_bar, "  Warning: Approaching limit  ");
        lv_obj_set_style_bg_color(g_alarm_bar, lv_color_hex(0xFF8800), 0);
    } else {
        lv_label_set_text(g_alarm_bar, "  Status: Normal  ");
        lv_obj_set_style_bg_color(g_alarm_bar, lv_color_hex(0x00AA00), 0);
    }

    lv_color_t c_norm = lv_color_hex(0x000000);
    lv_color_t c_warn = lv_color_hex(0xFF8800);
    lv_color_t c_crit = lv_color_hex(0xDD0000);

    lv_obj_set_style_text_color(g_temp_label,
        grain_lv == ALARM_CRITICAL ? c_crit :
        grain_lv == ALARM_WARNING  ? c_warn : c_norm, 0);

    lv_obj_set_style_text_color(g_env_label,
        env_lv == ALARM_CRITICAL ? c_crit :
        env_lv == ALARM_WARNING  ? c_warn : c_norm, 0);
}

static void history_record_timer_cb(lv_timer_t *timer)
{
    history_record_t rec;

    (void)timer;

    rec.timestamp  = rtc_time_get_timestamp();
    rec.grain_temp = ds18b20_get_last();
    aht20_get_last(&rec.env_temp, &rec.env_humi);

    history_push(&rec);
}

/* ============================================================
 *  LVGL 入口
 * ============================================================ */

void lv_user_gui_init(void)
{
    history_init();
    settings_init();      /* 内部会调 alarm_init，然后从 Flash 加载阈值 */

    main_screen_create();
    history_screen_create();
    settings_screen_create();
    settime_screen_create();

    lv_timer_create(ui_update_timer_cb, 1000, NULL);
    lv_timer_create(history_record_timer_cb, 5000, NULL);

    lv_screen_load(g_main_screen);
}

/* ============================================================
 *  main
 * ============================================================ */

int main(void)
{
    rt_kprintf("Hello RT-Thread\n");
    rt_kprintf("It's cortex-m55\n");

    rt_pin_mode(LED_PIN_G, PIN_MODE_OUTPUT);

    rtc_time_init();
    ds18b20_start_collector();
    aht20_start_collector();
    csv_logger_start();
    lvgl_thread_init();

    while (1) {
        rt_pin_write(LED_PIN_G, PIN_LOW);
        rt_thread_mdelay(500);
        rt_pin_write(LED_PIN_G, PIN_HIGH);
        rt_thread_mdelay(500);
    }
    return 0;
}

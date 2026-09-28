#include "history.h"

static history_record_t g_history[HISTORY_MAX];
static int g_head;      /* 下一次写入位置 */
static int g_count;     /* 已存条数 */

void history_init(void)
{
    g_head = 0;
    g_count = 0;
}

void history_push(const history_record_t *rec)
{
    if (rec == RT_NULL) {
        return;
    }

    g_history[g_head] = *rec;
    g_head = (g_head + 1) % HISTORY_MAX;

    if (g_count < HISTORY_MAX) {
        g_count++;
    }
}

int history_count(void)
{
    return g_count;
}

rt_bool_t history_get(int index, history_record_t *out)
{
    int pos;

    if (out == RT_NULL || index < 0 || index >= g_count) {
        return RT_FALSE;
    }

    /* index = 0 → g_head - 1，index = 1 → g_head - 2 ... */
    pos = (g_head - 1 - index + HISTORY_MAX) % HISTORY_MAX;
    *out = g_history[pos];
    return RT_TRUE;
}

#include "log.h"
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <string.h>

static FILE *g_log_fp = NULL;
static log_level_t g_log_level = LOG_INFO;
static pthread_mutex_t g_log_lock = PTHREAD_MUTEX_INITIALIZER;

static const char *level_to_str(log_level_t level)
{
    switch (level)
    {
    case LOG_ERROR:
        return "ERROR";
    case LOG_WARN:
        return "WARN";
    case LOG_INFO:
        return "INFO";
    case LOG_DEBUG:
        return "DEBUG";
    default:
        return "INFO";
    }
}

void log_init(log_level_t level, const char *file_path)
{
    pthread_mutex_lock(&g_log_lock);
    g_log_level = level;
    if (g_log_fp && g_log_fp != stdout) // 如果之前已经打开过日志文件，就先关掉（避免文件句柄泄漏）。
    {
        fclose(g_log_fp);
        g_log_fp = NULL;
    }
    if (file_path && file_path[0] != '\0') // 如果传了文件路径且不是空字符串，就尝试 fopen(file_path, "a")，"a" 表示“追加写入”，不覆盖旧日志。
    {
        g_log_fp = fopen(file_path, "a");
        if (!g_log_fp)
        {
            g_log_fp = stdout; // 如果没传文件路径，或打开失败，就默认输出到终端。
        }
    }
    else
    {
        g_log_fp = stdout;
    }
    pthread_mutex_unlock(&g_log_lock);
}

void log_close(void)
{
    pthread_mutex_lock(&g_log_lock);
    if (g_log_fp && g_log_fp != stdout)
    {
        fclose(g_log_fp);
    }
    g_log_fp = NULL;
    pthread_mutex_unlock(&g_log_lock);
}

void log_set_level(log_level_t level)
{
    pthread_mutex_lock(&g_log_lock);
    g_log_level = level;
    pthread_mutex_unlock(&g_log_lock);
}

log_level_t log_get_level(void)
{
    return g_log_level;
}

void log_write(log_level_t level, const char *fmt, ...)
{
    if (level > g_log_level)
    {
        return;
    }

    struct timeval tv;
    gettimeofday(&tv, NULL); // 得到秒和微秒
    struct tm tm_now;
    localtime_r(&tv.tv_sec, &tm_now); // 变成本地时间结构

    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_now); // 格式化成字符串

    pthread_mutex_lock(&g_log_lock);
    if (!g_log_fp)
    {
        g_log_fp = stdout;
    }

    fprintf(g_log_fp, "%s.%03ld [%s] ", ts, tv.tv_usec / 1000, level_to_str(level));

    va_list args;
    va_start(args, fmt);
    vfprintf(g_log_fp, fmt, args);
    va_end(args);

    fprintf(g_log_fp, "\n");
    fflush(g_log_fp);
    pthread_mutex_unlock(&g_log_lock);
}

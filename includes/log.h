#ifndef _LOG_H_
#define _LOG_H_

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LOG_ERROR = 0,
    LOG_WARN  = 1,
    LOG_INFO  = 2,
    LOG_DEBUG = 3
} log_level_t;

void log_init(log_level_t level, const char *file_path);
void log_close(void);
void log_set_level(log_level_t level);
log_level_t log_get_level(void);
void log_write(log_level_t level, const char *fmt, ...);

#define LOGE(...) log_write(LOG_ERROR, __VA_ARGS__)
#define LOGW(...) log_write(LOG_WARN,  __VA_ARGS__)
#define LOGI(...) log_write(LOG_INFO,  __VA_ARGS__)
#define LOGD(...) log_write(LOG_DEBUG, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif

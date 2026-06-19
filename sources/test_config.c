#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "test_config.h"

#define DEFAULT_SERVER_IP "192.168.5.2"
#define DEFAULT_SERVER_PORT 9999
#define DEFAULT_UART_DEV "/dev/ttyAS5"
#define DEFAULT_UART_BAUD 115200
#define DEFAULT_HEART_TIMEOUT 10
#define DEFAULT_HEART_MISS_MAX 3
#define DEFAULT_ENABLE_CRC 0
#define DEFAULT_RECONNECT_MIN_MS 1000
#define DEFAULT_RECONNECT_MAX_MS 30000
#define DEFAULT_STATS_INTERVAL 10
#define DEFAULT_LOG_LEVEL 2
#define DEFAULT_LOG_FILE ""

// 先把字符串前面的空格跳过
// 再把结尾空格删掉
// 这样允许 key = value 这种写法。
static void trim(char *s)
{
    char *p = s;
    while (isspace((unsigned char)*p))
        p++;
    if (p != s)
        memmove(s, p, strlen(p) + 1);
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1]))
    {
        s[len - 1] = '\0';
        len--;
    }
}

// 把 ERROR/WARN/INFO/DEBUG 字符串转成数字 0~3
// 如果不是这些字符串，就 atoi 当数字处理
static int parse_log_level(const char *val)
{
    if (!val)
        return DEFAULT_LOG_LEVEL;
    if (strcasecmp(val, "ERROR") == 0)
        return 0;
    if (strcasecmp(val, "WARN") == 0)
        return 1;
    if (strcasecmp(val, "INFO") == 0)
        return 2;
    if (strcasecmp(val, "DEBUG") == 0)
        return 3;
    return atoi(val);
}

void ConfigData_Default(struct Config_Data *out)
{
    if (!out)
        return;
    memset(out, 0, sizeof(*out));
    strncpy(out->IP, DEFAULT_SERVER_IP, sizeof(out->IP) - 1);
    out->Port = DEFAULT_SERVER_PORT;
    strncpy(out->UartDev, DEFAULT_UART_DEV, sizeof(out->UartDev) - 1);
    out->UartBaud = DEFAULT_UART_BAUD;
    out->HeartTimeout = DEFAULT_HEART_TIMEOUT;
    out->HeartMissMax = DEFAULT_HEART_MISS_MAX;
    out->EnableCrc = DEFAULT_ENABLE_CRC;
    out->ReconnectMinMs = DEFAULT_RECONNECT_MIN_MS;
    out->ReconnectMaxMs = DEFAULT_RECONNECT_MAX_MS;
    out->StatsInterval = DEFAULT_STATS_INTERVAL;
    out->LogLevel = DEFAULT_LOG_LEVEL;
    strncpy(out->LogFile, DEFAULT_LOG_FILE, sizeof(out->LogFile) - 1);
}

int ConfigData_Load(const char *path, struct Config_Data *out)
{
    if (!out)
        return -1;
    ConfigData_Default(out);

    FILE *fp = fopen(path ? path : "gateway.conf", "r");
    if (!fp)
    {
        perror("Open gateway.conf failed");
        return -1;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        trim(line);
        if (line[0] == '\0' || line[0] == '#' || line[0] == ';')
        {
            continue;
        }

        char *eq = strchr(line, '='); // eq指向等号出现的位置
        if (!eq)
            continue;
        // line 是字符数组的首地址，eq 是指向这个数组里“= 的位置”的指针，它们都指向同一块内存的不同位置。
        // *eq = '\0' 改掉那个位置的字符，就能把同一块内存“切成两段字符串”。
        *eq = '\0';
        char *key = line;
        char *val = eq + 1;
        trim(key);
        trim(val);

        if (strcasecmp(key, "SERVER_IP") == 0)
        {
            strncpy(out->IP, val, sizeof(out->IP) - 1);
        }
        else if (strcasecmp(key, "SERVER_PORT") == 0)
        {
            out->Port = atoi(val);
        }
        else if (strcasecmp(key, "UART_DEV") == 0)
        {
            strncpy(out->UartDev, val, sizeof(out->UartDev) - 1);
        }
        else if (strcasecmp(key, "UART_BAUD") == 0)
        {
            out->UartBaud = atoi(val);
        }
        else if (strcasecmp(key, "HEART_TIMEOUT") == 0)
        {
            out->HeartTimeout = atoi(val);
        }
        else if (strcasecmp(key, "HEART_MISS_MAX") == 0)
        {
            out->HeartMissMax = atoi(val);
        }
        else if (strcasecmp(key, "ENABLE_CRC") == 0)
        {
            out->EnableCrc = atoi(val);
        }
        else if (strcasecmp(key, "RECONNECT_MIN_MS") == 0)
        {
            out->ReconnectMinMs = atoi(val);
        }
        else if (strcasecmp(key, "RECONNECT_MAX_MS") == 0)
        {
            out->ReconnectMaxMs = atoi(val);
        }
        else if (strcasecmp(key, "STATS_INTERVAL") == 0)
        {
            out->StatsInterval = atoi(val);
        }
        else if (strcasecmp(key, "LOG_LEVEL") == 0)
        {
            out->LogLevel = parse_log_level(val);
        }
        else if (strcasecmp(key, "LOG_FILE") == 0)
        {
            strncpy(out->LogFile, val, sizeof(out->LogFile) - 1);
        }
    }

    fclose(fp);
    return 0;
}

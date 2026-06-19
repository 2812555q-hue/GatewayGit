#ifndef _TEST_CONFIG_H_   // 1. 如果没有定义过这个宏
#define _TEST_CONFIG_H_   // 2. 赶紧定义它

struct Config_Data {
    char IP[64];
    int Port;
    char UartDev[64];
    int UartBaud;
    int HeartTimeout;
    int HeartMissMax;
    int EnableCrc;
    int ReconnectMinMs;
    int ReconnectMaxMs;
    int StatsInterval;
    int LogLevel;
    char LogFile[128];
};

void ConfigData_Default(struct Config_Data *out);
int ConfigData_Load(const char *path, struct Config_Data *out);

#endif                   // 3. 结束

#ifndef _TEST_CONFIG_H_   // 1. 如果没有定义过这个宏
#define _TEST_CONFIG_H_   // 2. 赶紧定义它

struct Config_Data {
    char IP[20];
    int Port;
};
struct Config_Data ConfigData_Get(void);

#endif                   // 3. 结束


#ifndef _UART_H_   // 1. 如果没有定义过这个宏
#define _UART_H_   // 2. 赶紧定义它

int uart_init(const char *path, int baud);
void *Uart_thread(void* arg);
#endif                   // 3. 结束

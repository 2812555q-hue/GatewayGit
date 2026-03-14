#ifndef _TCP_CLIENT_H_   // 1. 如果没有定义过这个宏
#define _TCP_CLIENT_H_   // 2. 赶紧定义它

#include "gateway.h"
int Tcp_connect_server(int *sock);
void *Heartbeat_thread(void* arg);
void *Reconnect_thread(void* arg);
void *Tcp_client_thread(void* arg);
void trigger_reconnect(GateWayContext *p);
#endif                   // 3. 结束
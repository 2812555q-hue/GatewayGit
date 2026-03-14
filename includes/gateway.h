#ifndef _GATEWAY_H_   // 1. 如果没有定义过这个宏
#define _GATEWAY_H_   // 2. 赶紧定义它

#define WAIT_HEADER  1
#define WAIT_LENGTH  2
#define WAIT_PAYLOAD  3
#define WAIT_TAIL  4
#define HEART_TIMEOUT 10 //每次的超时时间
#define HEART_MISS_MAX 3 //允许超时应答最大次数

#include "time.h"
#include "pthread.h"
#include <semaphore.h>   

typedef struct 
{
    int uart_fd;
    int sock_fd;
    int Tcp_connected;//当前是否已连接，1表示已连接，0表示未连接
    int reconnecting;//当前是否正在重连，1表示正在重连，0表示还未重连
    unsigned char send_heart[4];
    unsigned char ack_heart[4];
    time_t heart_sendtime;      // 本轮心跳发送时间，用来判断这轮是否超时
    time_t last_heart_send;     // 上一次发送心跳的时间，用来控制发送周期
    
    int heart_waiting_ack;      // 当前是否在等ACK
    int timeout_cnt;            // 连续超时次数
    pthread_mutex_t tcp_lock;
   
}GateWayContext;
extern sem_t Reconnect_sem;//定义断线重连信号量

#endif                   // 3. 结束
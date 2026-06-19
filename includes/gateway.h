#ifndef _GATEWAY_H_ // 1. 如果没有定义过这个宏
#define _GATEWAY_H_ // 2. 赶紧定义它

#define WAIT_HEADER 1
#define WAIT_LENGTH 2
#define WAIT_PAYLOAD 3
#define WAIT_CRC 4
#define WAIT_TAIL 5

#include "time.h"
#include "pthread.h"
#include <semaphore.h>
#include <stdint.h>
#include "test_config.h"

typedef struct
{
    uint64_t uart_rx_frames;
    uint64_t uart_rx_crc_err;
    uint64_t uart_rx_len_err;
    uint64_t uart_rx_tail_err;
    uint64_t uart_tx_frames;

    uint64_t tcp_rx_frames;
    uint64_t tcp_rx_crc_err;
    uint64_t tcp_rx_len_err;
    uint64_t tcp_rx_tail_err;
    uint64_t tcp_tx_frames;

    uint64_t reconnect_cnt;
} GatewayStats;

typedef struct
{
    int uart_fd;
    int sock_fd;
    int Tcp_connected; // 当前是否已连接，1表示已连接，0表示未连接
    int reconnecting;  // 当前是否正在重连，1表示正在重连，0表示还未重连
    unsigned char send_heart[4];
    unsigned char ack_heart[4];
    time_t heart_sendtime;  // 本轮心跳发送时间，用来判断这轮是否超时
    time_t last_heart_send; // 上一次发送心跳的时间，用来控制发送周期

    int heart_waiting_ack;      // 当前是否在等ACK
    int timeout_cnt;            // 连续超时次数
    pthread_mutex_t tcp_lock;   // 保护 socket 相关共享数据
    pthread_mutex_t stats_lock; // 保护统计计数
    struct Config_Data config;  // 从配置文件读出来的所有参数
    GatewayStats stats;         // 统计计数结构（收发帧、CRC 错误、重连次数等）

} GateWayContext;
extern sem_t Reconnect_sem; // 定义断线重连信号量

#endif

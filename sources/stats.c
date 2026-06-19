#include "stats.h"
#include "gateway.h"
#include "log.h"
#include <unistd.h>
#include <string.h>

void *Stats_thread(void *arg)
{
    GateWayContext *p = (GateWayContext *)arg;
    GatewayStats last = {0};

    while (1)
    {
        sleep(p->config.StatsInterval > 0 ? p->config.StatsInterval : 10);
        pthread_mutex_lock(&p->stats_lock);
        GatewayStats now = p->stats;
        pthread_mutex_unlock(&p->stats_lock);

        GatewayStats delta;
        memset(&delta, 0, sizeof(delta));
        delta.uart_rx_frames = now.uart_rx_frames - last.uart_rx_frames;
        delta.uart_rx_crc_err = now.uart_rx_crc_err - last.uart_rx_crc_err;
        delta.uart_rx_len_err = now.uart_rx_len_err - last.uart_rx_len_err;
        delta.uart_rx_tail_err = now.uart_rx_tail_err - last.uart_rx_tail_err;
        delta.uart_tx_frames = now.uart_tx_frames - last.uart_tx_frames;

        delta.tcp_rx_frames = now.tcp_rx_frames - last.tcp_rx_frames;
        delta.tcp_rx_crc_err = now.tcp_rx_crc_err - last.tcp_rx_crc_err;
        delta.tcp_rx_len_err = now.tcp_rx_len_err - last.tcp_rx_len_err;
        delta.tcp_rx_tail_err = now.tcp_rx_tail_err - last.tcp_rx_tail_err;
        delta.tcp_tx_frames = now.tcp_tx_frames - last.tcp_tx_frames;

        delta.reconnect_cnt = now.reconnect_cnt - last.reconnect_cnt;

        LOGI("stats delta: uart_rx=%llu uart_tx=%llu uart_crc=%llu uart_len=%llu uart_tail=%llu tcp_rx=%llu tcp_tx=%llu tcp_crc=%llu tcp_len=%llu tcp_tail=%llu reconnect=%llu",
             (unsigned long long)delta.uart_rx_frames,
             (unsigned long long)delta.uart_tx_frames,
             (unsigned long long)delta.uart_rx_crc_err,
             (unsigned long long)delta.uart_rx_len_err,
             (unsigned long long)delta.uart_rx_tail_err,
             (unsigned long long)delta.tcp_rx_frames,
             (unsigned long long)delta.tcp_tx_frames,
             (unsigned long long)delta.tcp_rx_crc_err,
             (unsigned long long)delta.tcp_rx_len_err,
             (unsigned long long)delta.tcp_rx_tail_err,
             (unsigned long long)delta.reconnect_cnt);

        last = now;
    }

    return NULL;
}

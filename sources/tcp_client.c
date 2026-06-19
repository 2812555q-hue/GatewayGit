#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <time.h>
#include "gpio_lib.h"
#include "test_config.h"
#include "gateway.h"
#include "log.h"
#include "crc8.h"

#define BUFFER_SIZ (4 * 1024)

static void stats_inc(GateWayContext *p, uint64_t *field)
{
    pthread_mutex_lock(&p->stats_lock);
    (*field)++;
    pthread_mutex_unlock(&p->stats_lock);
}

static void set_socket_opts(int sockfd)
{
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
#ifdef TCP_KEEPIDLE
    int idle = 15;
    setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
#endif
#ifdef TCP_KEEPINTVL
    int intvl = 5;
    setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl));
#endif
#ifdef TCP_KEEPCNT
    int cnt = 3;
    setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, &cnt, sizeof(cnt));
#endif
}

int Tcp_connect_server(int *sock, const struct Config_Data *cfg)
{
    int sockfd;
    struct sockaddr_in server;

    if (!cfg || !sock)
        return -1;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        LOGE("create socket failed");
        return -1;
    }

    set_socket_opts(sockfd);

    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(cfg->Port);
    server.sin_addr.s_addr = inet_addr(cfg->IP);

    if (connect(sockfd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        LOGW("connect server fail: %s:%d errno=%d", cfg->IP, cfg->Port, errno);
        close(sockfd);
        return -1;
    }

    LOGI("connect server success: %s:%d", cfg->IP, cfg->Port);
    *sock = sockfd;
    return 0;
}

void trigger_reconnect(GateWayContext *p)
{
    int need_post = 0;
    pthread_mutex_lock(&p->tcp_lock);
    p->Tcp_connected = 0;
    if (p->reconnecting == 0)
    {
        p->reconnecting = 1;
        p->Tcp_connected = 0;
        p->heart_waiting_ack = 0;
        need_post = 1;
    }
    pthread_mutex_unlock(&p->tcp_lock);
    if (need_post)
    {
        stats_inc(p, &p->stats.reconnect_cnt);
        sem_post(&Reconnect_sem);
    }
}

void *Tcp_client_thread(void *arg)
{
    GateWayContext *p = (GateWayContext *)arg;
    int res, drop_frame = 0;
    unsigned char buf[1024], Packet_Buf[1024];
    int uartfd, sockfd, connected;
    int state = WAIT_HEADER, count = 0, exp_len = 0;
    int enable_crc = 0;
    unsigned char expected_crc = 0;

    while (1)
    {
        pthread_mutex_lock(&p->tcp_lock);
        sockfd = p->sock_fd;
        connected = p->Tcp_connected;
        enable_crc = p->config.EnableCrc;
        pthread_mutex_unlock(&p->tcp_lock);

        if (connected == 1)
        {
            res = recv(sockfd, buf, sizeof(buf), 0);
            if (res < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                {
                    usleep(100000);
                    continue;
                }
                LOGW("tcp recv error, errno=%d", errno);
                trigger_reconnect(p);
                exp_len = 0;
                state = WAIT_HEADER;
                count = 0;
                continue;
            }
            else if (res == 0)
            {
                LOGW("tcp peer closed");
                trigger_reconnect(p);
                exp_len = 0;
                state = WAIT_HEADER;
                count = 0;
                continue;
            }
            else
            {
                drop_frame = 0;
                for (int i = 0; i < res; i++)
                {
                    switch (state)
                    {
                    case WAIT_HEADER:
                        if (buf[i] == 0xAA)
                        {
                            count = 0;
                            state = WAIT_LENGTH;
                        }
                        break;
                    case WAIT_LENGTH:
                        exp_len = (unsigned char)buf[i];
                        if (exp_len > 1024 || exp_len == 0)
                        {
                            stats_inc(p, &p->stats.tcp_rx_len_err);
                            state = WAIT_HEADER;
                        }
                        else
                        {
                            state = WAIT_PAYLOAD;
                        }
                        break;
                    case WAIT_PAYLOAD:
                        Packet_Buf[count++] = buf[i];
                        if (count == exp_len)
                        {
                            state = enable_crc ? WAIT_CRC : WAIT_TAIL;
                        }
                        break;
                    case WAIT_CRC:
                        expected_crc = buf[i];
                        state = WAIT_TAIL;
                        break;
                    case WAIT_TAIL:
                        if (buf[i] == 0xEE)
                        {
                            if (enable_crc)
                            {
                                unsigned char crc = crc8_compute(Packet_Buf, exp_len);
                                if (crc != expected_crc)
                                {
                                    LOGW("tcp crc error: expect=0x%02X got=0x%02X len=%d", expected_crc, crc, exp_len);
                                    stats_inc(p, &p->stats.tcp_rx_crc_err);
                                    drop_frame = 1;
                                    break;
                                }
                            }

                            stats_inc(p, &p->stats.tcp_rx_frames);
                            if (exp_len == 1 && Packet_Buf[0] == 0x01)
                            {
                                pthread_mutex_lock(&p->tcp_lock);
                                p->heart_waiting_ack = 0;
                                p->timeout_cnt = 0;
                                p->last_heart_send = time(NULL);
                                pthread_mutex_unlock(&p->tcp_lock);
                                LOGD("heartbeat ack receive");
                            }
                            else
                            {
                                pthread_mutex_lock(&p->tcp_lock);
                                uartfd = p->uart_fd;
                                pthread_mutex_unlock(&p->tcp_lock);
                                write(uartfd, Packet_Buf, exp_len);
                                stats_inc(p, &p->stats.uart_tx_frames);
                            }
                        }
                        else
                        {
                            LOGW("tcp frame tail error: expect=0xEE got=0x%02X len=%d", buf[i], exp_len);
                            stats_inc(p, &p->stats.tcp_rx_tail_err);
                            exp_len = 0;
                            drop_frame = 1;
                        }
                        memset(Packet_Buf, 0, 1024);
                        state = WAIT_HEADER;
                        count = 0;
                        break;
                    default:
                        state = WAIT_HEADER;
                        break;
                    }
                    if (drop_frame == 1)
                        break;
                }
                LED_Set(71, 1);
                usleep(100000);
                LED_Set(71, 0);
            }
        }
        else
        {
            usleep(100000);
        }
    }
    pthread_exit(NULL);
}

static int next_backoff_ms(int current, int min_ms, int max_ms)
{
    if (current < min_ms)
        current = min_ms;
    int next = current * 2;
    if (next > max_ms)
        next = max_ms;
    return next;
}

void *Reconnect_thread(void *arg)
{
    GateWayContext *p = (GateWayContext *)arg;
    int backoff_ms = 0;

    while (1)
    {
        sem_wait(&Reconnect_sem);
        while (sem_trywait(&Reconnect_sem) == 0)
        {
            LOGD("drop duplicated reconnect request");
        }
        LOGI("reconnect thread wake up");
        pthread_mutex_lock(&p->tcp_lock);
        p->Tcp_connected = 0;
        p->reconnecting = 1;
        if (p->sock_fd >= 0)
        {
            shutdown(p->sock_fd, SHUT_RDWR);
            close(p->sock_fd);
            p->sock_fd = -1;
        }
        pthread_mutex_unlock(&p->tcp_lock);

        backoff_ms = p->config.ReconnectMinMs;
        if (backoff_ms <= 0)
            backoff_ms = 1000;
        int max_ms = p->config.ReconnectMaxMs;
        if (max_ms < backoff_ms)
            max_ms = backoff_ms;

        while (1)
        {
            int newfd = -1;
            if (Tcp_connect_server(&newfd, &p->config) == 0)
            {
                LOGI("reconnect success");
                pthread_mutex_lock(&p->tcp_lock);
                p->sock_fd = newfd;
                p->reconnecting = 0;
                p->Tcp_connected = 1;
                p->heart_waiting_ack = 0;
                p->timeout_cnt = 0;
                p->heart_sendtime = 0;
                p->last_heart_send = time(NULL);
                pthread_mutex_unlock(&p->tcp_lock);
                break;
            }
            else
            {
                int jitter = rand() % 200; // 生成0-199随即抖动
                int sleep_ms = backoff_ms + jitter;
                usleep(sleep_ms * 1000);
                backoff_ms = next_backoff_ms(backoff_ms, p->config.ReconnectMinMs, max_ms);
            }
        }
    }
    pthread_exit(NULL);
}

void *Heartbeat_thread(void *arg)
{
    GateWayContext *p = (GateWayContext *)arg;
    int ret = 0;
    int sockfd, connected;
    int waiting_ack = 0;
    time_t last_heart_send;
    int heart_timeout = 0;
    int heart_miss_max = 0;

    while (1)
    {
        sleep(1);
        pthread_mutex_lock(&p->tcp_lock);
        sockfd = p->sock_fd;
        waiting_ack = p->heart_waiting_ack;
        connected = p->Tcp_connected;
        last_heart_send = p->last_heart_send;
        heart_timeout = p->config.HeartTimeout;
        heart_miss_max = p->config.HeartMissMax;
        pthread_mutex_unlock(&p->tcp_lock);

        if (connected == 1)
        {
            if (waiting_ack == 0 && (time(NULL) - last_heart_send) >= heart_timeout)
            {
                ret = send(sockfd, p->send_heart, sizeof(p->send_heart), 0);
                if (ret <= 0)
                {
                    LOGW("heart send failed");
                    trigger_reconnect(p);
                    continue;
                }
                else
                {
                    LOGD("heartbeat req send");
                    pthread_mutex_lock(&p->tcp_lock);
                    p->heart_sendtime = time(NULL);
                    p->last_heart_send = p->heart_sendtime;
                    p->heart_waiting_ack = 1;
                    pthread_mutex_unlock(&p->tcp_lock);
                }
            }
            pthread_mutex_lock(&p->tcp_lock);
            if (p->heart_waiting_ack)
            {
                if (time(NULL) - p->heart_sendtime >= heart_timeout)
                {
                    p->timeout_cnt++;
                    p->heart_waiting_ack = 0;
                }
            }
            if (p->timeout_cnt >= heart_miss_max)
            {
                p->timeout_cnt = 0;
                pthread_mutex_unlock(&p->tcp_lock);
                trigger_reconnect(p);
                continue;
            }
            pthread_mutex_unlock(&p->tcp_lock);
        }
    }
    pthread_exit(NULL);
}

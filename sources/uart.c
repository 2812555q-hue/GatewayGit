#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include "gateway.h"
#include "tcp_client.h"
#include "gpio_lib.h"
#include "log.h"
#include "crc8.h"

static speed_t baud_to_speed(int baud)
{
    switch (baud)
    {
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
        default: return B115200;
    }
}

int uart_init(const char *path, int baud)
{
    int fd;

    LOGI("uart init: dev=%s baud=%d", path, baud);
    fd = open(path, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        LOGE("fail to open %s", path);
        return -1;
    }

    struct termios opt;
    tcflush(fd, TCIOFLUSH);
    if (tcgetattr(fd, &opt) != 0)
    {
        LOGE("tcgetattr failed for %s", path);
        close(fd);
        return -1;
    }

    speed_t spd = baud_to_speed(baud);
    cfsetospeed(&opt, spd);
    cfsetispeed(&opt, spd);

    opt.c_cflag &= ~CSIZE;
    opt.c_cflag |= CS8;
    opt.c_cflag &= ~PARENB;
    opt.c_cflag &= ~CSTOPB;

    opt.c_iflag &= ~(IXON | IXOFF | IXANY);
    opt.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    opt.c_oflag &= ~OPOST;

    opt.c_cc[VMIN] = 1;
    opt.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSANOW, &opt) != 0)
    {
        LOGE("tcsetattr failed for %s", path);
        close(fd);
        return -1;
    }

    LOGI("uart configured: %s %d 8N1", path, baud);
    return fd;
}

static void stats_inc(GateWayContext *p, uint64_t *field)
{
    pthread_mutex_lock(&p->stats_lock);
    (*field)++;
    pthread_mutex_unlock(&p->stats_lock);
}

void *Uart_thread(void* arg)
{
    GateWayContext *p = (GateWayContext *)arg;
    int res = 0, ret_send = 0;
    unsigned char buf[1024], Packet_Buf[1024];
    int uartfd = 0, sockfd = 0, connected;
    int state = WAIT_HEADER, count = 0, exp_len = 0;
    int drop_frame = 0;
    int enable_crc = 0;
    unsigned char expected_crc = 0;

    while (1)
    {
        pthread_mutex_lock(&p->tcp_lock);
        uartfd = p->uart_fd;
        enable_crc = p->config.EnableCrc;
        pthread_mutex_unlock(&p->tcp_lock);

        res = read(uartfd, buf, sizeof(buf));
        if (res <= 0)
        {
            usleep(10000);
            continue;
        }

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
                        stats_inc(p, &p->stats.uart_rx_len_err);
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
                                LOGW("uart crc error: expect=0x%02X got=0x%02X len=%d", expected_crc, crc, exp_len);
                                stats_inc(p, &p->stats.uart_rx_crc_err);
                                drop_frame = 1;
                                break;
                            }
                        }

                        stats_inc(p, &p->stats.uart_rx_frames);
                        pthread_mutex_lock(&p->tcp_lock);
                        connected = p->Tcp_connected;
                        sockfd = p->sock_fd;
                        pthread_mutex_unlock(&p->tcp_lock);

                        if (connected == 1)
                        {
                            ret_send = send(sockfd, Packet_Buf, exp_len, 0);
                            if (ret_send <= 0)
                            {
                                LOGW("uart->tcp send failed, trigger reconnect");
                                trigger_reconnect(p);
                            }
                            else
                            {
                                stats_inc(p, &p->stats.tcp_tx_frames);
                            }
                        }
                        else
                        {
                            LOGD("tcp not connected, drop uart payload len=%d", exp_len);
                        }
                    }
                    else
                    {
                        LOGW("uart frame tail error: expect=0xEE got=0x%02X len=%d", buf[i], exp_len);
                        stats_inc(p, &p->stats.uart_rx_tail_err);
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
            if (drop_frame) break;
        }

        LED_Set(71, 1);
        usleep(100000);
        LED_Set(71, 0);
    }

    pthread_exit(NULL);
}

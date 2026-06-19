#include "gpio_lib.h"
#include "uart.h"
#include "tcp_client.h"
#include "stats.h"
#include "log.h"
#include <semaphore.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "gateway.h"
#include <bits/getopt_core.h>

sem_t Reconnect_sem;

static void print_usage(const char *prog)
{
    printf("Usage: %s [-c config] [-v log_level]\n", prog);
    printf("  -c <path>    config file path (default: gateway.conf)\n");
    printf("  -v <level>   log level: ERROR|WARN|INFO|DEBUG or 0-3\n");
}

int main(int argc, char *argv[])
{
    // 在 Linux 下，如果你对一个已经断开的 socket 再次 send，系统可能给进程发 SIGPIPE信号，默认行为是直接把进程杀掉。
    // 这里选择忽略它，意思是：
    // 即使 socket 断了，程序也不要被系统强行终止；
    // 让 send() 返回错误码；
    // 再由你自己的代码决定后续怎么处理，比如触发重连。
    signal(SIGPIPE, SIG_IGN);

    const char *conf_path = "gateway.conf";
    int cli_log_level = -1; // 默认不从命令行强制设置日志等级
    int opt;
    while ((opt = getopt(argc, argv, "c:v:h")) != -1) // h是普通选项，-c 和V是必须有参数的选项，因为它后面有一个冒号。
    {
        switch (opt)
        {
        case 'c':
            conf_path = optarg; // optarg是一个全局变量，指向当前选项参数的字符串。比如-c后面跟着一个路径，那么optarg就会指向那个路径字符串。
            break;
        case 'v':
            cli_log_level = atoi(optarg); // atoi函数将字符串转换为整数。比如-v后面跟着一个数字字符串，那么atoi(optarg)就会返回那个数字的整数值。
            break;
        case 'h': // 如果用户输入了-h选项，程序会打印使用说明并退出。
        default:
            print_usage(argv[0]);
            return 0;
        }
    }

    struct Config_Data cfg; // 定义一个结构体变量cfg，用来存储配置数据。
    ConfigData_Default(&cfg);
    if (ConfigData_Load(conf_path, &cfg) != 0)
    {
        fprintf(stderr, "config load failed, using defaults\n");
    }

    log_init((log_level_t)cfg.LogLevel, cfg.LogFile);
    if (cli_log_level >= 0)
    {
        log_set_level((log_level_t)cli_log_level);
    }
    // 当前连接哪个服务器 、当前用哪个串口 、波特率是多少 、CRC 开没开
    LOGI("gateway start with config: server=%s:%d uart=%s baud=%d crc=%d", cfg.IP, cfg.Port, cfg.UartDev, cfg.UartBaud, cfg.EnableCrc);

    GateWayContext arg;
    memset(&arg, 0, sizeof(arg));
    arg.sock_fd = -1; // 用-1表示“当前没有合法的TCP连接
    arg.heart_sendtime = time(NULL);
    arg.heart_waiting_ack = 0;
    arg.last_heart_send = time(NULL);
    memcpy(arg.send_heart, (unsigned char[]){0xAA, 0x01, 0x00, 0xEE}, 4);
    memcpy(arg.ack_heart, (unsigned char[]){0xAA, 0x01, 0x01, 0xEE}, 4);
    pthread_mutex_init(&arg.tcp_lock, NULL);   // 保护 TCP 相关共享状态
    pthread_mutex_init(&arg.stats_lock, NULL); // 保护统计计数
    arg.config = cfg;

    sem_init(&Reconnect_sem, 0, 0);

    arg.uart_fd = uart_init(cfg.UartDev, cfg.UartBaud);
    if (arg.uart_fd < 0)
    {
        LOGE("uart init failed, exit");
        return 1;
    }

    int tcp_res = Tcp_connect_server(&arg.sock_fd, &cfg);
    pthread_t Uthread, Tthread, Rethread, Hthread, Sthread;

    pthread_create(&Rethread, NULL, Reconnect_thread, (void *)&arg);

    if (tcp_res == 0)
    {
        arg.Tcp_connected = 1;
        arg.reconnecting = 0;
    }
    else
    {
        arg.Tcp_connected = 0;
        arg.reconnecting = 1;
        sem_post(&Reconnect_sem);
    }

    for (int i = 0; i < 3; i++)
    {
        LED_Set(67, 1);
        usleep(100000);
        LED_Set(67, 0);
        usleep(100000);
    }

    LED_Set(67, 0);

    pthread_create(&Uthread, NULL, Uart_thread, (void *)&arg);
    pthread_create(&Tthread, NULL, Tcp_client_thread, (void *)&arg);
    pthread_create(&Hthread, NULL, Heartbeat_thread, (void *)&arg);
    pthread_create(&Sthread, NULL, Stats_thread, (void *)&arg); // 观测统计，负责定期打印统计信息，比如收发帧数、CRC 错误数、重连次数。

    pthread_join(Uthread, NULL);
    pthread_join(Tthread, NULL);
    pthread_join(Rethread, NULL);
    pthread_join(Hthread, NULL);
    pthread_join(Sthread, NULL);

    log_close();
    return 0;
}

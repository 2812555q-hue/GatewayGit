#include "gpio_lib.h"
#include "uart.h"
#include "tcp_client.h"
#include <semaphore.h>
#include <signal.h>
#include "gateway.h"

sem_t Reconnect_sem;//定义断线重连信号量

int main(int argc, char *argv[])
{
    signal(SIGPIPE, SIG_IGN);
    GateWayContext arg={0}; //必须赋值
    arg.sock_fd=-1;
    arg.heart_sendtime = time(NULL);
    arg.heart_waiting_ack=0;
    arg.last_heart_send = time(NULL);
    memcpy(arg.send_heart,(unsigned char[]){0xAA,0x01,0x00,0xEE},4);
    memcpy(arg.ack_heart,(unsigned char[]){0xAA,0x01,0x01,0xEE},4);
    pthread_t Uthread,Tthread,Rethread,Hthread;
    int uart_res,sock_res,tcp_res,Recon_res,Heart_res;
    sem_init(&Reconnect_sem,0,0);
    arg.uart_fd=uart_init();
    tcp_res=Tcp_connect_server(&arg.sock_fd);  
    pthread_mutex_init(&arg.tcp_lock, NULL);
    Recon_res = pthread_create(&Rethread, NULL, Reconnect_thread, (void *)&arg);
   
    if(tcp_res==0)
    {
        arg.Tcp_connected =1;
        arg.reconnecting=0;
    }
    else
    {
        arg.Tcp_connected =0;
        arg.reconnecting=1;
        sem_post(&Reconnect_sem);
    }
    for(int i=0;i<3;i++)
    {
        LED_Set(67,1);
        usleep(100000);        // 闪烁 100ms (足够肉眼捕捉)
        LED_Set(67,0);
        usleep(100000);
    }

    LED_Set(67,0);

    uart_res = pthread_create(&Uthread, NULL, Uart_thread, (void *)&arg);
    sock_res = pthread_create(&Tthread, NULL, Tcp_client_thread, (void *)&arg);
    Heart_res = pthread_create(&Hthread, NULL, Heartbeat_thread, (void *)&arg);

    pthread_join(Uthread, NULL); // main 会死等 Uthread 运行结束
    pthread_join(Tthread, NULL); 
    pthread_join(Rethread, NULL); 

    return 0;
}

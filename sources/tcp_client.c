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
#include "test_config.h"
#include <errno.h>
#include "gateway.h"
//#define HOST "192.168.28.136"        // 根据你服务器的IP地址修改
//#define PORT 8888                   // 根据你服务器进程绑定的端口号修改
#define BUFFER_SIZ (4 * 1024)           // 4k的数据区域


int Tcp_connect_server(int *sock)
{
    int sockfd, ret;
    struct sockaddr_in server;
    struct Config_Data pConf = ConfigData_Get();

    // 创建套接字描述符
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("create an endpoint for communication fail!\n");
       return -1;
    } 

    bzero(&server, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(pConf.Port);
	server.sin_addr.s_addr = inet_addr(pConf.IP);

    // 建立TCP连接
    if (connect(sockfd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1) 
    {
        printf("connect server fail...\n");
        printf("errno=%d\n", errno);
        close(sockfd);
        return -1;
    } 
    else
    {
         printf("connect server success...\n");
    }
    *sock=sockfd;
   
 
    /*
    while (1) {

		printf("please enter some text: ");
		fgets(buffer, BUFFER_SIZ, stdin);

        //输入了exit，退出循环（程序）
		if(strncmp(buffer, "exit", 4) == 0)
			break;

        write(sockfd, buffer, strlen(buffer));
    }

    close(sockfd);
    exit(0);
    */
   return 0;
}  

void trigger_reconnect(GateWayContext *p)
{
    int need_post=0;
    pthread_mutex_lock(&p->tcp_lock);
    p->Tcp_connected=0;
    if(p->reconnecting == 0)
    {
        p->reconnecting=1;
        p->Tcp_connected =0;
        p->heart_waiting_ack =0;
        need_post = 1;
    }
    pthread_mutex_unlock(&p->tcp_lock);
    if(need_post)
    {
        sem_post(&Reconnect_sem);
    } 
}



void *Tcp_client_thread(void* arg)
{
 
    GateWayContext *p = (GateWayContext *)arg;
    int res,drop_frame=0;
    unsigned char buf[1024],Packet_Buf[1024];
    int uartfd,sockfd,connected;
    int state=WAIT_HEADER,count=0,exp_len=0,need_post=0;
    while (1)
    {
        pthread_mutex_lock(&p->tcp_lock);
        sockfd = p->sock_fd;
        connected = p->Tcp_connected;
        pthread_mutex_unlock(&p->tcp_lock);
        if(connected == 1)
        {
           
            res=recv(sockfd,buf,sizeof(buf),0);
            if(res<0)
            {
                printf("recv error, errno=%d\n", errno);
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                {
                    usleep(100000);
                    continue;
                }
                trigger_reconnect(p);
                exp_len=0;
                state=WAIT_HEADER;
                count=0;
                continue;//这一轮循环结束直接进入下一轮
            }
            else if(res==0)
            {
                printf("peer closed\n");
                trigger_reconnect(p);
                exp_len=0;
                state=WAIT_HEADER;
                count=0;
                continue;
            }
            else 
            {
                drop_frame=0;
                for (int i = 0; i < res; i++)
                {
                    switch (state)
                    {
                        case WAIT_HEADER:
                        {
                            if(buf[i] == 0xAA)  
                            {
                                count=0;
                                state=WAIT_LENGTH;
                            }

                            break;
                        }
                        case WAIT_LENGTH:
                        {
                            exp_len=(unsigned char)buf[i];
                  
                            if(exp_len>1024 ||exp_len==0) 
                            {
                                state = WAIT_HEADER;//防止数组越界
                            }
                            else
                            {
                                state=WAIT_PAYLOAD;
                            }
                            break;
                        }
                        case WAIT_PAYLOAD:
                        {
                            Packet_Buf[count++]=buf[i];
                            if(count == exp_len) state = WAIT_TAIL;
                            break;
                        }
                        case WAIT_TAIL:
                        {  
                            if(buf[i] == 0xEE) 
                            {    
                                if(exp_len == 1 && Packet_Buf[0] == 0x01)//工程上通常通过协议约定避免,保留 PAYLOAD = 00 / 01 作为心跳
                                {
                                    pthread_mutex_lock(&p->tcp_lock);
                                    p->heart_waiting_ack=0;
                                    p->timeout_cnt =0;
                                    p->last_heart_send = time(NULL);
                                    pthread_mutex_unlock(&p->tcp_lock);
                                    printf("heartbeat ack receive\n");
                                }
                                else
                                {
                                    pthread_mutex_lock(&p->tcp_lock);
                                    uartfd = p->uart_fd;
                                    pthread_mutex_unlock(&p->tcp_lock);
                                    write(uartfd,Packet_Buf,exp_len);
                                }    
                            }
                            else
                            {
                                printf("UART Frame Error: invalid tail, expect=0xEE, got=0X%02X, len=%d\n",buf[i],exp_len);
                                exp_len=0;
                                drop_frame=1;
                            }
                            memset(Packet_Buf, 0, 1024);//数组初始化为0
                            state = WAIT_HEADER;
                            count = 0;
                            break;
                        }
                        default:state = WAIT_HEADER;break;
                    }
                        if(drop_frame == 1) break;
                }
                    LED_Set(71,1);
                    usleep(100000);        // 闪烁 100ms (足够肉眼捕捉)
                    LED_Set(71,0);
            }  
        }  
    } 
    pthread_exit(NULL);
}

void *Reconnect_thread(void* arg)
{
    GateWayContext *p = (GateWayContext *)arg;
    
    while (1)
    {
        sem_wait(&Reconnect_sem);
        // 清掉积压的重复重连请求
        while (sem_trywait(&Reconnect_sem) == 0)
        {
            printf("drop duplicated reconnect request\n");
        }
        printf("Reconnect thread wake up\n");
        pthread_mutex_lock(&p->tcp_lock);
        p->Tcp_connected=0;
        p->reconnecting=1;
        if(p->sock_fd>=0)
        {
            shutdown(p->sock_fd, SHUT_RDWR);
            close(p->sock_fd);
            p->sock_fd=-1;
        }
        pthread_mutex_unlock(&p->tcp_lock);
        while (1)
        {
           int newfd = -1;
           if(Tcp_connect_server(&newfd) == 0)
           {
                printf("reconnect success...\n");
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
                sleep(3);
           }
        }
        
        
    }
    pthread_exit(NULL);
    
}

void *Heartbeat_thread(void* arg)
{
    GateWayContext *p =(GateWayContext *)arg;
    int ret =0,need_post=0,waiting_ack=0;
    int sockfd,reconnecting,connected;
    time_t last_heart_send,heart_sendtime;
    while ((1))
    {
        sleep(1);
        pthread_mutex_lock(&p->tcp_lock);
        sockfd = p->sock_fd;
        waiting_ack = p->heart_waiting_ack;
        connected = p->Tcp_connected;
        reconnecting = p->reconnecting;
        last_heart_send = p->last_heart_send;
        heart_sendtime = p->heart_sendtime;
        pthread_mutex_unlock(&p->tcp_lock);
        if(connected == 1)
        {
            if(waiting_ack == 0 && (time(NULL) - last_heart_send) >=HEART_TIMEOUT)
            {
                ret = send(sockfd,p->send_heart,sizeof(p->send_heart),0);
                if(ret <=0)
                {
                    printf("heart send failed!\n");
                    trigger_reconnect(p);
                    continue;
                }
                else
                {
                    printf("heartbeat req send !\n");
                    pthread_mutex_lock(&p->tcp_lock);
                    p->heart_sendtime = time(NULL);
                    p->last_heart_send = p->heart_sendtime;
                    p->heart_waiting_ack = 1;
                    pthread_mutex_unlock(&p->tcp_lock);
                }
            }
            pthread_mutex_lock(&p->tcp_lock);
            if(p->heart_waiting_ack)//表示在等待应答
            {
                if(time(NULL) - p->heart_sendtime >= HEART_TIMEOUT)
                {
                    p->timeout_cnt++;
                    p->heart_waiting_ack =0;  
                }
            }
            if(p->timeout_cnt == HEART_MISS_MAX)
            {
                p->timeout_cnt =0;
                pthread_mutex_unlock(&p->tcp_lock);
                trigger_reconnect(p);
                continue;
            }
            pthread_mutex_unlock(&p->tcp_lock);
       }
    }
    pthread_exit(NULL);
    
}
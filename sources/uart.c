#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <string.h>
#include <sys/ioctl.h>
#include "gateway.h"
//#include "gpio_lib.h"
//第一部分代码/
//根据具体的设备修改
const char default_path[] = "/dev/ttyAS5";

int uart_init(void)
{
    int fd;
    int i;
    int res;
    char *path;
    path = (char *)default_path;

    //第二部分代码/
    //若无输入参数则使用默认终端设备
    
    /*if (argc > 1)
        path = argv[1];
    else
        path = (char *)default_path;*/

    //获取串口设备描述符
    printf("This is tty/usart demo.\n");
    fd = open(path, O_RDWR);
    if (fd < 0) {
        printf("Fail to Open %s device\n", path);
      //  return 0;
    }


     //第三部分代码/
    struct termios opt;
    //清空串口接收缓冲区
    tcflush(fd, TCIOFLUSH);
    // 获取串口参数opt
    tcgetattr(fd, &opt);
    //设置串口输出波特率
    cfsetospeed(&opt, B115200);
    //设置串口输入波特率
    cfsetispeed(&opt, B115200);
    
    //设置数据位数
    opt.c_cflag &= ~CSIZE;
    opt.c_cflag |= CS8;
    //校验位
    opt.c_cflag &= ~PARENB;
   // opt.c_iflag &= ~INPCK;


   // 在配置串口的 termios 结构体时加入：
    opt.c_iflag &= ~(IXON | IXOFF | IXANY); // 禁用软件流控，防止拦截 0x11, 0x12, 0x13
    opt.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // 开启原始输入模式

    //设置停止位
    opt.c_cflag &= ~CSTOPB;
    //更新配置
    tcsetattr(fd, TCSANOW, &opt);
    printf("Device %s is set to 115200bps,8N1\n",path);
    return fd;

#if 0



/**第三部分代码 **/
    struct termios opt;
    //清空串口接收缓冲区
    tcflush(fd, TCIOFLUSH);
    // 获取串口参数opt
    tcgetattr(fd, &opt);

    /** 1. 核心修改：禁用回显和规范模式 **/
    // ECHO: 禁用输入字符回显
    // ICANON: 禁用规范模式（进入原始模式，收到字符立即返回，不等待换行符）
    // ECHOE: 擦除回显
    // ISIG: 禁用终端控制信号（如 Ctrl+C）
    opt.c_lflag &= ~(ECHO | ECHOE | ICANON | ISIG);

    /** 2. 核心修改：禁用输出处理 **/
    // OPOST: 禁用输出处理（防止系统自动将 \n 转换为 \r\n 等）
    opt.c_oflag &= ~OPOST;

    /** 3. 核心修改：禁用输入转换 **/
    // 禁用软件流控，禁用换行符/回车符转换
    opt.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

    //设置串口输出波特率
    cfsetospeed(&opt, B115200);
    //设置串口输入波特率
    cfsetispeed(&opt, B115200);
    
    //设置数据位数
    opt.c_cflag &= ~CSIZE;
    opt.c_cflag |= CS8;
    //校验位
    opt.c_cflag &= ~PARENB;
    opt.c_iflag &= ~INPCK;
    //设置停止位
    opt.c_cflag &= ~CSTOPB;

    //更新配置
    tcsetattr(fd, TCSANOW, &opt);
    printf("Device %s is set to 115200bps,8N1 (Raw Mode)\n",path);

    return fd;


    #endif
   
  
}

/*要执行的线程*/
void *Uart_thread(void* arg)
{
    GateWayContext *p = (GateWayContext *)arg;
    int res=0,ret_send=0;
    unsigned char buf[1024],Packet_Buf[1024];
    int uartfd=0,sockfd=0,connected;
    int state=WAIT_HEADER,count=0,exp_len=0,drop_frame=0,need_post=0;
   while(1)
   {
    pthread_mutex_lock(&p->tcp_lock);
    uartfd = p->uart_fd;
    pthread_mutex_unlock(&p->tcp_lock);
    res = read(uartfd, buf, sizeof(buf));
    if(res)
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
                    exp_len=( unsigned char)buf[i];
                  
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
                        pthread_mutex_lock(&p->tcp_lock);
                        connected = p->Tcp_connected;
                        sockfd = p->sock_fd;
                        pthread_mutex_unlock(&p->tcp_lock);
                        if(connected == 1)//表示Tcp连接正常可以发送
                        {
                            ret_send=send(sockfd,Packet_Buf,exp_len,0);
                            if(ret_send<=0)
                            {
                                printf("send failed ,trigger reconnect\n");
                                trigger_reconnect(p);   
                            }
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
            if(drop_frame) break;
        }

        LED_Set(71,1);
        usleep(100000);        // 闪烁 100ms (足够肉眼捕捉)
        LED_Set(71,0);
        
    }
   }
   
  //   close(fd);
    /*退出线程*/
    pthread_exit(NULL);
}
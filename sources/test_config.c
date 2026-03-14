#include <stdio.h>

#include "test_config.h"


struct Config_Data ConfigData_Get(void)
{
    
    FILE *fp;
    struct Config_Data ConfigData={0};
    char line[100]={0};
    fp = fopen("gateway.conf", "r");
    if(fp == NULL)
    {
        perror("Open gateway.conf failed");
        printf("Doucment Open Err !\n");
    }
   // 只要还没读到文件末尾，就一直读
   else
   {
         while (fgets(line, sizeof(line), fp) != NULL) 
        {
            sscanf(line,"SERVER_IP=%s",ConfigData.IP);
            sscanf(line,"SERVER_PORT=%d",&ConfigData.Port);//注意取地址
        } 
        return ConfigData;
        fclose(fp);
   }
   
}
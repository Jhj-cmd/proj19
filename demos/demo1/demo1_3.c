#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>

#define BUF_LEN 1024

int main(void)
{
    DIR *dir = opendir("/proc");
    if(dir == NULL)
    {
        perror("opendir /proc failed");
        return 1;
    }

    struct dirent *entry;
    char path[BUF_LEN];
    char buf[BUF_LEN];

    while((entry = readdir(dir)) != NULL)
    {
        // 过滤，只保留名字以数字开头的目录项(PID)
        if(entry->d_name[0] < '0' || entry->d_name[0] > '9')
        {
            continue;
        }

        // 拼接 /proc/xxx/status 完整路径，存入临时数组path
        snprintf(path, sizeof(path), "/proc/%s/status", entry->d_name);

        FILE *fp = fopen(path, "r");
        if(fp == NULL)
        {
            continue;
        }

        // 全部是临时局部变量，每一轮循环会被覆盖
        char name[BUF_LEN] = {0};
        int threads = 0;
        long vmsize = 0;
        long vmrss = 0;

        // 复用demo2逻辑读取status文件
        while(fgets(buf, BUF_LEN, fp) != NULL)
        {
            if(strstr(buf, "Name:") == buf)
            {
                sscanf(buf, "Name:\t%s", name);
            }
            else if(strstr(buf, "Threads:") == buf)
            {
                sscanf(buf, "Threads:\t%d", &threads);
            }
            else if(strstr(buf, "VmSize:") == buf)
            {
                sscanf(buf, "VmSize:\t%ld", &vmsize);
            }
            else if(strstr(buf, "VmRSS:") == buf)
            {
                sscanf(buf, "VmRSS:\t%ld", &vmrss);
            }
        }
        fclose(fp);

        // 直接打印，数据不保存，下一轮直接覆盖
        printf("PID:%-6s 进程名:%-16s 线程:%-4d VmSize:%8ld kB VmRSS:%8ld kB\n",
               entry->d_name, name, threads, vmsize, vmrss);
    }

    closedir(dir);
    return 0;
}

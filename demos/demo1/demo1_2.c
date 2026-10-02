#include <stdio.h>
#include <string.h>

#define BUF_LEN 1024

int main(int argc, char *argv[])
{
    if(argc != 2)
    {
        printf("用法: %s <pid>\n", argv[0]);
        return 1;
    }

    char path[BUF_LEN];
    // 拼接 /proc/xxx/status 路径
    snprintf(path, sizeof(path), "/proc/%s/status", argv[1]);

    FILE *fp = fopen(path, "r");
    if(fp == NULL)
    {
        perror("fopen failed");
        printf("PID %s 可能不存在\n", argv[1]);
        return 1;
    }

    char buf[BUF_LEN];
    char name[BUF_LEN] = {0};
    int threads = 0;
    long vmsize = 0;
    long vmrss = 0;

    // 循环一行一行读文件
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

    // 打印提取出来的信息
    printf("进程名: %s\n", name);
    printf("线程数: %d\n", threads);
    printf("虚拟内存 VmSize: %ld kB\n", vmsize);
    printf("物理内存 VmRSS: %ld kB\n", vmrss);

    return 0;
}

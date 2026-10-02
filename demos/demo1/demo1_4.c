#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>

#define BUF_LEN 1024

// ==========1.自定义链表节点结构体【Demo3没有，Demo4新增核心】==========
struct proc_info
{
    char pid[32];
    char name[128];
    int threads;
    long vmsize;
    long vmrss;
    struct proc_info *next; // 指向下一个节点，构建链表
};

// 创建单个节点：把一组进程数据打包，堆上开辟内存
struct proc_info *create_node(const char *pid, const char *name, int threads, long vmsize, long vmrss)
{
    struct proc_info *new_node = (struct proc_info *)malloc(sizeof(struct proc_info));
    if(new_node == NULL)
    {
        perror("malloc failed");
        return NULL;
    }
    strcpy(new_node->pid, pid);
    strcpy(new_node->name, name);
    new_node->threads = threads;
    new_node->vmsize = vmsize;
    new_node->vmrss = vmrss;
    new_node->next = NULL;
    return new_node;
}

// 尾插法：新节点挂在链表最后
void add_tail(struct proc_info **head, struct proc_info *new_node)
{
    // 链表为空，新节点直接当头结点
    if(*head == NULL)
    {
        *head = new_node;
        return;
    }
    // 找到链表末尾
    struct proc_info *p = *head;
    while(p->next != NULL)
    {
        p = p->next;
    }
    p->next = new_node;
}

// 遍历链表，打印所有存储的进程信息
void print_list(struct proc_info *head)
{
    struct proc_info *p = head;
    printf("====================进程列表====================\n");
    while(p != NULL)
    {
        printf("PID:%-6s 进程名:%-16s 线程:%-4d VmSize:%8ld kB VmRSS:%8ld kB\n",
               p->pid, p->name, p->threads, p->vmsize, p->vmrss);
        p = p->next;
    }
}

// 释放链表所有堆内存，避免内存泄漏
void free_list(struct proc_info *head)
{
    struct proc_info *p = head;
    while(p != NULL)
    {
        struct proc_info *temp = p;
        p = p->next;
        free(temp);
    }
}

int main(void)
{
    struct proc_info *head = NULL; // 链表头指针，初始链表为空

    // ==========复用Demo1：打开/proc目录遍历==========
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
        // 过滤目录项，只保留PID（数字名称）
        if(entry->d_name[0] < '0' || entry->d_name[0] > '9')
        {
            continue;
        }

        // ==========复用Demo2：拼接路径、读取status==========
        snprintf(path, sizeof(path), "/proc/%s/status", entry->d_name);
        FILE *fp = fopen(path, "r");
        if(fp == NULL)
        {
            continue;
        }

        char name[BUF_LEN] = {0};
        int threads = 0;
        long vmsize = 0;
        long vmrss = 0;

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

        // =========【Demo3和Demo4最核心区别】=========
        // Demo3：直接printf打印
        // Demo4：创建节点，存入链表，暂时不打印
        struct proc_info *node = create_node(entry->d_name, name, threads, vmsize, vmrss);
        if(node != NULL)
        {
            add_tail(&head, node);
        }
    }
    closedir(dir);

    // 全部进程遍历完成！此时链表已经存好所有进程数据
    print_list(head);

    // 程序结束，释放堆内存
    free_list(head);
    return 0;
}

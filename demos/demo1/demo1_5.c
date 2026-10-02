#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>

#define BUF_LEN 1024

// 进程信息链表节点
struct proc_info
{
    char pid[32];
    char name[128];
    int threads;
    long vmsize;
    long vmrss;
    struct proc_info *next;
};

// 创建节点
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

// 尾插
void add_tail(struct proc_info **head, struct proc_info *new_node)
{
    if(*head == NULL)
    {
        *head = new_node;
        return;
    }
    struct proc_info *p = *head;
    while(p->next != NULL)
    {
        p = p->next;
    }
    p->next = new_node;
}

// ========= DEMO5新增核心：链表冒泡排序 =========
// 排序规则：按照物理内存vmrss 降序，占用内存大的放前面
void sort_list(struct proc_info *head)
{
    if(head == NULL || head->next == NULL)
        return;

    struct proc_info *p;
    struct proc_info *last = NULL;
    int swap_flag;

    do
    {
        swap_flag = 0;
        p = head;
        while(p->next != last)
        {
            // 前一个进程物理内存 < 后一个，交换节点内部数据
            if(p->vmrss < p->next->vmrss)
            {
                // 临时变量交换内容（简易方案，不交换指针）
                char tmp_pid[32];
                char tmp_name[128];
                int tmp_threads;
                long tmp_vmsize, tmp_vmrss;

                strcpy(tmp_pid, p->pid);
                strcpy(tmp_name, p->name);
                tmp_threads = p->threads;
                tmp_vmsize = p->vmsize;
                tmp_vmrss = p->vmrss;

                strcpy(p->pid, p->next->pid);
                strcpy(p->name, p->next->name);
                p->threads = p->next->threads;
                p->vmsize = p->next->vmsize;
                p->vmrss = p->next->vmrss;

                strcpy(p->next->pid, tmp_pid);
                strcpy(p->next->name, tmp_name);
                p->next->threads = tmp_threads;
                p->next->vmsize = tmp_vmsize;
                p->next->vmrss = tmp_vmrss;

                swap_flag = 1;
            }
            p = p->next;
        }
        last = p;
    } while(swap_flag);
}

// 遍历打印链表
void print_list(struct proc_info *head)
{
    struct proc_info *p = head;
    printf("====================进程列表(按物理内存降序)====================\n");
    while(p != NULL)
    {
        printf("PID:%-6s 进程名:%-16s 线程:%-4d VmSize:%8ld kB VmRSS:%8ld kB\n",
               p->pid, p->name, p->threads, p->vmsize, p->vmrss);
        p = p->next;
    }
}

// 释放链表
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
    struct proc_info *head = NULL;

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
        if(entry->d_name[0] < '0' || entry->d_name[0] > '9')
        {
            continue;
        }

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

        struct proc_info *node = create_node(entry->d_name, name, threads, vmsize, vmrss);
        if(node != NULL)
        {
            add_tail(&head, node);
        }
    }
    closedir(dir);

    // 【Demo5新增关键步骤】链表排序
    sort_list(head);

    // 排序完成后打印
    print_list(head);

    free_list(head);
    return 0;
}

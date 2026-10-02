#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

typedef struct Node {
    int pid;
    char name[64];
    int threads;
    long vmsize;
    long vmrss;
    struct Node *next;
} Node;

Node* create_node(int pid, char *name, int threads, long vmsize, long vmrss)
{
    Node *new_node = (Node*)malloc(sizeof(Node));
    new_node->pid = pid;
    strcpy(new_node->name, name);
    new_node->threads = threads;
    new_node->vmsize = vmsize;
    new_node->vmrss = vmrss;
    new_node->next = NULL;
    return new_node;
}

int read_proc(int pid, Node **head)
{
    char path[128];
    FILE *fp;
    char buf[256];
    char name[64] = {0};
    int threads = 0;
    long vmsize = 0, vmrss = 0;

    sprintf(path, "/proc/%d/status", pid);
    fp = fopen(path, "r");
    if (!fp) return -1;

    while (fgets(buf, sizeof(buf), fp))
    {
        if (strstr(buf, "Name:"))
        {
            sscanf(buf, "Name:\t%s", name);
        }
        else if (strstr(buf, "Threads:"))
        {
            sscanf(buf, "Threads:\t%d", &threads);
        }
        else if (strstr(buf, "VmSize:"))
        {
            sscanf(buf, "VmSize:\t%ld kB", &vmsize);
        }
        else if (strstr(buf, "VmRSS:"))
        {
            sscanf(buf, "VmRSS:\t%ld kB", &vmrss);
        }
    }
    fclose(fp);

    Node *new_node = create_node(pid, name, threads, vmsize, vmrss);
    if (*head == NULL)
    {
        *head = new_node;
    }
    else
    {
        Node *p = *head;
        while (p->next != NULL)
            p = p->next;
        p->next = new_node;
    }
    return 0;
}

void scan_proc(Node **head)
{
    DIR *dir = opendir("/proc");
    struct dirent *ent;
    if (!dir)
    {
        perror("opendir");
        return;
    }
    while ((ent = readdir(dir)) != NULL)
    {
        if (!isdigit(ent->d_name[0]))
            continue;
        int pid = atoi(ent->d_name);
        read_proc(pid, head);
    }
    closedir(dir);
}

Node* get_mid(Node *head)
{
    Node *slow = head, *fast = head->next;
    while (fast && fast->next)
    {
        slow = slow->next;
        fast = fast->next->next;
    }
    return slow;
}

Node* merge(Node *left, Node *right)
{
    Node dummy;
    Node *p = &dummy;
    dummy.next = NULL;
    while (left && right)
    {
        if (left->vmrss >= right->vmrss)
        {
            p->next = left;
            left = left->next;
        }
        else
        {
            p->next = right;
            right = right->next;
        }
        p = p->next;
    }
    p->next = left ? left : right;
    return dummy.next;
}

Node* merge_sort(Node *head)
{
    if (!head || !head->next)
        return head;
    Node *mid = get_mid(head);
    Node *right_head = mid->next;
    mid->next = NULL;
    Node *left = merge_sort(head);
    Node *right = merge_sort(right_head);
    return merge(left, right);
}

void print_list(Node *head)
{
    printf("====================================================================================\n");
    printf("%-8s %-22s %-8s %-14s %-14s\n",
           "PID", "Name", "Threads", "VmSize(kB)", "VmRSS(kB)");
    printf("====================================================================================\n");
    Node *p = head;
    while (p)
    {
        printf("%-8d %-22s %-8d %-14ld %-14ld\n",
               p->pid, p->name, p->threads, p->vmsize, p->vmrss);
        p = p->next;
    }
}

void free_list(Node *head)
{
    Node *p = head;
    while (p != NULL)
    {
        Node *tmp = p;
        p = p->next;
        free(tmp);
    }
}

// 封装原来demo5一整套快照逻辑
void snapshot(void)
{
    Node *head = NULL;
    scan_proc(&head);
    head = merge_sort(head);
    print_list(head);
    free_list(head);
}

int main()
{
    while(1)
    {
        snapshot();
        sleep(2);
        system("clear");
    }
    return 0;
}

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <sys/ptrace.h>
#include <linux/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/syscall.h>

typedef struct Node {
    int pid;
    char name[64];
    int threads;
    long vmsize;
    long vmrss;
    struct Node *next;
} Node;

/* 全局变量 */
volatile sig_atomic_t g_stop = 0;
Node *g_last_list = NULL;

/* 函数前置声明 */
void safe_exit(int sig);
Node* create_node(int pid, char *name, int threads, long vmsize, long vmrss);
int read_proc(int pid, Node **head);
void scan_proc(Node **head);
Node* get_mid(Node *head);
Node* merge(Node *left, Node *right);
Node* merge_sort(Node *head);
void print_list(Node *head);
void free_list(Node *head);
void write_full_log(Node *cur_list, time_t now);
int pid_exist(Node *list, int target_pid);
void write_new_proc_log(Node *cur_list, Node *last_list, time_t now, int round);
void write_exit_proc_log(Node *cur_list, Node *last_list, time_t now, int round);
Node* copy_list(Node *src);
void snapshot(Node **last_list_ptr, int round);
void clear_log_files(void);
static void read_str(pid_t pid, unsigned long addr, char *buf, int buf_len);
int ptrace_trace_pid(pid_t target_pid);

/* 信号处理：只设置标志位 */
void safe_exit(int sig)
{
    (void)sig;
    g_stop = 1;
}

Node* create_node(int pid, char *name, int threads, long vmsize, long vmrss)
{
    Node *new_node = (Node*)malloc(sizeof(Node));
    if (new_node == NULL) {
        perror("malloc node failed");
        return NULL;
    }
    new_node->pid = pid;
    strncpy(new_node->name, name, sizeof(new_node->name) - 1);
    new_node->name[sizeof(new_node->name) - 1] = '\0';
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

    while (fgets(buf, sizeof(buf), fp)) {
        if (strstr(buf, "Name:")) {
            sscanf(buf, "Name:\t%63s", name);
        } else if (strstr(buf, "Threads:")) {
            sscanf(buf, "Threads:\t%d", &threads);
        } else if (strstr(buf, "VmSize:")) {
            sscanf(buf, "VmSize:\t%ld kB", &vmsize);
        } else if (strstr(buf, "VmRSS:")) {
            sscanf(buf, "VmRSS:\t%ld kB", &vmrss);
        }
    }
    fclose(fp);

    Node *new_node = create_node(pid, name, threads, vmsize, vmrss);
    if (new_node == NULL) return -2;

    if (*head == NULL) {
        *head = new_node;
    } else {
        Node *p = *head;
        while (p->next != NULL) p = p->next;
        p->next = new_node;
    }
    return 0;
}

void scan_proc(Node **head)
{
    DIR *dir = opendir("/proc");
    struct dirent *ent;
    if (!dir) {
        perror("opendir");
        return;
    }
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;
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
    while (fast && fast->next) {
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
    while (left && right) {
        if (left->vmrss >= right->vmrss) {
            p->next = left;
            left = left->next;
        } else {
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
    while (p) {
        printf("%-8d %-22s %-8d %-14ld %-14ld\n",
               p->pid, p->name, p->threads, p->vmsize, p->vmrss);
        p = p->next;
    }
}

void free_list(Node *head)
{
    Node *p = head;
    while (p != NULL) {
        Node *tmp = p;
        p = p->next;
        free(tmp);
    }
}

void write_full_log(Node *cur_list, time_t now)
{
    FILE *fp = fopen("proc_log.txt", "a");
    if (fp == NULL) return;
    struct tm *t = localtime(&now);
    fprintf(fp, "===================== 快照记录 %04d-%02d-%02d %02d:%02d:%02d =====================\n",
            t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
            t->tm_hour, t->tm_min, t->tm_sec);
    fprintf(fp, "%-8s %-22s %-8s %-14s %-14s\n",
            "PID", "Name", "Threads", "VmSize(kB)", "VmRSS(kB)");
    Node *p = cur_list;
    while (p) {
        fprintf(fp, "%-8d %-22s %-8d %-14ld %-14ld\n",
                p->pid, p->name, p->threads, p->vmsize, p->vmrss);
        p = p->next;
    }
    fprintf(fp, "====================================================================================\n\n");
    fclose(fp);
}

int pid_exist(Node *list, int target_pid)
{
    Node *p = list;
    while (p) {
        if (p->pid == target_pid)
            return 1;
        p = p->next;
    }
    return 0;
}

void write_new_proc_log(Node *cur_list, Node *last_list, time_t now, int round)
{
    FILE *fp = fopen("new_proc_log.txt", "a");
    if (fp == NULL) return;
    struct tm *t = localtime(&now);
    Node *p = cur_list;
    int rank = 1;
    while (p) {
        if (!pid_exist(last_list, p->pid)) {
            fprintf(fp, "============================================================\n");
            fprintf(fp, "【新增进程】第%d轮快照 | 时间：%04d-%02d-%02d %02d:%02d:%02d\n",
                    round, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                    t->tm_hour, t->tm_min, t->tm_sec);
            fprintf(fp, "本轮排名：%d\nPID：%d\n进程名：%s\n线程数：%d\nVmSize：%ld kB\nVmRSS：%ld kB\n",
                    rank, p->pid, p->name, p->threads, p->vmsize, p->vmrss);
            fprintf(fp, "============================================================\n\n");
        }
        rank++;
        p = p->next;
    }
    fclose(fp);
}

void write_exit_proc_log(Node *cur_list, Node *last_list, time_t now, int round)
{
    FILE *fp = fopen("exit_proc_log.txt", "a");
    if (fp == NULL) return;
    struct tm *t = localtime(&now);
    Node *p = last_list;
    while (p) {
        if (!pid_exist(cur_list, p->pid)) {
            fprintf(fp, "============================================================\n");
            fprintf(fp, "【进程退出】第%d轮快照 | 时间：%04d-%02d-%02d %02d:%02d:%02d\n",
                    round, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                    t->tm_hour, t->tm_min, t->tm_sec);
            fprintf(fp, "退出PID：%d\n进程名：%s\n退出前线程数：%d\n退出前VmSize：%ld kB\n退出前VmRSS：%ld kB\n",
                    p->pid, p->name, p->threads, p->vmsize, p->vmrss);
            fprintf(fp, "============================================================\n\n");
        }
        p = p->next;
    }
    fclose(fp);
}

Node* copy_list(Node *src)
{
    Node *dst = NULL;
    Node *tail = NULL;
    Node *p = src;
    while (p) {
        Node *n = create_node(p->pid, p->name, p->threads, p->vmsize, p->vmrss);
        if (n == NULL) {
            free_list(dst);
            return NULL;
        }
        if (dst == NULL) {
            dst = n;
            tail = n;
        } else {
            tail->next = n;
            tail = n;
        }
        p = p->next;
    }
    return dst;
}

void snapshot(Node **last_list_ptr, int round)
{
    Node *cur_head = NULL;
    time_t now = time(NULL);

    scan_proc(&cur_head);
    cur_head = merge_sort(cur_head);

    system("clear");
    printf("====== 进程内存监控 周期2秒 | 第%d轮 ======\n", round);
    print_list(cur_head);
    printf("\n日志说明：\n");
    printf("1. proc_log.txt 全量进程快照\n");
    printf("2. new_proc_log.txt 本轮新启动进程\n");
    printf("3. exit_proc_log.txt 本轮退出消亡进程\n");

    write_full_log(cur_head, now);

    Node *old_last = *last_list_ptr;
    Node *new_last = copy_list(cur_head);

    if (new_last != NULL) {
        write_new_proc_log(cur_head, old_last, now, round);
        write_exit_proc_log(cur_head, old_last, now, round);
        free_list(old_last);
        *last_list_ptr = new_last;
    } else {
        fprintf(stderr, "警告：本轮链表拷贝失败，暂不生成新增/退出日志\n");
    }

    free_list(cur_head);
}

void clear_log_files(void)
{
    FILE *fp;
    fp = fopen("proc_log.txt", "w");
    if (fp) fclose(fp);
    fp = fopen("new_proc_log.txt", "w");
    if (fp) fclose(fp);
    fp = fopen("exit_proc_log.txt", "w");
    if (fp) fclose(fp);
    fp = fopen("trace_syscall.log", "w");
    if (fp) fclose(fp);
}

/* 读取目标进程字符串，健壮性增强 */
static void read_str(pid_t pid, unsigned long addr, char *buf, int buf_len)
{
    int i = 0;
    unsigned long data;
    buf[0] = '\0';
    if (addr == 0) return;

    while (i < buf_len - 1) {
        errno = 0;
        data = ptrace(PTRACE_PEEKTEXT, pid, addr + i, NULL);
        if (data == (unsigned long)-1 && errno != 0) {
            strncpy(buf, "[read_err]", buf_len - 1);
            buf[buf_len - 1] = '\0';
            return;
        }
        buf[i] = data & 0xff;
        if (buf[i] == '\0') break;
        i++;
    }
    buf[i] = '\0';
}

/* ptrace 跟踪单个进程，使用 PTRACE_GET_SYSCALL_INFO 精确区分入口/出口 */
int ptrace_trace_pid(pid_t target_pid)
{
    int status;
    struct ptrace_syscall_info info;
    char log_buf[512];

    if (ptrace(PTRACE_ATTACH, target_pid, NULL, NULL) == -1) {
        perror("ptrace attach");
        return -1;
    }
    if (waitpid(target_pid, &status, 0) == -1) {
        perror("waitpid after attach");
        ptrace(PTRACE_DETACH, target_pid, NULL, NULL);
        return -1;
    }
    ptrace(PTRACE_SETOPTIONS, target_pid, 0, PTRACE_O_TRACESYSGOOD);

    FILE *fp = fopen("trace_syscall.log", "a");
    if (!fp) {
        perror("open trace log");
        ptrace(PTRACE_DETACH, target_pid, NULL, NULL);
        return -1;
    }

    printf("开始跟踪 PID %d，日志保存到 trace_syscall.log，按 Ctrl+C 停止跟踪\n", target_pid);

    while (!g_stop) {
        if (ptrace(PTRACE_SYSCALL, target_pid, NULL, NULL) == -1) {
            if (errno == ESRCH) {
                printf("目标进程已退出\n");
                break;
            }
            perror("ptrace syscall");
            break;
        }
        if (waitpid(target_pid, &status, 0) == -1) {
            if (errno == EINTR) {
                if (g_stop) break;
                continue;
            }
            perror("waitpid");
            break;
        }

        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            printf("目标进程 %d 已退出\n", target_pid);
            break;
        }

        /* 确认是系统调用停止事件 */
        if (!(WSTOPSIG(status) & 0x80))
            continue;

        /* 使用 PTRACE_GET_SYSCALL_INFO 获取详细信息 */
        if (ptrace(PTRACE_GET_SYSCALL_INFO, target_pid, sizeof(info), &info) == -1) {
            perror("PTRACE_GET_SYSCALL_INFO");
            continue;
        }

        /* 仅处理系统调用入口，避免重复打印 */
        if (info.op != PTRACE_SYSCALL_INFO_ENTRY)
            continue;

        memset(log_buf, 0, sizeof(log_buf));
        long sysno = info.entry.nr;
        switch (sysno) {
            case SYS_mmap: {
                unsigned long addr = info.entry.args[0];
                unsigned long len = info.entry.args[1];
                unsigned long prot = info.entry.args[2];
                snprintf(log_buf, sizeof(log_buf),
                         "[MMAP] addr=0x%lx length=%lu bytes prot=0x%lx\n",
                         addr, len, prot);
                break;
            }
            case SYS_munmap: {
                unsigned long addr = info.entry.args[0];
                unsigned long len = info.entry.args[1];
                snprintf(log_buf, sizeof(log_buf),
                         "[MUNMAP] addr=0x%lx length=%lu bytes\n", addr, len);
                break;
            }
            case SYS_brk: {
                unsigned long brk_arg = info.entry.args[0];
                if (brk_arg == 0) {
                    snprintf(log_buf, sizeof(log_buf), "[BRK] query current heap top\n");
                } else {
                    snprintf(log_buf, sizeof(log_buf), "[BRK] set heap top to 0x%lx\n", brk_arg);
                }
                break;
            }
            case SYS_openat: {
                char fname[256] = {0};
                unsigned long path_addr = info.entry.args[1];  // openat 的第二个参数是路径
                read_str(target_pid, path_addr, fname, sizeof(fname));
                snprintf(log_buf, sizeof(log_buf), "[OPENAT] file=\"%s\"\n", fname);
                break;
            }
            case SYS_close: {
                unsigned long fd = info.entry.args[0];
                snprintf(log_buf, sizeof(log_buf), "[CLOSE] fd=%lu\n", fd);
                break;
            }
            default:
                break;
        }

        if (log_buf[0] != '\0') {
            printf("%s", log_buf);
            fputs(log_buf, fp);
            fflush(fp);
        }
    }

    fclose(fp);
    ptrace(PTRACE_DETACH, target_pid, NULL, NULL);
    printf("已解除进程附着\n");
    return 0;
}

int main(int argc, char *argv[])
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = safe_exit;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;          // 不设置 SA_RESTART，保证 waitpid 可被中断
    sigaction(SIGINT, &sa, NULL);

    clear_log_files();
    g_last_list = NULL;

    if (argc == 2) {
        pid_t pid = atoi(argv[1]);
        if (pid <= 0) {
            fprintf(stderr, "用法：sudo ./monitor PID\n");
            return 1;
        }
        printf("开始跟踪 PID %d，请确保使用 sudo 运行\n", pid);
        ptrace_trace_pid(pid);
    } else {
        int round = 1;
        while (!g_stop) {
            snapshot(&g_last_list, round);
            round++;
            sleep(2);
        }
        printf("\n收到退出信号，正在释放资源...\n");
        free_list(g_last_list);
        g_last_list = NULL;
        printf("资源释放完成，程序退出\n");
    }

    return 0;
}

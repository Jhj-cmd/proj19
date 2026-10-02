#include <stdio.h>
#include <dirent.h>
#include <ctype.h>

// 判断字符串是否全部为数字
int is_all_digit(const char *s)
{
    if(s == NULL || *s == '\0')
        return 0;
    while(*s != '\0')
    {
        if(!isdigit(*s))
        {
            return 0;
        }
        s++;
    }
    return 1;
}

int main(void)
{
    DIR *dir = opendir("/proc");
    if(dir == NULL)
    {
        perror("opendir /proc failed");
        return 1;
    }

    struct dirent *entry;
    while((entry = readdir(dir)) != NULL)
    {
        // 跳过 . 和 ..
        if(entry->d_name[0] == '.')
            continue;

        // 判断名字全数字 → PID
        if(is_all_digit(entry->d_name))
        {
            printf("pid = %s\n", entry->d_name);
        }
    }

    closedir(dir);
    return 0;
}

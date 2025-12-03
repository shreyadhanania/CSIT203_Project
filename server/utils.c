#include <string.h>
#include <time.h>
#include <stdio.h>
#include "utils.h"

void trim_newline(char *str) {
    if (!str) return;
    size_t len = strlen(str);
    while (len > 0 && (str[len-1] == '\n' || str[len-1] == '\r')) {
        str[len-1] = '\0';
        len--;
    }
}

// Format: YYYY-MM-DD HH:MM:SS
void current_timestamp(char *buf, size_t size) {
    if (!buf || size == 0) return;
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", &tm);
}

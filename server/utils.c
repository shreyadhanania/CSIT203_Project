#include <string.h>
#include <ctype.h>
#include "utils.h"

void trim_newline(char *str) {
    int len = strlen(str);
    if (str[len-1] == '\n') str[len-1] = '\0';
}

void parse_command(char *input, char *cmd, char *arg1, char *arg2, char *msg) {
    // cmd
    char *token = strtok(input, " ");
    if (token) strcpy(cmd, token);

    // arg1
    token = strtok(NULL, " ");
    if (token) strcpy(arg1, token);

    // arg2 or message
    token = strtok(NULL, "\n");
    if (token) strcpy(msg, token);
}

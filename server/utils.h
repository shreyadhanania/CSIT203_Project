#ifndef UTILS_H
#define UTILS_H

void trim_newline(char *str);

// Fill buf (size) with current timestamp in "YYYY-MM-DD HH:MM:SS" format
void current_timestamp(char *buf, size_t size);

#endif

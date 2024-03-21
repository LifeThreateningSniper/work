#pragma once

void close_port(HANDLE hPort);
int get_port(void);
HANDLE open_port(int num);
int read_port(HANDLE hPort, void *buffer, int len);
int read_port_line(HANDLE hPort, char *str, int maxlen, int timeout);
int read_port_line_cancellable(HANDLE hPort, char *str, int maxlen, int timeout, int *running);
int read_port_str(HANDLE hPort, char *str, int maxlen);
void wait_port(void);
int write_port(HANDLE hPort, const void *data, int len);
int write_port_str(HANDLE hPort, const char *str);

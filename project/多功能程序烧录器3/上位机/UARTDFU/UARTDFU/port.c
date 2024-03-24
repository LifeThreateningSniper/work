#include <stdio.h>
#include <tchar.h>
#include <Windows.h>
#include <CommCtrl.h>
#include <windowsx.h>
#include "port.h"

extern HWND hwndCheck[2], hwndCombo, hwndSpin;
extern TCHAR szMessage[500];
static DWORD lastopen;

/* 关闭串口 */
void close_port(HANDLE hPort)
{
	if (hPort != INVALID_HANDLE_VALUE)
	{
		wait_port();
		CloseHandle(hPort);
		lastopen = GetTickCount();
	}
}

/* 获取选择的串口号 */
int get_port(void)
{
	TCHAR name[30];

	ComboBox_GetText(hwndCombo, name, _countof(name));
	return _ttoi(name + 3);
}

/* 打开串口 */
HANDLE open_port(int num)
{
	int err;
	DCB dcb;
	HANDLE hPort;
	TCHAR name[30];

	// 打开串口
	if (num == -1)
		num = get_port();
	_sntprintf_s(name, _countof(name), _countof(name), TEXT("\\\\.\\COM%d"), num);
	hPort = CreateFile(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (hPort == INVALID_HANDLE_VALUE)
	{
		err = GetLastError();
		switch (err)
		{
		case ERROR_FILE_NOT_FOUND:
		case ERROR_INVALID_NAME:
			lstrcpy(szMessage, TEXT("找不到指定的串口！"));
			break;
		case ERROR_ACCESS_DENIED:
			lstrcpy(szMessage, TEXT("串口可能已被其他程序占用！"));
			break;
		default:
			_stprintf_s(szMessage, _countof(szMessage), TEXT("打开串口失败！错误代码: %d"), GetLastError());
		}
		return INVALID_HANDLE_VALUE;
	}

	// 设置波特率
	dcb.DCBlength = sizeof(DCB);
	GetCommState(hPort, &dcb);
	dcb.BaudRate = CBR_115200;
	dcb.ByteSize = 8;
	dcb.Parity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;
	dcb.fRtsControl = RTS_CONTROL_DISABLE;  // 关闭RTS
    dcb.fDtrControl = DTR_CONTROL_DISABLE;  // 关闭DTR
	SetCommState(hPort, &dcb);

	lastopen = GetTickCount();
	return hPort;
}

/* 从串口接收n个字节的数据 */
int read_port(HANDLE hPort, void *buffer, int len)
{
	int total;
	DWORD curr;

	for (total = 0; total != len; total += curr)
	{
		ReadFile(hPort, (char *)buffer + total, len - total, &curr, NULL);
		if (curr == 0)
			break; // 超时
	}
	return total;
}

int read_port_line(HANDLE hPort, char *str, int maxlen, int timeout)
{
	return read_port_line_cancellable(hPort, str, maxlen, timeout, NULL);
}

int read_port_line_cancellable(HANDLE hPort, char *str, int maxlen, int timeout, int *running)
{
	int i = 0;
	DWORD curr, start;

	start = GetTickCount();
	while (i != maxlen - 1)
	{
		if (running != NULL && !(*running))
			break; // 强制退出

		ReadFile(hPort, str + i, 1, &curr, NULL);
		if (curr != 1)
		{
			if (timeout != -1 && (int)(GetTickCount() - start) >= timeout)
				break; // 超时退出
		}
		else if (str[i] == '\n')
			break;
		else if (str[i] != '\r')
		{
			start = GetTickCount();
			i++;
		}
	}
	str[i] = '\0';
	return i;
}

int read_port_str(HANDLE hPort, char *str, int maxlen)
{
	int len;

	len = read_port(hPort, str, maxlen - 1);
	str[len] = '\0';
	return len;
}

/* 保证达到了连接最小保持时间 */
void wait_port(void)
{
	DWORD diff, interval, now, wait;

	if (Button_GetCheck(hwndCheck[1]))
	{
		now = GetTickCount();
		diff = now - lastopen;
		interval = (DWORD)SendMessage(hwndSpin, UDM_GETPOS, 0, 0);
		if (diff < interval)
		{
			wait = interval - diff;
			Sleep(wait);
		}
	}
}

/* 往串口发送n字节数据 */
int write_port(HANDLE hPort, const void *data, int len)
{
	DWORD curr = 0;

	WriteFile(hPort, data, len, &curr, NULL);
	return curr;
}

int write_port_str(HANDLE hPort, const char *str)
{
	return write_port(hPort, str, (int)strlen(str));
}

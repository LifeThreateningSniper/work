#include <Windows.h>
#include "encoding.h"

/* 获取文本框输入的内容, 并转换成UTF8编码 */
char *gettext_utf8(HWND hWnd)
{
	char *s = NULL;
	int n;
	wchar_t *ws;

	n = GetWindowTextLengthW(hWnd) + 1;
	ws = malloc(n * sizeof(wchar_t));
	if (ws != NULL)
	{
		GetWindowTextW(hWnd, ws, n);
		s = utf16to8(ws);
		free(ws);
	}
	return s;
}

/* UTF8转UTF16 */
wchar_t *utf8to16(char *s)
{
	wchar_t *ws;
	int n;

	n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
	ws = malloc(n * sizeof(wchar_t));
	if (ws != NULL)
		MultiByteToWideChar(CP_UTF8, 0, s, -1, ws, n);
	return ws;
}

int utf8to16_wbuf(char *s, wchar_t *wbuf, int bufsize)
{
	return MultiByteToWideChar(CP_UTF8, 0, s, -1, wbuf, bufsize / sizeof(wchar_t));
}

/* UTF16转UTF8 */
char *utf16to8(wchar_t *ws)
{
	char *s;
	int n;

	n = WideCharToMultiByte(CP_UTF8, 0, ws, -1, NULL, 0, NULL, NULL);
	s = malloc(n);
	if (s != NULL)
		WideCharToMultiByte(CP_UTF8, 0, ws, -1, s, n, NULL, NULL);
	return s;
}
#pragma once

char *gettext_utf8(HWND hWnd);
wchar_t *utf8to16(char *s);
int utf8to16_wbuf(char *s, wchar_t *wbuf, int bufsize);
char *utf16to8(wchar_t *ws);

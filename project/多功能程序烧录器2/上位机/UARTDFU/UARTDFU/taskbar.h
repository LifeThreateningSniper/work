#ifndef __TASKBAR_H
#define __TASKBAR_H

int Taskbar_ClearNormalProgress(HWND hWnd);
int Taskbar_SetProgress(HWND hWnd, TBPFLAG state, ULONGLONG value, ULONGLONG total);

#endif

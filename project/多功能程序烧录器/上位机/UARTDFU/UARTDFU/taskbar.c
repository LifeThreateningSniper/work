#define COBJMACROS
#include <Shobjidl.h>
#include "taskbar.h"

static TBPFLAG curr_state;

/* 当没有错误发生时, 才关闭进度显示 */
int Taskbar_ClearNormalProgress(HWND hWnd)
{
	int ret;

	if (curr_state != TBPF_ERROR)
	{
		ret = Taskbar_SetProgress(hWnd, TBPF_NOPROGRESS, 0, 0);
		if (ret == -1)
			return -1;

		return 1;
	}
	else
		return 0;
}

/* 设置Win7任务栏图标进度条指示 */
// 使用前必须确保线程已经调用了CoInitializeEx函数初始化COM
// 线程退出时要记得执行CoUninitialize
int Taskbar_SetProgress(HWND hWnd, TBPFLAG state, ULONGLONG value, ULONGLONG total)
{
	ITaskbarList3 *pTaskbar;

	CoCreateInstance(&CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, &IID_ITaskbarList3, &pTaskbar);
	if (pTaskbar != NULL)
	{
		ITaskbarList3_HrInit(pTaskbar);
		ITaskbarList3_SetProgressValue(pTaskbar, hWnd, value, total);
		ITaskbarList3_SetProgressState(pTaskbar, hWnd, state);
		ITaskbarList3_Release(pTaskbar);

		curr_state = state;
		return 0;
	}
	
	// XP系统下无法设置任务栏进度条, 会返回-1
	return -1;
}

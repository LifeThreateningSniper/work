#define COBJMACROS
#include <Shobjidl.h>
#include "taskbar.h"

static TBPFLAG curr_state;

/* ��û�д�����ʱ, �Źرս�����ʾ */
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

/* ����Win7������ͼ�������ָʾ */
// ʹ��ǰ����ȷ���߳��Ѿ�������CoInitializeEx������ʼ��COM
// �߳��˳�ʱҪ�ǵ�ִ��CoUninitialize
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
	
	// XPϵͳ���޷�����������������, �᷵��-1
	return -1;
}

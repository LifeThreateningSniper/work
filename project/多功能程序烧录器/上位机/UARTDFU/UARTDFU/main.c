#include <stdint.h>
#include <stdio.h>
#include <tchar.h>
#include <Windows.h>
#include <windowsx.h>
#include <Shobjidl.h>
#include <CommCtrl.h>
#include "bluetooth.h"
#include "hexfile.h"
#include "port.h"
#include "taskbar.h"
#include "resource.h"
#include "UARTDFU.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' language='*' publicKeyToken='6595b64144ccf1df'\"")

static void browse_files(void);
static DWORD CALLBACK browse_files_thread(LPVOID lpParameter);
static uint8_t calc_crc8(const void *data, int len);
static BOOL confirm_cancel(void);
static void download_firmware(uintptr_t header);
static DWORD CALLBACK download_firmware_process(LPVOID lpParameter);
static void enable_controls(BOOL enabled);
static int load_commports(void);
static void set_progress(int value);
static void update_fileinfo(const HexFile *hexfile);
static INT_PTR CALLBACK DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
#if ADVANCED_MODE
static void load_configuration(void);
static void reset_and_run(int reset);
static DWORD CALLBACK reset_and_run_process(LPVOID lpParameter);
static void save_configuration(void);
#endif

HINSTANCE hinstMain;
HWND hwndCheck[2], hwndCombo, hwndSpin;
TCHAR szMessage[500];
static int downloading;
static BOOL file_loaded;
static HANDLE hThread;
static HWND hwndButton[7], hwndEdit[3], hwndDlg, hwndProgress;
#if ADVANCED_MODE
static int defport;
static BOOL conf_changed;
#endif

/* ����ѡ��̼��ļ��ĶԻ��� */
static void browse_files(void)
{
	// GetOpenFileNameA����������COINIT_MULTITHREADED����������, ����XPϵͳ�¡��ҵĵ��ԡ���ʾ�����κ�����
	// �½�һ���̵߳������У��ɽ���������
	CreateThread(NULL, 0, browse_files_thread, NULL, 0, NULL);
}

static DWORD CALLBACK browse_files_thread(LPVOID lpParameter)
{
	char name[MAX_PATH];
	int ret;
	BOOL bret;
	HexFile hexfile;
	OPENFILENAMEA ofn = {0};

	name[0] = '\0';
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
	ofn.hInstance = hinstMain;
	ofn.hwndOwner = hwndDlg;
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.lpstrDefExt = "hex";
	ofn.lpstrFile = name;
#if ADVANCED_MODE
	ofn.lpstrFilter = "HEX�ļ� (*.hex)\0*.HEX\0�����ļ� (*.*)\0*.*\0";
#else
	ofn.lpstrFilter = "�̼��ļ� (*.hex)\0*.HEX\0";
#endif
	ofn.nMaxFile = sizeof(name);
	bret = GetOpenFileNameA(&ofn);
	if (bret)
	{
		ret = HexFile_Load(&hexfile, name);
		if (ret == 0)
		{
			file_loaded = TRUE;
			update_fileinfo(&hexfile);
			HexFile_Free(&hexfile);
		}
		else
		{
			MessageBox(hwndDlg, TEXT("��ȡ�ļ�ʧ�ܣ�"), TEXT("���ļ�"), MB_ICONWARNING);
			file_loaded = FALSE;
			update_fileinfo(NULL);
		}
		SetWindowTextA(hwndEdit[0], name);
	}
	return bret;
}

/* ����CRC8У���� */
static uint8_t calc_crc8(const void *data, int len)
{
	const uint8_t *p = data;
	int i, j;
	uint16_t temp = 0;

	if (len != 0)
		temp = p[0] << 8;

	for (i = 1; i <= len; i++)
	{
		if (i != len)
			temp |= p[i];
		for (j = 0; j < 8; j++)
		{
			if (temp & 0x8000)
				temp ^= POLYNOMIAL_CRC8 << 7;
			temp <<= 1;
		}
	}
	return temp >> 8;
}

/* ȷ��ȡ������ */
static BOOL confirm_cancel(void)
{
	if (downloading == 2)
		return MessageBox(hwndDlg, TEXT("��ȷ��Ҫȡ���̼�������\nȡ�����豸�����������������������������ع̼���"), TEXT("ȡ������"), MB_ICONQUESTION | MB_OKCANCEL) == IDOK;
	else
		return TRUE;
}

/* ���ع̼� */
static void download_firmware(uintptr_t header)
{
	if (downloading)
	{
		if (confirm_cancel())
		{
			downloading = 0;
			EnableWindow(hwndButton[0], FALSE);
#if ADVANCED_MODE
			EnableWindow(hwndButton[5], FALSE);
#endif
		}
	}
	else
	{
		downloading = 1;
		szMessage[0] = '\0';
		enable_controls(FALSE);
		hThread = CreateThread(NULL, 0, download_firmware_process, (LPVOID)header, 0, NULL);
	}
}

/* ���ع̼����߳� */
static DWORD CALLBACK download_firmware_process(LPVOID lpParameter)
{
	char filename[MAX_PATH];
	double progress;
	int i, j, ret;
	uint8_t buffer[16];
	uint8_t crc;
	uintptr_t header = (uintptr_t)lpParameter;
	COMMTIMEOUTS timeouts = {0};
	DeviceResponse resp;
	FirmwareInfo info;
	HANDLE hPort;
	HexFile hexfile;
#if ADVANCED_MODE
	CRCConfig crccfg;
#endif

	// ��ʼ��COM (��������������ͼ���ϵĽ�����)
	CoInitializeEx(NULL, COINIT_MULTITHREADED);

	// ��Ϊȡ����ť
	if (header == HEADER_FIRMWARE_INFO)
	{
		Button_SetText(hwndButton[0], TEXT("ȡ��"));
		EnableWindow(hwndButton[0], TRUE);
	}
#if ADVANCED_MODE
	else if (header == HEADER_CRC_CONFIG)
	{
		Button_SetText(hwndButton[5], TEXT("ȡ��"));
		EnableWindow(hwndButton[5], TRUE);
	}
#endif

	// �򿪴���
	set_progress(-1);
	hPort = open_port(-1);
	if (hPort == INVALID_HANDLE_VALUE)
		goto end;

	// ���ó�ʱʱ��
	timeouts.ReadTotalTimeoutConstant = 80;
	timeouts.WriteTotalTimeoutConstant = 10000;
	SetCommTimeouts(hPort, &timeouts);

	// ����豸
#if ADVANCED_MODE
	SetDlgItemText(hwndDlg, IDC_STATIC5, TEXT("�밴�¸�λ��"));
#else
	SetDlgItemText(hwndDlg, IDC_STATIC5, TEXT("���ڼ���豸..."));
#endif
	while (downloading)
	{
		memset(buffer, 0xab, sizeof(buffer));
		write_port(hPort, buffer, sizeof(buffer));
		
		ret = read_port(hPort, buffer, sizeof(buffer));
		if (ret == sizeof(buffer))
		{
			for (j = 0; j < sizeof(buffer); j++)
			{
				if (buffer[j] != 0xcd)
					break;
			}
			if (j == sizeof(buffer))
				break;
		}
	}
	if (!downloading)
		goto end;

	set_progress(0);
	timeouts.ReadTotalTimeoutConstant = 10000;
	SetCommTimeouts(hPort, &timeouts);

	switch (header)
	{
	case HEADER_FIRMWARE_INFO:
		// ��ȡHEX�ļ�������HEX�ļ���Ϣ
		GetWindowTextA(hwndEdit[0], filename, sizeof(filename));
		ret = HexFile_Load(&hexfile, filename);
		if (ret == -1)
		{
			lstrcpy(szMessage, TEXT("��ȡ�̼��ļ�ʧ�ܣ�"));
			break;
		}
		update_fileinfo(&hexfile);

		info.header = HEADER_FIRMWARE_INFO;
		info.size = hexfile.size;
		info.start_addr = hexfile.start_addr;
		info.end_addr = hexfile.end_addr;
		info.entry_point = hexfile.entry_point;
		info.firmware_checksum = calc_crc8(hexfile.data, hexfile.size);
		info.header_checksum = calc_crc8(&info, sizeof(info) - 1);
		write_port(hPort, &info, sizeof(info));

		while (downloading)
		{
			ret = read_port(hPort, &resp, sizeof(resp));
			if (ret != sizeof(resp) || calc_crc8(&resp, sizeof(resp)) != 0)
			{
				lstrcpy(szMessage, TEXT("���ع̼�ʱ��������"));
				break;
			}
			else if (resp.addr == 0)
			{
				_stprintf_s(szMessage, _countof(szMessage), TEXT("�̼���С�����豸Flash����: %.1lfKB"), resp.size / 1024.0);
				break;
			}
			else if (resp.addr == 0xffffffff && resp.size == 0xffffffff)
			{
				// ���·��͹̼���Ϣ
				write_port(hPort, &info, sizeof(info));
				continue;
			}
			else if (downloading == 1 && resp.addr != hexfile.start_addr)
			{
#if ADVANCED_MODE
				_stprintf_s(szMessage, _countof(szMessage), TEXT("�̼���ַ��Ч��\n�뽫Keil MDK��Ŀ������Targetѡ���IROM1��ֵ��Ϊ0x%08x��\n����system_*.c�е�VECT_TAB_OFFSET��Ϊ0x%04xU��\nȻ�����±���̼���"), resp.addr, resp.addr - 0x08000000);
#else
				lstrcpy(szMessage, TEXT("�ù̼��������ڴ��豸��"));
#endif
				break;
			}
			else if (resp.size == 0)
				break; // ���ͽ���
			else if (resp.size == 0xffffffff)
				continue; // �����ȴ� (�������Flash��Ҫ�ܳ�ʱ��, Ϊ�˱�����ʾ��ʱ, ��ʹ�����ָ��)
			else if (resp.addr - hexfile.start_addr + resp.size > hexfile.size)
			{
				lstrcpy(szMessage, TEXT("�豸�����쳣��"));
				break;
			}

			if (downloading == 1)
			{
				downloading = 2;
#if ADVANCED_MODE
				Button_SetCheck(hwndCheck[0], TRUE);
#endif
			}

			write_port(hPort, hexfile.data + (resp.addr - hexfile.start_addr), resp.size);
			progress = (double)(resp.addr - hexfile.start_addr + resp.size) / hexfile.size;
			i = (int)(progress * 100 + 0.5);
			if (i > 100)
				i = 100;
			set_progress(i);

			crc = calc_crc8(hexfile.data + (resp.addr - hexfile.start_addr), resp.size);
			write_port(hPort, &crc, 1);
		}

		HexFile_Free(&hexfile);
#if !ADVANCED_MODE
		// ������ɺ������̼�
		buffer[0] = 0xab;
		write_port(hPort, buffer, 1);
#endif
		break;
#if ADVANCED_MODE
	case HEADER_CRC_CONFIG:
		crccfg.header = HEADER_CRC_CONFIG;
		crccfg.crc_enabled = Button_GetCheck(hwndCheck[0]);
		crccfg.header_checksum = calc_crc8(&crccfg, sizeof(crccfg) - 1);
		do
		{
			write_port(hPort, &crccfg, sizeof(crccfg));
			ret = read_port(hPort, &resp, sizeof(resp));
			if (ret != sizeof(resp) || calc_crc8(&resp, sizeof(resp)) != 0)
				resp.size = 0; // ��Ӧ��������, ��Ϊʧ��
		} while (downloading && resp.addr == 0xffffffff && resp.size == 0xffffffff); // �ж��Ƿ��ش�crccfg

		if (resp.size != 1)
			lstrcpy(szMessage, TEXT("�޸�CRC����ʧ�ܣ�"));
		break;
#endif
	}
	
end:
	close_port(hPort);
	if (hwndDlg != NULL)
		PostMessage(hwndDlg, WM_USER, 1, header);
	CoUninitialize();
	return 0;
}

/* ���û���ÿؼ� */
static void enable_controls(BOOL enabled)
{
	EnableWindow(hwndButton[0], enabled && file_loaded);
	EnableWindow(hwndButton[1], enabled);
	EnableWindow(hwndButton[2], enabled);
	EnableWindow(hwndButton[4], enabled);
	EnableWindow(hwndCheck[1], enabled);
	EnableWindow(GetDlgItem(hwndDlg, IDC_STATIC13), enabled);
	EnableWindow(hwndCombo, enabled);
	EnableWindow(GetDlgItem(hwndDlg, IDC_STATIC9), enabled);
	EnableWindow(hwndEdit[2], enabled && Button_GetCheck(hwndCheck[1]));
	InvalidateRect(hwndSpin, NULL, FALSE);

#if ADVANCED_MODE
	EnableWindow(hwndButton[3], enabled);
	EnableWindow(hwndButton[5], enabled);
	EnableWindow(hwndButton[6], enabled);
	EnableWindow(hwndCheck[0], enabled);
	Edit_SetReadOnly(hwndEdit[1], !enabled);
#endif
}

/* ��ע����ж�ȡ�����б� */
static int load_commports(void)
{
#if ADVANCED_MODE
	int selection;
#endif
	DWORD index, type, namelen, valuelen;
	HKEY key;
	LSTATUS status;
	TCHAR name[50];
	TCHAR value[50];

	ComboBox_ResetContent(hwndCombo);
	status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("HARDWARE\\DEVICEMAP\\SERIALCOMM"), 0, KEY_READ, &key);
	if (status != ERROR_SUCCESS)
		return -1;

	index = 0;
	while (1)
	{
		namelen = _countof(name);
		valuelen = sizeof(value);
		status = RegEnumValue(key, index, name, &namelen, NULL, &type, (LPBYTE)value, &valuelen);
		if (status != ERROR_SUCCESS)
			break;
		ComboBox_AddString(hwndCombo, value);
		index++;
	}
	if (index != 0)
	{
#if ADVANCED_MODE
		_stprintf_s(value, _countof(value), TEXT("COM%d"), defport);
		selection = ComboBox_FindString(hwndCombo, 0, value);
		if (selection != -1)
			ComboBox_SetCurSel(hwndCombo, selection);
		else
#endif
			ComboBox_SetCurSel(hwndCombo, 0);
	}

	RegCloseKey(key);
	return 0;
}

/* ��ȡ������Ϣ */
#if ADVANCED_MODE
static void load_configuration(void)
{
	char path[MAX_PATH];
	char *p, *q, *r;
	int n, size;
	FILE *fp;
	HANDLE module;
	LRESULT range;

	module = GetModuleHandle(NULL);
	GetModuleFileNameA(module, path, MAX_PATH);
	p = strrchr(path, '\\');
	if (p == NULL)
		return;
	*p = '\0';
	SetCurrentDirectoryA(path);
	
	size = MAX_PATH - (int)(p - path) - 1;
	strncat_s(p, size, "\\conf.txt", size);
	p[size] = '\0';

	fopen_s(&fp, path, "r");
	if (fp == NULL)
		return;
	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	p = (char *)malloc(size + 1);
	if (p != NULL)
	{
		size = (int)fread(p, 1, size, fp);
		p[size] = '\0';

		q = strchr(p, '\n');
		if (q != NULL)
		{
			*q = '\0';
			q++;

			r = strchr(q, '\n');
			if (r != NULL)
			{
				*r = '\0';
				n = atoi(r + 1); // ������ʾ��ѡ��, ������ʾû�й�ѡ, ����ֵ������ʱ�ĺ�����
				if (n > 0)
				{
					Button_SetCheck(hwndCheck[1], TRUE);
					EnableWindow(hwndEdit[2], TRUE);
					InvalidateRect(hwndSpin, NULL, FALSE);
				}
				else if (n < 0)
					n = -n;

				range = SendMessage(hwndSpin, UDM_GETRANGE, 0, 0);
				if (n >= HIWORD(range) && n <= LOWORD(range))
					SendMessage(hwndSpin, UDM_SETPOS, 0, n);
			}
			SetWindowTextA(hwndEdit[1], q);
		}

		if (_strnicmp(p, "com", 3) == 0)
			defport = atoi(p + 3);
		free(p);
	}
	fclose(fp);
	conf_changed = FALSE;
}
#endif

#if ADVANCED_MODE
/* ��λ�����й̼� */
static void reset_and_run(int reset)
{
	int len;
	LPTSTR cmdfmt;

	if (!downloading)
	{
		len = Edit_GetTextLength(hwndEdit[1]) + 2;
		cmdfmt = (LPTSTR)malloc(len * sizeof(TCHAR));
		if (cmdfmt != NULL)
		{
			cmdfmt[0] = (reset) ? 'Y' : 'N';
			Edit_GetText(hwndEdit[1], cmdfmt + 1, len - 1);
			downloading = 1;
			szMessage[0] = '\0';
			enable_controls(FALSE);
			hThread = CreateThread(NULL, 0, reset_and_run_process, cmdfmt, 0, NULL);
			if (hThread == NULL)
				free(cmdfmt);
		}
	}
}

/* ���͸�λ������߳� */
static DWORD CALLBACK reset_and_run_process(LPVOID lpParameter)
{
	int err = 0;
	int i, len, port;
	uint32_t data;
	BOOL ret;
	COMMTIMEOUTS timeouts = {0};
	HANDLE hPort;
	LPTSTR cmdfmt = (LPTSTR)lpParameter + 1;
	LPTSTR cmd;
	PROCESS_INFORMATION pi = {0};
	STARTUPINFO si = {0};

	CoInitializeEx(NULL, COINIT_MULTITHREADED);
	len = lstrlen(cmdfmt) + 50;
	cmd = (LPTSTR)malloc(len * sizeof(TCHAR));
	if (cmd != NULL)
	{
		set_progress(-1);
		port = get_port();
		if (*(cmdfmt - 1) == 'Y')
		{
			// ��λ
			hPort = open_port(port);
			if (hPort != INVALID_HANDLE_VALUE)
			{
				timeouts.WriteTotalTimeoutConstant = 10000;
				SetCommTimeouts(hPort, &timeouts);
				
				data = 0xabababab;
				wait_port(); // �ȵȴ�
				i = write_port(hPort, &data, 4); // ��λ
				if (i != 4)
				{
					err = 1;
					lstrcpy(szMessage, TEXT("��λʧ�ܣ�"));
				}
				close_port(hPort); // ��λ�����Ϲش���, ׼���������ڵ��Թ���
				wait_port();
			}
			else
				err = 1;
		}

		if (!err)
		{
			_sntprintf_s(cmd, len, len, cmdfmt, port);
			for (i = 0; cmd[i] != '\0'; i++)
			{
				if (cmd[i] != ' ' && cmd[i] != '\t')
					break;
			}

			if (cmd[i] != '\0') // ���Ϊ��
			{
				si.cb = sizeof(STARTUPINFO);
				ret = CreateProcess(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
				if (!ret)
					lstrcpy(szMessage, TEXT("�������ڵ��Թ���ʧ�ܣ�\nָ���Ĵ��ڵ��Թ��߿���δ��װ��"));
			}
		}
		free(cmd);
	}

	free(lpParameter);
	if (hwndDlg != NULL)
		PostMessage(hwndDlg, WM_USER, 2, 0);
	CoUninitialize();
	return 0;
}

/* �������� */
static void save_configuration(void)
{
	char *str;
	int i, n, len;
	FILE *fp;
	HWND list[2] = {hwndCombo, hwndEdit[1]};

	fopen_s(&fp, "conf.txt", "w");
	if (fp != NULL)
	{
		for (i = 0; i < _countof(list); i++)
		{
			len = GetWindowTextLengthA(list[i]) + 1;
			str = (char *)malloc(len);
			if (str != NULL)
			{
				GetWindowTextA(list[i], str, len);
				fprintf(fp, "%s\n", str);
				free(str);
			}
			else
				fprintf(fp, "\n");
		}

		n = (int)SendMessage(hwndSpin, UDM_GETPOS, 0, 0);
		if (!Button_GetCheck(hwndCheck[1]))
			n = -n;
		fprintf(fp, "%d\n", n);

		fclose(fp);
		conf_changed = FALSE;
	}
}
#endif

/* ���ý��������� */
static void set_progress(int value)
{
	FLASHWINFO flash;
	LONG_PTR style;
	TCHAR str[20];

	style = GetWindowLongPtr(hwndProgress, GWL_STYLE);
	if (value == -1)
	{
		// ѭ������
		if ((style & PBS_MARQUEE) == 0)
		{
			SetWindowLongPtr(hwndProgress, GWL_STYLE, style | PBS_MARQUEE);
			SendMessage(hwndProgress, PBM_SETMARQUEE, 1, 0);
		}
		Taskbar_SetProgress(hwndDlg, TBPF_INDETERMINATE, 0, 0);
	}
	else
	{
		if (style & PBS_MARQUEE)
		{
			SendMessage(hwndProgress, PBM_SETMARQUEE, 0, 0);
			SetWindowLongPtr(hwndProgress, GWL_STYLE, style & ~PBS_MARQUEE);
		}

		if (value >= 0 && value <= 100)
		{
			// ��������
			SendMessage(hwndProgress, PBM_SETPOS, value, 0);
			Taskbar_SetProgress(hwndDlg, TBPF_NORMAL, value, 100);
		}
		else if (value == 101)
		{
			// ���
			value = 100;
			SendMessage(hwndProgress, PBM_SETPOS, 100, 0);

			Taskbar_SetProgress(hwndDlg, TBPF_NORMAL, 100, 100);
			flash.cbSize = sizeof(FLASHWINFO);
			flash.dwFlags = FLASHW_TIMERNOFG | FLASHW_TRAY; // һֱ��˸���û��л��ش���
			flash.dwTimeout = 0;
			flash.hwnd = hwndDlg;
			flash.uCount = 3;
			FlashWindowEx(&flash); // ������ͼ����˸ (��ɫ)
		}
		else if (value == 102)
		{
			// �������������
			Taskbar_SetProgress(hwndDlg, TBPF_NOPROGRESS, 0, 0);
		}
		else if (value == -2)
		{
			// ����
			Taskbar_SetProgress(hwndDlg, TBPF_ERROR, 1, 1);
		}
	}

	if (value == 0)
		SetDlgItemText(hwndDlg, IDC_STATIC5, TEXT("�Ѽ�⵽�豸"));
	else if (value > 0 && value <= 100)
	{
		_stprintf_s(str, _countof(str), TEXT("����: %d%%"), value);
		SetDlgItemText(hwndDlg, IDC_STATIC5, str);
	}
}

/* ��ʾ�̼��ļ���Ϣ */
static void update_fileinfo(const HexFile *hexfile)
{
	TCHAR str[50];

	if (hexfile == NULL)
	{
		SetDlgItemText(hwndDlg, IDC_STATIC2, TEXT(""));
		SetDlgItemText(hwndDlg, IDC_STATIC3, TEXT(""));
		SetDlgItemText(hwndDlg, IDC_STATIC4, TEXT(""));
	}
	else
	{
		if (hexfile->size < 1024)
			_stprintf_s(str, _countof(str), TEXT("%uB"), hexfile->size);
		else if (hexfile->size < 1048576)
			_stprintf_s(str, _countof(str), TEXT("%.2fKB (%u�ֽ�)"), hexfile->size / 1024.0f, hexfile->size);
		else
			_stprintf_s(str, _countof(str), TEXT("%.2fMB (%u�ֽ�)"), hexfile->size / 1048576.0f, hexfile->size);
		SetDlgItemText(hwndDlg, IDC_STATIC2, str);

		_stprintf_s(str, _countof(str), TEXT("0x%08x - 0x%08x"), hexfile->start_addr, hexfile->end_addr - 1);
		SetDlgItemText(hwndDlg, IDC_STATIC3, str);

		_stprintf_s(str, _countof(str), TEXT("0x%08x"), hexfile->entry_point);
		SetDlgItemText(hwndDlg, IDC_STATIC4, str);
	}
	EnableWindow(hwndButton[0], hexfile != NULL);
}

/* �Ի�����Ϣ���� */
static INT_PTR CALLBACK DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PNMLINK pnmLink;

	switch (uMsg)
	{
	case WM_COMMAND:
		wmId = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		switch (wmId)
		{
		case IDC_BUTTON1:
			browse_files();
			break;
		case IDC_BUTTON2:
			load_commports();
			break;
		case IDC_BUTTON4:
			DialogBox(hinstMain, MAKEINTRESOURCE(IDD_DIALOG3), hDlg, Bluetooth_DlgProc);
			break;
		case IDC_CHECK2:
			EnableWindow(hwndEdit[2], Button_GetCheck(hwndCheck[1]));
			InvalidateRect(hwndSpin, NULL, FALSE); // �ػ���ֵ������Ť (����XPϵͳ)
#if ADVANCED_MODE
			conf_changed = TRUE;
#endif
			break;
#if ADVANCED_MODE
		case IDC_BUTTON3:
			reset_and_run(1);
			break;
		case IDC_BUTTON5:
			download_firmware(HEADER_CRC_CONFIG);
			break;
		case IDC_BUTTON6:
			reset_and_run(0);
			break;
		case IDC_COMBO1:
			if (wmEvent == CBN_EDITCHANGE || wmEvent == CBN_SELCHANGE)
				conf_changed = TRUE;
			break;
		case IDC_EDIT2:
		case IDC_EDIT3:
			if (wmEvent == EN_CHANGE)
				conf_changed = TRUE;
			break;
#endif
		case IDOK:
			download_firmware(HEADER_FIRMWARE_INFO);
			break;
		case IDCANCEL:
			// ����˶Ի���رհ�ť
			if (confirm_cancel())
			{
#if ADVANCED_MODE
				if (conf_changed)
					save_configuration();
#endif
				hwndDlg = NULL;
				EndDialog(hDlg, 0);
			}
			break;
		}
		break;
	case WM_INITDIALOG:
		hwndDlg = hDlg;
		hwndButton[0] = GetDlgItem(hDlg, IDOK);
		hwndButton[1] = GetDlgItem(hDlg, IDC_BUTTON1);
		hwndButton[2] = GetDlgItem(hDlg, IDC_BUTTON2);
		hwndButton[4] = GetDlgItem(hDlg, IDC_BUTTON4);
		hwndCheck[1] = GetDlgItem(hDlg, IDC_CHECK2);
		hwndCombo = GetDlgItem(hDlg, IDC_COMBO1);
		hwndEdit[0] = GetDlgItem(hDlg, IDC_EDIT1);
		hwndEdit[2] = GetDlgItem(hDlg, IDC_EDIT3);
		hwndProgress = GetDlgItem(hDlg, IDC_PROGRESS1);
		hwndSpin = GetDlgItem(hDlg, IDC_SPIN1);
		SendMessage(hwndSpin, UDM_SETRANGE, 0, MAKELPARAM(30000, 1));
		SendMessage(hwndSpin, UDM_SETPOS, 0, 2000); // Ĭ����ʱ2��
#if ADVANCED_MODE
		hwndButton[3] = GetDlgItem(hDlg, IDC_BUTTON3);
		hwndButton[5] = GetDlgItem(hDlg, IDC_BUTTON5);
		hwndButton[6] = GetDlgItem(hDlg, IDC_BUTTON6);
		hwndCheck[0] = GetDlgItem(hDlg, IDC_CHECK1);
		Button_SetCheck(hwndCheck[0], TRUE);
		hwndEdit[1] = GetDlgItem(hDlg, IDC_EDIT2);
#endif

		lParam = (LPARAM)LoadImage(hinstMain, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
		SendMessage(hDlg, WM_SETICON, ICON_BIG, lParam);
		lParam = (LPARAM)LoadImage(hinstMain, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
		SendMessage(hDlg, WM_SETICON, ICON_SMALL, lParam);

		SendMessage(hwndProgress, PBM_SETRANGE, 0, MAKELPARAM(1, 100));
		SetDlgItemText(hDlg, IDC_STATIC5, DEFAULT_TEXT);
		update_fileinfo(NULL);
#if ADVANCED_MODE
		load_configuration();
#endif
		load_commports();
		break;
	case WM_NOTIFY:
		pnmLink = (PNMLINK)lParam;
		if (pnmLink->hdr.code == NM_CLICK && pnmLink->hdr.idFrom == IDC_SYSLINK1)
			MessageBox(hDlg, SAMPLE_CODE, TEXT("ʾ������"), 0);
		break;
	case WM_USER:
		/* �����̼߳����˳�ʱҪִ�еĴ��� */
		if (wParam >= 1 && wParam <= 2)
		{
			switch (wParam)
			{
			case 1:
				/* ������� */
				if (downloading)
				{
					if (szMessage[0] != '\0')
						set_progress(-2);
					else
						set_progress(101);
				}

				switch (lParam)
				{
				case HEADER_FIRMWARE_INFO:
					if (downloading)
					{
						if (szMessage[0] == '\0')
							MessageBox(hDlg, TEXT("�̼�������ϣ�"), TEXT("���ع̼�"), MB_ICONINFORMATION);
						else
							MessageBox(hDlg, szMessage, TEXT("���ع̼�"), MB_ICONWARNING);
					}
					Button_SetText(hwndButton[0], TEXT("����"));
					break;
#if ADVANCED_MODE
				case HEADER_CRC_CONFIG:
					if (downloading)
					{
						if (szMessage[0] == '\0')
							MessageBox(hDlg, TEXT("�޸�CRC���óɹ���"), TEXT("CRC����"), MB_ICONINFORMATION);
						else
							MessageBox(hDlg, szMessage, TEXT("CRC����"), MB_ICONWARNING);
					}
					Button_SetText(hwndButton[5], TEXT("����CRCУ��"));
					break;
#endif
				}
				SetDlgItemText(hDlg, IDC_STATIC5, DEFAULT_TEXT);
				break;
			case 2:
				/* ��λ�����й̼���� */
				if (szMessage[0] != '\0')
				{
					set_progress(-2);
					MessageBox(hDlg, szMessage, TEXT("����"), MB_ICONWARNING);
				}
				break;
			}

			downloading = 0;
			enable_controls(TRUE);
			set_progress(102);
		}
		break;
	}
	return 0;
}

/* ������ */
int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	LRESULT ret;

	hinstMain = hInstance;
	InitCommonControls();
	CoInitializeEx(NULL, COINIT_MULTITHREADED);

#if ADVANCED_MODE
	ret = DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DlgProc);
#else
	ret = DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG2), NULL, DlgProc);
#endif

	if (hThread != NULL)
	{
		downloading = 0;
		WaitForSingleObject(hThread, INFINITE);
	}
	CoUninitialize();
	return (int)ret;
}

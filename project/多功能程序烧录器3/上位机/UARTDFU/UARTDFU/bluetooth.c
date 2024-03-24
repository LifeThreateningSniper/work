#include <stdio.h>
#include <tchar.h>
#include <Windows.h>
#include <windowsx.h>
#include <CommCtrl.h>
#include <Uxtheme.h>
#include "bluetooth.h"
#include "encoding.h"
#include "port.h"
#include "resource.h"

#pragma comment(lib, "uxtheme.lib")

static int add_device(const BluetoothDevice *device);
static DWORD CALLBACK bluetooth_process(LPVOID lpParameter);
static void copy_hc05_addr(char *dest, int destsize, const char *src);
static void dlg_init(void);
static void enable_controls(BOOL enabled);
static void start_bluetooth_process(uintptr_t opcode);

extern HINSTANCE hinstMain;
extern TCHAR szMessage[500];
static int searching_devices, testing_baudrate;
static int thread_active;
static HANDLE hThread;
static HIMAGELIST hImageList;
static HWND hwndButton[4], hwndDlg, hwndEdit[2], hwndList;

static int add_device(const BluetoothDevice *device)
{
	char buf[100];
	wchar_t wbuf[100];
	LVITEMW lvi;

	lvi.mask = LVIF_IMAGE | LVIF_PARAM | LVIF_TEXT; // ����ͼ�ꡢ�Զ���������ı�
	if (device != NULL)
	{
		if (device->name[0] == '\0')
			_snprintf_s(buf, sizeof(buf), sizeof(buf), "%s", device->addr); // ֻ��ʾ������ַ
		else
			_snprintf_s(buf, sizeof(buf), sizeof(buf), "%s (%s)", device->name, device->addr); // ��ʾ�������ƺ͵�ַ
		utf8to16_wbuf(buf, wbuf, sizeof(wbuf)); // UTF8תUTF16
		lvi.iImage = 0; // ����ͼ��
	}
	else
	{
		wcscpy_s(wbuf, _countof(wbuf), L"���豸 (���ڻص��ӻ�ģʽ)");
		lvi.iImage = 1; // ��ͼ��
	}
	lvi.iItem = 0; // ��ʱ�Ȳ��뵽��ͷ, �ȴ��Զ����� (�ؼ��������Զ�����)
	lvi.iSubItem = 0; // ��һ��
	lvi.lParam = (device != NULL) ? device->param : -1; // �豸��� (����ʱʹ��)
	lvi.pszText = wbuf;
	lvi.iItem = (int)SendMessage(hwndList, LVM_INSERTITEMW, 0, (LPARAM)&lvi); // ����ֵΪ�������к�

	if (device != NULL && device->rssi_valid)
	{
		_snwprintf_s(wbuf, _countof(wbuf), _countof(wbuf), L"%d", device->rssi);
		lvi.mask = LVIF_TEXT; // ֻ�����ı�
		lvi.iSubItem = 1; // �ڶ���
		lvi.pszText = wbuf;
		SendMessage(hwndList, LVM_SETITEMW, 0, (LPARAM)&lvi);
	}
	return lvi.iItem;
}

static DWORD CALLBACK bluetooth_process(LPVOID lpParameter)
{
	const int baudrate_list[] = {38400, 115200, 57600, 19200, 9600, 4800, 2400, 0}; // HC-05����ģ��ATģʽ�µĲ����ʹ̶�Ϊ38400, ����Ҫ�ŵ�һ��
	char addr[20];
	char buffer[200];
	char *p, *q;
	int baudrate[3] = {0}; // �����±�ֱ��ʾ�����ʡ�ֹͣλ��У��λ
	int i, j, n;
	int msgtype = 1;
	wchar_t *ws;
	uintptr_t opcode = (uintptr_t)lpParameter;
	BluetoothDevice device;
	BOOL ishc05 = FALSE, hc05_baudok = FALSE;
	COMMTIMEOUTS timeouts = {0};
	DCB dcb;
	HANDLE hPort;
	LVITEMA lvi;

	SetDlgItemText(hwndDlg, IDC_STATIC2, TEXT("���ڴ򿪴���..."));
	hPort = open_port(-1); // �򿪴���, ��������Ὣ������Ϣ����szMessage
	if (hPort == INVALID_HANDLE_VALUE)
	{
		msgtype = 2;
		goto end;
	}
	dcb.DCBlength = sizeof(DCB);
	GetCommState(hPort, &dcb);

	/* ������ƥ�� */
	SetDlgItemText(hwndDlg, IDC_STATIC2, TEXT("����ƥ�䲨����..."));
	timeouts.ReadTotalTimeoutConstant = 500;
	timeouts.WriteTotalTimeoutConstant = 1000; // �������ڱ������÷��ͳ�ʱʱ��, �����������豸�����ڶ����޵ȴ�
	SetCommTimeouts(hPort, &timeouts);
	testing_baudrate = 1;
	for (i = 0; baudrate_list[i] != 0; i++)
	{
		if (dcb.BaudRate != baudrate_list[i])
		{
			dcb.BaudRate = baudrate_list[i];
			SetCommState(hPort, &dcb);
		}
		for (j = 0; j < 2; j++)
		{
			if (!testing_baudrate)
			{
				lstrcpy(szMessage, TEXT("������ȡ��"));
				goto end;
			}
			write_port_str(hPort, "AT\r\n");
			read_port_line(hPort, buffer, sizeof(buffer), 200);
			if (strcmp(buffer, "OK") == 0)
				break;
		}
		if (j != 2)
			break;
	}
	testing_baudrate = 0;
	baudrate[0] = baudrate_list[i];
	if (baudrate[0] == 0)
	{
		// �Ҳ������ʵĲ�����, ����������������ʹ��ATָ�����
		msgtype = 2;
		lstrcpy(szMessage, TEXT("�������ڲ�����ӦATָ�\n��ʹ�õ���HC-05ģ�飬�����ڰ�סKEY�����������ͨ�磬�����޷�����ATģʽ��\n�����ʶ��ƥ��ʧ�ܺ�HC-05ģ���п����������ɳ���ģ�������ϵ硣\n��ʹ�õ���CC2541ģ�飬���������Ϊ�������������豸��\n����Ҫ�����豸���ӣ���������������豸�����²��ϡ�"));
		goto end;
	}
	else if (baudrate[0] == 38400)
	{
		// �ж��Ƿ�ΪHC-05ģ��
		read_port_str(hPort, buffer, sizeof(buffer)); // ���յ�����δ���������
		write_port_str(hPort, "AT+UART?\r\n");
		read_port_line(hPort, buffer, sizeof(buffer), 0);
		if (strncmp(buffer, "+UART:", 6) == 0)
		{
			n = sscanf_s(buffer, "+UART:%d,%d,%d", &baudrate[0], &baudrate[1], &baudrate[2]);
			hc05_baudok = (n == 3 && baudrate[0] == 115200 && baudrate[1] == 0 && baudrate[2] == 0);
			read_port_line(hPort, buffer, sizeof(buffer), 0);
			ishc05 = (strcmp(buffer, "OK") == 0);
		}
	}

	/* ���ò����� */
	if ((!ishc05 && baudrate[0] != 115200) || (ishc05 && !hc05_baudok))
	{
		if (!ishc05)
			_sntprintf_s(szMessage, _countof(szMessage), _countof(szMessage), TEXT("��ǰ�������ڵĲ�����Ϊ%d��\n�Ƿ���Ҫ�޸�Ϊ115200��"), baudrate[0]);
		else
			_sntprintf_s(szMessage, _countof(szMessage), _countof(szMessage), TEXT("��ǰ�������ڵ�ͨ��ģʽ�Ĳ���������Ϊ��%d,%d,%d����\n�Ƿ���Ҫ�޸�Ϊ��115200,0,0����\n\nע��: HC-05ģ���ATģʽ�����ʹ̶�Ϊ38400��\n����ָ����ͨ��ģʽ�£�Ҳ���ǲ���סKEY������ģ���ϵ�ʱ�Ĳ����ʡ�"), baudrate[0], baudrate[1], baudrate[2]);

		if (MessageBox(hwndDlg, szMessage, TEXT("��������"), MB_ICONQUESTION | MB_OKCANCEL) == IDOK)
			szMessage[0] = '\0';
		else
		{
			lstrcpy(szMessage, TEXT("���ڲ����ʲ���ȷ"));
			goto end;
		}
		
		if (!ishc05)
		{
			write_port_str(hPort, "AT+BAUD8\r\n");
			read_port_line(hPort, buffer, sizeof(buffer), 0);
			if (strcmp(buffer, "+BAUD=8") == 0 || strcmp(buffer, "+BAUD=115200") == 0)
			{
				read_port_line(hPort, buffer, sizeof(buffer), 0);
				if (strcmp(buffer, "OK") == 0)
				{
					dcb.BaudRate = CBR_115200;
					SetCommState(hPort, &dcb);
				}
			}
			if (dcb.BaudRate != CBR_115200)
			{
				lstrcpy(szMessage, TEXT("���ô��ڲ�����ʧ��"));
				goto end;
			}

			for (i = 0; i < 10; i++)
			{
				write_port_str(hPort, "AT+BAUD\r\n");
				read_port_line(hPort, buffer, sizeof(buffer), 200);
				if (strcmp(buffer, "+BAUD=8") == 0)
					break;
			}
			if (i == 10)
			{
				lstrcpy(szMessage, TEXT("���ڲ��������óɹ���û����Ч"));
				goto end;
			}
		}
		else
		{
			write_port_str(hPort, "AT+UART=115200,0,0\r\n");
			read_port_line(hPort, buffer, sizeof(buffer), 0);
			if (strcmp(buffer, "OK") != 0)
			{
				lstrcpy(szMessage, TEXT("���ô��ڲ�����ʧ��"));
				goto end;
			}
		}
	}

	/* ִ������ */
	timeouts.ReadTotalTimeoutConstant = (!ishc05) ? 25 : 300;
	SetCommTimeouts(hPort, &timeouts);
	switch (opcode)
	{
	case 1:
		/* �����豸 */
		SetDlgItemText(hwndDlg, IDC_STATIC2, TEXT("���������豸..."));
		if (!ishc05)
		{
			i = 10;
			write_port_str(hPort, "AT+ROLE0\r\n");
			read_port_line(hPort, buffer, sizeof(buffer), 0);
			if (strcmp(buffer, "+ROLE=0") == 0)
			{
				read_port_line(hPort, buffer, sizeof(buffer), 0);
				if (strcmp(buffer, "OK") == 0)
				{
					// �ȴ���Ч
					for (i = 0; i < 10; i++)
					{
						write_port_str(hPort, "AT+ROLE\r\n");
						read_port_line(hPort, buffer, sizeof(buffer), 200);
						Sleep(500); // ��ʱ, ��������û��Ӧ (ʱ��һ��Ҫ��, ��Ȼ�����ȡ�������ƻ�Ϊ��)
						if (strcmp(buffer, "+ROLE=0") == 0)
							break;
					}
				}
			}
			if (i == 10)
			{
				lstrcpy(szMessage, TEXT("���ô��ڴ�ģʽʧ��"));
				break;
			}

			// ������, ĳЩCC2541����ģ��ֻ���ڴ�ģʽ�²��ܻ�ȡ���������ƺ͵�ַ
			write_port_str(hPort, "AT+NAME\r\n");
			read_port_line(hPort, buffer, sizeof(buffer), 200); // ���һ������������0, ������һ�е������ַ��������ó��豸����������, �������������\r\n��ͷ
			if (strncmp(buffer, "+NAME=", 6) == 0)
			{
				ws = utf8to16(buffer + 6);
				if (ws != NULL)
				{
					SetWindowTextW(hwndEdit[0], ws);
					free(ws);
				}
			}

			write_port_str(hPort, "AT+LADDR\r\n");
			read_port_line(hPort, buffer, sizeof(buffer), 0);
			if (strncmp(buffer, "+LADDR=", 7) == 0)
				SetDlgItemTextA(hwndDlg, IDC_STATIC4, buffer + 7);

			write_port_str(hPort, "AT+PIN\r\n");
			read_port_line(hPort, buffer, sizeof(buffer), 0);
			if (strncmp(buffer, "+PIN=", 5) == 0)
				SetWindowTextA(hwndEdit[1], buffer + 5);
		
			// ����ǰ��Ҫ���л�����ģʽ
			i = 10;
			write_port_str(hPort, "AT+ROLE1\r\n");
			read_port_line(hPort, buffer, sizeof(buffer), 0);
			if (strcmp(buffer, "+ROLE=1") == 0)
			{
				read_port_line(hPort, buffer, sizeof(buffer), 0);
				if (strcmp(buffer, "OK") == 0)
				{
					// �ȴ���Ч
					for (i = 0; i < 10; i++)
					{
						write_port_str(hPort, "AT+ROLE\r\n");
						read_port_line(hPort, buffer, sizeof(buffer), 200);
						Sleep(500); // ��ʱ, ��������û��Ӧ (ʱ��Ҳһ��Ҫ��, ��Ȼ�����޷���Ӧ��������)
						if (strcmp(buffer, "+ROLE=1") == 0)
							break;
					}
				}
			}
			if (i == 10)
			{
				lstrcpy(szMessage, TEXT("���ô�����ģʽʧ��"));
				break;
			}
			
			// ��ʼ����
			write_port_str(hPort, "AT+INQ\r\n");
			searching_devices = 1;
			while (read_port_line_cancellable(hPort, buffer, sizeof(buffer), 15000, &searching_devices))
			{
				if (strncmp(buffer, "+INQ:", 5) == 0)
				{
					p = strchr(buffer, ' ');
					if (p == NULL)
						p = strchr(buffer, '=');
					if (p != NULL)
					{
						*p = '\0';
						memset(&device, 0, sizeof(device));
						device.param = atoi(buffer + 5);

						p += 3;
						for (i = 0; i < 6; i++)
						{
							device.addr[3 * i] = p[2 * i];
							device.addr[3 * i + 1] = p[2 * i + 1];
							device.addr[3 * i + 2] = (i == 5) ? '\0' : ':';
						}

						if (p[12] == ' ')
						{
							p += 13;
							q = strchr(p, ' ');
							if (q != NULL)
								*q = '\0';
							device.rssi_valid = TRUE;
							device.rssi = atoi(p);
						}
						add_device(&device);
					}
				}
				else if (strcmp(buffer, "+INQE") == 0)
				{
					read_port_line(hPort, buffer, sizeof(buffer), 250); // �п��ܻ��յ���Devices Found��
					break;
				}
			}
		}
		else
		{
			write_port_str(hPort, "AT+NAME?\r\n");
			read_port_line(hPort, buffer, sizeof(buffer), 0);
			if (strncmp(buffer, "+NAME:", 6) == 0)
			{
				ws = utf8to16(buffer + 6);
				if (ws != NULL)
				{
					SetWindowTextW(hwndEdit[0], ws);
					free(ws);
				}
			}
			read_port_line(hPort, buffer, sizeof(buffer), 0); // ����"OK"��һ��

			write_port_str(hPort, "AT+ADDR?\r\n");
			read_port_line(hPort, buffer, sizeof(buffer), 0);
			if (strncmp(buffer, "+ADDR:", 6) == 0)
			{
				copy_hc05_addr(addr, sizeof(addr), buffer + 6);
				SetDlgItemTextA(hwndDlg, IDC_STATIC4, addr);
			}
			read_port_line(hPort, buffer, sizeof(buffer), 0);

			write_port_str(hPort, "AT+PSWD?\r\n");
			read_port_line(hPort, buffer, sizeof(buffer), 0);
			if (strncmp(buffer, "+PSWD:", 6) == 0)
				SetWindowTextA(hwndEdit[1], buffer + 6);
			read_port_line(hPort, buffer, sizeof(buffer), 0);

			// ��ʼ����
			write_port_str(hPort, "AT+INQ\r\n");
			searching_devices = 1;
			while (read_port_line_cancellable(hPort, buffer, sizeof(buffer), 15000, &searching_devices))
			{
				if (strncmp(buffer, "+INQ:", 5) == 0)
				{
					memset(&device, 0, sizeof(device));
					copy_hc05_addr(device.addr, sizeof(device.addr), buffer + 5);
					p = strchr(buffer, ',');
					if (p != NULL)
					{
						p = strchr(p + 1, ',');
						if (p != NULL)
						{
							p++;
							q = strchr(p, ',');
							if (q != NULL)
							{
								*q = '\0';
								strncpy_s(device.name, sizeof(device.name), q + 1, sizeof(device.name) - 1);
							}
							device.rssi_valid = TRUE;
							sscanf_s(p, "%x", &device.rssi);
							device.rssi = (short)device.rssi;
							add_device(&device);
						}
					}
				}
				else if (strcmp(buffer, "OK") == 0)
					break;
				else if (strncmp(buffer, "ERROR", 5) == 0)
				{
					searching_devices = 0;
					lstrcpy(szMessage, TEXT("�豸����ʧ��"));
					goto end;
				}
			}

			// ֹͣ����
			write_port_str(hPort, "AT+INQC\r\n");
			read_port_str(hPort, buffer, sizeof(buffer));
		}
		if (searching_devices)
		{
			searching_devices = 0;
			lstrcpy(szMessage, TEXT("�豸�������"));
		}
		else
			lstrcpy(szMessage, TEXT("������ȡ��"));
		break;
	case 2:
		/* �����豸 */
		SetDlgItemText(hwndDlg, IDC_STATIC2, TEXT("���������豸..."));
		lvi.mask = LVIF_PARAM | LVIF_TEXT; // ��ȡ�������ı�
		lvi.iItem = ListView_GetNextItem(hwndList, -1, LVNI_SELECTED); // ѡ���е��к�
		lvi.iSubItem = 0;
		if (!ishc05)
		{
			lvi.pszText = addr + 2; // ����ı��Ļ�����
			lvi.cchTextMax = sizeof(addr) - 2; // �������Ĵ�С
			SendMessage(hwndList, LVM_GETITEMA, 0, (LPARAM)&lvi);

			if (lvi.lParam != -1)
			{
				// ������ģʽ�������豸
				addr[0] = '0';
				addr[1] = 'x';
				for (i = 1; i < 6; i++)
				{
					addr[2 * i + 2] = addr[3 * i + 2];
					addr[2 * i + 3] = addr[3 * i + 3];
				}
				addr[14] = '\0';

				_snprintf_s(buffer, sizeof(buffer), sizeof(buffer), "AT+CONN%d\r\n", (int)lvi.lParam);
				write_port_str(hPort, buffer);
				read_port_line(hPort, buffer, sizeof(buffer), 0);
				if (strncmp(buffer, "+Connecting", 11) == 0 && strcmp(buffer + 13, addr) == 0)
				{
					read_port_line(hPort, buffer, sizeof(buffer), 10000);
					if (strncmp(buffer, "+Connected", 10) == 0 && strcmp(buffer + 12, addr) == 0)
					{
						msgtype = 4;
						lstrcpy(szMessage, TEXT("�����豸�ɹ���"));
						break;
					}
				}
				else if (strcmp(buffer, "OK") == 0)
				{
					msgtype = 4;
					lstrcpy(szMessage, TEXT("ģ���ѿ�ʼ�����豸����򿪴��ڲ鿴�Ƿ����ӳɹ���\n���豸���ڸ�����ģ���һֱ�������ӣ�ֱ���豸�����ڸ��������ӳɹ���"));
					break;
				}
			}
			else
			{
				// �ص��ӻ�ģʽ
				write_port_str(hPort, "AT+ROLE0\r\n");
				read_port_line(hPort, buffer, sizeof(buffer), 0);
				if (strcmp(buffer, "+ROLE=0") == 0)
				{
					read_port_line(hPort, buffer, sizeof(buffer), 0);
					if (strcmp(buffer, "OK") == 0)
					{
						msgtype = 4;
						lstrcpy(szMessage, TEXT("������Ϊ�ӻ�ģʽ��"));
						break;
					}
				}

				lstrcpy(szMessage, TEXT("���ô��ڴ�ģʽʧ��"));
				break;
			}
		}
		else
		{
			lvi.pszText = buffer;
			lvi.cchTextMax = sizeof(buffer);
			SendMessage(hwndList, LVM_GETITEMA, 0, (LPARAM)&lvi);

			if (lvi.lParam == 0)
			{
				// ��Ϊ����ģʽ
				n = (int)strlen(buffer);
				if (buffer[n - 1] == ')')
				{
					buffer[n - 1] = '\0';
					p = &buffer[n - 18];
				}
				else
					p = buffer;
				addr[0] = p[0];
				addr[1] = p[1];
				addr[2] = p[3];
				addr[3] = p[4];
				addr[4] = ',';
				addr[5] = p[6];
				addr[6] = p[7];
				addr[7] = ',';
				addr[8] = p[9];
				addr[9] = p[10];
				addr[10] = p[12];
				addr[11] = p[13];
				addr[12] = p[15];
				addr[13] = p[16];
				addr[14] = '\0';

				write_port_str(hPort, "AT+ROLE=1\r\n");
				read_port_line(hPort, buffer, sizeof(buffer), 0);
				if (strcmp(buffer, "OK") == 0)
				{
					write_port_str(hPort, "AT+CMODE=0\r\n");
					read_port_line(hPort, buffer, sizeof(buffer), 0);
					if (strcmp(buffer, "OK") == 0)
					{
						_snprintf_s(buffer, sizeof(buffer), sizeof(buffer), "AT+BIND=%s\r\n", addr);
						write_port_str(hPort, buffer);
						read_port_line(hPort, buffer, sizeof(buffer), 0);
						if (strcmp(buffer, "OK") == 0)
						{
							msgtype = 4;
							lstrcpy(szMessage, TEXT("�����ú��豸��������\n�����ɿ�KEY��������¸�����ģ�������ϵ磬ģ�齫�Զ�������ѡ�豸��ͨ�š�ͨ��ʱ��ʹ��115200�����ʡ�\n����ѡ�豸Ϊ��һ��HC-05����ģ�飬��ô�������ģ�������������Ҫ�͵�ǰ����ģ����ͬ���������ӳɹ���\n����ѡ�豸Ϊ���ԣ�������ģ��û�кʹ˵�����ԣ�������ģ�����ӵ���ʱ�����Ի��Զ���ʾ����������豸��Ҫ������������롣\n\n��ע�⣺HC-05����ģ��͵�����Ժ󣬻��������COM�ڡ�\nһ���Ǵ��루Incoming������һ���Ǵ�����Outgoing����\n��������ģ���Ƿ��ڵ��Ը�����ʼ�ն��ܳɹ��򿪴���COM�ڡ�\n��ֻ�е�����ģ���ڵ��Ը���ʱ�����ܳɹ��򿪴���COM�ڡ�\n������������ģ���������ӵ��ԣ����Ե�����Ӧ�ô򿪴���COM�ڡ�"));
							break;
						}
					}
				}
			}
			else
			{
				// �ص��ӻ�ģʽ
				write_port_str(hPort, "AT+ROLE=0\r\n");
				read_port_line(hPort, buffer, sizeof(buffer), 0);
				if (strcmp(buffer, "OK") == 0)
				{
					msgtype = 4;
					lstrcpy(szMessage, TEXT("������Ϊ�ӻ�ģʽ��\n�����ɿ�KEY��������¸�����ģ�������ϵ磬Ȼ���ڵ����ϴ򿪴���COM�ڣ�������������ģ�顣ͨ��ʱ��ʹ��115200�����ʡ�"));
					break;
				}
			}
		}
		// �����Ӳ���, ��ֻ��Connectingû��Connected, �豸���ܼ�����ӦATָ��
		msgtype = 2;
		lstrcpy(szMessage, TEXT("�����豸ʧ�ܣ�"));
		break;
	case 3:
		/* ������ */
		SetDlgItemText(hwndDlg, IDC_STATIC2, TEXT("�����޸��豸����..."));
		if (!ishc05)
		{
			p = gettext_utf8(hwndEdit[0]);
			if (p != NULL)
			{
				_snprintf_s(buffer, sizeof(buffer), sizeof(buffer), "AT+NAME%s\r\n", p);
				free(p);

				write_port_str(hPort, buffer);
				read_port_line(hPort, buffer, sizeof(buffer), 0);
				if (strncmp(buffer, "+NAME=", 6) == 0)
					read_port_line(hPort, buffer, sizeof(buffer), 0);
			}

			if (p == NULL || strcmp(buffer, "OK") != 0)
			{
				lstrcpy(szMessage, TEXT("�豸�����޸�ʧ��"));
				break;
			}

			// ��λ����Ч
			write_port_str(hPort, "AT+RESET\r\n");
			read_port_line(hPort, buffer, sizeof(buffer), 0);
			if (strcmp(buffer, "+RESET") == 0)
				read_port_line(hPort, buffer, sizeof(buffer), 0);
			if (strcmp(buffer, "OK") != 0)
			{
				msgtype = 3;
				lstrcpy(szMessage, TEXT("�豸�����޸ĳɹ�������Ҫ����ģ��������Ч��"));
				break;
			}
		}
		else
		{
			p = gettext_utf8(hwndEdit[0]);
			if (p != NULL)
			{
				_snprintf_s(buffer, sizeof(buffer), sizeof(buffer), "AT+NAME=%s\r\n", p);
				free(p);

				write_port_str(hPort, buffer);
				read_port_line(hPort, buffer, sizeof(buffer), 0);
			}

			if (p == NULL || strcmp(buffer, "OK") != 0)
			{
				lstrcpy(szMessage, TEXT("�豸�����޸�ʧ��"));
				break;
			}

			// HC-05����Ҫ��λ, ����������Ч
		}
		lstrcpy(szMessage, TEXT("�豸�����޸ĳɹ�"));
		break;
	case 4:
		/* �������� */
		SetDlgItemText(hwndDlg, IDC_STATIC2, TEXT("�����޸��������..."));
		if (!ishc05)
		{
			p = gettext_utf8(hwndEdit[1]);
			if (p != NULL)
			{
				_snprintf_s(buffer, sizeof(buffer), sizeof(buffer), "AT+PIN%s\r\n", p);
				free(p);

				write_port_str(hPort, buffer);
				read_port_line(hPort, buffer, sizeof(buffer), 0);
				if (strncmp(buffer, "+PIN=", 5) == 0)
					read_port_line(hPort, buffer, sizeof(buffer), 0);
			}

			if (p == NULL || strcmp(buffer, "OK") != 0)
			{
				lstrcpy(szMessage, TEXT("�豸��������޸�ʧ��"));
				break;
			}

			// ��λ����Ч
			write_port_str(hPort, "AT+RESET\r\n");
			read_port_line(hPort, buffer, sizeof(buffer), 0);
			if (strcmp(buffer, "+RESET") == 0)
				read_port_line(hPort, buffer, sizeof(buffer), 0);
			if (strcmp(buffer, "OK") != 0)
			{
				msgtype = 3;
				lstrcpy(szMessage, TEXT("�豸��������޸ĳɹ�������Ҫ����ģ��������Ч��"));
				break;
			}
		}
		else
		{
			p = gettext_utf8(hwndEdit[1]);
			if (p != NULL)
			{
				_snprintf_s(buffer, sizeof(buffer), sizeof(buffer), "AT+PSWD=%s\r\n", p);
				free(p);

				write_port_str(hPort, buffer);
				read_port_line(hPort, buffer, sizeof(buffer), 0);
			}

			if (p == NULL || strcmp(buffer, "OK") != 0)
			{
				lstrcpy(szMessage, TEXT("�豸��������޸�ʧ��"));
				break;
			}
		}
		lstrcpy(szMessage, TEXT("�豸��������޸ĳɹ�"));
		break;
	}

end:
	close_port(hPort);
	PostMessage(hwndDlg, WM_USER, opcode, msgtype);
	return 0;
}

static void copy_hc05_addr(char *dest, int destsize, const char *src)
{
	int values[3] = {0};

	sscanf_s(src, "%x:%x:%x", &values[0], &values[1], &values[2]);
	_snprintf_s(dest, destsize, destsize, "%02X:%02X:%02X:%02X:%02X:%02X",
		(values[0] >> 8) & 0xff, values[0] & 0xff,
		values[1] & 0xff,
		(values[2] >> 16) & 0xff, (values[2] >> 8) & 0xff, values[2] & 0xff
	);
}

static void dlg_init(void)
{
	int cx, cy;
	HICON hIcon;
	LVCOLUMN lvc;

	SetDlgItemText(hwndDlg, IDC_STATIC2, TEXT(""));
	SetDlgItemText(hwndDlg, IDC_STATIC4, TEXT(""));

	hwndButton[0] = GetDlgItem(hwndDlg, IDOK);
	hwndButton[1] = GetDlgItem(hwndDlg, IDC_BUTTON1);
	hwndButton[2] = GetDlgItem(hwndDlg, IDC_BUTTON2);
	hwndButton[3] = GetDlgItem(hwndDlg, IDC_BUTTON3);
	hwndEdit[0] = GetDlgItem(hwndDlg, IDC_EDIT1);
	hwndEdit[1] = GetDlgItem(hwndDlg, IDC_EDIT2);

	hwndList = GetDlgItem(hwndDlg, IDC_LIST1);
	ListView_SetExtendedListViewStyle(hwndList, LVS_EX_DOUBLEBUFFER);
	
	// ����Vista����б�ؼ�
	// ��ע��: ���ô˺�����������ɹ���, һ��Ҫ��ӦListView��NM_KILLFOCUS��Ϣ, ����InvalidateRect�ػ�ؼ�
	// ��Ȼ����ʧȥ�����, ѡ������ֲ����ǻ�ɫ��, ��ͼ�겿����Ȼ����ɫ��
	SetWindowTheme(hwndList, L"Explorer", NULL);

	cx = GetSystemMetrics(SM_CXSMICON);
	cy = GetSystemMetrics(SM_CYSMICON);
	hImageList = ImageList_Create(cx, cy, ILC_COLOR32, 1, 1);
	hIcon = LoadImage(hinstMain, MAKEINTRESOURCE(IDI_ICON2), IMAGE_ICON, cx, cy, LR_DEFAULTCOLOR);
	ImageList_AddIcon(hImageList, hIcon);
	ListView_SetImageList(hwndList, hImageList, LVSIL_SMALL);

	lvc.mask = LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 250;
	lvc.pszText = TEXT("�豸��ַ");
	ListView_InsertColumn(hwndList, 0, &lvc);
	lvc.cx = 100;
	lvc.pszText = TEXT("�ź�ǿ��");
	ListView_InsertColumn(hwndList, 1, &lvc);
	add_device(NULL);
}

static void enable_controls(BOOL enabled)
{
	EnableWindow(hwndButton[0], enabled);
	EnableWindow(hwndButton[1], enabled);
	EnableWindow(hwndButton[2], enabled);
	EnableWindow(hwndButton[3], (enabled && ListView_GetSelectedCount(hwndList) != 0));
	Edit_SetReadOnly(hwndEdit[0], !enabled);
	Edit_SetReadOnly(hwndEdit[1], !enabled);
}

static void start_bluetooth_process(uintptr_t opcode)
{
	if (!thread_active)
	{
		thread_active = 1;
		szMessage[0] = '\0';
		enable_controls(FALSE);
		if (opcode == 1)
		{
			ListView_DeleteAllItems(hwndList);
			add_device(NULL);
		}
		hThread = CreateThread(NULL, 0, bluetooth_process, (LPVOID)opcode, 0, NULL);
	}
}

INT_PTR CALLBACK Bluetooth_DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int wmId;
	LPCTSTR title = NULL;
	LPNMHDR lpnmhdr;
	LPNMITEMACTIVATE lpnmia;

	switch (uMsg)
	{
	case WM_COMMAND:
		wmId = LOWORD(wParam);
		switch (wmId)
		{
		case IDOK:
			start_bluetooth_process(1);
			break;
		case IDCANCEL:
			if (thread_active)
			{
				// ����ֹͣ�߳�����ĺ�ʱ����
				searching_devices = 0;
				testing_baudrate = 0;
			}
			else
				EndDialog(hDlg, 0);
			break;
		case IDC_BUTTON1:
			if (Edit_GetTextLength(hwndEdit[0]) != 0)
				start_bluetooth_process(3);
			else
			{
				MessageBox(hDlg, TEXT("�豸���Ʋ���Ϊ�գ�"), TEXT("�������豸"), MB_ICONWARNING);
				SetFocus(hwndEdit[0]);
			}
			break;
		case IDC_BUTTON2:
			if (Edit_GetTextLength(hwndEdit[1]) != 0)
				start_bluetooth_process(4);
			else
			{
				MessageBox(hDlg, TEXT("�豸������벻��Ϊ�գ�"), TEXT("��������"), MB_ICONWARNING);
				SetFocus(hwndEdit[1]);
			}
			break;
		case IDC_BUTTON3:
			if (ListView_GetSelectedCount(hwndList) != 0)
				start_bluetooth_process(2);
			break;
		}
		break;
	case WM_INITDIALOG:
		hwndDlg = hDlg;
		dlg_init();
		break;
	case WM_NOTIFY:
		lpnmhdr = (LPNMHDR)lParam;
		if (lpnmhdr->idFrom == IDC_LIST1)
		{
			switch (lpnmhdr->code)
			{
			case LVN_ITEMACTIVATE:
			case NM_CLICK:
			case NM_RCLICK:
				lpnmia = (LPNMITEMACTIVATE)lpnmhdr;
				if (lpnmia->iItem != -1 && !thread_active)
				{
					EnableWindow(hwndButton[3], TRUE);

					// ��˫������Ŀ, ��ʼ����
					if (lpnmhdr->code == LVN_ITEMACTIVATE)
						start_bluetooth_process(2);
				}
				else
					EnableWindow(hwndButton[3], FALSE);
				break;
			case LVN_KEYDOWN:
				PostMessage(hDlg, WM_USER, 100, 0);
				break;
			case NM_KILLFOCUS:
				InvalidateRect(lpnmhdr->hwndFrom, NULL, FALSE);
				break;
			}
		}
		break;
	case WM_USER:
		if (wParam >= 1 && wParam <= 4)
		{
			switch (wParam)
			{
			case 1:
				title = TEXT("�����豸");
				break;
			case 2:
				title = TEXT("�����豸");
				break;
			case 3:
				title = TEXT("�������豸");
				break;
			case 4:
				title = TEXT("��������");
				break;
			}

			switch (lParam)
			{
			case 1:
				// �ı���ʾ����
				SetDlgItemText(hDlg, IDC_STATIC2, szMessage);
				break;
			case 2:
				// �������洰��
				MessageBox(hDlg, szMessage, title, MB_ICONWARNING);
				SetDlgItemText(hDlg, IDC_STATIC2, TEXT(""));
				break;
			case 3:
				// ������Ϣ����
				MessageBox(hDlg, szMessage, title, MB_ICONINFORMATION);
				SetDlgItemText(hDlg, IDC_STATIC2, TEXT(""));
				break;
			case 4:
				// ������Ϣ���ڲ��رնԻ���
				MessageBox(hDlg, szMessage, title, MB_ICONINFORMATION);
				EndDialog(hDlg, 0);
				break;
			}
			thread_active = 0;
			enable_controls(TRUE);
		}
		else if (wParam == 100)
			EnableWindow(hwndButton[3], ListView_GetSelectedCount(hwndList) != 0 && !thread_active);
		break;
	}
	return 0;
}

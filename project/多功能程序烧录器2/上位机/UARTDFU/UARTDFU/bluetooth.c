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

	lvi.mask = LVIF_IMAGE | LVIF_PARAM | LVIF_TEXT; // 设置图标、自定义参数和文本
	if (device != NULL)
	{
		if (device->name[0] == '\0')
			_snprintf_s(buf, sizeof(buf), sizeof(buf), "%s", device->addr); // 只显示蓝牙地址
		else
			_snprintf_s(buf, sizeof(buf), sizeof(buf), "%s (%s)", device->name, device->addr); // 显示蓝牙名称和地址
		utf8to16_wbuf(buf, wbuf, sizeof(wbuf)); // UTF8转UTF16
		lvi.iImage = 0; // 蓝牙图标
	}
	else
	{
		wcscpy_s(wbuf, _countof(wbuf), L"空设备 (用于回到从机模式)");
		lvi.iImage = 1; // 无图标
	}
	lvi.iItem = 0; // 暂时先插入到开头, 等待自动排序 (控件已启用自动排序)
	lvi.iSubItem = 0; // 第一列
	lvi.lParam = (device != NULL) ? device->param : -1; // 设备序号 (连接时使用)
	lvi.pszText = wbuf;
	lvi.iItem = (int)SendMessage(hwndList, LVM_INSERTITEMW, 0, (LPARAM)&lvi); // 返回值为排序后的行号

	if (device != NULL && device->rssi_valid)
	{
		_snwprintf_s(wbuf, _countof(wbuf), _countof(wbuf), L"%d", device->rssi);
		lvi.mask = LVIF_TEXT; // 只设置文本
		lvi.iSubItem = 1; // 第二列
		lvi.pszText = wbuf;
		SendMessage(hwndList, LVM_SETITEMW, 0, (LPARAM)&lvi);
	}
	return lvi.iItem;
}

static DWORD CALLBACK bluetooth_process(LPVOID lpParameter)
{
	const int baudrate_list[] = {38400, 115200, 57600, 19200, 9600, 4800, 2400, 0}; // HC-05蓝牙模块AT模式下的波特率固定为38400, 所以要放第一个
	char addr[20];
	char buffer[200];
	char *p, *q;
	int baudrate[3] = {0}; // 三个下标分别表示波特率、停止位和校验位
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

	SetDlgItemText(hwndDlg, IDC_STATIC2, TEXT("正在打开串口..."));
	hPort = open_port(-1); // 打开串口, 若出错则会将错误消息填入szMessage
	if (hPort == INVALID_HANDLE_VALUE)
	{
		msgtype = 2;
		goto end;
	}
	dcb.DCBlength = sizeof(DCB);
	GetCommState(hPort, &dcb);

	/* 波特率匹配 */
	SetDlgItemText(hwndDlg, IDC_STATIC2, TEXT("正在匹配波特率..."));
	timeouts.ReadTotalTimeoutConstant = 500;
	timeouts.WriteTotalTimeoutConstant = 1000; // 蓝牙串口必须设置发送超时时间, 避免因蓝牙设备不存在而无限等待
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
				lstrcpy(szMessage, TEXT("操作已取消"));
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
		// 找不到合适的波特率, 或者已有主机连上使得AT指令不可用
		msgtype = 2;
		lstrcpy(szMessage, TEXT("蓝牙串口不能响应AT指令！\n若使用的是HC-05模块，必须在按住KEY按键的情况下通电，否则无法进入AT模式。\n波特率多次匹配失败后，HC-05模块有可能死机，可尝试模块重新上电。\n若使用的是CC2541模块，则可能是因为蓝牙已连接上设备。\n若想要更换设备连接，请拔下蓝牙串口设备再重新插上。"));
		goto end;
	}
	else if (baudrate[0] == 38400)
	{
		// 判断是否为HC-05模块
		read_port_str(hPort, buffer, sizeof(buffer)); // 吸收掉所有未读完的数据
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

	/* 配置波特率 */
	if ((!ishc05 && baudrate[0] != 115200) || (ishc05 && !hc05_baudok))
	{
		if (!ishc05)
			_sntprintf_s(szMessage, _countof(szMessage), _countof(szMessage), TEXT("当前蓝牙串口的波特率为%d。\n是否需要修改为115200？"), baudrate[0]);
		else
			_sntprintf_s(szMessage, _countof(szMessage), _countof(szMessage), TEXT("当前蓝牙串口的通信模式的波特率配置为“%d,%d,%d”。\n是否需要修改为“115200,0,0”？\n\n注意: HC-05模块的AT模式波特率固定为38400。\n这里指的是通信模式下，也就是不按住KEY按键给模块上电时的波特率。"), baudrate[0], baudrate[1], baudrate[2]);

		if (MessageBox(hwndDlg, szMessage, TEXT("蓝牙串口"), MB_ICONQUESTION | MB_OKCANCEL) == IDOK)
			szMessage[0] = '\0';
		else
		{
			lstrcpy(szMessage, TEXT("串口波特率不正确"));
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
				lstrcpy(szMessage, TEXT("配置串口波特率失败"));
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
				lstrcpy(szMessage, TEXT("串口波特率配置成功但没有生效"));
				goto end;
			}
		}
		else
		{
			write_port_str(hPort, "AT+UART=115200,0,0\r\n");
			read_port_line(hPort, buffer, sizeof(buffer), 0);
			if (strcmp(buffer, "OK") != 0)
			{
				lstrcpy(szMessage, TEXT("配置串口波特率失败"));
				goto end;
			}
		}
	}

	/* 执行命令 */
	timeouts.ReadTotalTimeoutConstant = (!ishc05) ? 25 : 300;
	SetCommTimeouts(hPort, &timeouts);
	switch (opcode)
	{
	case 1:
		/* 搜索设备 */
		SetDlgItemText(hwndDlg, IDC_STATIC2, TEXT("正在搜索设备..."));
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
					// 等待生效
					for (i = 0; i < 10; i++)
					{
						write_port_str(hPort, "AT+ROLE\r\n");
						read_port_line(hPort, buffer, sizeof(buffer), 200);
						Sleep(500); // 延时, 以免命令没反应 (时间一定要够, 不然后面获取蓝牙名称会为空)
						if (strcmp(buffer, "+ROLE=0") == 0)
							break;
					}
				}
			}
			if (i == 10)
			{
				lstrcpy(szMessage, TEXT("配置串口从模式失败"));
				break;
			}

			// 经测试, 某些CC2541蓝牙模块只有在从模式下才能获取到蓝牙名称和地址
			write_port_str(hPort, "AT+NAME\r\n");
			read_port_line(hPort, buffer, sizeof(buffer), 200); // 最后一个参数不能是0, 以免下一行的命令字符串被设置成设备的蓝牙名称, 结果蓝牙名称以\r\n开头
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
		
			// 搜索前需要先切换到主模式
			i = 10;
			write_port_str(hPort, "AT+ROLE1\r\n");
			read_port_line(hPort, buffer, sizeof(buffer), 0);
			if (strcmp(buffer, "+ROLE=1") == 0)
			{
				read_port_line(hPort, buffer, sizeof(buffer), 0);
				if (strcmp(buffer, "OK") == 0)
				{
					// 等待生效
					for (i = 0; i < 10; i++)
					{
						write_port_str(hPort, "AT+ROLE\r\n");
						read_port_line(hPort, buffer, sizeof(buffer), 200);
						Sleep(500); // 延时, 以免命令没反应 (时间也一定要够, 不然后面无法响应搜索命令)
						if (strcmp(buffer, "+ROLE=1") == 0)
							break;
					}
				}
			}
			if (i == 10)
			{
				lstrcpy(szMessage, TEXT("配置串口主模式失败"));
				break;
			}
			
			// 开始搜索
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
					read_port_line(hPort, buffer, sizeof(buffer), 250); // 有可能会收到“Devices Found”
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
			read_port_line(hPort, buffer, sizeof(buffer), 0); // 接收"OK"这一行

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

			// 开始搜索
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
					lstrcpy(szMessage, TEXT("设备搜索失败"));
					goto end;
				}
			}

			// 停止搜索
			write_port_str(hPort, "AT+INQC\r\n");
			read_port_str(hPort, buffer, sizeof(buffer));
		}
		if (searching_devices)
		{
			searching_devices = 0;
			lstrcpy(szMessage, TEXT("设备搜索完毕"));
		}
		else
			lstrcpy(szMessage, TEXT("操作已取消"));
		break;
	case 2:
		/* 连接设备 */
		SetDlgItemText(hwndDlg, IDC_STATIC2, TEXT("正在连接设备..."));
		lvi.mask = LVIF_PARAM | LVIF_TEXT; // 获取参数和文本
		lvi.iItem = ListView_GetNextItem(hwndList, -1, LVNI_SELECTED); // 选中行的行号
		lvi.iSubItem = 0;
		if (!ishc05)
		{
			lvi.pszText = addr + 2; // 存放文本的缓冲区
			lvi.cchTextMax = sizeof(addr) - 2; // 缓冲区的大小
			SendMessage(hwndList, LVM_GETITEMA, 0, (LPARAM)&lvi);

			if (lvi.lParam != -1)
			{
				// 在主机模式下连接设备
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
						lstrcpy(szMessage, TEXT("连接设备成功！"));
						break;
					}
				}
				else if (strcmp(buffer, "OK") == 0)
				{
					msgtype = 4;
					lstrcpy(szMessage, TEXT("模块已开始连接设备，请打开串口查看是否连接成功。\n若设备不在附近，模块会一直尝试连接，直到设备出现在附近并连接成功。"));
					break;
				}
			}
			else
			{
				// 回到从机模式
				write_port_str(hPort, "AT+ROLE0\r\n");
				read_port_line(hPort, buffer, sizeof(buffer), 0);
				if (strcmp(buffer, "+ROLE=0") == 0)
				{
					read_port_line(hPort, buffer, sizeof(buffer), 0);
					if (strcmp(buffer, "OK") == 0)
					{
						msgtype = 4;
						lstrcpy(szMessage, TEXT("已配置为从机模式。"));
						break;
					}
				}

				lstrcpy(szMessage, TEXT("配置串口从模式失败"));
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
				// 设为主机模式
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
							lstrcpy(szMessage, TEXT("已配置好设备连接请求。\n请在松开KEY键的情况下给蓝牙模块重新上电，模块将自动连接所选设备并通信。通信时请使用115200波特率。\n若所选设备为另一个HC-05蓝牙模块，那么这个蓝牙模块的配对密码必须要和当前蓝牙模块相同，才能连接成功。\n若所选设备为电脑，且蓝牙模块没有和此电脑配对，则蓝牙模块连接电脑时，电脑会自动提示配对新蓝牙设备并要求输入配对密码。\n\n请注意：HC-05蓝牙模块和电脑配对后，会产生两个COM口。\n一个是传入（Incoming），另一个是传出（Outgoing）。\n无论蓝牙模块是否在电脑附近，始终都能成功打开传入COM口。\n但只有当蓝牙模块在电脑附近时，才能成功打开传出COM口。\n由于这是蓝牙模块主动连接电脑，所以电脑上应该打开传入COM口。"));
							break;
						}
					}
				}
			}
			else
			{
				// 回到从机模式
				write_port_str(hPort, "AT+ROLE=0\r\n");
				read_port_line(hPort, buffer, sizeof(buffer), 0);
				if (strcmp(buffer, "OK") == 0)
				{
					msgtype = 4;
					lstrcpy(szMessage, TEXT("已配置为从机模式。\n请在松开KEY键的情况下给蓝牙模块重新上电，然后在电脑上打开传出COM口，即可连接蓝牙模块。通信时请使用115200波特率。"));
					break;
				}
			}
		}
		// 若连接不上, 则只有Connecting没有Connected, 设备还能继续响应AT指令
		msgtype = 2;
		lstrcpy(szMessage, TEXT("连接设备失败！"));
		break;
	case 3:
		/* 重命名 */
		SetDlgItemText(hwndDlg, IDC_STATIC2, TEXT("正在修改设备名称..."));
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
				lstrcpy(szMessage, TEXT("设备名称修改失败"));
				break;
			}

			// 复位后生效
			write_port_str(hPort, "AT+RESET\r\n");
			read_port_line(hPort, buffer, sizeof(buffer), 0);
			if (strcmp(buffer, "+RESET") == 0)
				read_port_line(hPort, buffer, sizeof(buffer), 0);
			if (strcmp(buffer, "OK") != 0)
			{
				msgtype = 3;
				lstrcpy(szMessage, TEXT("设备名称修改成功，但需要重启模块后才能生效。"));
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
				lstrcpy(szMessage, TEXT("设备名称修改失败"));
				break;
			}

			// HC-05不需要复位, 立即就能生效
		}
		lstrcpy(szMessage, TEXT("设备名称修改成功"));
		break;
	case 4:
		/* 设置密码 */
		SetDlgItemText(hwndDlg, IDC_STATIC2, TEXT("正在修改配对密码..."));
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
				lstrcpy(szMessage, TEXT("设备配对密码修改失败"));
				break;
			}

			// 复位后生效
			write_port_str(hPort, "AT+RESET\r\n");
			read_port_line(hPort, buffer, sizeof(buffer), 0);
			if (strcmp(buffer, "+RESET") == 0)
				read_port_line(hPort, buffer, sizeof(buffer), 0);
			if (strcmp(buffer, "OK") != 0)
			{
				msgtype = 3;
				lstrcpy(szMessage, TEXT("设备配对密码修改成功，但需要重启模块后才能生效。"));
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
				lstrcpy(szMessage, TEXT("设备配对密码修改失败"));
				break;
			}
		}
		lstrcpy(szMessage, TEXT("设备配对密码修改成功"));
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
	
	// 启用Vista风格列表控件
	// 请注意: 调用此函数设置主题成功后, 一定要响应ListView的NM_KILLFOCUS消息, 调用InvalidateRect重绘控件
	// 不然窗口失去焦点后, 选择框文字部分是灰色的, 但图标部分仍然是蓝色的
	SetWindowTheme(hwndList, L"Explorer", NULL);

	cx = GetSystemMetrics(SM_CXSMICON);
	cy = GetSystemMetrics(SM_CYSMICON);
	hImageList = ImageList_Create(cx, cy, ILC_COLOR32, 1, 1);
	hIcon = LoadImage(hinstMain, MAKEINTRESOURCE(IDI_ICON2), IMAGE_ICON, cx, cy, LR_DEFAULTCOLOR);
	ImageList_AddIcon(hImageList, hIcon);
	ListView_SetImageList(hwndList, hImageList, LVSIL_SMALL);

	lvc.mask = LVCF_TEXT | LVCF_WIDTH;
	lvc.cx = 250;
	lvc.pszText = TEXT("设备地址");
	ListView_InsertColumn(hwndList, 0, &lvc);
	lvc.cx = 100;
	lvc.pszText = TEXT("信号强度");
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
				// 请求停止线程里面的耗时操作
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
				MessageBox(hDlg, TEXT("设备名称不能为空！"), TEXT("重命名设备"), MB_ICONWARNING);
				SetFocus(hwndEdit[0]);
			}
			break;
		case IDC_BUTTON2:
			if (Edit_GetTextLength(hwndEdit[1]) != 0)
				start_bluetooth_process(4);
			else
			{
				MessageBox(hDlg, TEXT("设备配对密码不能为空！"), TEXT("设置密码"), MB_ICONWARNING);
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

					// 若双击了项目, 则开始连接
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
				title = TEXT("搜索设备");
				break;
			case 2:
				title = TEXT("连接设备");
				break;
			case 3:
				title = TEXT("重命名设备");
				break;
			case 4:
				title = TEXT("设置密码");
				break;
			}

			switch (lParam)
			{
			case 1:
				// 改变提示文字
				SetDlgItemText(hDlg, IDC_STATIC2, szMessage);
				break;
			case 2:
				// 弹出警告窗口
				MessageBox(hDlg, szMessage, title, MB_ICONWARNING);
				SetDlgItemText(hDlg, IDC_STATIC2, TEXT(""));
				break;
			case 3:
				// 弹出信息窗口
				MessageBox(hDlg, szMessage, title, MB_ICONINFORMATION);
				SetDlgItemText(hDlg, IDC_STATIC2, TEXT(""));
				break;
			case 4:
				// 弹出信息窗口并关闭对话框
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

#pragma once

typedef struct
{
	char name[50];
	char addr[18];
	BOOL rssi_valid;
	int rssi;
	LPARAM param;
} BluetoothDevice;

INT_PTR CALLBACK Bluetooth_DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

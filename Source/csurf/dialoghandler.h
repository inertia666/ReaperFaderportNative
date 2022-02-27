#include "WDL/ptrlist.h"
#include "csurf.h"

#define CONFIG_FLAG_IS_FADERPORT8 1

#define DOUBLE_CLICK_INTERVAL 250 /* ms */

#ifndef _DIALOGHANDLER_H_
#define _DIALOGHANDLER_H_


static void parseParms(const char* str, int parms[2]);

static WDL_DLGRET dlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
	{
		int parms[2];
		parseParms((const char*)lParam, parms);

		int n = GetNumMIDIInputs();
		int x = SendDlgItemMessage(hwndDlg, IDC_COMBO2, CB_ADDSTRING, 0, (LPARAM)"None");
		SendDlgItemMessage(hwndDlg, IDC_COMBO2, CB_SETITEMDATA, x, -1);
		x = SendDlgItemMessage(hwndDlg, IDC_COMBO3, CB_ADDSTRING, 0, (LPARAM)"None");
		SendDlgItemMessage(hwndDlg, IDC_COMBO3, CB_SETITEMDATA, x, -1);
		for (x = 0; x < n; x++)
		{
			char buf[512];
			if (GetMIDIInputName(x, buf, sizeof(buf)))
			{
				int a = SendDlgItemMessage(hwndDlg, IDC_COMBO2, CB_ADDSTRING, 0, (LPARAM)buf);
				SendDlgItemMessage(hwndDlg, IDC_COMBO2, CB_SETITEMDATA, a, x);
				if (x == parms[0]) SendDlgItemMessage(hwndDlg, IDC_COMBO2, CB_SETCURSEL, a, 0);
			}
		}
		n = GetNumMIDIOutputs();
		for (x = 0; x < n; x++)
		{
			char buf[512];
			if (GetMIDIOutputName(x, buf, sizeof(buf)))
			{
				int a = SendDlgItemMessage(hwndDlg, IDC_COMBO3, CB_ADDSTRING, 0, (LPARAM)buf);
				SendDlgItemMessage(hwndDlg, IDC_COMBO3, CB_SETITEMDATA, a, x);
				if (x == parms[1]) SendDlgItemMessage(hwndDlg, IDC_COMBO3, CB_SETCURSEL, a, 0);
			}
		}
		//SetDlgItemInt(hwndDlg, IDC_EDIT1, parms[0], FALSE);
		//if (parms[2] & CONFIG_FLAG_IS_FADERPORT8)
		//	CheckDlgButton(hwndDlg, IDC_CHECK1, BST_CHECKED);

	}
	break;
	case WM_USER + 1024:
		if (wParam > 1 && lParam)
		{
			char tmp[512];

			int indev = -1, outdev = -1, offs = 0, size = 9;
			int r = SendDlgItemMessage(hwndDlg, IDC_COMBO2, CB_GETCURSEL, 0, 0);
			if (r != CB_ERR) indev = SendDlgItemMessage(hwndDlg, IDC_COMBO2, CB_GETITEMDATA, r, 0);
			r = SendDlgItemMessage(hwndDlg, IDC_COMBO3, CB_GETCURSEL, 0, 0);
			if (r != CB_ERR)  outdev = SendDlgItemMessage(hwndDlg, IDC_COMBO3, CB_GETITEMDATA, r, 0);

			BOOL t;
			r = GetDlgItemInt(hwndDlg, IDC_EDIT1, &t, TRUE);
			if (t) offs = r;
			r = GetDlgItemInt(hwndDlg, IDC_EDIT2, &t, FALSE);
			if (t)
			{
				if (r < 1)r = 1;
				else if (r > 256)r = 256;
				size = r;
			}

			sprintf(tmp, "%d %d", indev, outdev);
			lstrcpyn((char*)lParam, tmp, wParam);
		}
		break;
	}
	return 0;
}

inline static void parseParms(const char* str, int parms[2])
{
	parms[0] = 0;
	parms[1] = 9;

	const char* p = str;
	if (p)
	{
		int x = 0;
		while (x < 2)
		{
			while (*p == ' ') p++;
			if ((*p < '0' || *p > '9') && *p != '-') break;
			parms[x++] = atoi(p);
			while (*p && *p != ' ') p++;
		}
	}
}


#endif
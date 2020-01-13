// https://docs.microsoft.com/zh-cn/archive/blogs/calvin_hsia/detouring-code
// DetourSample.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "DetourSample.h"
#include <string>
/*
Start Visual Studio
File->New->Project->C++ Desktop Application. Call it "DetourSample"
Replace the entire DetourSample.cpp with this code.

From https://github.com/Microsoft/Detours copy 4 files: detours.h, detours.cpp, disasm.cpp, Modules.cpp next to this file
Include the 3 .cpp files in the project (Solution Explorer->Show all files->Right click->Include in Project)
Need to turn off precompiled headers for all configurations (both debug/release, 32 bit, 64 bit). Project->Properties->Configuration->C/C++->PrecompiledHeaders->Not using PrecompiledHeaders
Many C++ projects consist of lots of C++ files, which have many #include at the top. Each #include file needs to be expanded and compiled for each C++ file.
The PrecompiledHeaders option specifies an included file name like "windows.h". Every C++ file is assumed to be identical up to the line that includes "windows.h"
This allows that identical part to be precompiled into a .pch file on disk, which is only needed to be done once for all the C++ files.
Works in 64 bit and 32 bit: Build->Configuration->Active Solution Platform->x86 or x64


*/
#ifdef _WIN64
char g_szArch[] = "x64 64 bit";
#define _X86_
#else
char g_szArch[] = "x86 32 bit";
#endif
#include "detours.h"

using namespace std;

#define MAX_LOADSTRING 100

class CMyApplication;
CMyApplication * g_CMyApplication;

// Forward declarations of functions included in this code module:
LRESULT CALLBACK WindowProcedure(HWND, UINT, WPARAM, LPARAM);

/*
Many windows API functions that require a string parameter have 2 versions: an A (for ANSI) and W (for Wide Unicode) strings.
For example, MessageBoxA and MessageBoxW.
If you just call MessageBox, the relevant header (WinUser.h) contains:
#ifdef UNICODE
#define MessageBox  MessageBoxW
#else
#define MessageBox  MessageBoxA
#endif // !UNICODE

Detouring the W version might detour both the W and the A version because the A version might just convert to W and call the W version.
That's not true with MessageBoxA and MessageBoxW. MessageBoxA on Win10 converts to unicode and then calls (undocumented) MessageBoxTimeOutW
We can look at the exports of user32.dll from a VS command prompt:
C:\>link /dump /exports \Windows\System32\user32.dll | find /i "messagebo"
2149  282 0006F000 MessageBoxA
2150  283 0006F060 MessageBoxExA
2151  284 0006F090 MessageBoxExW
2152  285 0006F0C0 MessageBoxIndirectA
2153  286 0006F260 MessageBoxIndirectW
2154  287 0006F320 MessageBoxTimeoutA
2155  288 0006F470 MessageBoxTimeoutW
2156  289 0006F640 MessageBoxW
2408  387 0006F6A0 SoftModalMessageBox
*/

decltype(&CreateWindowExW) g_real_CreateWindowExW = &CreateWindowExW;

HWND
WINAPI
MyCreateWindowExW(
	_In_ DWORD dwExStyle,
	_In_opt_ LPCWSTR lpClassName,
	_In_opt_ LPCWSTR lpWindowName,
	_In_ DWORD dwStyle,
	_In_ int X,
	_In_ int Y,
	_In_ int nWidth,
	_In_ int nHeight,
	_In_opt_ HWND hWndParent,
	_In_opt_ HMENU hMenu,
	_In_opt_ HINSTANCE hInstance,
	_In_opt_ LPVOID lpParam)
{
	wstring winName(lpWindowName);
	winName += L" detour";
	return g_real_CreateWindowExW(dwExStyle, lpClassName, winName.c_str(), dwStyle, X, Y,
		nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
}

decltype(&ShowWindow) g_real_ShowWindow = &ShowWindow;

BOOL
WINAPI
MyShowWindow(
	_In_ HWND hWnd,
	_In_ int nCmdShow)
{
	return g_real_ShowWindow(hWnd, nCmdShow);
}

// decltype is like typeof()
// initialize a pointer to the real version of the API
decltype(&MessageBoxA) g_real_MessageBoxA = &MessageBoxA;
decltype(&MessageBoxW) g_real_MessageBoxW = &MessageBoxW;

// here is my implementation that will be called when detoured.
int WINAPI MyMessageBoxA(
	_In_opt_ HWND hWnd,
	_In_opt_ LPCSTR lpText,
	_In_opt_ LPCSTR lpCaption,
	_In_ UINT uType)
{
	string caption(lpCaption);
	caption += " The detoured version of MessageboxA";
	string text(lpText);
	text += " hi from detoured version";
	// we now call the real implementation of MessageBox.
	// (we could log various things about detoured API calls, such as
	//   the callstack and the return value)
	return g_real_MessageBoxA(hWnd, text.c_str(), caption.c_str(), uType);
}

int WINAPI MyMessageBoxW(
	_In_opt_ HWND hWnd,
	_In_opt_ LPCWSTR lpText,
	_In_opt_ LPCWSTR lpCaption,
	_In_ UINT uType)
{
	wstring caption(lpCaption);
	caption += L" The unicode detoured version of MessageboxA";
	wstring text(lpText);
	text += L" hi from unicode detoured version";
	return g_real_MessageBoxW(hWnd, text.c_str(), caption.c_str(), uType);
}


class CMyApplication
{
	WCHAR _szTitle[MAX_LOADSTRING];                  // The title bar text
	WCHAR _szWindowClass[MAX_LOADSTRING];            // the main window class name
	HINSTANCE _hInstance;                                // current instance
	HWND _hWnd;
	HWND _hwndBtnMsgBox;
	HWND _hwndChkDetour;
	POINTS _size;
public:
	CMyApplication(HINSTANCE hInstance)
	{
		_hInstance = hInstance;
		// Initialize global strings
		LoadStringW(_hInstance, IDS_APP_TITLE, _szTitle, MAX_LOADSTRING);
		LoadStringW(_hInstance, IDC_DETOURSAMPLE, _szWindowClass, MAX_LOADSTRING);

		WNDCLASSEXW wcex;
		wcex.cbSize = sizeof(WNDCLASSEX);

		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = WindowProcedure;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = hInstance;
		wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDC_DETOURSAMPLE));
		wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_DETOURSAMPLE);
		wcex.lpszClassName = _szWindowClass;
		wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
		RegisterClassExW(&wcex);
	}

	void Initialize()
	{
		_hWnd = CreateWindowW(_szWindowClass, _szTitle, WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, _hInstance, nullptr);

		MoveWindow(_hWnd, 400, 400, 700, 400, /*bRepaint*/ false);
		ShowWindow(_hWnd, SW_NORMAL);
		UpdateWindow(_hWnd);
		ShowStatusMsg(L"Click to show MessageBox, RightClick to toggle detours (%S)", g_szArch);
		_hwndBtnMsgBox = CreateWindow(
			L"BUTTON",  // Predefined class; Unicode assumed 
			L"MsgBox",      // Button text 
			WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
			0,         // x position 
			0,         // y position 
			100,        // Button width
			20,        // Button height
			_hWnd,     // Parent window
			NULL,       // No menu.
			_hInstance,
			NULL);      // Pointer not needed.

		_hwndChkDetour = CreateWindow(
			L"BUTTON",  // Predefined class; Unicode assumed 
			L"Detour",      // Button text 
			WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,  // Styles 
			130,         // x position 
			0,         // y position 
			100,        // Button width
			20,        // Button height
			_hWnd,     // Parent window
			NULL,       // No menu.
			_hInstance,
			NULL);      // Pointer not needed.
	}

	int _topStatline = 25;
	int _statLine = _topStatline;
	void ShowStatusMsg(LPCWSTR pText, ...)
	{
		if (_hWnd != NULL)
		{
			va_list args;
			va_start(args, pText);
			wstring strtemp(1000, '\0');
			_vsnwprintf_s(&strtemp[0], 1000, _TRUNCATE, pText, args);
			va_end(args);
			auto len = wcslen(strtemp.c_str());
			strtemp.resize(len);
			HDC hDC = GetDC(_hWnd);
			HFONT hFont = (HFONT)GetStockObject(ANSI_FIXED_FONT);
			HFONT hOldFont = (HFONT)SelectObject(hDC, hFont);
			TEXTMETRIC textMetric;
			if (GetTextMetrics(hDC, &textMetric) && textMetric.tmAveCharWidth > 0)
			{
				// pad to full line to erase any prior content
				auto nCharsOnLine = _size.x / textMetric.tmAveCharWidth;
				if (nCharsOnLine > 0 && nCharsOnLine >= (int)strtemp.size())
				{
					auto nCharsToPad = nCharsOnLine - strtemp.size();
					if (nCharsToPad > 0)
					{
						strtemp.insert(strtemp.size(), nCharsToPad, ' ');
					}
					TextOut(hDC, 0, _statLine, strtemp.c_str(), (int)strtemp.size());
					_statLine += textMetric.tmHeight;
					if (_statLine > _size.y)
					{
						_statLine = _topStatline;
					}
				}
			}
			SelectObject(hDC, hOldFont);
			ReleaseDC(_hWnd, hDC);
		}
	}

	bool _fIsDetouring = false;
	// Macro to show error. The expression is Stringized using the "#" operator.
	// the "%S" is used (upper case S) to convert to Unicode
#define IFFAILSHOWERROR(expr) \
 if ((err = expr) != NO_ERROR) \
   {\
      g_CMyApplication->ShowStatusMsg(L"Error %d %S", err, #expr); \
   }\

	void DoDetours()
	{
		int err;
		IFFAILSHOWERROR(DetourTransactionBegin());
		if (!_fIsDetouring)
		{
			ShowStatusMsg(L"Start Detouring");
			IFFAILSHOWERROR(DetourAttach((PVOID *)&g_real_MessageBoxA, MyMessageBoxA));
			IFFAILSHOWERROR(DetourAttach((PVOID *)&g_real_MessageBoxW, MyMessageBoxW));
			IFFAILSHOWERROR(DetourAttach((PVOID *)&g_real_CreateWindowExW, MyCreateWindowExW));
			IFFAILSHOWERROR(DetourAttach((PVOID *)&g_real_ShowWindow, MyShowWindow));
		}
		else
		{
			ShowStatusMsg(L"Stop  Detouring");
			IFFAILSHOWERROR(DetourDetach((PVOID *)&g_real_MessageBoxA, MyMessageBoxA));
			IFFAILSHOWERROR(DetourDetach((PVOID *)&g_real_MessageBoxW, MyMessageBoxW));
			IFFAILSHOWERROR(DetourDetach((PVOID *)&g_real_CreateWindowExW, MyCreateWindowExW));
			IFFAILSHOWERROR(DetourDetach((PVOID *)&g_real_ShowWindow, MyShowWindow));
		}
		IFFAILSHOWERROR(DetourTransactionCommit());
		_fIsDetouring = !_fIsDetouring;
	}

	void DoTest()
	{
#if 1
		HWND hwnd = CreateWindowEx(0, _szWindowClass, L"test", WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, _hInstance, nullptr);
		ShowWindow(hwnd, SW_SHOW);
#else
		MessageBoxA(_hWnd, "Some Text", g_szArch, MB_OK);
		//            MessageBoxW(_hWnd, _T("Some Unicode Text"), _T("Caption"), MB_OK);
#endif
	}
	LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_COMMAND:
		{
			int wmId = LOWORD(wParam);
			// Parse the menu selections:
			switch (wmId)
			{
			case BN_CLICKED:
				if ((HWND)lParam == _hwndBtnMsgBox)
				{
					DoTest();
				}
				else if ((HWND)lParam == _hwndChkDetour)
				{
					DoDetours();
				}
				break;
			case IDM_EXIT:
				DestroyWindow(hWnd);
				break;
			default:
				return DefWindowProc(hWnd, message, wParam, lParam);
			}
		}
		break;
		case WM_SIZE:
		{
			auto pts = MAKEPOINTS(lParam);
			ShowStatusMsg(L"Size  %5d %5d", pts.x, pts.y);
			_size = pts;
		}
		break;
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			EndPaint(hWnd, &ps);
		}
		break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		return 0;
	}

	int DoMessageLoop()
	{
		HACCEL hAccelTable = LoadAccelerators(_hInstance, MAKEINTRESOURCE(IDC_DETOURSAMPLE));
		MSG msg;
		// Main message loop:
		while (GetMessage(&msg, nullptr, 0, 0))
		{
			if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		return (int)msg.wParam;
	}
};

LRESULT CALLBACK WindowProcedure(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	return g_CMyApplication->WndProc(hWnd, message, wParam, lParam);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	// create an instance of CMyApplication  on stack
	CMyApplication aCMyApplication(hInstance);
	g_CMyApplication = &aCMyApplication;
	g_CMyApplication->Initialize();

	return g_CMyApplication->DoMessageLoop();
}
#include "framework.h"
#include "AntiStudentPlayComputer.h"
#include <ctime>
#include <shellapi.h>
#include <string>
#include <thread>
#include <chrono>

#define MAX_LOADSTRING 100
#define PASSWORD "1234"
#define DELETE_KEY_ID 11
#define WM_CHECK_TIME (WM_USER + 1)

// 全局变量:
HINSTANCE hInst;                                // 当前实例
WCHAR szTitle[MAX_LOADSTRING];                  // 标题栏文本
WCHAR szWindowClass[MAX_LOADSTRING];            // 主窗口类名
HWND hWndMain;                                  // 主窗口句柄
HWND hEdit;                                     // 密码输入框
BOOL isWindowCreated = FALSE;                   // 标识窗口是否已经创建
HHOOK hKeyboardHook;                            // 键盘钩子句柄
BOOL isRunning = TRUE;                          // 标识程序是否运行

// 时间段设置
const int startHour = 1;
const int startMinute = 5;
const int endHour = 18;
const int endMinute = 30;

// 函数声明
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
void                SetAutoStart();
bool                IsWithinTimeRange();
void                CheckTimeAndRun();
void                CreateSoftKeyboard(HWND hWnd);
void                HideWindow();
DWORD WINAPI        BackgroundTask(LPVOID lpParam);
void                SetWindowTransparency(HWND hWnd, BYTE bAlpha);
void                SetFullScreen(HWND hWnd);
LRESULT CALLBACK    KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
void                SetKeyboardHook();
void                RemoveKeyboardHook();
void                CreateMainWindow();
void                DestroyMainWindow();
void                HandleSoftKeyboardInput(int wmId);
void                TimeCheckThread();

// 主程序入口
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_MY, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow)) {
        return FALSE;
    }

    std::thread timeCheckThread(TimeCheckThread);
    timeCheckThread.detach(); // 后台线程，每秒检查一次时间

    MSG msg;
    // 主消息循环:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    isRunning = FALSE; // 退出程序时停止后台线程

    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MY));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL; // 删除菜单
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;

    hWndMain = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWndMain)
    {
        return FALSE;
    }

    // 隐藏主窗口
    ShowWindow(hWndMain, SW_HIDE);
    UpdateWindow(hWndMain);

    return TRUE;
}

void SetAutoStart()
{
    HKEY hKey;
    LPCTSTR skPath = _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");
    LONG lRes = RegOpenKeyEx(HKEY_CURRENT_USER, skPath, 0, KEY_SET_VALUE, &hKey);

    if (lRes == ERROR_SUCCESS)
    {
        TCHAR szPath[MAX_PATH];
        GetModuleFileName(NULL, szPath, MAX_PATH);
        RegSetValueEx(hKey, _T("ControlUseConputerTimeTool"), 0, REG_SZ, (LPBYTE)szPath, (lstrlen(szPath) + 1) * sizeof(TCHAR));
        RegCloseKey(hKey);
    }
}

bool IsWithinTimeRange()
{
    time_t now = time(0);
    tm localtm;
    localtime_s(&localtm, &now);
    int hour = localtm.tm_hour;
    int minute = localtm.tm_min;

    return (hour > startHour || (hour == startHour && minute >= startMinute)) &&
        (hour < endHour || (hour == endHour && minute < endMinute));
}

void CheckTimeAndRun()
{
    if (IsWithinTimeRange())
    {
        if (!isWindowCreated)
        {
            CreateMainWindow();
        }
    }
    else
    {
        if (isWindowCreated)
        {
            DestroyMainWindow();
        }
    }
}

void CreateSoftKeyboard(HWND hWnd)
{
    int xPos = 10, yPos = 50, btnWidth = 30, btnHeight = 30, btnMargin = 10;
    for (int i = 1; i <= 10; ++i)
    {
        CreateWindowW(L"BUTTON", std::to_wstring(i % 10).c_str(), WS_CHILD | WS_VISIBLE,
            xPos, yPos, btnWidth, btnHeight, hWnd, (HMENU)i, hInst, NULL);
        xPos += btnWidth + btnMargin;
        if (i % 5 == 0) { yPos += btnHeight + btnMargin; xPos = 10; }
    }
    // 添加删除键
    CreateWindowW(L"BUTTON", L"Del", WS_CHILD | WS_VISIBLE,
        xPos, yPos, btnWidth, btnHeight, hWnd, (HMENU)DELETE_KEY_ID, hInst, NULL);
}

void HideWindow()
{
    ShowWindow(hWndMain, SW_HIDE);
    CreateThread(NULL, 0, BackgroundTask, NULL, 0, NULL);
}

DWORD WINAPI BackgroundTask(LPVOID lpParam)
{
    while (true)
    {
        Sleep(1000);
    }
    return 0;
}

void SetWindowTransparency(HWND hWnd, BYTE bAlpha)
{
    LONG exStyle = GetWindowLong(hWnd, GWL_EXSTYLE);
    SetWindowLong(hWnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
    SetLayeredWindowAttributes(hWnd, 0, (255 * bAlpha) , LWA_ALPHA);
}

void SetFullScreen(HWND hWnd)
{
    SetWindowLong(hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    SetWindowPos(hWnd, HWND_TOP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), SWP_SHOWWINDOW);
}

void CreateMainWindow()
{
    if (isWindowCreated)
    {
        return;
    }

    hWndMain = CreateWindowW(szWindowClass, szTitle, WS_POPUP | WS_VISIBLE,
        0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), nullptr, nullptr, hInst, nullptr);

    if (hWndMain)
    {
        SetFullScreen(hWndMain);
        SetWindowTransparency(hWndMain, 5);
        SetWindowPos(hWndMain, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

        hEdit = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_PASSWORD,
            10, 10, 200, 25, hWndMain, NULL, hInst, NULL);

        CreateSoftKeyboard(hWndMain);

        ShowWindow(hWndMain, SW_SHOW);
        UpdateWindow(hWndMain);

        SetAutoStart();
        SetKeyboardHook(); // 设置键盘钩子

        isWindowCreated = TRUE;
    }
}

void DestroyMainWindow()
{
    if (!isWindowCreated)
    {
        return;
    }

    if (hWndMain)
    {
        DestroyWindow(hWndMain);
        hWndMain = NULL;
        RemoveKeyboardHook(); // 移除键盘钩子
        isWindowCreated = FALSE;
    }
}

void HandleSoftKeyboardInput(int wmId)
{
    if (wmId >= 1 && wmId <= 10)
    {
        WCHAR buf[2];
        wsprintf(buf, L"%d", wmId % 10);
        SendMessage(hEdit, EM_REPLACESEL, 0, (LPARAM)buf);
    }
    else if (wmId == DELETE_KEY_ID)
    {
        int len = GetWindowTextLength(hEdit);
        if (len > 0)
        {
            SendMessage(hEdit, EM_SETSEL, len - 1, len);
            SendMessage(hEdit, EM_REPLACESEL, TRUE, (LPARAM)L"");
        }
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CHECK_TIME:
        CheckTimeAndRun();
        break;
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        HandleSoftKeyboardInput(wmId);

        int len = GetWindowTextLength(hEdit) + 1;
        WCHAR* password = new WCHAR[len];
        GetWindowText(hEdit, password, len);

        if (wcscmp(password, TEXT(PASSWORD)) == 0)
        {
            RemoveKeyboardHook(); // 移除键盘钩子
            HideWindow();
        }

        delete[] password;

        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
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

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        // 拦截所有按键
        return 1;
    }
    return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
}

void SetKeyboardHook()
{
    hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, hInst, 0);
}

void RemoveKeyboardHook()
{
    UnhookWindowsHookEx(hKeyboardHook);
}

void TimeCheckThread()
{
    while (isRunning)
    {
        if (IsWithinTimeRange())
        {
            PostMessage(hWndMain, WM_CHECK_TIME, 0, 0);
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

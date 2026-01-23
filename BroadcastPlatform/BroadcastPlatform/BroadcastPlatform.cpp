// BroadcastPlatform.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <sstream>

#include "BroadcastPlatform.h"
#include "UDPServer.h"

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

// UDP Server
HWND hMainWnd;
HWND hLogEdit;
HWND hClientList;
HWND hMessageEdit;
HWND hSendButton;
HWND hStartButton;
HWND hStopButton;

UDPServer* g_server = nullptr;
void AddLogMessage(const std::string& message);
void UpdateClientList();
void OnServerMessage(const std::string& message,
    const std::string& senderIP,
    int senderPort);
void OnClientStatusChanged(const std::string& ip, int port, bool connected);


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_BROADCASTPLATFORM, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    // 创建UDP服务器
    g_server = new UDPServer(9091);
    g_server->SetMessageCallback(OnServerMessage);
    g_server->SetClientCallback(OnClientStatusChanged);

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_BROADCASTPLATFORM));

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

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_BROADCASTPLATFORM));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_BROADCASTPLATFORM);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, 800, 600, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}


// 创建控件
void CreateControls(HWND hWnd)
{
    // 日志文本框
    hLogEdit = CreateWindowEx(
        WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT(""),
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
        ES_AUTOVSCROLL | ES_READONLY,
        10, 10, 500, 300,
        hWnd, (HMENU)1001, hInst, nullptr
    );

    // 设置字体
    HFONT hFont = CreateFont(14, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        TEXT("Consolas"));
    SendMessage(hLogEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

    // 客户端列表
    hClientList = CreateWindowEx(
        WS_EX_CLIENTEDGE, WC_LISTVIEW, TEXT(""),
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
        520, 10, 250, 300,
        hWnd, (HMENU)1002, hInst, nullptr
    );

    // 设置列表视图列
    LVCOLUMN lvc = {};
    lvc.mask = LVCF_WIDTH | LVCF_TEXT;

    lvc.cx = 80;
    lvc.pszText = (LPWSTR)TEXT("IP");
    ListView_InsertColumn(hClientList, 0, &lvc);

    lvc.cx = 60;
    lvc.pszText = (LPWSTR)TEXT("Port");
    ListView_InsertColumn(hClientList, 1, &lvc);

    lvc.cx = 80;
    lvc.pszText = (LPWSTR)TEXT("Name");
    ListView_InsertColumn(hClientList, 2, &lvc);

    // 消息输入框
    hMessageEdit = CreateWindowEx(
        WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT(""),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        10, 320, 400, 25,
        hWnd, (HMENU)1003, hInst, nullptr
    );

    // 发送按钮
    hSendButton = CreateWindowEx(
        0, TEXT("BUTTON"), TEXT("Send"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        420, 320, 80, 25,
        hWnd, (HMENU)1004, hInst, nullptr
    );

    // 启动按钮
    hStartButton = CreateWindowEx(
        0, TEXT("BUTTON"), TEXT("Start Server"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        10, 360, 100, 30,
        hWnd, (HMENU)1005, hInst, nullptr
    );

    // 停止按钮
    hStopButton = CreateWindowEx(
        0, TEXT("BUTTON"), TEXT("Stop Server"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        120, 360, 100, 30,
        hWnd, (HMENU)1006, hInst, nullptr
    );
    EnableWindow(hStopButton, FALSE);
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        CreateControls(hWnd);
        break;
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            case 1004: // Send按钮
                if (g_server && g_server->IsRunning())
                {
                    char buffer[1024];
                    GetWindowTextA(hMessageEdit, buffer, sizeof(buffer));

                    if (strlen(buffer) > 0)
                    {
                        std::string msg = "[SERVER] " + std::string(buffer);
                        g_server->Broadcast(msg);

                        AddLogMessage("Broadcast: " + msg);

                        SetWindowTextA(hMessageEdit, "");
                    }
                }
                break;

            case 1005: // Start Server
                if (g_server && !g_server->IsRunning())
                {
                    if (g_server->Start())
                    {
                        AddLogMessage("Server started on port 8888");
                        EnableWindow(hStartButton, FALSE);
                        EnableWindow(hStopButton, TRUE);
                    }
                }
                break;

            case 1006: // Stop Server
                if (g_server && g_server->IsRunning())
                {
                    g_server->Stop();
                    AddLogMessage("Server stopped");
                    EnableWindow(hStartButton, TRUE);
                    EnableWindow(hStopButton, FALSE);

                    // 清空客户端列表
                    ListView_DeleteAllItems(hClientList);
                }
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
            // TODO: Add any drawing code that uses hdc here...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        if (g_server)
        {
            g_server->Stop();
        }
        PostQuitMessage(0);
        break;
    case WM_TIMER:
        if (wParam == 1)
        {
            // 定时更新客户端列表
            UpdateClientList();
        }
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
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


void AddLogMessage(const std::string& message)
{
    // 获取当前时间
    SYSTEMTIME st;
    GetLocalTime(&st);

    char timeStr[32];
    sprintf_s(timeStr, "[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);

    std::string fullMessage = timeStr + message + "\r\n";

    // 添加到日志文本框
    int len = GetWindowTextLengthA(hLogEdit);
    SendMessageA(hLogEdit, EM_SETSEL, len, len);
    SendMessageA(hLogEdit, EM_REPLACESEL, 0, (LPARAM)fullMessage.c_str());

    // 滚动到底部
    SendMessage(hLogEdit, WM_VSCROLL, SB_BOTTOM, 0);
}

void UpdateClientList()
{
    if (!g_server) return;

    ListView_DeleteAllItems(hClientList);

    auto clients = g_server->GetConnectedClients();
    int index = 0;

    for (const auto& client : clients)
    {
        LVITEM lvi = {};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = index;
        lvi.iSubItem = 0;

        int wideCharLength = MultiByteToWideChar(CP_UTF8, 0, client.ip.c_str(), -1, nullptr, 0);
        LPWSTR clientIP = new WCHAR[wideCharLength];
        MultiByteToWideChar(CP_UTF8, 0, client.ip.c_str(), -1, clientIP, wideCharLength);

        lvi.pszText = clientIP;
        ListView_InsertItem(hClientList, &lvi);

        wideCharLength = MultiByteToWideChar(CP_UTF8, 0, std::to_string(client.port).c_str(), -1, nullptr, 0);
        LPWSTR clientPort = new WCHAR[wideCharLength];
        MultiByteToWideChar(CP_UTF8, 0, std::to_string(client.port).c_str(), -1, clientPort, wideCharLength);
        ListView_SetItemText(hClientList, index, 1, clientPort);

        wideCharLength = MultiByteToWideChar(CP_UTF8, 0, client.name.c_str(), -1, nullptr, 0);
        LPWSTR clientName = new WCHAR[wideCharLength];
        MultiByteToWideChar(CP_UTF8, 0, client.name.c_str(), -1, clientName, wideCharLength);
        ListView_SetItemText(hClientList, index, 2, clientName);

        index++;
    }
}

void OnServerMessage(const std::string& message,
    const std::string& senderIP,
    int senderPort)
{
    std::string logMsg = senderIP + ":" + std::to_string(senderPort) +
        " > " + message;
    AddLogMessage(logMsg);
}

void OnClientStatusChanged(const std::string& ip, int port, bool connected)
{
    std::string msg = ip + ":" + std::to_string(port);
    msg += connected ? " connected" : " disconnected";
    AddLogMessage(msg);

    // 更新客户端列表
    UpdateClientList();
}
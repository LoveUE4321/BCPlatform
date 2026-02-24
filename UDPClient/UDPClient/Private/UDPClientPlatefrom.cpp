// UDPClient.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "UDPClient.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <windows.h>
#include <commctrl.h>

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:

UDPClient gClient;
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

//std::string serverIP, portStr, clientName;
//int serverPort;

void AddLogMessage(const std::string& message);

void OnMessageReceived(const std::string& message)
{
    std::cout << "\n[Incoming] " << message << std::endl;
}

void OnConnectionChanged(bool connected, const std::string& reason)
{
    AddLogMessage(reason);
   /* if (connected)
    {
        std::cout << "\n[Connected] " << reason << std::endl;
    }
    else
    {
        std::cout << "\n[Disconnected] " << reason << std::endl;
    }*/
}

void PrintMenu()
{
    std::cout << "\n=== UDP Client Console ===" << std::endl;
    std::cout << "1. Connect to Server" << std::endl;
    std::cout << "2. Disconnect" << std::endl;
    std::cout << "3. Send Chat Message" << std::endl;
    std::cout << "4. Send Ping" << std::endl;
    std::cout << "5. Show Status" << std::endl;
    std::cout << "6. Exit" << std::endl;
    std::cout << "Choice: ";
}


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
    LoadStringW(hInstance, IDC_UDPCLIENT, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    gClient.SetMessageCallback(OnMessageReceived);
    gClient.SetConnectionCallback(OnConnectionChanged);

    //serverPort = portStr.empty() ? 8888 : std::stoi(portStr);
    //gClient.Connect(serverIP, serverPort, 0, clientName);

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_UDPCLIENT));

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
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_UDPCLIENT));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_UDPCLIENT);
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

HWND hMainWnd;
HWND hLogEdit;
HWND hClientList;
HWND hMessageEdit;
HWND hPortEdit;
HWND hSendButton;
HWND hStartButton;
HWND hStopButton;

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
    /* hClientList = CreateWindowEx(
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
    ListView_InsertColumn(hClientList, 2, &lvc);*/

    // IP输入框
    hMessageEdit = CreateWindowEx(
        WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT("127.0.0.1"),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        10, 320, 200, 25,
        hWnd, (HMENU)1003, hInst, nullptr
    );

    // Port输入框
    hPortEdit = CreateWindowEx(
        WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT("8888"),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        220, 320, 80, 25,
        hWnd, (HMENU)1007, hInst, nullptr
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
        0, TEXT("BUTTON"), TEXT("Connet Server"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        10, 360, 100, 30,
        hWnd, (HMENU)1005, hInst, nullptr
    );

    // 停止按钮
    hStopButton = CreateWindowEx(
        0, TEXT("BUTTON"), TEXT("Stop Connect"),
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
                if (gClient.IsConnected())
                {
                    //std::cout << "Enter message: ";
                    std::string message = "Enter message: ";
                    //std::getline(std::cin, message);

                    if (gClient.SendChat(message))
                    {
                        std::cout << "Message sent." << std::endl;
                    }
                    else
                    {
                        std::cout << "Failed to send message." << std::endl;
                    }
                }
                else
                {
                    std::cout << "Not connected to server!" << std::endl;
                }
                break;

            case 1005: // Start Server
            {
                char serverIP[256];
                GetWindowTextA(hMessageEdit, serverIP, sizeof(serverIP));

                char serverPort[256];
                GetWindowTextA(hPortEdit, serverPort, sizeof(serverPort));
                int Port = std::stoi(serverPort);

                std::string clientName = "UDP Client";
                gClient.Connect(serverIP, Port, 0, clientName);

                EnableWindow(hStartButton, FALSE);
                EnableWindow(hStopButton, TRUE);
            }
                break;

            case 1006: // Stop Server
                gClient.Disconnect();

                EnableWindow(hStartButton, TRUE);
                EnableWindow(hStopButton, FALSE);
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
        PostQuitMessage(0);
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
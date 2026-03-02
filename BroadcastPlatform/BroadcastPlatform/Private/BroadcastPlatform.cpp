// BroadcastPlatform.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <sstream>

#pragma comment(lib, "comctl32.lib")

#include "BroadcastPlatform.h"
#include "UDPServer.h"

#define MAX_LOADSTRING 100
#define SERVER_PORT 8899

//// 初始化通用控件
//#pragma comment(linker,"\"/manifestdependency:type='win32' \
//name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
//processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

HFONT hFont;

// UDP Server
HWND hMainWnd;
HWND hLogEdit;
HWND hClientList;
HWND hRoomList;
HWND hClientsEdit;
HWND hSendButton;
HWND hStartButton;
HWND hStopButton;
HWND hListBox;

HWND hComboRoom;
HWND hComboHost;
HWND hComboClients;

HWND g_hEdit;        // 嵌入的编辑框
HWND g_hCombo;    // 嵌入的下拉框

int g_editRow = -1;         // 当前编辑的行
int g_editCol = -1;         // 当前编辑的列

std::vector<int> g_HostIDList;
std::vector<int> g_ClientIDList;

UDPServer* g_server = nullptr;
void AddLogMessage(const std::string& message);
void AddLogMessageW(wchar_t* message);
void UpdateClientList();
void OnServerMessage(const std::string& message,
    const std::string& senderIP,
    int senderPort);
void OnClientStatusChanged(const std::string& ip, int port, bool connected);
void GetCheckedSummary(HWND hListView, wchar_t* buffer, size_t bufferSize);


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // 初始化通用控件
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);

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
    g_server = new UDPServer(SERVER_PORT);
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
      CW_USEDEFAULT, 0, 870, 500, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

void CreateWndFont(HWND hWnd)
{
    // 设置字体
    if (hFont == nullptr)
    {
        hFont = CreateFont(14, 0, 0, 0, FW_NORMAL,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
            TEXT("Consolas"));
    }    

    SendMessage(hWnd, WM_SETFONT, (WPARAM)hFont, TRUE);
}
// create output wnd
void CreateTextView(HWND hWnd)
{
    // 日志文本框
    hLogEdit = CreateWindowEx(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
        ES_AUTOVSCROLL | ES_READONLY,
        10, 360, 500, 100,
        hWnd, (HMENU)1001, hInst, nullptr
    );

    wchar_t logstr[] = L"输出日志...\r\n";
    // 添加到日志文本框
    int len = GetWindowTextLength(hLogEdit);
    SendMessage(hLogEdit, EM_SETSEL, len, len);
    SendMessage(hLogEdit, EM_REPLACESEL, 0, (LPARAM)logstr);

    // 设置字体
    CreateWndFont(hLogEdit);
}

// create listview
void CreateListView(HWND hWnd)
{
    // 客户端列表
    hClientList = CreateWindowEx(
        WS_EX_CLIENTEDGE, WC_LISTVIEW, TEXT(""),
        WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
        10, 5, 500, 200,
        hWnd, (HMENU)1002, hInst, nullptr
    );

    // 设置扩展样式 - 关键部分：启用网格线
    DWORD exStyle = LVS_EX_CHECKBOXES |
        LVS_EX_FULLROWSELECT |   // 整行选中
        LVS_EX_GRIDLINES |        // 网格线（核心）
        LVS_EX_DOUBLEBUFFER;      // 双缓冲防闪烁


    ListView_SetExtendedListViewStyle(hClientList, exStyle);

    // 设置背景色
    //ListView_SetBkColor(hClientList, RGB(200, 200, 200));

    // 设置列表视图列
    LVCOLUMN lvc = { 0 };
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_FMT;

    lvc.cx = 20;
    lvc.iSubItem = 0;
    lvc.fmt = LVCFMT_IMAGE;
    lvc.pszText = (LPWSTR)TEXT("");
    ListView_InsertColumn(hClientList, 0, &lvc);

    lvc.cx = 50;
    lvc.iSubItem = 1;
    lvc.fmt = LVCFMT_CENTER;
    lvc.pszText = (LPWSTR)TEXT("设备号");
    ListView_InsertColumn(hClientList, 1, &lvc);

    lvc.cx = 80;
    lvc.iSubItem = 2;
    lvc.fmt = LVCFMT_CENTER;
    lvc.pszText = (LPWSTR)TEXT("设备SN");
    ListView_InsertColumn(hClientList, 2, &lvc);

    lvc.cx = 80;
    lvc.iSubItem = 3;
    lvc.fmt = LVCFMT_CENTER;
    lvc.pszText = (LPWSTR)TEXT("状态");
    ListView_InsertColumn(hClientList, 3, &lvc);

    lvc.cx = 80;
    lvc.iSubItem = 4;
    lvc.fmt = LVCFMT_CENTER;
    lvc.pszText = (LPWSTR)TEXT("进度");
    ListView_InsertColumn(hClientList, 4, &lvc);

    g_HostIDList.clear();
    for (int i = 0; i < 0; ++i)
    {
        LVITEM lvi = {};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = i;
        lvi.iSubItem = 0;
        lvi.pszText = (LPWSTR)TEXT("");
        ListView_InsertItem(hClientList, &lvi);

        // id sn state prog
        int id = i + 1;
        std::string idStr = std::to_string(i + 1);
        ListView_SetItemText(hClientList, i, 1, (LPWSTR)idStr.c_str());
        ListView_SetItemText(hClientList, i, 2, (LPWSTR)TEXT("dfdfe23432"));
        ListView_SetItemText(hClientList, i, 3, (LPWSTR)idStr.c_str());
        ListView_SetItemText(hClientList, i, 4, (LPWSTR)TEXT("1-2-3"));

        SendMessage(hComboHost, CB_ADDSTRING, 0, (LPARAM)idStr.c_str());
        g_HostIDList.push_back(id);
    }

}


// add room info to roomlist
void AddRoomView(HWND hWnd)
{
    //if (!g_server) return;

    for (int i = 0; i < 5; ++i)
    {
        LVITEM lvi = {};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = i;
        lvi.iSubItem = 0;
        lvi.pszText = (LPWSTR)TEXT("");
        ListView_InsertItem(hRoomList, &lvi);

        std::string id = std::to_string(i+1);
        ListView_SetItemText(hRoomList, i, 0, (LPWSTR)id.c_str());

        ListView_SetItemText(hRoomList, i, 1, (LPWSTR)TEXT("Num.1"));

        ListView_SetItemText(hRoomList, i, 2, (LPWSTR)TEXT("Num.2"));

        ListView_SetItemText(hRoomList, i, 3, (LPWSTR)TEXT("Online"));

    }
}

// create room list 
void CreateRoomList(HWND hWnd)
{
    // room list
    hRoomList = CreateWindowEx(
        WS_EX_CLIENTEDGE, WC_LISTVIEW, TEXT(""),
        WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
        10, 210, 500, 150,
        hWnd, (HMENU)1009, hInst, nullptr
    );

    // 设置扩展样式 - 关键部分：启用网格线
    DWORD exStyle = 
        LVS_EX_FULLROWSELECT |   // 整行选中
        LVS_EX_GRIDLINES |        // 网格线（核心）
        LVS_EX_DOUBLEBUFFER;      // 双缓冲防闪烁


    ListView_SetExtendedListViewStyle(hRoomList, exStyle);

    // 设置背景色
    //ListView_SetBkColor(hRoomList, RGB(200, 200, 200));

    // 设置列表视图列
    LVCOLUMN lvc = { 0 };
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_FMT;

    lvc.cx = 50;
    lvc.iSubItem = 0;
    lvc.fmt = LVCFMT_CENTER;
    lvc.pszText = (LPWSTR)TEXT("房间");
    ListView_InsertColumn(hRoomList, 0, &lvc);

    lvc.cx = 80;
    lvc.iSubItem = 1;
    lvc.fmt = LVCFMT_CENTER;
    lvc.pszText = (LPWSTR)TEXT("主机");
    ListView_InsertColumn(hRoomList, 1, &lvc);

    lvc.cx = 80;
    lvc.iSubItem = 2;
    lvc.fmt = LVCFMT_CENTER;
    lvc.pszText = (LPWSTR)TEXT("从机");
    ListView_InsertColumn(hRoomList, 2, &lvc);

    lvc.cx = 80;
    lvc.iSubItem = 3;
    lvc.fmt = LVCFMT_CENTER;
    lvc.pszText = (LPWSTR)TEXT("进度");
    ListView_InsertColumn(hRoomList, 3, &lvc);

    //
    //AddRoomView(hWnd);
}

// create combox
HWND CreateCombox(HWND hWnd, HMENU hMenu, int x, int y, int width, int height, const std::vector<std::wstring>& items, bool bDefault= false)
{
    HWND hCombo = CreateWindowExW(
        0, L"COMBOBOX", L"DROPDOWNLIST",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST,
        x, y, width, height,
        hWnd, hMenu, hInst, nullptr);

    // 添加项
    for (const auto& item : items)
    {
        LRESULT nItem = SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)item.c_str());
    }

    // 默认选择第一项
    if(bDefault)
        SendMessage(hCombo, CB_SETCURSEL, 0, 0);

    CreateWndFont(hCombo);

    return hCombo;
}

// create button 
void CreateButton(HWND hWnd)
{
    // 启动按钮
    hStartButton = CreateWindowEx(
        0, TEXT("BUTTON"), TEXT("Start Server"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        520, 5, 100, 30,
        hWnd, (HMENU)1005, hInst, nullptr
    );

    // 停止按钮
    hStopButton = CreateWindowEx(
        0, TEXT("BUTTON"), TEXT("Stop Server"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        650, 5, 100, 30,
        hWnd, (HMENU)1006, hInst, nullptr
    );
    EnableWindow(hStopButton, FALSE);

    // 消息输入框
    /*hClientsEdit = CreateWindowEx(
        WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT(""),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        10, 320, 400, 25,
        hWnd, (HMENU)1003, hInst, nullptr
    );*/

    // 发送按钮
    /*hSendButton = CreateWindowEx(
        0, TEXT("BUTTON"), TEXT("Send"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        660, 10, 80, 25,
        hWnd, (HMENU)1004, hInst, nullptr
    );*/
}


// 创建嵌入的编辑框
HWND CreateInPlaceEdit(HWND hwndParent, int x, int y, int width, int height, const wchar_t* text, int row, int col) 
{
    HWND hEdit = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        TEXT("EDIT"), text,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL,
        x, y, width, height,
        hwndParent,
        (HMENU)2003,
        hInst,
        NULL
    );

    CreateWndFont(hEdit);

    // 保存编辑位置信息
    SetWindowLongPtr(hEdit, GWLP_USERDATA, (LONG_PTR)MAKELONG(row, col));

    return hEdit;
}

// 创建嵌入的下拉框
HWND CreateInPlaceCombo(HWND hwndParent, int x, int y, int width, int height,
    const std::vector<std::wstring>& items,
    const wchar_t* currentText, int row, int col) 
{
    HWND hCombo = CreateWindowExW(
        0,
        (LPWSTR)"COMBOBOX",
        NULL,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL | CBS_HASSTRINGS,
        x, y, width, height,
        hwndParent,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );

    // 添加下拉项
    for (const auto& item : items) {
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)item.c_str());
    }

    // 设置当前选中项
    LRESULT index = SendMessageW(hCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)currentText);
    if (index != CB_ERR) {
        SendMessageW(hCombo, CB_SETCURSEL, index, 0);
    }

    // 设置字体
    HFONT hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, (LPWSTR)"微软雅黑");
    SendMessage(hCombo, WM_SETFONT, (WPARAM)hFont, TRUE);

    // 保存下拉框位置信息
    SetWindowLongPtr(hCombo, GWLP_USERDATA, (LONG_PTR)MAKELONG(row, col));

    return hCombo;
}

// 获取单元格的屏幕位置
BOOL GetCellRect(HWND hListView, int row, int col, RECT* rect) 
{
    // 获取行矩形
    if (!ListView_GetItemRect(hListView, row, rect, LVIR_BOUNDS))
    {
        return FALSE;
    }

    // 调整到指定列
    rect->left = 0;
    for (int i = 1; i <= col; i++) {
        rect->left += ListView_GetColumnWidth(hListView, i);
    }
    rect->right = rect->left + ListView_GetColumnWidth(hListView, col);

    // 转换为屏幕坐标
    POINT pt = { rect->left, rect->top };
    ClientToScreen(hListView, &pt);
    rect->left = pt.x;
    rect->top = pt.y;
    rect->right = pt.x + (rect->right - rect->left);
    rect->bottom = pt.y + (rect->bottom - rect->top);

    return TRUE;
}

// create 
void CreateStaticText(HWND hWnd)
{
    int top = 50;
    HWND hTextRoom = CreateWindowExW(
        0, L"static", L"房间",
        WS_CHILD | WS_VISIBLE | WS_BORDER | SS_CENTER | SS_CENTERIMAGE,
        520, top, 70, 25,
        hWnd, (HMENU)1007, hInst, nullptr);

    HWND hTextHost = CreateWindowExW(
        0, L"static", L"主机",
        WS_CHILD | WS_VISIBLE | WS_BORDER | SS_CENTER | SS_CENTERIMAGE,
        600, top, 50, 25,
        hWnd, (HMENU)1007, hInst, nullptr);

    HWND hTextClients = CreateWindowExW(
        0, L"static", L"从机",
        WS_CHILD | WS_VISIBLE | WS_BORDER | SS_CENTER | SS_CENTERIMAGE,
        660, top, 50, 25,
        hWnd, (HMENU)1007, hInst, nullptr);
    
    // add room list
    std::vector<std::wstring> vRoomList = { TEXT("Room1"), TEXT("Room2"), TEXT("Room3"), TEXT("Room4"), TEXT("Room5") };
    hComboRoom = CreateCombox(hWnd, (HMENU)3001, 520, top+25, 70, 200, vRoomList, true);

    // add host list
    //std::vector<std::wstring> vHostList = { TEXT("1"), TEXT("2"), TEXT("3"), TEXT("4"), TEXT("5") };
    std::vector<std::wstring> vHostList = {};
    hComboHost = CreateCombox(hWnd, (HMENU)3002, 600, top + 25, 50, 200, vHostList);

    // add client list input
    hClientsEdit = CreateWindowEx(
        WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT(""),
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        660, top + 25, 120, 25,
        hWnd, (HMENU)2003, hInst, nullptr);

    vHostList = {};
    hComboClients = CreateCombox(hWnd, (HMENU)3003, 660, top + 50, 50, 100, vHostList);   

    // add launch button
    HWND btLaunch = CreateWindowEx(
        0, TEXT("BUTTON"), TEXT("Launch"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        785, top + 25, 60, 22,
        hWnd, (HMENU)2005, hInst, nullptr);

    // set font
    CreateWndFont(hTextRoom);
    CreateWndFont(hTextHost);                            
    CreateWndFont(hTextClients);
    CreateWndFont(hClientsEdit);
    CreateWndFont(btLaunch);
}
// 创建控件
void CreateControls(HWND hWnd)
{               
    //
    CreateStaticText(hWnd);
    CreateTextView(hWnd);
    CreateListView(hWnd);
    CreateRoomList(hWnd);
    CreateButton(hWnd);
    
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
            int nNotifyCode = HIWORD(wParam);
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
                    GetWindowTextA(hClientsEdit, buffer, sizeof(buffer));

                    if (strlen(buffer) > 0)
                    {
                        std::string msg = "[SERVER] " + std::string(buffer);
                        g_server->Broadcast(msg);

                        AddLogMessage("Broadcast: " + msg);

                        SetWindowTextA(hClientsEdit, "");
                    }
                }
                break;

            case 1005: // Start Server
                if (g_server && !g_server->IsRunning())
                {
                    if (g_server->Start())
                    {
                        std::string msg = std::to_string(SERVER_PORT);
                        AddLogMessage("Server started on port " + msg);
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
            case 2005:
                if (g_server && g_server->IsRunning())
                {
                    wchar_t buffer[64];
                    wchar_t rmName[64];
                    std::vector<int> groupIDs;

                    // get room name
                    HWND curWnd = GetDlgItem(hWnd, 3001);
                    GetWindowText(curWnd, rmName, sizeof(rmName)/sizeof(rmName[0]));
                    //wchar_t* rmName = rmName;
                    //AddLogMessage("Create Room :"+ std::string(buffer).c_str());

                    // get host id
                    curWnd = GetDlgItem(hWnd, 3002);
                    GetWindowText(curWnd, buffer, sizeof(buffer) / sizeof(buffer[0]));

                    int rmID = std::stoi(buffer);
                    groupIDs.push_back(rmID);

                    // get clients id
                    curWnd = GetDlgItem(hWnd, 2003);
                    GetWindowText(curWnd, buffer, sizeof(buffer) / sizeof(buffer[0]));
                    
                    char* s = (char*)buffer;
                    std::istringstream iss(s);
                    std::string token;

                    while (std::getline(iss, token, '/'))
                    {
                        groupIDs.push_back(std::stoi(token));
                    }

                    g_server->OnLaunchButton(rmName, groupIDs);
                }
                break;
            case 3001:
                break;
            case 3003:
            {
                HWND curWnd = GetDlgItem(hWnd, wmId);
                switch (nNotifyCode)
                {
                case CBN_SELCHANGE:
                    char buffer[32];
                    GetWindowTextA(curWnd, buffer, sizeof(buffer));
                    AddLogMessage("Add Client ID :" + std::string(buffer));

                    int wideCharLength = MultiByteToWideChar(CP_UTF8, 0, buffer, -1, nullptr, 0);
                    LPWSTR devNum = new WCHAR[wideCharLength];
                    MultiByteToWideChar(CP_UTF8, 0, buffer, -1, devNum, wideCharLength);

                    // 移动光标到末尾
                    SendMessage(hClientsEdit, EM_SETSEL, -1, 0);
                    // 插入文本（在光标处）
                    SendMessage(hClientsEdit, EM_REPLACESEL, TRUE, (LPARAM)devNum);

                    SendMessage(hClientsEdit, EM_SETSEL, -1, 0);
                    SendMessage(hClientsEdit, EM_REPLACESEL, TRUE, (LPARAM)TEXT("/"));

                    break;
                }
            }                
                break;
            case 3002:
            {
                HWND curWnd = GetDlgItem(hWnd, wmId);
                switch (nNotifyCode)
                {
                case CBN_SELCHANGE:
                    char buffer[32];
                    GetWindowTextA(curWnd, buffer, sizeof(buffer));
                    int id = atoi(buffer);
                    AddLogMessage("Create Room :" + std::to_string(id));

                    g_ClientIDList.clear();
                    SendMessage(hComboClients, CB_RESETCONTENT, 0, 0);  // clear combo box

                    for (auto value : g_HostIDList)
                    {
                        if (value != id)
                        {
                            g_ClientIDList.push_back(value);
                        }
                    }

                    for (auto value : g_ClientIDList)
                    {
                        SendMessage(hComboClients, CB_ADDSTRING, 0, (LPARAM)std::to_string(value).c_str());
                    }

                    break;
                }
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
    //    // 添加表头复选框需要自定义绘制
    //case NM_CUSTOMDRAW: 
    // {
    //    LPNMLVCUSTOMDRAW lpcd = (LPNMLVCUSTOMDRAW)lParam;
    //    if (lpcd->nmcd.dwDrawStage == CDDS_PREPAINT) {
    //        // 在这里绘制表头复选框
    //        return CDRF_NOTIFYITEMDRAW;
    //    }
    //    break;
    //}
    case WM_NOTIFY:
    {
        if (((LPNMHDR)lParam)->hwndFrom == hClientList)
        {
            switch (((LPNMHDR)lParam)->code)
            {
            case LVN_ITEMCHANGED:
            {
                // 处理复选框状态改变
                NMLISTVIEW* pnmv = (NMLISTVIEW*)lParam;

                // 检查是否是复选框状态改变
                if ((pnmv->uChanged & LVIF_STATE) && (pnmv->uNewState & LVIS_STATEIMAGEMASK))
                {
                    // 获取新的复选框状态
                    BOOL newState = (ListView_GetCheckState(hClientList, pnmv->iItem) != 0);

                    // 更新状态栏显示
                    wchar_t statusText[256];
                    GetCheckedSummary(hClientList, statusText, 256);
                }
                break;
            }
            }
        }       
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

void AddLogMessageW(wchar_t* message)
{
    // 获取当前时间
    SYSTEMTIME st;
    GetLocalTime(&st);

    wchar_t timeStr[32];
    swprintf_s(timeStr, L"[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);
    wcscat_s(timeStr, 32, message);

    // 添加到日志文本框
    int len = GetWindowTextLength(hLogEdit);
    SendMessage(hLogEdit, EM_SETSEL, len, len);
    SendMessage(hLogEdit, EM_REPLACESEL, 0, (LPARAM)timeStr);

    // 滚动到底部
    SendMessage(hLogEdit, WM_VSCROLL, SB_BOTTOM, 0);
}

void UpdateClientList()
{
    if (!g_server) return;

    ListView_DeleteAllItems(hClientList);

    auto clients = g_server->GetConnectedClients();
    int index = 0;

    //ListView_SetBkColor(hClientList, RGB(200, 200, 240));

    for (const auto& client : clients)
    {
        LVITEM lvi = {};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = index;
        lvi.iSubItem = 0;
        lvi.pszText = (LPWSTR)TEXT("");
        ListView_InsertItem(hClientList, &lvi);

        // id sn state prog
        int wideCharLength = MultiByteToWideChar(CP_UTF8, 0, std::to_string(client.num).c_str(), -1, nullptr, 0);
        LPWSTR devNum = new WCHAR[wideCharLength];
        MultiByteToWideChar(CP_UTF8, 0, std::to_string(client.num).c_str(), -1, devNum, wideCharLength);
        ListView_SetItemText(hClientList, index, 1, devNum);

        wideCharLength = MultiByteToWideChar(CP_UTF8, 0, client.sn.c_str(), -1, nullptr, 0);
        LPWSTR sn = new WCHAR[wideCharLength];
        MultiByteToWideChar(CP_UTF8, 0, client.sn.c_str(), -1, sn, wideCharLength);
        ListView_SetItemText(hClientList, index, 2, sn);

        wideCharLength = MultiByteToWideChar(CP_UTF8, 0, std::to_string(client.state).c_str(), -1, nullptr, 0);
        LPWSTR state = new WCHAR[wideCharLength];
        MultiByteToWideChar(CP_UTF8, 0, std::to_string(client.state).c_str(), -1, state, wideCharLength);
        ListView_SetItemText(hClientList, index, 3, state);

        wideCharLength = MultiByteToWideChar(CP_UTF8, 0, client.progress.c_str(), -1, nullptr, 0);
        LPWSTR prog = new WCHAR[wideCharLength];
        MultiByteToWideChar(CP_UTF8, 0, client.progress.c_str(), -1, prog, wideCharLength);
        ListView_SetItemText(hClientList, index, 4, prog);

        index++;

        // update host combo
        SendMessage(hComboHost, CB_ADDSTRING, 0, (LPARAM)std::to_string(client.num).c_str());
        g_HostIDList.push_back(client.num);
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

// ========== 复选框操作函数 ==========

// 获取指定行的复选框状态
BOOL GetCheckState(HWND hListView, int row) 
{
    return ListView_GetCheckState(hListView, row);
}

// 设置指定行的复选框状态
void SetCheckState(HWND hListView, int row, BOOL checked)
{
    ListView_SetCheckState(hListView, row, checked);
}

// 切换指定行的复选框状态
void ToggleCheckState(HWND hListView, int row)
{
    BOOL currentState = GetCheckState(hListView, row);
    SetCheckState(hListView, row, !currentState);
}

// 统计选中项数量
int GetCheckedCount(HWND hListView)
{
    int count = 0;
    int itemCount = ListView_GetItemCount(hListView);

    for (int i = 0; i < itemCount; i++)
    {
        if (GetCheckState(hListView, i))
        {
            count++;
        }
    }

    return count;
}

// 生成选中项摘要文本
void GetCheckedSummary(HWND hListView, wchar_t* buffer, size_t bufferSize)
{
    int checkedCount = GetCheckedCount(hListView);

    if (checkedCount == 0)
    {
        wcscpy_s(buffer, bufferSize, L"No one Choice.");
        return;
    }

    wchar_t temp[1024] = L"已选中: ";
    int itemCount = ListView_GetItemCount(hListView);
    int first = 1;

    for (int i = 0; i < itemCount; i++)
    {
        if (GetCheckState(hListView, i))
        {
            wchar_t name[50];
            ListView_GetItemText(hListView, i, 2, name, 50); // 获取姓名列

            if (!first)
            {
                wcscat_s(temp, 1024, L", ");
            }
            wcscat_s(temp, 1024, name);
            wcscat_s(temp, 1024, L"\r\n");

            AddLogMessageW(temp);

          
            // wchar_t to string
            //int len = WideCharToMultiByte(CP_UTF8, 0, temp, -1, NULL, 0, NULL, NULL);
            //if (len <= 0) return ;
            //
            //std::string msg(len-1, 0);
            //WideCharToMultiByte(CP_UTF8, 0, temp, -1, &msg[0], len, NULL, NULL);
            //
            //AddLogMessage(msg);

            first = 0;
        }
    }

    //wcscpy_s(buffer, bufferSize, temp);
}
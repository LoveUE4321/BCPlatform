#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <map>
#include <chrono>
#include <iostream>
#include <functional>
#include "JsonMessage.h"

#pragma comment(lib, "Ws2_32.lib")

enum GameState
{
    GS_Idle,
    GS_Create,
    GS_Join,
    GS_Playing,
    GS_Logout,
};

// 客户端信息结构
struct ClientInfo
{
    std::chrono::system_clock::time_point lastActive;

    std::string ip;
    std::string name;
    std::string sn; // serial number
    std::string progress; // game progress

    int port;
    int num;    // device number
    GameState state;  // run state    
};

struct RoomInfo
{
    std::string roomName;
    int hostDevNum;
    std::vector<int> clientsDevNum;
    std::string progress;
};

// 消息结构
struct UDPMessage
{
    std::string content;
    std::string senderIP;
    int senderPort;
    std::chrono::system_clock::time_point timestamp;
};


class UDPServer
{
public:
    UDPServer(int port = 8888);
    ~UDPServer();

    bool Start();
    void Stop();
    bool IsRunning() const { return isRunning_; }

    // 发送消息
    bool SendToClient(const std::string& ip, int port, const std::string& message);
    //bool Broadcast(const std::string& message);
    bool Broadcast(const std::string& message, const std::string& excludeIP = "", int excludePort = 0);
    bool UpdateToClients(const std::string& name, const std::string& excludeIP, int excludePort );
    
    //
    bool OnLaunchButton(wchar_t* roomName, std::vector<int> groupNums);

    // 获取信息
    std::vector<ClientInfo> GetConnectedClients() const;
    int GetClientCount() const;
    std::queue<UDPMessage> GetReceivedMessages();
    std::vector<RoomInfo> GetRoomInfos() const;

    // 回调函数类型
    using MessageCallback = std::function<void(const std::string& message, const std::string& senderIP, int senderPort)>;
    using ClientCallback = std::function<void(const std::string& ip, int port, bool connected)>;
    using RoomInfoCallback = std::function<void()>;

    void SetMessageCallback(MessageCallback callback) { messageCallback_ = callback; }
    void SetClientCallback(ClientCallback callback) { clientCallback_ = callback; }
    void SetRoomInfoCallback(RoomInfoCallback callback) { rmInfoCallback_ = callback; }

    // 发送JSON消息
    void sendJSONMessage(const JSONMessage& msg);

    // 处理Json消息
    void handleReceivedMessage(const JSONMessage& msg, const std::string& senderIp, int senderPort);
    
private:
    // Socket初始化
    bool InitializeSocket();
    void CleanupSocket();

    // 工作线程
    void ReceiveThreadFunc();
    void ProcessThreadFunc();

    // 消息处理
    void ProcessMessage(const std::string& message, const std::string& senderIP, int senderPort);

    void HandlePing(const std::string& senderIP, int senderPort);
    void HandleConnect(const JSONMessage& msg, const std::string& senderIP, int senderPort);
    void HandleChat(const std::string& message, const std::string& senderIP, int senderPort);
    void HandleDisConnect(const JSONMessage& msg, const std::string& senderIP, int senderPort);
    void HandleCreateRoom(const JSONMessage& msg, const std::string& senderIP, int senderPort);

    // 客户端管理
    void AddClient(const std::string& ip, int port, const JSONMessage& msg);
    void UpdateClient(const std::string& ip, int port, const JSONMessage& msg);
    void RemoveClient(const std::string name, const std::string& ip, int port);
    void UpdateClientActivity(const std::string& ip, int port);
    void RemoveInactiveClients(int timeoutSeconds = 30);
    std::string GetClientKey(const std::string& ip, int port) const;

    void UpdateRoomInfo(std::string rmName, int devNum);

private:
    // Socket相关
    SOCKET serverSocket_ = INVALID_SOCKET;
    sockaddr_in serverAddr_{};
    int port_ = 8888;

    // 线程控制
    std::atomic<bool> isRunning_{ false };
    std::thread receiveThread_;
    std::thread processThread_;

    // 客户端管理
    std::map<std::string, ClientInfo> clients_;
    std::map<int,std::string> clientList;
    std::map<std::string, RoomInfo> roomInfos;
    std::vector<int> clientNums;
    mutable std::mutex clientsMutex_;

    // 消息队列
    std::queue<UDPMessage> messageQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCond_;

    // 回调函数
    MessageCallback messageCallback_ = nullptr;
    ClientCallback clientCallback_ = nullptr;
    RoomInfoCallback rmInfoCallback_ = nullptr;

    // 缓冲区
    static const int BUFFER_SIZE = 65536;
    char buffer_[BUFFER_SIZE];
};


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
    int state;  // run state    
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
    bool UpdateClients(const std::string& name, const std::string& excludeIP, int excludePort );

    // 获取信息
    std::vector<ClientInfo> GetConnectedClients() const;
    int GetClientCount() const;
    std::queue<UDPMessage> GetReceivedMessages();

    // 回调函数类型
    using MessageCallback = std::function<void(const std::string& message,
        const std::string& senderIP,
        int senderPort)>;
    using ClientCallback = std::function<void(const std::string& ip, int port, bool connected)>;

    void SetMessageCallback(MessageCallback callback) { messageCallback_ = callback; }
    void SetClientCallback(ClientCallback callback) { clientCallback_ = callback; }

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
    void ProcessMessage(const std::string& message,
        const std::string& senderIP,
        int senderPort);

    void HandlePing(const std::string& senderIP, int senderPort);
    void HandleConnect(const std::string& message, const std::string& senderIP, int senderPort);
    void HandleChat(const std::string& message, const std::string& senderIP, int senderPort);
    void HandleDisConnect(const std::string& message, const std::string& senderIP, int senderPort);

    // 客户端管理
    void AddClient(const std::string& ip, int port, const std::string& name = "");
    void RemoveClient(const std::string name, const std::string& ip, int port);
    void UpdateClientActivity(const std::string& ip, int port);
    void RemoveInactiveClients(int timeoutSeconds = 30);
    std::string GetClientKey(const std::string& ip, int port) const;

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
    mutable std::mutex clientsMutex_;

    // 消息队列
    std::queue<UDPMessage> messageQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCond_;

    // 回调函数
    MessageCallback messageCallback_ = nullptr;
    ClientCallback clientCallback_ = nullptr;

    // 缓冲区
    static const int BUFFER_SIZE = 65536;
    char buffer_[BUFFER_SIZE];
};


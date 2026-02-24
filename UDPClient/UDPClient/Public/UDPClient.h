#pragma once

#include "resource.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <iostream>
#include <functional>

#include "JsonMessage.h"
#pragma comment(lib, "Ws2_32.lib")

class UDPClient
{

public:
    UDPClient();
    ~UDPClient();

    bool Connect(const std::string& serverIP, int serverPort,
        int localPort = 0, const std::string& clientName = "Client");
    void Disconnect();
    bool IsConnected() const { return isConnected_; }

    // 发送消息
    bool Send(const std::string& message);
    bool SendChat(const std::string& message);
    void SendPing();

    // 接收消息
    std::queue<std::string> GetReceivedMessages();
    bool HasMessages() const;

    // 回调函数
    using MessageCallback = std::function<void(const std::string& message)>;
    using ConnectionCallback = std::function<void(bool connected, const std::string& reason)>;

    void SetMessageCallback(MessageCallback callback) { messageCallback_ = callback; }
    void SetConnectionCallback(ConnectionCallback callback) { connectionCallback_ = callback; }

    // 获取信息
    std::string GetServerInfo() const;
    std::string GetLocalInfo() const;
    std::string GetClientName() const { return clientName_; }

private:
    bool InitializeSocket();
    void CleanupSocket();

    // 工作线程
    void ReceiveThreadFunc();
    void ProcessThreadFunc();
    void HeartbeatThreadFunc();

    // 消息处理
    void ProcessReceivedMessage(const std::string& message);
    void HandlePong();
    void HandleConnectAck(const std::string& message);
    bool HandleReceivedMessage(const JSONMessage& msg);

    // 连接管理
    bool EstablishConnection();
    void CheckConnectionStatus();

private:
    // Socket相关
    SOCKET clientSocket_ = INVALID_SOCKET;
    sockaddr_in serverAddr_{};
    sockaddr_in localAddr_{};

    // 连接信息
    std::string serverIP_;
    int serverPort_ = 0;
    int localPort_ = 0;
    std::string clientName_;

    // 状态
    std::atomic<bool> isRunning_{ false };
    std::atomic<bool> isConnected_{ false };
    std::atomic<bool> waitingForAck_{ false };

    // 线程
    std::thread receiveThread_;
    std::thread processThread_;
    std::thread heartbeatThread_;

    // 消息队列
    std::queue<std::string> messageQueue_;
    mutable std::mutex queueMutex_;
    std::condition_variable queueCond_;

    // 回调函数
    MessageCallback messageCallback_ = nullptr;
    ConnectionCallback connectionCallback_ = nullptr;

    // 缓冲区
    static const int BUFFER_SIZE = 65536;
    char buffer_[BUFFER_SIZE];

    // 超时控制
    std::chrono::system_clock::time_point lastReceiveTime_;
    std::chrono::system_clock::time_point lastSendTime_;
    std::chrono::system_clock::time_point connectionStartTime_;

    // 统计
    int sentMessages_ = 0;
    int receivedMessages_ = 0;
    int failedSends_ = 0;
};

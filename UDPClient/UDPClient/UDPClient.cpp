#include "UDPClient.h"
#include <sstream>
#include <iomanip>

UDPClient::UDPClient()
{
    // 初始化Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0)
    {
        std::cerr << "WSAStartup failed: " << result << std::endl;
    }

    lastReceiveTime_ = std::chrono::system_clock::now();
    lastSendTime_ = std::chrono::system_clock::now();
}

UDPClient::~UDPClient()
{
    Disconnect();
    WSACleanup();
}

bool UDPClient::InitializeSocket()
{
    // 创建Socket
    clientSocket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (clientSocket_ == INVALID_SOCKET)
    {
        std::cerr << "Failed to create socket: " << WSAGetLastError() << std::endl;
        return false;
    }

    // 设置Socket为非阻塞模式
    u_long mode = 1;
    if (ioctlsocket(clientSocket_, FIONBIO, &mode) != 0)
    {
        std::cerr << "Failed to set non-blocking mode: " << WSAGetLastError() << std::endl;
        closesocket(clientSocket_);
        return false;
    }

    // 允许地址重用
    int optval = 1;
    if (setsockopt(clientSocket_, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval)) == SOCKET_ERROR)
    {
        std::cerr << "Failed to set SO_REUSEADDR: " << WSAGetLastError() << std::endl;
    }

    // 设置发送缓冲区大小
    int sendBufSize = 1024 * 1024; // 1MB
    if (setsockopt(clientSocket_, SOL_SOCKET, SO_SNDBUF,
        (char*)&sendBufSize, sizeof(sendBufSize)) == SOCKET_ERROR)
    {
        std::cerr << "Failed to set send buffer size: " << WSAGetLastError() << std::endl;
    }

    // 设置接收缓冲区大小
    int recvBufSize = 1024 * 1024; // 1MB
    if (setsockopt(clientSocket_, SOL_SOCKET, SO_RCVBUF,
        (char*)&recvBufSize, sizeof(recvBufSize)) == SOCKET_ERROR)
    {
        std::cerr << "Failed to set receive buffer size: " << WSAGetLastError() << std::endl;
    }

    // 绑定本地地址（如果指定了端口）
    if (localPort_ > 0)
    {
        localAddr_.sin_family = AF_INET;
        localAddr_.sin_addr.s_addr = htonl(INADDR_ANY);
        localAddr_.sin_port = htons(localPort_);

        if (bind(clientSocket_, (sockaddr*)&localAddr_, sizeof(localAddr_)) == SOCKET_ERROR)
        {
            std::cerr << "Failed to bind socket: " << WSAGetLastError() << std::endl;
            closesocket(clientSocket_);
            return false;
        }
    }

    // 设置服务器地址
    serverAddr_.sin_family = AF_INET;
    serverAddr_.sin_port = htons(serverPort_);

    if (inet_pton(AF_INET, serverIP_.c_str(), &serverAddr_.sin_addr) != 1)
    {
        std::cerr << "Invalid server IP address: " << serverIP_ << std::endl;
        closesocket(clientSocket_);
        return false;
    }

    return true;
}

void UDPClient::CleanupSocket()
{
    if (clientSocket_ != INVALID_SOCKET)
    {
        closesocket(clientSocket_);
        clientSocket_ = INVALID_SOCKET;
    }
}

bool UDPClient::Connect(const std::string& serverIP, int serverPort,
    int localPort, const std::string& clientName)
{
    if (isConnected_) return true;

    serverIP_ = serverIP;
    serverPort_ = serverPort;
    localPort_ = localPort;
    clientName_ = clientName.empty() ? "Client" : clientName;

    if (!InitializeSocket())
    {
        if (connectionCallback_)
        {
            connectionCallback_(false, "Failed to initialize socket");
        }
        return false;
    }

    isRunning_ = true;

    // 启动接收线程
    receiveThread_ = std::thread(&UDPClient::ReceiveThreadFunc, this);

    // 启动处理线程
    processThread_ = std::thread(&UDPClient::ProcessThreadFunc, this);

    // 启动心跳线程
    heartbeatThread_ = std::thread(&UDPClient::HeartbeatThreadFunc, this);

    connectionStartTime_ = std::chrono::system_clock::now();

    // 尝试建立连接
    if (!EstablishConnection())
    {
        Disconnect();
        return false;
    }

    std::cout << "Connecting to server " << serverIP_ << ":" << serverPort_
        << " as " << clientName_ << std::endl;

    return true;
}

void UDPClient::Disconnect()
{
    if (!isRunning_) return;

    // 发送断开连接消息
    if (isConnected_)
    {
        Send("DISCONNECT");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    isRunning_ = false;
    isConnected_ = false;

    // 通知处理线程
    queueCond_.notify_all();

    // 等待线程结束
    if (receiveThread_.joinable())
    {
        receiveThread_.join();
    }

    if (processThread_.joinable())
    {
        processThread_.join();
    }

    if (heartbeatThread_.joinable())
    {
        heartbeatThread_.join();
    }

    // 清理Socket
    CleanupSocket();

    // 清空消息队列
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        while (!messageQueue_.empty())
        {
            messageQueue_.pop();
        }
    }

    std::cout << "Disconnected from server" << std::endl;
}

bool UDPClient::EstablishConnection()
{
    // 发送连接请求
    std::string connectMsg = "CONNECT " + clientName_;

    waitingForAck_ = true;
    int attempts = 0;
    const int maxAttempts = 10;

    while (attempts < maxAttempts && waitingForAck_ && isRunning_)
    {
        if (!Send(connectMsg))
        {
            std::cerr << "Failed to send connection request (attempt "
                << attempts + 1 << ")" << std::endl;
        }

        attempts++;

        // 等待ACK
        for (int i = 0; i < 10 && waitingForAck_ && isRunning_; i++)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            // 检查是否有ACK消息
            std::queue<std::string> messages = GetReceivedMessages();
            while (!messages.empty())
            {
                std::string msg = messages.front();
                messages.pop();

                if (msg.find("CONNECT_ACK") == 0)
                {
                    waitingForAck_ = false;
                    isConnected_ = true;

                    if (connectionCallback_)
                    {
                        connectionCallback_(true, "Connected successfully");
                    }

                    return true;
                }
            }
        }
    }

    if (connectionCallback_ && waitingForAck_)
    {
        connectionCallback_(false, "Connection timeout");
        return false;
    }

    return true;
}

void UDPClient::ReceiveThreadFunc()
{
    std::cout << "Receive thread started" << std::endl;

    while (isRunning_)
    {
        sockaddr_in fromAddr{};
        int fromAddrLen = sizeof(fromAddr);

        int bytesReceived = recvfrom(clientSocket_, buffer_, BUFFER_SIZE - 1, 0,
            (sockaddr*)&fromAddr, &fromAddrLen);

        if (bytesReceived > 0)
        {
            buffer_[bytesReceived] = '\0';

            // 验证消息来自服务器
            char fromIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &fromAddr.sin_addr, fromIP, INET_ADDRSTRLEN);
            int fromPort = ntohs(fromAddr.sin_port);

            if (fromIP == serverIP_ && fromPort == serverPort_)
            {
                std::string message(buffer_, bytesReceived);

                // 添加到消息队列
                {
                    std::lock_guard<std::mutex> lock(queueMutex_);
                    messageQueue_.push(message);
                }

                // 通知处理线程
                queueCond_.notify_one();

                // 更新最后接收时间
                lastReceiveTime_ = std::chrono::system_clock::now();
                receivedMessages_++;
            }
        }
        else if (bytesReceived == SOCKET_ERROR)
        {
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK)
            {
                std::cerr << "recvfrom error: " << error << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::cout << "Receive thread stopped" << std::endl;
}

void UDPClient::ProcessThreadFunc()
{
    std::cout << "Process thread started" << std::endl;

    while (isRunning_)
    {
        std::string message;

        {
            std::unique_lock<std::mutex> lock(queueMutex_);

            if (messageQueue_.empty())
            {
                queueCond_.wait_for(lock, std::chrono::milliseconds(100));
            }

            if (!messageQueue_.empty())
            {
                message = messageQueue_.front();
                messageQueue_.pop();
            }
            else
            {
                // 定期检查连接状态
                CheckConnectionStatus();
                continue;
            }
        }

        // 处理消息
        ProcessReceivedMessage(message);

        // 回调通知
        if (messageCallback_)
        {
            messageCallback_(message);
        }
    }

    std::cout << "Process thread stopped" << std::endl;
}

void UDPClient::HeartbeatThreadFunc()
{
    std::cout << "Heartbeat thread started" << std::endl;

    while (isRunning_)
    {
        if (isConnected_)
        {
            // 每5秒发送一次心跳
            SendPing();

            // 检查是否收到服务器响应
            auto now = std::chrono::system_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(
                now - lastReceiveTime_);

            if (duration.count() > 30) // 30秒无响应认为断开
            {
                std::cout << "Server not responding for " << duration.count()
                    << " seconds, disconnecting..." << std::endl;

                isConnected_ = false;
                if (connectionCallback_)
                {
                    connectionCallback_(false, "Server timeout");
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    std::cout << "Heartbeat thread stopped" << std::endl;
}

void UDPClient::ProcessReceivedMessage(const std::string& message)
{
    std::cout << "[Server] " << message << std::endl;

    if (message.find("PONG") == 0)
    {
        HandlePong();
    }
    else if (message.find("CONNECT_ACK") == 0)
    {
        HandleConnectAck(message);
    }
    else
    {
        if (connectionCallback_)
        {
            connectionCallback_(true, message);
        }
    }
    // 其他消息类型处理...
}

void UDPClient::HandlePong()
{
    // 心跳响应
    // std::cout << "Received PONG from server" << std::endl;
}

void UDPClient::HandleConnectAck(const std::string& message)
{
    if (waitingForAck_)
    {
        waitingForAck_ = false;
        isConnected_ = true;

        std::cout << message << std::endl;

        if (connectionCallback_)
        {
            connectionCallback_(true, message);
        }
    }
}

void UDPClient::CheckConnectionStatus()
{
    if (isConnected_)
    {
        auto now = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            now - lastReceiveTime_);

        if (duration.count() > 60)
        {
            // 长时间未收到消息，发送ping测试
            SendPing();
        }
    }
}

bool UDPClient::Send(const std::string& message)
{
    if (clientSocket_ == INVALID_SOCKET) return false;

    int bytesSent = sendto(clientSocket_, message.c_str(), (int)message.length(), 0,
        (sockaddr*)&serverAddr_, sizeof(serverAddr_));

    if (bytesSent == SOCKET_ERROR)
    {
        int error = WSAGetLastError();
        std::cerr << "sendto failed: " << error << std::endl;
        failedSends_++;

        if (error == WSAECONNRESET || error == WSAENETUNREACH)
        {
            // 网络问题，可能需要重新连接
            isConnected_ = false;
            if (connectionCallback_)
            {
                connectionCallback_(false, "Network error");
            }
        }

        return false;
    }

    sentMessages_++;
    lastSendTime_ = std::chrono::system_clock::now();

    return true;
}

bool UDPClient::SendChat(const std::string& message)
{
    std::string chatMsg = "CHAT " + message;
    return Send(chatMsg);
}

void UDPClient::SendPing()
{
    Send("PING");
}

std::queue<std::string> UDPClient::GetReceivedMessages()
{
    std::lock_guard<std::mutex> lock(queueMutex_);

    std::queue<std::string> result;
    result.swap(messageQueue_);

    return result;
}

bool UDPClient::HasMessages() const
{
    std::lock_guard<std::mutex> lock(queueMutex_);

    return !messageQueue_.empty();
}

std::string UDPClient::GetServerInfo() const
{
    return serverIP_ + ":" + std::to_string(serverPort_);
}

std::string UDPClient::GetLocalInfo() const
{
    char localIP[INET_ADDRSTRLEN] = { 0 };
    sockaddr_in addr;
    int addrLen = sizeof(addr);

    if (getsockname(clientSocket_, (sockaddr*)&addr, &addrLen) == 0)
    {
        inet_ntop(AF_INET, &addr.sin_addr, localIP, INET_ADDRSTRLEN);
        int port = ntohs(addr.sin_port);
        return std::string(localIP) + ":" + std::to_string(port);
    }

    return "Unknown";
}
#include "UDPServer.h"

#include "UDPServer.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

UDPServer::UDPServer(int port) : port_(port)
{
    // 初始化Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0)
    {
        std::cerr << "WSAStartup failed: " << result << std::endl;
    }
}

UDPServer::~UDPServer()
{
    Stop();
    WSACleanup();
}

bool UDPServer::InitializeSocket()
{
    // 创建Socket
    serverSocket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (serverSocket_ == INVALID_SOCKET)
    {
        std::cerr << "Failed to create socket: " << WSAGetLastError() << std::endl;
        return false;
    }

    // 设置Socket为非阻塞模式
    u_long mode = 1; // 1 = non-blocking
    if (ioctlsocket(serverSocket_, FIONBIO, &mode) != 0)
    {
        std::cerr << "Failed to set non-blocking mode: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket_);
        return false;
    }

    // 允许地址重用
    int optval = 1;
    if (setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR,
        (char*)&optval, sizeof(optval)) == SOCKET_ERROR)
    {
        std::cerr << "Failed to set SO_REUSEADDR: " << WSAGetLastError() << std::endl;
    }

    // 绑定地址
    serverAddr_.sin_family = AF_INET;
    serverAddr_.sin_addr.s_addr = INADDR_ANY;
    serverAddr_.sin_port = htons(port_);

    if (bind(serverSocket_, (sockaddr*)&serverAddr_, sizeof(serverAddr_)) == SOCKET_ERROR)
    {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        // 回调通知
        if (messageCallback_)
        {
            messageCallback_("Bind failed: ", "serverIP", port_);
        }
        closesocket(serverSocket_);
        return false;
    }

    return true;
}

void UDPServer::CleanupSocket()
{
    if (serverSocket_ != INVALID_SOCKET)
    {
        closesocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET;
    }
}

bool UDPServer::Start()
{
    if (isRunning_) return true;

    if (!InitializeSocket())
    {
        return false;
    }

    isRunning_ = true;

    // 启动接收线程
    receiveThread_ = std::thread(&UDPServer::ReceiveThreadFunc, this);

    // 启动处理线程
    processThread_ = std::thread(&UDPServer::ProcessThreadFunc, this);

    std::cout << "UDP Server started on port " << port_ << std::endl;

    return true;
}

void UDPServer::Stop()
{
    if (!isRunning_) return;

    auto joinMsg = JSONMessage::CreateSendMsgByType("", JSONMessage::MessageType::DisConnect);
    std::string strTemp = joinMsg->ToJSON();
    Broadcast(strTemp);

    isRunning_ = false;

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

    // 清理Socket
    CleanupSocket();

    // 清空队列
    std::lock_guard<std::mutex> lock(queueMutex_);
    while (!messageQueue_.empty())
    {
        messageQueue_.pop();
    }

    // 清空客户端列表
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        clients_.clear();
        clientList.clear();
        roomInfos.clear();
    }

    std::cout << "UDP Server stopped" << std::endl;
}

void UDPServer::ReceiveThreadFunc()
{
    std::cout << "Receive thread started" << std::endl;

    while (isRunning_)
    {
        sockaddr_in clientAddr{};
        int clientAddrLen = sizeof(clientAddr);

        // 接收数据
        int bytesReceived = recvfrom(serverSocket_, buffer_, BUFFER_SIZE - 1, 0, (sockaddr*)&clientAddr, &clientAddrLen);

        if (bytesReceived > 0)
        {
            buffer_[bytesReceived] = '\0';

            // 获取客户端IP和端口
            char clientIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
            int clientPort = ntohs(clientAddr.sin_port); 

            // 创建消息
            UDPMessage msg;
            msg.content = std::string(buffer_, bytesReceived);
            msg.senderIP = clientIP;
            msg.senderPort = clientPort;
            msg.timestamp = std::chrono::system_clock::now();

            // 添加到队列
            {
                std::lock_guard<std::mutex> lock(queueMutex_);
                messageQueue_.push(msg);
            }

            // 通知处理线程
            queueCond_.notify_one();

            // 更新客户端活动时间
            UpdateClientActivity(clientIP, clientPort);
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

        // 短暂休眠以减少CPU占用
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::cout << "Receive thread stopped" << std::endl;
}

void UDPServer::ProcessThreadFunc()
{
    std::cout << "Process thread started" << std::endl;

    while (isRunning_)
    {
        UDPMessage msg;

        {
            std::unique_lock<std::mutex> lock(queueMutex_);

            // 等待消息或超时
            if (messageQueue_.empty())
            {
                queueCond_.wait_for(lock, std::chrono::milliseconds(100));
            }

            if (!messageQueue_.empty())
            {
                msg = messageQueue_.front();
                messageQueue_.pop();
            }
            else
            {
                // 定时清理不活跃客户端
                RemoveInactiveClients();
                continue;
            }
        }

        // 处理消息
        ProcessMessage(msg.content, msg.senderIP, msg.senderPort);

        //// 回调通知
        //if (messageCallback_ || msg.content.find("PING") != 0 || msg.content.find("LOC:") != 0)
        //{
        //    messageCallback_(msg.content, msg.senderIP, msg.senderPort);
        //}
    }

    std::cout << "Process thread stopped" << std::endl;
}

void UDPServer::ProcessMessage(const std::string& message, const std::string& senderIP, int senderPort)
{
    auto Jm= JSONMessage::FromJSON(message);
    if (Jm == nullptr)
    {
        // 回调通知
        if (messageCallback_)
        {
            messageCallback_("Json Msg is null", senderIP, senderPort);
        }
        return;
    }
    
    handleReceivedMessage(*Jm, senderIP, senderPort);

    return;

    std::cout << "[" << senderIP << ":" << senderPort << "] " << message << std::endl;
    bool bBroadcast = true;
    if (message.find("PING") == 0)
    {
        HandlePing(senderIP, senderPort);
        bBroadcast = false;
    }
    else if (message.find("CONNECT") == 0)
    {
        HandleConnect(*Jm, senderIP, senderPort);
    }
    else if (message.find("CHAT") == 0)
    {
        HandleChat(message, senderIP, senderPort);
    }
    else if (message.find("DISCONNECT") == 0)
    {
        //RemoveClient(senderIP, senderPort);
        if (clientCallback_)
        {
            clientCallback_(senderIP, senderPort, false);
        }
    }
    else
    {
        // 自定义消息处理
        //std::string broadcastMsg = "[" + senderIP + ":" + std::to_string(senderPort) + "] " + message;
        size_t index = message.find_first_of(":");
        std::string mark = message.substr(0, index);
        if (mark.find("LOC") == 0)
        {
            // client transfrom msg broadcast to other client
            Broadcast(message, senderIP, senderPort);
        }
        bBroadcast = false;
    }

    // 回调通知
    if (messageCallback_ && bBroadcast)
    {
        messageCallback_(message, senderIP, senderPort);
    }
}

void UDPServer::HandlePing(const std::string& senderIP, int senderPort)
{
    auto ackMsg = JSONMessage::CreateSendMsgByType(std::to_string(senderPort), JSONMessage::MessageType::Heartbeat);
    std::string strTemp = ackMsg->ToJSON();

    SendToClient(senderIP, senderPort, strTemp);
}

void UDPServer::HandleConnect(const JSONMessage& msg, const std::string& senderIP, int senderPort)
{
    // 提取客户端名称
    std::string clientName = msg.GetSenderId();
    if (clientName.length() ==0)
    {
        clientName = "Unknown";
    }

    AddClient(senderIP, senderPort, msg);

    // 发送欢迎消息
    //std::string welcomeMsg = "ACK:" + clientName + std::to_string(senderPort);
    //SendToClient(senderIP, senderPort, welcomeMsg);
    
    auto ackMsg = JSONMessage::CreateSendMsgByType(clientName, JSONMessage::MessageType::Ack);
    std::string strTemp = ackMsg->ToJSON();
    SendToClient(senderIP, senderPort, strTemp);

    // 通知其他客户端
    //std::string joinMsg = "JOIN:" + clientName + std::to_string(senderPort);// +" has joined the server";
    auto joinMsg = JSONMessage::CreateSendMsgByType(clientName, JSONMessage::MessageType::Join);
    strTemp = joinMsg->ToJSON();
    Broadcast(strTemp, senderIP, senderPort);
                                                                                                                                                                                                
    // Update Other Clients
    UpdateToClients(clientName, senderIP, senderPort);

    // 回调通知
    if (clientCallback_)
    {
        clientCallback_(senderIP, senderPort, true);
    }
                                                                                                                                                                       
    std::cout << clientName << " connected from " << senderIP << ":" << senderPort << std::endl;
}

void UDPServer::HandleChat(const std::string& message, const std::string& senderIP, int senderPort)
{
    // 提取聊天内容
    std::string chatContent = message.substr(5); // 去掉"CHAT "

    // 查找发送者名称
    std::string senderName = "Unknown";
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        std::string key = GetClientKey(senderIP, senderPort);
        if (clients_.find(key) != clients_.end())
        {
            senderName = clients_[key].name;
        }
    }

    // 广播聊天消息
    std::string broadcastMsg = "[CHAT][" + senderName + "] " + chatContent;
    Broadcast(broadcastMsg);
}

void UDPServer::HandleDisConnect(const JSONMessage& msg, const std::string& senderIP, int senderPort)
{
    std::string name = msg.GetSenderId();

    // broadcast client disconnect
    auto joinMsg = JSONMessage::CreateSendMsgByType(name, JSONMessage::MessageType::DisConnect);
    std::string strTemp = joinMsg->ToJSON();
    Broadcast(strTemp, senderIP, senderPort);

    //
    RemoveClient(name, senderIP, senderPort);

    // room info
    int devNum = -1;
    devNum = msg.GetData("Num", devNum);
    std::string rmName;
    rmName = msg.GetData("Room", rmName);
    UpdateRoomInfo(rmName, devNum);

    if (clientCallback_)
    {
        clientCallback_(senderIP, senderPort, false);
    }

    if (rmInfoCallback_)
    {
        rmInfoCallback_();
    }
}

void UDPServer::HandleCreateRoom(const JSONMessage& msg, const std::string& senderIP, int senderPort)
{
    std::string clientName = msg.GetSenderId();
    if (clientName.length() == 0)
    {
        clientName = "Unknown";
    }


    auto ackMsg = JSONMessage::CreateSendMsgByType(clientName, JSONMessage::MessageType::Ack);
    std::string strTemp = ackMsg->ToJSON();
    SendToClient(senderIP, senderPort, strTemp);
}

bool UDPServer::SendToClient(const std::string& ip, int port, const std::string& message)
{
    if (serverSocket_ == INVALID_SOCKET) return false;

    sockaddr_in clientAddr{};
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &clientAddr.sin_addr) != 1)
    {
        std::cerr << "Invalid IP address: " << ip << std::endl;
        return false;
    }

    int bytesSent = sendto(serverSocket_, message.c_str(), (int)message.length(), 0, (sockaddr*)&clientAddr, sizeof(clientAddr));
    if (bytesSent == SOCKET_ERROR)
    {
        std::cerr << "sendto failed: " << WSAGetLastError() << std::endl;
        return false;
    }

    return true;
}

bool UDPServer::Broadcast(const std::string& message, const std::string& excludeIP /*= ""*/, int excludePort /*= 0*/)
{
    if (serverSocket_ == INVALID_SOCKET) return false;

    std::lock_guard<std::mutex> lock(clientsMutex_);

    bool success = true;
    for (const auto& pair : clients_)
    {
        const ClientInfo& client = pair.second;

        // 排除指定客户端
        if (client.ip == excludeIP && client.port == excludePort)
        {
            continue;
        }

        if (!SendToClient(client.ip, client.port, message))
        {
            success = false;
        }
    }

    return success;
}

bool UDPServer::UpdateToClients(const std::string& name, const std::string& excludeIP, int excludePort)
{
    if (serverSocket_ == INVALID_SOCKET) return false;

    std::lock_guard<std::mutex> lock(clientsMutex_);

    bool success = true;
    for (const auto& pair : clients_)
    {
        const ClientInfo& client = pair.second;
        if (name.compare(client.name) == 0)
        {
            continue;
        }

        auto joinMsg = JSONMessage::CreateSendMsgByType(client.name, JSONMessage::MessageType::Join);
        std::string strTemp = joinMsg->ToJSON();
        if (!SendToClient(excludeIP, excludePort, strTemp))
        {
            success = false;
        }
    }

    return success;
}

void UDPServer::AddClient(const std::string& ip, int port, const JSONMessage& msg)
{
    std::lock_guard<std::mutex> lock(clientsMutex_);
    std::string key = GetClientKey(ip, port);
    std::string name = msg.GetSenderId();

    int devNum = -1;
    devNum = msg.GetData("Num", devNum);
    int state = -1;
    state = msg.GetData("State", state);
    std::string sn;
    sn = msg.GetData("SN", sn);
    std::string prog;
    prog = msg.GetData("Progress", prog);

    // add to client list
    clientList[devNum] = sn;
    
    ClientInfo client;
    client.ip = ip;
    client.port = port;
    client.name = name.empty() ? "Client_" + std::to_string(port) : name;
    client.lastActive = std::chrono::system_clock::now();

    client.sn = sn;
    client.num = devNum;
    client.state = (GameState)state;
    client.progress = prog;

    clients_[key] = client;
}

void UDPServer::UpdateClient(const std::string& ip, int port, const JSONMessage& msg)
{
    std::lock_guard<std::mutex> lock(clientsMutex_);
    std::string key = GetClientKey(ip, port);
    std::string name = msg.GetSenderId();

    int devNum = -1;
    devNum = msg.GetData("Num", devNum);
    int state = -1;
    state = msg.GetData("State", state);
    std::string sn;
    sn = msg.GetData("SN", sn);
    std::string prog;
    prog = msg.GetData("Progress", prog);
    std::string room;
    room = msg.GetData("Room", room);

    // update client info
    ClientInfo client;
    client.ip = ip;
    client.port = port;
    client.name = name.empty() ? "Client_" + std::to_string(port) : name;
    client.lastActive = std::chrono::system_clock::now();

    client.sn = sn;
    client.num = devNum;
    client.state = (GameState)state;
    client.progress = prog;

    clients_[key] = client;

    // add room info
    std::string rmKey = room;

    RoomInfo rmInfo;
    auto info = roomInfos.find(rmKey);
    if (info != roomInfos.end())
    {
        rmInfo.roomName = info->second.roomName;
        rmInfo.hostDevNum = info->second.hostDevNum;
        rmInfo.progress = info->second.progress;

        //
        rmInfo.clientsDevNum.push_back(devNum);
    }
    else
    {
        rmInfo.roomName = room;
        rmInfo.hostDevNum = devNum;
        rmInfo.progress = prog;
    }        

    roomInfos[rmKey] = rmInfo;

    if (clientCallback_)
    {
        clientCallback_(ip, port, true);
    }

    if (rmInfoCallback_)
    {
        rmInfoCallback_();
    }
}

void UDPServer::RemoveClient(const std::string name, const std::string& ip, int port)
{
    std::lock_guard<std::mutex> lock(clientsMutex_);

    std::string key = GetClientKey(ip, port);
    
    clientList.erase(clients_[key].num);
    clients_.erase(key);
}

void UDPServer::UpdateClientActivity(const std::string& ip, int port)
{
    std::lock_guard<std::mutex> lock(clientsMutex_);

    std::string key = GetClientKey(ip, port);
    if (clients_.find(key) != clients_.end())
    {
        clients_[key].lastActive = std::chrono::system_clock::now();
    }
}

void UDPServer::RemoveInactiveClients(int timeoutSeconds)
{
    auto now = std::chrono::system_clock::now();

    std::lock_guard<std::mutex> lock(clientsMutex_);

    std::vector<std::string> toRemove;

    for (const auto& pair : clients_)
    {
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - pair.second.lastActive);

        if (duration.count() > timeoutSeconds)
        {
            toRemove.push_back(pair.first);

            //std::cout << "Client " << pair.second.name << " ("<< pair.second.ip << ":" << pair.second.port << ") timed out" << std::endl;
            //messageCallback_("Remove Inactive Client: ", pair.second.ip, pair.second.port);
            //if (clientCallback_)
            //{
            //    clientCallback_(pair.second.ip, pair.second.port, false);
            //}

        }
    }

    for (const auto& key : toRemove)
    {
        std::string ip = clients_[key].ip;
        int port = clients_[key].port;
        std::cout << "Client " << clients_[key].name << " (" << ip << ":" << port << ") timed out" << std::endl;
        messageCallback_("Remove Inactive Client: ", ip, port);

        clientList.erase(clients_[key].num);
        clients_.erase(key);
        if (clientCallback_)
        {
            clientCallback_(ip, port, false);
        }
    }
}

std::string UDPServer::GetClientKey(const std::string& ip, int port) const
{
    return ip + ":" + std::to_string(port);
}

std::vector<ClientInfo> UDPServer::GetConnectedClients() const
{
    //std::lock_guard<std::mutex> lock(clientsMutex_);

    std::vector<ClientInfo> result;
    for (const auto& pair : clients_)
    {
        result.push_back(pair.second);
    }

    return result;
}

std::vector<RoomInfo> UDPServer::GetRoomInfos() const
{
    std::vector<RoomInfo> result;
    for (const auto& pair : roomInfos)
    {
        result.push_back(pair.second);
    }

    return result;
}


int UDPServer::GetClientCount() const
{
    std::lock_guard<std::mutex> lock(clientsMutex_);
    return (int)clients_.size();
}

std::queue<UDPMessage> UDPServer::GetReceivedMessages()
{
    std::lock_guard<std::mutex> lock(queueMutex_);

    std::queue<UDPMessage> result;
    result.swap(messageQueue_);

    return result;
}

void UDPServer::sendJSONMessage(const JSONMessage& msg)
{
    // 转换为字节数组
    std::vector<uint8_t> data = msg.ToBytes();
       
}

void UDPServer::handleReceivedMessage(const JSONMessage& msg, const std::string& senderIp, int senderPort)
{
    // 根据消息类型处理
    switch (msg.GetType()) 
    {
    case JSONMessage::MessageType::Connect:
    {
        HandleConnect(msg, senderIp, senderPort);
    }
        break;
    case JSONMessage::MessageType::Location:
    {
        std::string jsonMsg = msg.ToJSON();

        Broadcast(jsonMsg, senderIp, senderPort);
    }
        break;
    case JSONMessage::MessageType::Heartbeat:
    {
        // 处理心跳响应
        HandlePing(senderIp, senderPort);
        break;
    }

    case JSONMessage::MessageType::DisConnect:
    {
        HandleDisConnect(msg, senderIp, senderPort);
        
        break;
    } 
    case JSONMessage::MessageType::StatusUpdate:      
    {
        int devNum;
        devNum = msg.GetData("Num", devNum);
        GameState state;
        state = (GameState)msg.GetData("State", state);
        std::string rmName;
        rmName = msg.GetData("Room", rmName);

        switch (state)
        {
        case GameState::GS_Create:
        {
            int index = 0;
            for (const auto& pair : clients_)
            {
                const ClientInfo& client = pair.second;
                if (clientNums[0] == client.num)
                {
                    index++;
                    UpdateClient(client.ip, client.port, msg);
                    continue;
                }

                json data;
                data["Num"] = clientNums[index];
                data["SN"] = clientList[clientNums[index]];
                data["State"] = GameState::GS_Join;
                data["Room"] = rmName;

                std::string clientName = msg.GetSenderId();
                auto joinMsg = JSONMessage::CreateSendMsgByType(clientName, JSONMessage::MessageType::StatusUpdate, data);

                std::string strTemp = joinMsg->ToJSON();
                if (!SendToClient(client.ip, client.port, strTemp))
                {
                }
                index++;
            }
        }            
            break;

        case GameState::GS_Join:
            for (const auto& pair : clients_)
            {
                const ClientInfo& client = pair.second;
                if (devNum == client.num)
                {
                    UpdateClient(client.ip, client.port, msg);
                    break;
                }
            }
            break;
        }
    }
        break;                                                                                                        
    default:
        // 显示原始JSON
        break;
    }
}

bool UDPServer::OnLaunchButton(wchar_t* roomName, std::vector<int> groupNums)
{
    if (serverSocket_ == INVALID_SOCKET) return false;

    std::lock_guard<std::mutex> lock(clientsMutex_);

    clientNums = groupNums;

    bool success = true;
    for (const auto& pair : clients_)
    {
        const ClientInfo& client = pair.second;
        if (groupNums[0]==client.num)
        {
            json data;
            data["Num"] = client.num;
            data["SN"] = client.sn;
            data["State"] = GameState::GS_Create;
            data["Room"] = roomName;

            auto joinMsg = JSONMessage::CreateSendMsgByType(client.name, JSONMessage::MessageType::StatusUpdate, data);

            std::string strTemp = joinMsg->ToJSON();
            if (!SendToClient(client.ip, client.port, strTemp))
            {
                success = false;
            }

            break;
        }       
    }

    return success;
}

void UDPServer::UpdateRoomInfo(std::string rmName, int devNum)
{
    auto info = roomInfos.find(rmName);
    if (info != roomInfos.end())
    {
        if (info->second.hostDevNum == devNum)
        {
            roomInfos.erase(info);
        }
        else
        {
            auto it = std::find(info->second.clientsDevNum.begin(), info->second.clientsDevNum.end(), devNum);
            if (it != info->second.clientsDevNum.end())
            {
                info->second.clientsDevNum.erase(it);
            }
        }
    }
}

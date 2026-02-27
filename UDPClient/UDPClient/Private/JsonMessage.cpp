#include "JsonMessage.h"
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <winsock2.h>


JSONMessage::JSONMessage()
    : m_type(MessageType::Custom)
    , m_timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count())
    , m_messageId(GenerateMessageId()) 
{
}

JSONMessage::JSONMessage(MessageType type, const std::string& senderId)
    : m_type(type)
    , m_senderId(senderId)
    , m_timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count())
    , m_messageId(GenerateMessageId()) 
{
}

std::shared_ptr<JSONMessage> JSONMessage::FromJSON(const std::string& jsonStr) 
{
    try {
        json j = json::parse(jsonStr);

        auto msg = std::make_shared<JSONMessage>();
        msg->m_type = static_cast<MessageType>(j.value("type", 0));
        msg->m_senderId = j.value("sender", "");
        msg->m_receiverId = j.value("receiver", "");
        msg->m_messageId = j.value("id", "");
        msg->m_timestamp = j.value("timestamp", 0LL);

        if (j.contains("data")) {
            msg->m_data = j["data"];
        }

        return msg;
    }
    catch (const json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        return nullptr; 
    }
    catch (...) {
        std::cerr << "Unknown error parsing JSON" << std::endl;
        return nullptr;
    }
}

std::string JSONMessage::ToJSON() const
{
    json j;
    j["type"] = static_cast<int>(m_type);
    j["sender"] = m_senderId;
    j["receiver"] = m_receiverId;
    j["id"] = m_messageId;
    j["timestamp"] = m_timestamp;
    j["data"] = m_data;

    return j.dump(); // 紧凑格式
    // 如果需要格式化：j.dump(4);
}

std::vector<uint8_t> JSONMessage::ToBytes() const
{
    std::string jsonStr = ToJSON();

    // 添加4字节长度前缀（网络字节序）
    uint32_t length = htonl(static_cast<uint32_t>(jsonStr.size()));

    std::vector<uint8_t> result(sizeof(length) + jsonStr.size());

    // 复制长度前缀
    memcpy(result.data(), &length, sizeof(length));

    // 复制JSON数据
    memcpy(result.data() + sizeof(length), jsonStr.data(), jsonStr.size());

    return result;
}

std::shared_ptr<JSONMessage> JSONMessage::FromBytes(const std::vector<uint8_t>& data)
{
    if (data.size() < sizeof(uint32_t)) {
        return nullptr;
    }

    // 读取长度前缀
    uint32_t length;
    memcpy(&length, data.data(), sizeof(length));
    length = ntohl(length);

    // 检查数据长度
    if (data.size() < sizeof(length) + length) {
        return nullptr;
    }

    // 提取JSON字符串
    std::string jsonStr(data.begin() + sizeof(length),
        data.begin() + sizeof(length) + length);

    return FromJSON(jsonStr);
}

std::string JSONMessage::GenerateMessageId() 
{
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;

    std::stringstream ss;
    ss << std::hex << timestamp << "-" << dis(gen);
    return ss.str();
}

std::shared_ptr<JSONMessage> JSONMessage::CreateHeartbeat(
    const std::string& senderId,
    const std::map<std::string, json>& status)
{

    auto msg = std::make_shared<JSONMessage>(MessageType::Heartbeat, senderId);

    if (!status.empty()) {
        json data;
        for (const auto& [key, value] : status) {
            data[key] = value;
        }
        msg->m_data = data;
    }

    return msg;
}

std::shared_ptr<JSONMessage> JSONMessage::CreateStatusUpdate(const std::string& senderId,
    const std::string& status,
    const std::map<std::string, json>& extra)
{

    auto msg = std::make_shared<JSONMessage>(MessageType::StatusUpdate, senderId);

    json data;
    data["status"] = status;

    for (const auto& [key, value] : extra) {
        data[key] = value;
    }

    msg->m_data = data;
    return msg;
}


std::shared_ptr<JSONMessage> JSONMessage::CreateInfoUpdate(const std::string& senderId,
    const std::string& status,
    const std::map<std::string, json>& extra)
{

    auto msg = std::make_shared<JSONMessage>(MessageType::StatusUpdate, senderId);

    json data;
    //data["status"] = status;

    for (const auto& [key, value] : extra) {
        data[key] = value;
    }

    msg->m_data = data;
    return msg;
}


std::shared_ptr<JSONMessage> JSONMessage::CreateCommand(const std::string& senderId,
    const std::string& command,
    const std::map<std::string, json>& params)
{

    /*auto msg = std::make_shared<JSONMessage>(MessageType::Command, senderId);

    json data;
    data["command"] = command;

    if (!params.empty()) {
        data["params"] = json::object();
        for (const auto& [key, value] : params) {
            data["params"][key] = value;
        }
    }

    msg->m_data = data;
    return msg;*/

    return nullptr;
}

std::shared_ptr<JSONMessage> JSONMessage::CreateSendMsgByType(
    const std::string& senderId,
    MessageType type,
    const std::map<std::string, json>& extra)
{
    auto msg = std::make_shared<JSONMessage>(type, senderId);

    return msg;
}
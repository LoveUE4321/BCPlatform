#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <iostream>

#include "json.hpp"

using json = nlohmann::json;

class JSONMessage {
public:
    // 消息类型枚举
    enum class MessageType {
        Connect = 0,
        Ack,
        Join,
        Heartbeat,
        Location,
        StatusUpdate,
        DisConnect,
        Custom = 100
    };
    /*enum class MessageType {
        Heartbeat = 1,
        StatusUpdate = 2,
        Command = 3,
        Data = 4,
        Error = 5,
        Custom = 100
    };*/

    // 构造函数
    JSONMessage();
    JSONMessage(MessageType type, const std::string& senderId);

    // 从JSON字符串创建
    static std::shared_ptr<JSONMessage> FromJSON(const std::string& jsonStr);

    // 转换为JSON字符串
    std::string ToJSON() const;

    // 转换为字节数组（带长度前缀）
    std::vector<uint8_t> ToBytes() const;

    // 从字节数组解析
    static std::shared_ptr<JSONMessage> FromBytes(const std::vector<uint8_t>& data);

    // Getter/Setter
    MessageType GetType() const { return m_type; }
    void SetType(MessageType type) { m_type = type; }

    const std::string& GetSenderId() const { return m_senderId; }
    void SetSenderId(const std::string& senderId) { m_senderId = senderId; }

    const std::string& GetReceiverId() const { return m_receiverId; }
    void SetReceiverId(const std::string& receiverId) { m_receiverId = receiverId; }

    const std::string& GetMessageId() const { return m_messageId; }
    void SetMessageId(const std::string& messageId) { m_messageId = messageId; }

    int64_t GetTimestamp() const { return m_timestamp; }
    void SetTimestamp(int64_t timestamp) { m_timestamp = timestamp; }

    // 数据操作
    template<typename T>
    void SetData(const std::string& key, const T& value) {
        m_data[key] = value;
    }

    template<typename T>
    T GetData(const std::string& key, const T& defaultValue = T()) const {
        auto it = m_data.find(key);
        if (it != m_data.end()) {
            try {
                return it->get<T>();
            }
            catch (...) {
                return defaultValue;
            }
        }
        return defaultValue;
    }

    bool HasData(const std::string& key) const {
        return m_data.find(key) != m_data.end();
    }

    // 创建特定类型的消息
    static std::shared_ptr<JSONMessage> CreateHeartbeat(
        const std::string& senderId,
        const std::map<std::string, json>& status = {});

    static std::shared_ptr<JSONMessage> CreateStatusUpdate(
        const std::string& senderId,
        const std::string& status,
        const std::map<std::string, json>& extra = {});

    static std::shared_ptr<JSONMessage> CreateStatusUpdate(const std::string& senderId,
        const std::map<std::string, json>& extra);

    static std::shared_ptr<JSONMessage> CreateCommand(
        const std::string& senderId,
        const std::string& command,
        const std::map<std::string, json>& params = {});

    static std::shared_ptr<JSONMessage> CreateSendMsgByType(
        const std::string& senderId,
        MessageType type,
        const std::map<std::string, json>& extra = {});

    static std::shared_ptr<JSONMessage> CreateSendMsgByType(
        const std::string& senderId,
        MessageType type,
        const json extra);

private:
    MessageType m_type;
    std::string m_senderId;
    std::string m_receiverId;
    std::string m_messageId;
    int64_t m_timestamp;
    json m_data;

    // 生成消息ID
    static std::string GenerateMessageId();
};
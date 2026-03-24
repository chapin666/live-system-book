/**
 * Chapter 27: 安全防护示例
 * JWT Token 管理、加密、内容审核
 */

#include <iostream>
#include <string>
#include <map>
#include <set>
#include <chrono>
#include <sstream>
#include <functional>

// 模拟 Base64 编解码
namespace base64 {
    std::string Encode(const std::string& input) {
        // 简化实现，实际使用库
        return input; 
    }
    std::string Decode(const std::string& input) {
        return input;
    }
}

// 获取当前时间戳
int64_t GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
}

// 生成随机 nonce
std::string GenerateNonce() {
    static int counter = 0;
    return std::to_string(GetCurrentTimestamp()) + "_" + std::to_string(counter++);
}

// JWT Payload 结构
struct JWTPayload {
    std::string user_id;
    std::string room_id;
    int64_t iat;  // 签发时间
    int64_t exp;  // 过期时间
    std::string nonce;
    
    std::string ToJSON() const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"user_id\":\"" << user_id << "\",";
        oss << "\"room_id\":\"" << room_id << "\",";
        oss << "\"iat\":" << iat << ",";
        oss << "\"exp\":" << exp << ",";
        oss << "\"nonce\":\"" << nonce << "\"";
        oss << "}";
        return oss.str();
    }
};

// Token 管理器
class TokenManager {
public:
    explicit TokenManager(const std::string& secret_key) 
        : secret_key_(secret_key) {}
    
    // 生成 Token
    std::string GenerateToken(const std::string& user_id,
                              const std::string& room_id,
                              int expire_seconds = 3600) {
        JWTPayload payload;
        payload.user_id = user_id;
        payload.room_id = room_id;
        payload.iat = GetCurrentTimestamp();
        payload.exp = payload.iat + expire_seconds;
        payload.nonce = GenerateNonce();
        
        return EncodeJWT(payload);
    }
    
    // 验证 Token
    bool VerifyToken(const std::string& token,
                     std::string& out_user_id,
                     std::string& out_room_id) {
        // 1. 检查是否被吊销
        if (revoked_tokens_.count(token)) {
            std::cerr << "Token has been revoked\n";
            return false;
        }
        
        // 2. 解析并验证
        JWTPayload payload;
        if (!DecodeAndVerifyJWT(token, payload)) {
            return false;
        }
        
        // 3. 检查过期时间
        int64_t now = GetCurrentTimestamp();
        if (now > payload.exp) {
            std::cerr << "Token expired\n";
            return false;
        }
        
        out_user_id = payload.user_id;
        out_room_id = payload.room_id;
        return true;
    }
    
    // 吊销 Token
    void RevokeToken(const std::string& token) {
        revoked_tokens_.insert(token);
        std::cout << "Token revoked\n";
    }
    
    // 清理过期吊销记录
    void CleanupRevokedTokens() {
        // 实际实现应检查过期时间
        if (revoked_tokens_.size() > 10000) {
            revoked_tokens_.clear();
        }
    }

private:
    std::string EncodeJWT(const JWTPayload& payload) {
        std::string header = R"({"alg":"HS256","typ":"JWT"})";
        std::string payload_json = payload.ToJSON();
        
        std::string encoded_header = base64::Encode(header);
        std::string encoded_payload = base64::Encode(payload_json);
        
        std::string signature = Sign(encoded_header + "." + encoded_payload);
        
        return encoded_header + "." + encoded_payload + "." + signature;
    }
    
    bool DecodeAndVerifyJWT(const std::string& token, JWTPayload& out_payload) {
        // 解析 token
        size_t first_dot = token.find('.');
        size_t second_dot = token.find('.', first_dot + 1);
        
        if (first_dot == std::string::npos || second_dot == std::string::npos) {
            return false;
        }
        
        std::string header = token.substr(0, first_dot);
        std::string payload = token.substr(first_dot + 1, second_dot - first_dot - 1);
        std::string signature = token.substr(second_dot + 1);
        
        // 验证签名
        std::string expected = Sign(header + "." + payload);
        if (signature != expected) {
            std::cerr << "Invalid signature\n";
            return false;
        }
        
        // 解析 payload (简化)
        out_payload.user_id = "user_123";
        out_payload.room_id = "room_456";
        out_payload.iat = GetCurrentTimestamp();
        out_payload.exp = GetCurrentTimestamp() + 3600;
        
        return true;
    }
    
    std::string Sign(const std::string& data) {
        // 简化实现，实际使用 HMAC-SHA256
        return "signature_" + std::to_string(data.length());
    }
    
    std::string secret_key_;
    std::set<std::string> revoked_tokens_;
};

// 简单内容过滤器
class ContentFilter {
public:
    ContentFilter() {
        // 初始化敏感词列表
        sensitive_words_ = {
            "spam", "abuse", "inappropriate"
        };
    }
    
    // 检查文本内容
    bool CheckText(const std::string& text) {
        std::string lower_text = text;
        for (auto& c : lower_text) c = std::tolower(c);
        
        for (const auto& word : sensitive_words_) {
            if (lower_text.find(word) != std::string::npos) {
                std::cout << "Sensitive word detected: " << word << std::endl;
                return false;
            }
        }
        return true;
    }
    
    // 检查图像 (模拟)
    bool CheckImage(const std::string& image_data) {
        // 实际实现应调用 AI 审核服务
        // 返回 true 表示通过，false 表示违规
        return true;
    }

private:
    std::vector<std::string> sensitive_words_;
};

// 防重放攻击检查器
class ReplayChecker {
public:
    // 检查 nonce 是否已使用
    bool CheckAndRecordNonce(const std::string& nonce) {
        if (used_nonces_.count(nonce)) {
            return false;  // 重放攻击
        }
        
        used_nonces_.insert(nonce);
        
        // 限制存储数量
        if (used_nonces_.size() > 100000) {
            used_nonces_.clear();
        }
        
        return true;
    }

private:
    std::set<std::string> used_nonces_;
};

int main() {
    std::cout << "Chapter 27: 安全防护示例\n";
    std::cout << "========================\n\n";
    
    // Token 管理演示
    TokenManager token_mgr("my_secret_key_12345");
    
    std::string token = token_mgr.GenerateToken("user_123", "room_456", 3600);
    std::cout << "Generated Token: " << token.substr(0, 50) << "...\n";
    
    std::string user_id, room_id;
    if (token_mgr.VerifyToken(token, user_id, room_id)) {
        std::cout << "Token valid: user=" << user_id << ", room=" << room_id << "\n";
    }
    
    // 内容过滤演示
    ContentFilter filter;
    std::cout << "\nContent filter test:\n";
    std::cout << "'Hello world': " << (filter.CheckText("Hello world") ? "PASS" : "BLOCK") << "\n";
    std::cout << "'This is spam': " << (filter.CheckText("This is spam") ? "PASS" : "BLOCK") << "\n";
    
    // 重放检查演示
    ReplayChecker replay;
    std::cout << "\nReplay check test:\n";
    std::cout << "First use of nonce_1: " << (replay.CheckAndRecordNonce("nonce_1") ? "PASS" : "REJECT") << "\n";
    std::cout << "Second use of nonce_1: " << (replay.CheckAndRecordNonce("nonce_1") ? "PASS" : "REJECT") << "\n";
    
    return 0;
}
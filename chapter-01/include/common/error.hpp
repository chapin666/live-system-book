#pragma once

#include <string>
#include <system_error>

namespace live {

// 错误码定义
enum class ErrorCode {
    OK = 0,
    
    // 文件/IO 错误 (1xx)
    FILE_NOT_FOUND = 100,
    FILE_READ_ERROR,
    FILE_WRITE_ERROR,
    FILE_FORMAT_ERROR,
    
    // 网络错误 (2xx)
    NETWORK_ERROR = 200,
    CONNECTION_FAILED,
    CONNECTION_TIMEOUT,
    CONNECTION_CLOSED,
    HTTP_ERROR,
    RTMP_ERROR,
    
    // 解码错误 (3xx)
    DECODE_ERROR = 300,
    DECODER_NOT_FOUND,
    DECODER_OPEN_FAILED,
    DECODER_DECODE_ERROR,
    UNSUPPORTED_CODEC,
    UNSUPPORTED_FORMAT,
    
    // 编码错误 (4xx)
    ENCODE_ERROR = 400,
    ENCODER_NOT_FOUND,
    ENCODER_OPEN_FAILED,
    ENCODER_ENCODE_ERROR,
    
    // 渲染错误 (5xx)
    RENDER_ERROR = 500,
    DISPLAY_INIT_FAILED,
    SDL_ERROR,
    
    // 采集错误 (6xx)
    CAPTURE_ERROR = 600,
    CAMERA_NOT_FOUND,
    MICROPHONE_NOT_FOUND,
    CAPTURE_INIT_FAILED,
    
    // 参数/状态错误 (7xx)
    INVALID_ARGUMENT = 700,
    INVALID_STATE,
    BUFFER_FULL,
    BUFFER_EMPTY,
    OUT_OF_MEMORY,
    
    // 未知错误
    UNKNOWN = 999
};

// 错误码分类
inline bool IsFileError(ErrorCode code) {
    return static_cast<int>(code) >= 100 && static_cast<int>(code) < 200;
}

inline bool IsNetworkError(ErrorCode code) {
    return static_cast<int>(code) >= 200 && static_cast<int>(code) < 300;
}

inline bool IsDecodeError(ErrorCode code) {
    return static_cast<int>(code) >= 300 && static_cast<int>(code) < 400;
}

// 错误类
class Error {
public:
    Error() : code_(ErrorCode::OK) {}
    explicit Error(ErrorCode code, const std::string& message = "") 
        : code_(code), message_(message) {}
    
    // 静态工厂方法
    static Error OK() { return Error(); }
    static Error FileNotFound(const std::string& path) {
        return Error(ErrorCode::FILE_NOT_FOUND, "File not found: " + path);
    }
    static Error NetworkError(const std::string& detail) {
        return Error(ErrorCode::NETWORK_ERROR, "Network error: " + detail);
    }
    static Error DecodeError(const std::string& detail) {
        return Error(ErrorCode::DECODE_ERROR, "Decode error: " + detail);
    }
    
    // 查询
    bool IsOK() const { return code_ == ErrorCode::OK; }
    bool IsError() const { return code_ != ErrorCode::OK; }
    ErrorCode Code() const { return code_; }
    const std::string& Message() const { return message_; }
    
    // 类型检查
    bool IsFileError() const { return live::IsFileError(code_); }
    bool IsNetworkError() const { return live::IsNetworkError(code_); }
    bool IsDecodeError() const { return live::IsDecodeError(code_); }
    
    // 转换为字符串
    std::string ToString() const {
        if (IsOK()) return "OK";
        return "Error[" + std::to_string(static_cast<int>(code_)) + "]: " + message_;
    }
    
    // 布尔转换（方便 if(error) 判断）
    explicit operator bool() const { return IsError(); }
    
private:
    ErrorCode code_;
    std::string message_;
};

// 错误码转字符串
inline const char* ErrorCodeToString(ErrorCode code) {
    switch (code) {
        case ErrorCode::OK: return "OK";
        case ErrorCode::FILE_NOT_FOUND: return "FILE_NOT_FOUND";
        case ErrorCode::FILE_READ_ERROR: return "FILE_READ_ERROR";
        case ErrorCode::FILE_WRITE_ERROR: return "FILE_WRITE_ERROR";
        case ErrorCode::FILE_FORMAT_ERROR: return "FILE_FORMAT_ERROR";
        case ErrorCode::NETWORK_ERROR: return "NETWORK_ERROR";
        case ErrorCode::CONNECTION_FAILED: return "CONNECTION_FAILED";
        case ErrorCode::CONNECTION_TIMEOUT: return "CONNECTION_TIMEOUT";
        case ErrorCode::CONNECTION_CLOSED: return "CONNECTION_CLOSED";
        case ErrorCode::HTTP_ERROR: return "HTTP_ERROR";
        case ErrorCode::RTMP_ERROR: return "RTMP_ERROR";
        case ErrorCode::DECODE_ERROR: return "DECODE_ERROR";
        case ErrorCode::DECODER_NOT_FOUND: return "DECODER_NOT_FOUND";
        case ErrorCode::DECODER_OPEN_FAILED: return "DECODER_OPEN_FAILED";
        case ErrorCode::DECODER_DECODE_ERROR: return "DECODER_DECODE_ERROR";
        case ErrorCode::UNSUPPORTED_CODEC: return "UNSUPPORTED_CODEC";
        case ErrorCode::UNSUPPORTED_FORMAT: return "UNSUPPORTED_FORMAT";
        case ErrorCode::ENCODE_ERROR: return "ENCODE_ERROR";
        case ErrorCode::ENCODER_NOT_FOUND: return "ENCODER_NOT_FOUND";
        case ErrorCode::ENCODER_OPEN_FAILED: return "ENCODER_OPEN_FAILED";
        case ErrorCode::ENCODER_ENCODE_ERROR: return "ENCODER_ENCODE_ERROR";
        case ErrorCode::RENDER_ERROR: return "RENDER_ERROR";
        case ErrorCode::DISPLAY_INIT_FAILED: return "DISPLAY_INIT_FAILED";
        case ErrorCode::SDL_ERROR: return "SDL_ERROR";
        case ErrorCode::CAPTURE_ERROR: return "CAPTURE_ERROR";
        case ErrorCode::CAMERA_NOT_FOUND: return "CAMERA_NOT_FOUND";
        case ErrorCode::MICROPHONE_NOT_FOUND: return "MICROPHONE_NOT_FOUND";
        case ErrorCode::CAPTURE_INIT_FAILED: return "CAPTURE_INIT_FAILED";
        case ErrorCode::INVALID_ARGUMENT: return "INVALID_ARGUMENT";
        case ErrorCode::INVALID_STATE: return "INVALID_STATE";
        case ErrorCode::BUFFER_FULL: return "BUFFER_FULL";
        case ErrorCode::BUFFER_EMPTY: return "BUFFER_EMPTY";
        case ErrorCode::OUT_OF_MEMORY: return "OUT_OF_MEMORY";
        case ErrorCode::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN_ERROR_CODE";
    }
}

// 流输出
inline std::ostream& operator<<(std::ostream& os, const Error& err) {
    os << err.ToString();
    return os;
}

} // namespace live

// 宏简化错误检查
#define RETURN_IF_ERROR(expr) \
    do { \
        auto _err = (expr); \
        if (_err.IsError()) return _err; \
    } while(0)

#define RETURN_ERROR(code, msg) \
    return live::Error(live::ErrorCode::code, msg)

#define RETURN_OK() \
    return live::Error()

#pragma once

#include <string>
#include <sstream>
#include <fstream>
#include <memory>
#include <mutex>
#include <chrono>
#include <iomanip>

namespace quant_hub {

enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL
};

class Logger {
public:
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    void setLogFile(const std::string& filename) {
        std::lock_guard<std::mutex> lock(mutex_);
        logFile_.open(filename, std::ios::app);
    }

    void setLogLevel(LogLevel level) {
        logLevel_ = level;
    }

    template<typename... Args>
    void log(LogLevel level, const char* file, int line, Args&&... args) {
        if (level < logLevel_) return;

        std::stringstream ss;
        ss << getCurrentTimestamp() << " "
           << std::setw(7) << std::left << levelToString(level) << " "
           << "[" << file << ":" << line << "] ";
        (ss << ... << std::forward<Args>(args)) << std::endl;

        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << ss.str();
        if (logFile_.is_open()) {
            logFile_ << ss.str();
            logFile_.flush();
        }
    }

private:
    Logger() : logLevel_(LogLevel::INFO) {}
    ~Logger() {
        if (logFile_.is_open()) {
            logFile_.close();
        }
    }

    static std::string getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S")
           << '.' << std::setfill('0') << std::setw(3) << now_ms.count();
        return ss.str();
    }

    static const char* levelToString(LogLevel level) {
        switch (level) {
            case LogLevel::TRACE:   return "TRACE";
            case LogLevel::DEBUG:   return "DEBUG";
            case LogLevel::INFO:    return "INFO";
            case LogLevel::WARNING: return "WARN";
            case LogLevel::ERROR:   return "ERROR";
            case LogLevel::FATAL:   return "FATAL";
            default:                return "UNKNOWN";
        }
    }

    std::mutex mutex_;
    std::ofstream logFile_;
    LogLevel logLevel_;
};

#define LOG_TRACE(...) \
    Logger::getInstance().log(LogLevel::TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(...) \
    Logger::getInstance().log(LogLevel::DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...) \
    Logger::getInstance().log(LogLevel::INFO, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARNING(...) \
    Logger::getInstance().log(LogLevel::WARNING, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) \
    Logger::getInstance().log(LogLevel::ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_FATAL(...) \
    Logger::getInstance().log(LogLevel::FATAL, __FILE__, __LINE__, __VA_ARGS__)

} // namespace quant_hub

#pragma once

#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <fstream>
#include <stdexcept>
#include <sstream>

namespace quant_hub {

class Config {
public:
    static Config& getInstance() {
        static Config instance;
        return instance;
    }

    void loadFromFile(const std::string& filename) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open config file: " + filename);
        }

        std::string line;
        while (std::getline(file, line)) {
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);
                config_[key] = value;
            }
        }
    }

    void saveToFile(const std::string& filename) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::ofstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file for writing: " + filename);
        }

        for (const auto& [key, value] : config_) {
            file << key << "=" << value << "\n";
        }
    }

    template<typename T>
    T get(const std::string& path) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = config_.find(path);
        if (it == config_.end()) {
            throw std::runtime_error("Config path not found: " + path);
        }
        return convertValue<T>(it->second);
    }

    template<typename T>
    T get(const std::string& path, const T& defaultValue) const {
        try {
            return get<T>(path);
        } catch (const std::exception&) {
            return defaultValue;
        }
    }

    template<typename T>
    void set(const std::string& path, const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::stringstream ss;
        ss << value;
        config_[path] = ss.str();
    }

private:
    Config() = default;
    
    template<typename T>
    T convertValue(const std::string& str) const;

    mutable std::mutex mutex_;
    std::map<std::string, std::string> config_;
};

template<>
inline std::string Config::convertValue<std::string>(const std::string& str) const {
    return str;
}

template<>
inline int Config::convertValue<int>(const std::string& str) const {
    return std::stoi(str);
}

template<>
inline double Config::convertValue<double>(const std::string& str) const {
    return std::stod(str);
}

template<>
inline bool Config::convertValue<bool>(const std::string& str) const {
    return str == "true" || str == "1";
}

} // namespace quant_hub

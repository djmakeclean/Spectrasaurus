#pragma once

#include <fstream>
#include <mutex>
#include <string>
#include <sstream>

class DebugLogger
{
public:
    static DebugLogger& getInstance()
    {
        static DebugLogger instance;
        return instance;
    }

    void log(const std::string& message)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (logFile.is_open())
        {
            logFile << message << std::endl;
            logFile.flush(); // Ensure it writes immediately
        }
    }

    template<typename... Args>
    void logf(Args... args)
    {
        std::ostringstream oss;
        (oss << ... << args);
        log(oss.str());
    }

private:
    DebugLogger()
    {
        // Absolute path to the project directory
        std::string logPath = "/Users/demichel/Documents/Spectrasaurus/debug_log.txt";
        logFile.open(logPath, std::ios::out | std::ios::trunc);
        if (logFile.is_open())
        {
            log("=== Spectrasaurus Debug Log Started ===");
        }
    }

    ~DebugLogger()
    {
        if (logFile.is_open())
        {
            log("=== Spectrasaurus Debug Log Ended ===");
            logFile.close();
        }
    }

    std::ofstream logFile;
    std::mutex mutex;

    DebugLogger(const DebugLogger&) = delete;
    DebugLogger& operator=(const DebugLogger&) = delete;
};

// Convenience macro
#define DEBUG_LOG(...) DebugLogger::getInstance().logf(__VA_ARGS__)

//
// Created by jim on 12 Jun 2026.
//

#ifndef CPPSCRIPT_LOG_HPP
#define CPPSCRIPT_LOG_HPP

#include <iostream>


class Endl_class { };

static Endl_class Endl;

class Log
{
public:
    enum LogLevel : int { None = -1, Error, Warning, Info, Debug };

    Log(LogLevel level) : log_level(level)
    {
        switch (level)
        {
            case Error: if (max_level >= Error) std::cerr << " [ERR] ";
                break;
            case Warning: if (max_level >= Warning) std::cerr << " [WRN] ";
                break;
            case Info: if (max_level >= Info) std::cerr << " [INF] ";
                break;
            case Debug: if (max_level >= Debug) std::cerr << " [DBG] ";
                break;
        }
    }

    Log() = delete;

    static void Set_LogLevel(LogLevel level) { max_level = level; }

private:
    static LogLevel max_level;

    LogLevel log_level;

    bool is_endl = false;

    friend Log&& operator<<(Log&& log, const Endl_class&);

    template <typename Out>
    friend Log&& operator<<(Log&& log, const Out& out);
};

Log::LogLevel Log::max_level = Log::None;


inline Log&& operator<<(Log&& log, const Endl_class&)
{
    if (Log::max_level < log.log_level)
        return std::move(log);

    std::cerr << std::endl;
    log.is_endl = true;
    return std::move(log);
}

template <typename Out>
Log&& operator<<(Log&& log, const Out& out)
{
    if (Log::max_level < log.log_level)
        return std::move(log);

    if (log.is_endl)
    {
        if (Log::max_level > 0)
            std::cerr << "         "; // Append spacing
        log.is_endl = false;
    }
    std::cerr << out;
    return std::move(log);
}


#define ERR Log(Log::LogLevel::Error)
#define WARN Log(Log::LogLevel::Warning)
#define INFO Log(Log::LogLevel::Info)
#define DEBUG Log(Log::LogLevel::Debug)


#endif //CPPSCRIPT_LOG_HPP

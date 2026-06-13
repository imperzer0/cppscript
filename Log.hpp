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
    // Log Level
    enum Level : int { None = -1, Error, Warning, Info, Debug };

    Log(Level level) : log_level(level)
    {
        // Print log level before printing the content
        // Meant to be used like this:
        // Log(LEVEL) << "String" << number << Endl;
        //
        // Always use Endl object for '\n'.

        switch (level)
        {
            // If Error (0) is enabled
            case Error: if (max_level >= Error) std::cerr << " [ERR] ";
                break;
            // If Warning (1) is enabled
            case Warning: if (max_level >= Warning) std::cerr << " [WRN] ";
                break;
            // If Info (2) is enabled
            case Info: if (max_level >= Info) std::cerr << " [INF] ";
                break;
            // If Debug (3) is enabled
            case Debug: if (max_level >= Debug) std::cerr << " [DBG] ";
                break;
        }
    }

    Log() = delete;

    static void Set_LogLevel(Level level) { max_level = level; }

private:
    static Level max_level;

    Level log_level; // Used to block output when some log levels are disabled

    bool is_endl = false;

    friend Log&& operator<<(Log&& log, const Endl_class&);

    template <typename Out>
    friend Log&& operator<<(Log&& log, const Out& out);
};

Log::Level Log::max_level = Log::None;

// Without Endl:
// [ERR] some error \n
// new line starts from here;
//
// With Endl:
// [ERR] some error \n
//       new line starts from here;
//
// Handle custom endline object
inline Log&& operator<<(Log&& log, const Endl_class&)
{
    // Don't print anything if this log level is disabled
    if (Log::max_level < log.log_level)
        return std::move(log);

    std::cerr << std::endl;
    log.is_endl = true;
    return std::move(log);
}

// Matches any type and passes it to std::ostream object
template <typename Out>
Log&& operator<<(Log&& log, const Out& out)
{
    // Don't print anything if this log level is disabled
    if (Log::max_level < log.log_level)
        return std::move(log);

    if (log.is_endl) // Previous line ended with Endl
    {
        if (Log::max_level > 0)
            std::cerr << "         "; // Append spacing
        log.is_endl = false; // Reset
    }
    std::cerr << out;
    return std::move(log);
}


#define ERR Log(Log::Level::Error)
#define WARN Log(Log::Level::Warning)
#define INFO Log(Log::Level::Info)
#define DEBUG Log(Log::Level::Debug)


#endif //CPPSCRIPT_LOG_HPP

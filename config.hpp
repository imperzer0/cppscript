//
// Created by jim on 12 Jun 2026.
//

#pragma once

#include <toml++/toml.hpp>

#include "Log.hpp"
#include "wrappers.cpp"

#define APPNAME "cppscript"
#define CONFIG_PATH "/etc/" APPNAME "/"
#define MAIN_CONFIG "config.toml"

class Config // Interface
{
protected:
    toml::table config;

    Config(const std::string& file) { config = toml::parse_file(file); }
};

class MainConfig : Config
{
    static MainConfig* instance;

    MainConfig() : Config(CONFIG_PATH MAIN_CONFIG) { }

public:
    static MainConfig& Instance()
    {
        if (!instance)
            instance = new MainConfig();
        return *instance;
    }

    std::string get_cache_folder_path()
    {
        auto toml_path = config["cache"]["folder"];

        constexpr const char* DEFAULT = "~/.cache/" APPNAME;

        std::string val;
        if (toml_path.type() == toml::node_type::string)
            val = toml_path.value_or(DEFAULT);
        else
            val = DEFAULT;

        return std::move(val);
    }


    Log::LogLevel get_log_level()
    {
        auto toml_path = config["log_level"];
        if (toml_path.type() == toml::node_type::integer)
            return toml_path.value_or(Log::None);

        if (toml_path.type() == toml::node_type::string)
        {
            std::string val = toml_path.value_or("None");
            Log::LogLevel log_level;
            if (val == "Error")
                log_level = Log::Error;
            else if (val == "Warning")
                log_level = Log::Warning;
            else if (val == "Info")
                log_level = Log::Info;
            else if (val == "Debug")
                log_level = Log::Debug;
            else
                log_level = Log::None;
            return log_level;
        }

        return Log::None;
    }
};

MainConfig* MainConfig::instance = nullptr;

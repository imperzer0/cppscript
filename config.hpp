//
// Created by jim on 12 Jun 2026.
//

#pragma once

#include <toml++/toml.hpp>

#include "Log.hpp"
#include "constants.h"

#define CONFIG_PATH "/etc/" APPNAME "/"
#define MAIN_CONFIG "config.toml"

// Interface
// Meant to be extended, only handles config parsing
class Config
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
    // Singleton Instance
    static MainConfig& Instance()
    {
        if (instance == nullptr)
            instance = new MainConfig(); // Create only one instance
        return *instance;
    }

    // Get Cache Folder Path from main config
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


    // Get Log Level from main config
    // In the config it can be a string or an integer
    Log::Level get_log_level()
    {
        auto toml_path = config["log_level"];
        if (toml_path.type() == toml::node_type::integer)
            return toml_path.value_or(Log::None);

        if (toml_path.type() == toml::node_type::string)
        {
            std::string val = toml_path.value_or("None");
            Log::Level log_level;
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

        return Log::None; // If something went wrong just leave it off
    }

    using CXX_Flags = std::vector<std::string>;

    CXX_Flags get_cxx_flags()
    {
        auto toml_path = config["cxx"]["flags"];

        if (toml_path.type() != toml::node_type::array)
        {
            // Not an array
            if (toml_path.type() == toml::node_type::string)
                return {toml_path.value_or("")}; // A string
            return CXX_Flags{ };                  // Something else - return empty array
        }

        CXX_Flags res = { };

        for (const auto& arg : *toml_path.as_array())
            if (arg.type() == toml::node_type::string)
                res.push_back(arg.value_or(""));

        return std::move(res);
    }
};

MainConfig* MainConfig::instance = nullptr;

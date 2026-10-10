//
// Created by jim on 12 Jun 2026.
//

#pragma once

#include <toml++/toml.hpp>

#include "Log.hpp"
#include <constants.h>

#define CONFIG_PATH "/etc/" APPNAME "/"
#define MAIN_CONFIG "config.toml"

// Interface
// Meant to be extended, only handles config parsing
class Config
{
protected:
    toml::table config;

    Config(const std::string& file);
};

class MainConfig : Config
{
    static MainConfig* instance;

    MainConfig();

public:
    // Singleton Instance
    static MainConfig& Instance();

    // Get Cache Folder Path from main config
    std::string get_cache_folder_path();


    // Get Log Level from main config
    // In the config it can be a string or an integer
    Log::Level get_log_level();

    using CXX_Flags = std::vector<std::string>;

    CXX_Flags get_cxx_flags();
};

// Config.h - reads config/config.json (or the file named in the LAB_CONFIG env variable).
// Relative paths inside the file are resolved relative to the folder that contains the config file,
// so the server finds the database no matter which folder you start it from.
#pragma once
#include <string>

struct Config {
    std::string listenAddress = "127.0.0.1";
    int port = 8080;
    std::string dbPath;
    std::string schemaFile;
    std::string sampleFile;
    bool loadSampleData = true;
    std::string frontendDir;
    int sessionTimeoutMinutes = 30;
};

Config loadConfig();

#include "Config.h"

#include <json/json.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;

static std::string resolve(const fs::path& base, const std::string& p) {
    fs::path path(p);
    if (path.is_relative()) path = base / path;
    return fs::weakly_canonical(path).string();
}

Config loadConfig() {
    const char* env = std::getenv("LAB_CONFIG");
    fs::path file = env ? fs::path(env) : fs::path("config/config.json");
    std::ifstream in(file);
    if (!in) {
        throw std::runtime_error("Config file not found: " + file.string() +
                                 "  (copy config/config.example.json to config/config.json, or set LAB_CONFIG)");
    }
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;
    if (!Json::parseFromStream(builder, in, &root, &errs)) throw std::runtime_error("Bad config JSON: " + errs);

    fs::path base = fs::absolute(file).parent_path();
    Config c;
    c.listenAddress = root.get("listen_address", "127.0.0.1").asString();
    c.port = root.get("port", 8080).asInt();
    c.dbPath = resolve(base, root.get("db_path", "../database/LabEquipmentDB.db").asString());
    c.schemaFile = resolve(base, root.get("schema_file", "../database/schema.sql").asString());
    c.sampleFile = resolve(base, root.get("sample_file", "../database/sample_data.sql").asString());
    c.loadSampleData = root.get("load_sample_data", true).asBool();
    c.frontendDir = resolve(base, root.get("frontend_dir", "../frontend").asString());
    c.sessionTimeoutMinutes = root.get("session_timeout_minutes", 30).asInt();
    return c;
}

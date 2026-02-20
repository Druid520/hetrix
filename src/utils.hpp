#pragma once
#include <string>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <filesystem>
#include <sstream>
#include <fstream>
#include <iostream>
#include "logger.hpp"

namespace fs = std::filesystem;

namespace Utils {

//home / workspace paths

inline std::string homeDir() {
    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') return std::string(home);
    struct passwd* pw = getpwuid(getuid());
    if (pw) return std::string(pw->pw_dir);
    return "/tmp";
}

//main directory: ~/hetrix/
inline std::string hetrixDir() {
    const char* env = std::getenv("HETRIX_DIR");
    if (env && env[0] != '\0') return std::string(env);
    return homeDir() + "/hetrix";
}

inline std::string binDir() {
    const char* env = std::getenv("HETRIX_BIN");
    if (env && env[0] != '\0') return std::string(env);
    return hetrixDir() + "/bin";
}

inline std::string workspaceDir() {
    const char* env = std::getenv("HETRIX_WORKSPACE");
    if (env && env[0] != '\0') return std::string(env);
    return hetrixDir() + "/workspace";
}

inline std::string reposDir() {
    return hetrixDir() + "/repos";
}

inline std::string indexPath() {
    const char* env = std::getenv("HETRIX_INDEX");
    if (env && env[0] != '\0') return std::string(env);
    return hetrixDir() + "/index.json";
}

//identity + theft

inline bool isRoot() {
    return getuid() == 0;
}

inline std::string installDir() {
    if (isRoot()) return "/usr/local/bin";
    return binDir();
}

//command avalability

inline bool commandExists(const std::string& cmd) {
    return std::system(("command -v " + cmd + " >/dev/null 2>&1").c_str()) == 0;
}

//gigitty script

//ru command with full output streaming (verbose)
inline int runCommand(const std::string& cmd, const std::string& workdir = "") {
    std::string full = workdir.empty() ? cmd : "cd " + workdir + " && " + cmd;
    Logger::detail("$ " + full);
    return std::system(full.c_str());
}

//run command, capture stdout+stderr to string (silent)
inline std::string captureCommand(const std::string& cmd) {
    std::string result;
    FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");
    if (!pipe) return "";
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe)) result += buf;
    pclose(pipe);
    return result;
}

//filesystem helpers

inline bool mkdirp(const std::string& path) {
    try {
        fs::create_directories(path);
        return true;
    } catch (const std::exception& e) {
        Logger::err("mkdir failed: " + path + " — " + e.what());
        return false;
    }
}

inline bool rmrf(const std::string& path) {
    try {
        if (fs::exists(path)) fs::remove_all(path);
        return true;
    } catch (const std::exception& e) {
        Logger::err("rmrf failed: " + path + " — " + e.what());
        return false;
    }
}

inline bool exists(const std::string& path) {
    return fs::exists(path);
}

inline std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

inline bool writeFile(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << content;
    return true;
}

//haystack in a needle

inline std::vector<std::string> findFiles(const std::string& dir, const std::string& ext) {
    std::vector<std::string> result;
    if (!fs::exists(dir)) return result;
    for (auto& e : fs::recursive_directory_iterator(dir,
            fs::directory_options::skip_permission_denied)) {
        if (e.is_regular_file() && e.path().extension() == ext)
            result.push_back(e.path().string());
    }
    return result;
}

inline std::vector<std::string> findExecutables(const std::string& dir) {
    std::vector<std::string> result;
    if (!fs::exists(dir)) return result;
    for (auto& e : fs::recursive_directory_iterator(dir,
            fs::directory_options::skip_permission_denied)) {
        if (!e.is_regular_file()) continue;
        auto perms = e.status().permissions();
        if ((perms & fs::perms::owner_exec) != fs::perms::none) {
            std::string ext = e.path().extension();
            //skip script, lib, object files
            if (ext == ".sh" || ext == ".py" || ext == ".so" || //forgot why .py is here
                ext == ".a"  || ext == ".o"  || ext == ".la" ||
                ext == ".pc" || ext == ".cmake") continue;
            //skip hidden files
            if (e.path().filename().string()[0] == '.') continue;
            result.push_back(e.path().string());
        }
    }
    return result;
}

//string helpers

inline std::string trim(const std::string& s) {
    auto a = s.find_first_not_of(" \t\n\r");
    if (a == std::string::npos) return "";
    auto b = s.find_last_not_of(" \t\n\r");
    return s.substr(a, b - a + 1);
}

//binary dir on PATH check
//my mind is a infinite sanctum of knowledge
inline bool isOnPath(const std::string& dir) {
    const char* pathEnv = std::getenv("PATH");
    if (!pathEnv) return false;
    std::string pathStr(pathEnv);
    std::string token;
    std::istringstream ss(pathStr);
    while (std::getline(ss, token, ':')) {
        if (token == dir) return true;
    }
    return false;
}

} //namespace Utils. i really dont see them

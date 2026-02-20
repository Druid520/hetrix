#pragma once
#include "config.hpp"
#include "logger.hpp"
#include "utils.hpp"
#include <string>
#include <map>
#include <optional>
#include <stdexcept>
#include <sstream>
#include <filesystem>
#include <unistd.h>

namespace fs = std::filesystem;

//lazy ass JSON lexer / parser (handles the index.json string-value subset)
//supports bare true/false/number values mapped to strings.

namespace JsonParser {

struct Token {
    enum Type { LBRACE, RBRACE, COLON, COMMA, STRING, END } type;
    std::string value;
};

class Lexer {
    const std::string& src;
    size_t pos = 0;
public:
    explicit Lexer(const std::string& s) : src(s) {}

    void skipWS() {
        while (pos < src.size() && std::isspace((unsigned char)src[pos])) ++pos;
        //skip // line comments
        if (pos + 1 < src.size() && src[pos] == '/' && src[pos+1] == '/') {
            while (pos < src.size() && src[pos] != '\n') ++pos;
            skipWS();
        }
    }

    std::string parseString() {
        ++pos; // skip opening "
        std::string r;
        while (pos < src.size() && src[pos] != '"') {
            if (src[pos] == '\\' && pos + 1 < src.size()) {
                ++pos;
                switch (src[pos]) {
                    case '"':  r += '"';  break;
                    case '\\': r += '\\'; break;
                    case '/':  r += '/';  break;
                    case 'n':  r += '\n'; break;
                    case 't':  r += '\t'; break;
                    default:   r += src[pos]; break;
                }
            } else { r += src[pos]; }
            ++pos;
        }
        ++pos; //skip closing "
        return r;
    }

    Token next() {
        skipWS();
        if (pos >= src.size()) return {Token::END, ""};
        char c = src[pos];
        if (c == '{') { ++pos; return {Token::LBRACE, ""}; }
        if (c == '}') { ++pos; return {Token::RBRACE, ""}; }
        if (c == ':') { ++pos; return {Token::COLON,  ""}; }
        if (c == ',') { ++pos; return {Token::COMMA,  ""}; }
        if (c == '"') return {Token::STRING, parseString()};
        //bare value: true/false/number — read until delimiter
        std::string bare;
        while (pos < src.size() && src[pos] != ',' && src[pos] != '}'
               && src[pos] != '\n' && !std::isspace((unsigned char)src[pos])) {
            bare += src[pos++];
        }
        if (!bare.empty()) return {Token::STRING, bare};
        throw std::runtime_error(std::string("Unexpected char '") + c
                                 + "' at pos " + std::to_string(pos));
    }

    Token peek() { size_t s = pos; Token t = next(); pos = s; return t; }
};

inline std::map<std::string, std::string> parseObject(Lexer& lex) {
    std::map<std::string, std::string> obj;
    if (lex.next().type != Token::LBRACE) throw std::runtime_error("Expected '{'");
    while (true) {
        auto t = lex.peek();
        if (t.type == Token::RBRACE) { lex.next(); break; }
        if (t.type == Token::COMMA)  { lex.next(); continue; }
        auto key = lex.next();
        if (key.type != Token::STRING) throw std::runtime_error("Expected key string");
        if (lex.next().type != Token::COLON) throw std::runtime_error("Expected ':'");
        auto val = lex.next();
        if (val.type != Token::STRING)
            throw std::runtime_error("Expected value for key: " + key.value);
        obj[key.value] = val.value;
    }
    return obj;
}

inline PackageIndex parse(const std::string& json) {
    PackageIndex idx;
    Lexer lex(json);
    if (lex.next().type != Token::LBRACE) throw std::runtime_error("Expected top-level '{}'");
    while (true) {
        auto t = lex.peek();
        if (t.type == Token::RBRACE) { lex.next(); break; }
        if (t.type == Token::COMMA)  { lex.next(); continue; }
        auto name = lex.next();
        if (name.type != Token::STRING) throw std::runtime_error("Expected package name");
        if (lex.next().type != Token::COLON) throw std::runtime_error("Expected ':'");

        auto f = parseObject(lex);

        //skip comment/section keys
        if (name.value.empty() || name.value[0] == '_') continue;

        PackageEntry e;
        e.name          = name.value;
        e.url           = f.count("url")            ? f["url"]            : "";
        e.pull_type     = f.count("pull_type")      ? f["pull_type"]      : "auto";
        e.version       = f.count("version")        ? f["version"]        : "";
        e.build         = f.count("build")          ? f["build"]          : "auto";
        e.args          = f.count("args")           ? f["args"]           : "";
        e.main_file     = f.count("main")           ? f["main"]           : "";
        e.sub_dir       = f.count("sub_dir")        ? f["sub_dir"]        : "";
        e.build_dir     = f.count("build_dir")      ? f["build_dir"]      : "";
        e.output        = f.count("output")         ? f["output"]         : name.value;
        e.install_cmd   = f.count("install_cmd")    ? f["install_cmd"]    : "";
        e.auto_add_cmds = f.count("auto_add_cmds")  ? f["auto_add_cmds"]  : "1";
        e.description   = f.count("description")    ? f["description"]    : "";

        idx[name.value] = e;
    }
    return idx;
}

} //namespace JsonParser
//jsonparser tried to touch me
//ConfigManager 
//im watching south park fuck off
class ConfigManager {
    std::string idxPath;

    static std::string binaryDir() {
        char buf[4096] = {};
        if (::readlink("/proc/self/exe", buf, sizeof(buf)-1) > 0)
            return fs::path(buf).parent_path().string();
        return "";
    }

    std::string findSeedIndex() const {
        for (auto& dir : {binaryDir(), fs::current_path().string()}) {
            if (dir.empty()) continue;
            std::string p = dir + "/index.json";
            if (Utils::exists(p) && p != idxPath) return p;
        }
        return "";
    }

    static std::string blankTemplate() {
        return R"({
  "_doc": "hetrix index.json  --  see README.txt for all fields.",
  "_example": {
    "url":           "https://github.com/user/repo",
    "pull_type":     "auto",
    "build":         "auto",
    "args":          "",
    "sub_dir":       "",
    "output":        "binary-name",
    "version":       "",
    "auto_add_cmds": "1",
    "description":   "short description"
  }
}
)";
    }

public:
    explicit ConfigManager() : idxPath(Utils::indexPath()) {}

    PackageIndex load() const {
        if (!Utils::exists(idxPath)) {
            Logger::warn("No index.json at " + idxPath + "  --  run: hetrix init");
            return {};
        }
        std::string raw = Utils::readFile(idxPath);
        if (raw.empty()) { Logger::warn("index.json is empty."); return {}; }
        try {
            return JsonParser::parse(raw);
        } catch (const std::exception& ex) {
            Logger::err("JSON parse error in " + idxPath);
            Logger::err("  " + std::string(ex.what()));
            Logger::info("Run: python3 -m json.tool " + idxPath);
            return {};
        }
    }

    std::optional<PackageEntry> find(const std::string& name) const {
        auto idx = load();
        auto it  = idx.find(name);
        if (it == idx.end()) return std::nullopt;
        return it->second;
    }

    bool initIndex() {
        if (Utils::exists(idxPath)) {
            Logger::info("index.json already exists -- left untouched.");
            Logger::info("Path: " + idxPath);
            return true;
        }
        std::string seed = findSeedIndex();
        if (!seed.empty()) {
            Logger::step("Seeding index.json from: " + seed);
            try {
                fs::copy_file(seed, idxPath, fs::copy_options::overwrite_existing);
                Logger::ok("Copied -> " + idxPath);
                return true;
            } catch (const std::exception& ex) {
                Logger::warn("Copy failed: " + std::string(ex.what()) + "  --  writing template.");
            }
        }
        Logger::warn("No seed index.json found -- writing blank template.");
        bool ok = Utils::writeFile(idxPath, blankTemplate());
        if (ok) Logger::ok("Template written: " + idxPath);
        return ok;
    }

    void setIndexPath(const std::string& p) { idxPath = p; }
    const std::string& getIndexPath() const  { return idxPath; }
};

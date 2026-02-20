#pragma once
#include "config.hpp"
#include "logger.hpp"
#include "utils.hpp"
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

//fetcher  --  downloads source via git or curl, pull_type has 5 stars in gta

class Fetcher {
    PackageEntry pkg;
    std::string  workDir;

    //sniff the world wide web

    bool looksLikeGit(const std::string& url) const {
        return url.find("github.com")    != std::string::npos //no gitlab? you stupid motherfucker.
            || url.find("gitlab.com")    != std::string::npos
            || url.find("bitbucket.org") != std::string::npos
            || url.find("codeberg.org")  != std::string::npos
            || url.find("sr.ht")         != std::string::npos
            || url.find(".git")          != std::string::npos;
    }

    bool looksLikeArchive(const std::string& url) const {
        return url.find(".tar.gz")  != std::string::npos
            || url.find(".tgz")     != std::string::npos
            || url.find(".tar.bz2") != std::string::npos
            || url.find(".tar.xz")  != std::string::npos
            || url.find(".zip")     != std::string::npos;
    }

    //version / tag detection

    std::string detectLatestVersion(const std::string& url) const {
        if (!Utils::commandExists("git")) return "";
        std::string out = Utils::captureCommand(
            "git ls-remote --tags --refs " + url + " 2>/dev/null"
            " | awk -F'/' '{print $NF}'"
            " | grep -E '^v?[0-9]+\\.[0-9]'"
            " | sort -V | tail -1");
        return Utils::trim(out);
    }

    //git clone

    bool fetchGit(const std::string& dest) {
        if (!Utils::commandExists("git")) {
            Logger::err("'git' is not installed. Install it and retry.");
            return false;
        }

        std::string tag = pkg.version.empty() ? detectLatestVersion(pkg.url) : pkg.version;
        if (!tag.empty())
            Logger::info("version detected: " + tag);
        else
            Logger::info("no version tag found -- cloning default branch (HEAD).");

        Logger::fetching("cloning " + pkg.url + " ...");
        Logger::info("please wait...");
        Logger::blank();

        std::string cmd = "git clone --recurse-submodules --progress ";
        if (!tag.empty())
            cmd += "--branch " + tag + " --depth 1 ";
        else
            cmd += "--depth 1 ";
        cmd += pkg.url + " " + dest;

        if (Utils::runCommand(cmd) != 0) {
            Logger::blank();
            Logger::err("git clone failed.");
            return false;
        }
        Logger::blank();
        Logger::ok("clone complete  ->  " + dest);
        return true;
    }

    //curl download (archive or direct binary. breaks philosophy but ok)

    bool fetchCurl(const std::string& dest) {
        if (!Utils::commandExists("curl")) {
            Logger::err("'curl' is not installed. Install it and retry.");
            return false;
        }

        const std::string& url = pkg.url;

        //direct binary (not an archive)
        if (!looksLikeArchive(url)) {
            Utils::mkdirp(dest);
            std::string outBin = dest + "/" + (pkg.output.empty() ? pkg.name : pkg.output);
            Logger::fetching("downloading binary: " + url);
            Logger::info("please wait...");
            Logger::blank();
            if (Utils::runCommand("curl -L --progress-bar -o " + outBin + " " + url) != 0) {
                Logger::err("curl download failed."); return false;
            }
            Logger::blank();
            Utils::runCommand("chmod +x " + outBin);
            Logger::ok("binary downloaded: " + outBin);
            return true;
        }

        //archive download
        Utils::mkdirp(dest);
        std::string archivePath = workDir + "/" + pkg.name + "_src";
        if      (url.find(".zip")     != std::string::npos) archivePath += ".zip";
        else if (url.find(".tar.gz")  != std::string::npos) archivePath += ".tar.gz";
        else if (url.find(".tgz")     != std::string::npos) archivePath += ".tgz";
        else if (url.find(".tar.bz2") != std::string::npos) archivePath += ".tar.bz2";
        else if (url.find(".tar.xz")  != std::string::npos) archivePath += ".tar.xz";
        else                                                  archivePath += ".tar.gz";

        Logger::fetching("downloading archive: " + url);
        Logger::info("please wait...");
        Logger::blank();

        if (Utils::runCommand("curl -L --progress-bar -o " + archivePath + " " + url) != 0) {
            Logger::err("curl download failed."); return false;
        }
        Logger::blank();
        Logger::ok("download complete: " + archivePath);
        Logger::step("extracting...");

        std::string extractCmd;
        if (archivePath.find(".zip") != std::string::npos) {
            if (!Utils::commandExists("unzip")) {
                Logger::err("'unzip' not found."); return false;
            }
            extractCmd = "unzip -q " + archivePath + " -d " + dest + "_unzip"
                         " && mv " + dest + "_unzip/*/* " + dest + " 2>/dev/null"
                         " || mv " + dest + "_unzip/* "   + dest + " 2>/dev/null"
                         " ; rm -rf " + dest + "_unzip";
        } else {
            extractCmd = "tar -xf " + archivePath + " -C " + dest + " --strip-components=1";
        }
        if (Utils::runCommand(extractCmd) != 0) {
            Logger::err("extraction failed."); return false;
        }
        Utils::rmrf(archivePath);
        Logger::ok("extraction complete.");
        return true;
    }

    //local repo copy
    //this is torture

    bool fetchLocal(const std::string& src, const std::string& dest) {
        if (!fs::exists(src)) {
            Logger::err("Local repo path not found: " + src);
            return false;
        }
        Logger::fetching("copying local source: " + src + " -> " + dest);
        try {
            fs::copy(src, dest, fs::copy_options::recursive);
            Logger::ok("local copy complete.");
            return true;
        } catch (const std::exception& ex) {
            Logger::err("copy failed: " + std::string(ex.what()));
            return false;
        }
    }

public:
    Fetcher(const PackageEntry& entry, const std::string& wdir)
        : pkg(entry), workDir(wdir) {}

    //returns destination directory path, or "" on failure.
    std::string fetch() {
        std::string dest = workDir + "/" + pkg.name;

        if (fs::exists(dest)) {
            Logger::warn("stale workspace dir found -- removing: " + dest);
            Utils::rmrf(dest);
        }

        //local
        std::string localRepo = Utils::reposDir() + "/" + pkg.name;
        if (fs::exists(localRepo)) {
            Logger::info("found local repo override: " + localRepo);
            Utils::mkdirp(dest);
            return fetchLocal(localRepo, dest) ? dest : "";
        }

        if (pkg.url.empty()) {
            Logger::err("No URL for '" + pkg.name + "' and no local repo override.");
            return "";
        }

        //resolve pull_type
        std::string pt = pkg.pull_type;
        if (pt.empty() || pt == "auto") {
            pt = looksLikeGit(pkg.url) ? "git" : "curl";
        }

        Logger::info("pull_type: " + pt);

        if (pt == "git") {
            Logger::info("found " + pkg.name + " powered by git!");
            return fetchGit(dest) ? dest : "";
        }

        if (pt == "curl") {
            Logger::info("found " + pkg.name + " for curl download.");
            return fetchCurl(dest) ? dest : "";
        }

        Logger::err("Unknown pull_type '" + pt + "'.  Use: git | curl | auto");
        return "";
    }
};

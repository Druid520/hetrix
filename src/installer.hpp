#pragma once
#include "config.hpp"
#include "logger.hpp"
#include "utils.hpp"
#include <string>
#include <vector>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

//installer -- move to bin or keep in place, im too lazy, yes auto_add_cmds field

class Installer {
    PackageEntry pkg;
    std::string  installDir;

    void makeExec(const std::string& path) {
        try {
            auto p = fs::status(path).permissions();
            p |= fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec;
            fs::permissions(path, p);
        } catch (...) {}
    }

public:
    Installer(const PackageEntry& entry, const std::string& binDir)
        : pkg(entry), installDir(binDir) {}

    //returns true if the binary was moved/installed, false if kept in place.
    bool promptAndInstall(const std::vector<std::string>& binaries,
                          const std::string& srcDir,
                          bool forceMove = false,
                          bool forceKeep = false) {
        if (binaries.empty()) {
            Logger::warn("No executables found to install for '" + pkg.name + "'.");
            Logger::warn("Build may have succeeded -- check workspace: " + srcDir);
            return false;
        }

        Logger::blank();
        Logger::divider();
        std::cout << CLR_BCYAN "  built executables found:\n" CLR_RESET;
        for (size_t i = 0; i < binaries.size(); ++i) {
            std::cout << "    " CLR_BOLD << "[" << i+1 << "] " CLR_RESET
                      << fs::path(binaries[i]).filename().string()
                      << CLR_DIM " (" << binaries[i] << ")" CLR_RESET "\n";
        }
        Logger::divider();
        Logger::blank();

        //auto_add_cmds: "0" means always keep, skip prompt entirely
        if (pkg.auto_add_cmds == "0") {
            Logger::info("auto_add_cmds=0 -- keeping in workspace, not moving to bin.");
            keepInPlace(binaries, srcDir);
            return false;
        }

        bool doMove = false;

        if (forceMove) {
            doMove = true;
        } else if (forceKeep) {
            doMove = false;
        } else {
            std::cout << CLR_BOLD
                      << "  move to commands path" CLR_BBLUE " (1)" CLR_RESET CLR_BOLD
                      << " or keep as executable in place" CLR_BBLUE " (2)" CLR_RESET CLR_BOLD
                      << "?  " CLR_RESET;
            std::flush(std::cout);

            std::string choice;
            std::getline(std::cin, choice);
            choice = Utils::trim(choice);

            if (choice == "1" || choice.empty()) {
                doMove = true;
            } else if (choice == "2") {
                doMove = false;
            } else {
                Logger::warn("Invalid choice '" + choice + "' -- defaulting to move (1).");
                doMove = true;
            }
        }

        return doMove ? moveToBin(binaries) : (keepInPlace(binaries, srcDir), false);
    }

    bool moveToBin(const std::vector<std::string>& binaries) {
        Utils::mkdirp(installDir);
        Logger::blank();
        Logger::installing("moving to bin...");
        int count = 0;
        for (auto& binPath : binaries) {
            fs::path src(binPath);
            std::string destPath = installDir + "/" + src.filename().string();
            try {
                fs::copy_file(src, destPath, fs::copy_options::overwrite_existing);
                makeExec(destPath);
                Logger::ok("installed: " + src.filename().string() + " -> " + destPath);
                ++count;
            } catch (const std::exception& ex) {
                Logger::err("failed to move " + binPath + ": " + ex.what());
            }
        }
        return count > 0;
    }

    void keepInPlace(const std::vector<std::string>& binaries, const std::string& srcDir) {
        Logger::blank();
        Logger::info("keeping executables in place:");
        for (auto& b : binaries) {
            makeExec(b);
            Logger::ok("  " + b);
        }
        Logger::info("workspace kept at: " + srcDir);
    }

    bool runCustomInstall(const std::string& cmd, const std::string& workdir) {
        Logger::installing("running custom install: " + cmd);
        if (Utils::runCommand(cmd, workdir) != 0) {
            Logger::err("custom install command failed."); return false;
        }
        Logger::ok("custom install complete.");
        return true;
    }

    void printPathHint() {
        if (!Utils::isRoot() && !Utils::isOnPath(installDir)) {
            Logger::blank();
            Logger::warn(installDir + " is not in your PATH.");
            Logger::info("Add to ~/.bashrc or ~/.profile:");
            Logger::info("  export PATH=\"$PATH:" + installDir + "\"");
        }
    }

    const std::string& getInstallDir() const { return installDir; }
};
//deal with it fuckers! my code is mostly uncommented!

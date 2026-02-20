#pragma once
#include "config.hpp"
#include "logger.hpp"
#include "utils.hpp"
#include <string>
#include <vector>
#include <sstream>
#include <filesystem>
#include <thread>

namespace fs = std::filesystem;
//sisyphus ass code
//build system enum + helpers

enum class BuildSystem {
    CMAKE, MAKE, AUTOCONF, GPP, GCC, MESON,
    CARGO, GO, ZIG, DUNE,
    PYTHON, NODEJS, RUBY,
    UNKNOWN
};

inline std::string buildSystemName(BuildSystem b) {
    switch(b) {
        case BuildSystem::CMAKE:    return "cmake";
        case BuildSystem::MAKE:     return "make";
        case BuildSystem::AUTOCONF: return "autoconf / configure";
        case BuildSystem::GPP:      return "g++";
        case BuildSystem::GCC:      return "gcc";
        case BuildSystem::MESON:    return "meson + ninja";
        case BuildSystem::CARGO:    return "cargo (Rust)";
        case BuildSystem::GO:       return "go build (Go)";
        case BuildSystem::ZIG:      return "zig build (Zig)";
        case BuildSystem::DUNE:     return "dune (OCaml)";
        case BuildSystem::PYTHON:   return "pip / python (Python)";
        case BuildSystem::NODEJS:   return "npm / node (Node.js)";
        case BuildSystem::RUBY:     return "gem / rake (Ruby)";
        default:                    return "unknown";
    }
}

inline BuildSystem parseBuildHint(const std::string& h) {
    if (h == "cmake")                  return BuildSystem::CMAKE;
    if (h == "make")                   return BuildSystem::MAKE;
    if (h == "g++")                    return BuildSystem::GPP;
    if (h == "gcc")                    return BuildSystem::GCC;
    if (h == "meson")                  return BuildSystem::MESON;
    if (h == "autoconf")               return BuildSystem::AUTOCONF;
    if (h == "cargo" || h == "rust")   return BuildSystem::CARGO;
    if (h == "go"    || h == "golang") return BuildSystem::GO;
    if (h == "zig")                    return BuildSystem::ZIG;
    if (h == "dune"  || h == "ocaml")  return BuildSystem::DUNE;
    if (h == "python"|| h == "pip")    return BuildSystem::PYTHON;
    if (h == "node"  || h == "npm")    return BuildSystem::NODEJS;
    if (h == "ruby"  || h == "gem")    return BuildSystem::RUBY;
    return BuildSystem::UNKNOWN;
}
//im crine :sob: i aint wanna do this no more.
inline BuildSystem detectBuildSystem(const std::string& dir) {
    auto has = [&](const std::string& f){ return fs::exists(dir + "/" + f); };

    if (has("CMakeLists.txt"))                                     return BuildSystem::CMAKE;
    if (has("meson.build"))                                        return BuildSystem::MESON;
    if (has("configure") || has("configure.ac") || has("configure.in"))
                                                                   return BuildSystem::AUTOCONF;
    if (has("Makefile") || has("makefile") || has("GNUmakefile"))  return BuildSystem::MAKE;
    if (has("Cargo.toml"))                                         return BuildSystem::CARGO;
    if (has("go.mod"))                                             return BuildSystem::GO;
    if (has("build.zig"))                                          return BuildSystem::ZIG;
    if (has("dune-project"))                                       return BuildSystem::DUNE;
    if (has("pyproject.toml") || has("setup.py") || has("setup.cfg"))
                                                                   return BuildSystem::PYTHON;
    if (has("package.json"))                                       return BuildSystem::NODEJS;
    if (!Utils::findFiles(dir, ".gemspec").empty())                return BuildSystem::RUBY;
    if (!Utils::findFiles(dir, ".cpp").empty())                    return BuildSystem::GPP;
    if (!Utils::findFiles(dir, ".c").empty())                      return BuildSystem::GCC;
    return BuildSystem::UNKNOWN;
}

//ball inspection helpers
//true if the args string already contains a -o flag (but not -O optimisation dumbass)
static bool argsHaveOutputFlag(const std::string& args) {
    std::istringstream ss(args);
    std::string tok;
    while (ss >> tok) {
        if (tok == "-o") return true;
        // -ofoo merged form, but NOT -O this time! (uppercase = optimisation level)
        if (tok.size() >= 2 && tok[0] == '-' && tok[1] == 'o'
            && (tok.size() == 2 || std::islower((unsigned char)tok[2])))
            return true;
    }
    return false;
}

//true if args contains a path token (starts with . or /)
//used to detect "go build -o bin ./cmd/foo" patterns
static bool argsHavePath(const std::string& args) {
    std::istringstream ss(args);
    std::string tok;
    while (ss >> tok) {
        if (!tok.empty() && (tok[0] == '.' || tok[0] == '/')) return true;
    }
    return false;
}

//bob the builder

class Builder {
    std::string  srcDir;    //root of cloned / extracted source
    std::string  buildDir;  //where built binaries end up. close enough?
    PackageEntry pkg;

    int nproc() {
        int n = (int)std::thread::hardware_concurrency();
        return n > 0 ? n : 2;
    }

    bool check(const std::string& tool) {
        if (Utils::commandExists(tool)) return true;
        Logger::err("Required tool not found: " + tool);
        Logger::info("Install it then retry.");
        return false;
    }

    int run(const std::string& cmd, const std::string& wd = "") {
        return Utils::runCommand(cmd, wd.empty() ? srcDir : wd);
    }

    //resolve -o flag for gcc/g++ only.
    //priority:
    //1. pkg.output set in JSON   -> always use "-o <output>"
    //2. args already have -o     -> don't add another
    //3. nothing set              -> "-o <pkg.name>"
    std::string resolveGccOutputFlag() {
        if (!pkg.output.empty())
            return " -o " + pkg.output;
        if (argsHaveOutputFlag(pkg.args)) {
            Logger::detail("-o flag found in args -- not adding another");
            return "";
        }
        return " -o " + pkg.name;
    }

    //C / C++ strategics

    bool buildCMake() {
        if (!check("cmake") || !check("make")) return false;
        buildDir = srcDir + "/_build";
        Utils::mkdirp(buildDir);
        std::string args = pkg.args.empty() ? "-DCMAKE_BUILD_TYPE=Release" : pkg.args;
        Logger::building("cmake " + args + " ..");
        if (run("cmake " + args + " ..", buildDir) != 0) {
            Logger::err("cmake configure failed."); return false;
        }
        Logger::building("make -j" + std::to_string(nproc()));
        if (run("make -j" + std::to_string(nproc()), buildDir) != 0) {
            Logger::err("make failed."); return false;
        }
        return true;
    }

    bool buildMake() {
        if (!check("make")) return false;
        std::string extra = pkg.args.empty() ? "" : " " + pkg.args;
        Logger::building("make -j" + std::to_string(nproc()) + extra);
        if (run("make -j" + std::to_string(nproc()) + extra) != 0) {
            Logger::err("make failed."); return false;
        }
        return true;
    }

    bool buildAutoconf() {
        if (!check("make")) return false;
        if (!fs::exists(srcDir + "/configure")) {
            Logger::building("autoreconf -fi");
            if (!check("autoreconf")) return false;
            if (run("autoreconf -fi") != 0) { Logger::err("autoreconf failed."); return false; }
        }
        std::string cfgArgs = pkg.args.empty() ? "" : " " + pkg.args;
        Logger::building("./configure" + cfgArgs);
        if (run("./configure" + cfgArgs) != 0) { Logger::err("configure failed."); return false; }
        Logger::building("make -j" + std::to_string(nproc()));
        if (run("make -j" + std::to_string(nproc())) != 0) { Logger::err("make failed."); return false; }
        return true;
    }

    bool buildGpp() {
        if (!check("g++")) return false;
        std::string flags   = pkg.args.empty() ? "-std=c++17 -O2 -Wall" : pkg.args;
        std::string outflag = resolveGccOutputFlag();

        if (!pkg.main_file.empty() && fs::exists(srcDir + "/" + pkg.main_file)) {
            Logger::building("g++ " + flags + " " + pkg.main_file + outflag);
            return run("g++ " + flags + " " + pkg.main_file + outflag) == 0;
        }

        auto files = Utils::findFiles(srcDir, ".cpp");
        if (files.empty()) { Logger::err("No .cpp files found."); return false; }
        std::string fileList;
        for (auto& f : files) fileList += " " + fs::relative(f, srcDir).string();
        Logger::building("g++ " + flags + " [" + std::to_string(files.size()) + " files]" + outflag);
        return run("g++ " + flags + fileList + outflag) == 0;
    }

    bool buildGcc() {
        if (!check("gcc")) return false;
        std::string flags   = pkg.args.empty() ? "-O2 -Wall" : pkg.args;
        std::string outflag = resolveGccOutputFlag();

        if (!pkg.main_file.empty() && fs::exists(srcDir + "/" + pkg.main_file)) {
            Logger::building("gcc " + flags + " " + pkg.main_file + outflag);
            if (run("gcc " + flags + " " + pkg.main_file + outflag) != 0) {
                Logger::err("gcc compilation failed."); return false;
            }
            return true;
        }

        auto files = Utils::findFiles(srcDir, ".c");
        if (files.empty()) { Logger::err("No .c files found."); return false; }
        std::string fileList;
        for (auto& f : files) fileList += " " + fs::relative(f, srcDir).string();
        Logger::building("gcc " + flags + " [" + std::to_string(files.size()) + " files]" + outflag);
        if (run("gcc " + flags + fileList + outflag) != 0) {
            Logger::err("gcc compilation failed."); return false;
        }
        return true;
    }

    bool buildMeson() {
        if (!check("meson") || !check("ninja")) return false;
        buildDir = srcDir + "/_build";
        Logger::building("meson setup _build --buildtype=release");
        if (run("meson setup " + buildDir + " --buildtype=release") != 0) {
            Logger::err("meson setup failed."); return false;
        }
        Logger::building("ninja -C _build");
        if (run("ninja -C " + buildDir) != 0) { Logger::err("ninja failed."); return false; }
        return true;
    }

    //Rust / Cargo (no im not doing this in rust fuck off)

    bool buildCargo() {
        if (!check("cargo")) return false;
        std::string flags = pkg.args.empty() ? "--release" : pkg.args;
        Logger::building("cargo build " + flags);
        Logger::info("this may take a while (downloading crates)...");
        if (run("cargo build " + flags) != 0) {
            Logger::err("cargo build failed."); return false;
        }
        buildDir = srcDir + "/target/release";
        return true;
    }

    //overly complex GO, probably shouldnt compile

    bool buildGo() {
        if (!check("go")) return false;
        std::string output = pkg.output.empty() ? pkg.name : pkg.output;

        std::string cmd;
        bool hasO    = argsHaveOutputFlag(pkg.args);
        bool hasPath = argsHavePath(pkg.args);

        if (!pkg.args.empty()) {
            if (hasO && hasPath) {
                //e.g. args = "-o validator ./cmd/validator"  -- use verbatim
                cmd = "go build " + pkg.args;
            } else if (hasO && !hasPath) {
                //user set -o but no path -- im gonna rip your eyes out
                cmd = "go build " + pkg.args;
            } else if (!hasO && hasPath) {
                //user set a path but no -o -- add the output flag
                cmd = "go build -o " + output + " " + pkg.args;
            } else {
                //args are just flags (e.g. build tags) -- add output and build .
                cmd = "go build -o " + output + " " + pkg.args + " .";
            }
        } else {
            //no args at all
            cmd = "go build -o " + output + " .";
        }

        Logger::building(cmd);
        if (run(cmd) != 0) {
            Logger::err("go build failed."); return false;
        }
        return true;
    }

    //Zig. why does anyone use zig? why do we have to deal with it?

    bool buildZig() {
        if (!check("zig")) return false;
        std::string flags = pkg.args.empty() ? "-Doptimize=ReleaseSafe" : pkg.args;
        Logger::building("zig build " + flags);
        if (run("zig build " + flags) != 0) {
            Logger::err("zig build failed."); return false;
        }
        buildDir = srcDir + "/zig-out/bin";
        return true;
    }

    //OCaml / Dune. js use golang.

    bool buildDune() {
        if (!check("dune")) return false;
        Logger::building("dune build --release");
        if (run("dune build --release") != 0) {
            Logger::err("dune build failed."); return false;
        }
        buildDir = srcDir + "/_build/default";
        return true;
    }

    //Python. Cobra Kai!

    bool buildPython() {
        if (fs::exists(srcDir + "/pyproject.toml") || fs::exists(srcDir + "/setup.py")
                || fs::exists(srcDir + "/setup.cfg")) {
            std::string pip = Utils::commandExists("pip3") ? "pip3" : "pip";
            if (!check(pip)) return false;
            Logger::building(pip + " install --user .");
            if (run(pip + " install --user . --break-system-packages 2>/dev/null"
                    " || " + pip + " install --user .") != 0) {
                Logger::err("pip install failed."); return false;
            }
            Logger::info("installed via pip  --  binary in ~/.local/bin/");
            return true;
        }
        if (!pkg.main_file.empty() && fs::exists(srcDir + "/" + pkg.main_file)) {
            std::string out  = pkg.output.empty() ? pkg.name : pkg.output;
            std::string dest = srcDir + "/" + out;
            Logger::building("staging: " + pkg.main_file + " -> " + out);
            if (run("cp " + pkg.main_file + " " + dest + " && chmod +x " + dest) != 0) {
                Logger::err("failed to stage python script."); return false;
            }
            return true;
        }
        Logger::err("No pyproject.toml, setup.py, setup.cfg, or 'main' file found.");
        return false;
    }

    //Node.js for dumbass frontend devs. use a api lol.

    bool buildNodejs() {
        if (!check("npm")) return false;
        Logger::building("npm install");
        if (run("npm install") != 0) { Logger::err("npm install failed."); return false; }
        std::string pkgJson = Utils::readFile(srcDir + "/package.json");
        if (pkgJson.find("\"build\"") != std::string::npos) {
            Logger::building("npm run build");
            if (run("npm run build") != 0) {
                Logger::err("npm run build failed."); return false;
            }
        }
        run("npm link 2>/dev/null || true");
        Logger::info("linked via npm.");
        return true;
    }

    //Ruby

    bool buildRuby() {
        auto gemspecs = Utils::findFiles(srcDir, ".gemspec");
        if (!gemspecs.empty()) {
            if (!check("gem")) return false;
            Logger::building("gem build + gem install --user-install");
            if (run("gem build " + fs::path(gemspecs[0]).filename().string()) != 0) {
                Logger::err("gem build failed."); return false;
            }
            if (run("gem install --user-install *.gem") != 0) {
                Logger::err("gem install failed."); return false;
            }
            return true;
        }
        if (fs::exists(srcDir + "/Rakefile")) {
            if (!check("rake")) return false;
            Logger::building("rake install");
            if (run("rake install") != 0) { Logger::err("rake install failed."); return false; }
            return true;
        }
        Logger::err("No .gemspec or Rakefile found for Ruby build.");
        return false;
    }

public:
    Builder(const std::string& dir, const PackageEntry& entry)
        : srcDir(dir), buildDir(dir), pkg(entry) {

        //Apply sub_dir if set
        if (!pkg.sub_dir.empty()) {
            std::string sub = dir + "/" + pkg.sub_dir;
            if (fs::exists(sub)) {
                srcDir   = sub;
                buildDir = sub;
                Logger::info("building in sub_dir: " + pkg.sub_dir);
            } else {
                Logger::warn("sub_dir '" + pkg.sub_dir + "' not found -- building in root");
            }
        }
        //Apply explicit build_dir override
        if (!pkg.build_dir.empty()) {
            buildDir = dir + "/" + pkg.build_dir;
        }
    }

    bool build() {
        BuildSystem bs;
        if (!pkg.build.empty() && pkg.build != "auto") {
            bs = parseBuildHint(pkg.build);
            Logger::info(pkg.build + " instructions found!  compiling...");
        } else {
            bs = detectBuildSystem(srcDir);
            Logger::info("auto-detected: " + buildSystemName(bs));
            Logger::info("compiling...");
        }
        Logger::blank();

        bool ok = false;
        switch (bs) {
            case BuildSystem::CMAKE:    ok = buildCMake();    break;
            case BuildSystem::MAKE:     ok = buildMake();     break;
            case BuildSystem::AUTOCONF: ok = buildAutoconf(); break;
            case BuildSystem::GPP:      ok = buildGpp();      break;
            case BuildSystem::GCC:      ok = buildGcc();      break;
            case BuildSystem::MESON:    ok = buildMeson();    break;
            case BuildSystem::CARGO:    ok = buildCargo();    break;
            case BuildSystem::GO:       ok = buildGo();       break;
            case BuildSystem::ZIG:      ok = buildZig();      break;
            case BuildSystem::DUNE:     ok = buildDune();     break;
            case BuildSystem::PYTHON:   ok = buildPython();   break;
            case BuildSystem::NODEJS:   ok = buildNodejs();   break;
            case BuildSystem::RUBY:     ok = buildRuby();     break;
            default:
                Logger::err("Cannot determine build system for '" + pkg.name + "'.");
                Logger::err("Set a 'build' field in index.json.");
                Logger::info("Supported: cmake make gcc g++ autoconf meson "
                             "cargo go zig dune python node ruby");
                return false;
        }
        if (ok) { Logger::blank(); Logger::ok("compiled!"); }
        return ok;
    }

    //Find built binaries -- searches all known asshole locations
    std::vector<std::string> findBuiltBinaries() {
        std::vector<std::string> bins;

        if (!pkg.output.empty()) {
            std::vector<std::string> searchDirs = {
                buildDir,
                srcDir,
                srcDir + "/target/release",   //cargo
                srcDir + "/zig-out/bin",       //zig
                srcDir + "/_build/default",    //dune
                srcDir + "/bin",
                srcDir + "/out",
                srcDir + "/dist",
                srcDir + "/build",
                srcDir + "/.bin"
            };
            if (!pkg.build_dir.empty())
                searchDirs.insert(searchDirs.begin(), pkg.build_dir);

            for (auto& d : searchDirs) {
                std::string p = d + "/" + pkg.output;
                if (fs::exists(p) && fs::is_regular_file(p)) {
                    bins.push_back(p);
                    return bins;
                }
            }
        }

        //Scan for chrismas elves
        for (auto& dir : {buildDir, srcDir}) {
            auto found = Utils::findExecutables(dir);
            for (auto& f : found) {
                fs::path p(f);
                if (p.filename() == "configure"  || p.filename() == "libtool"
                 || p.filename() == "compile"     || p.filename() == "install-sh"
                 || p.filename() == "missing"     || p.filename() == "depcomp"
                 || p.extension() == ".so"        || p.extension() == ".dylib")
                    continue;
                bins.push_back(f);
            }
            if (!bins.empty()) return bins;
        }
        return bins;
    }

    const std::string& getBuildDir() const { return buildDir; }
    const std::string& getSrcDir()   const { return srcDir; }
};

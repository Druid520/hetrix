#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <thread>
#include <algorithm>
//tedd if you fucking remove these ill rip out your spine

#include "logger.hpp"
#include "utils.hpp"
#include "config.hpp"
#include "config_manager.hpp"
#include "fetcher.hpp"
#include "builder.hpp"
#include "installer.hpp"

namespace fs = std::filesystem;

//workspace bootstrap
void initWorkspace() {
    Utils::mkdirp(Utils::hetrixDir());
    Utils::mkdirp(Utils::binDir());
    Utils::mkdirp(Utils::workspaceDir());
    Utils::mkdirp(Utils::reposDir());
}

//option bag
struct Opts {
    std::string command;
    std::string arg;

    bool forceMove  = false;  // --move
    bool forceKeep  = false;  // --keep
    bool noClean    = false;  // --no-clean
    bool editIndex  = false;  // --edit
    bool forceInit  = false;  // --force

    std::string customIndex;  // --index <path>
};

Opts parseArgs(int argc, char* argv[]) {
    Opts o;
    for (int i = 1; i < argc; ++i) {
        std::string a(argv[i]);
        if      (a == "--move")     { o.forceMove = true; }
        else if (a == "--keep")     { o.forceKeep = true; }
        else if (a == "--no-clean") { o.noClean   = true; }
        else if (a == "--edit")     { o.editIndex = true; }
        else if (a == "--force")    { o.forceInit = true; }
        else if ((a == "--index" || a == "-i") && i + 1 < argc) {
            o.customIndex = argv[++i];
        } else if (o.command.empty()) {
            o.command = a;
        } else if (o.arg.empty()) {
            o.arg = a;
        }
    }
    return o;
}

//yelp
void printHelp(const std::string& /*prog*/) {
    auto section = [](const std::string& title) {
        std::cout << "\n" CLR_BBLUE "  " << title << "\n" CLR_RESET;
    };
    auto row = [](const std::string& cmd, const std::string& desc) {
        std::cout << "  " CLR_BOLD << cmd << CLR_RESET;
        int pad = 36 - (int)cmd.size();
        if (pad > 0) std::cout << std::string(pad, ' ');
        std::cout << desc << "\n";
    };
    auto frow = [](const std::string& f, const std::string& d) {
        std::cout << "  " CLR_GREEN << f << CLR_RESET;
        int pad = 18 - (int)f.size();
        if (pad > 0) std::cout << std::string(pad, ' ');
        std::cout << d << "\n";
    };
    auto pathRow = [](const std::string& env, const std::string& val, const std::string& desc) {
        std::cout << "  " CLR_CYAN << env << CLR_RESET " = " CLR_DIM << val << CLR_RESET "\n"
                  << "  " << std::string(env.size() + 3, ' ') << CLR_DIM << desc << CLR_RESET "\n";
    };

    section("COMMANDS");
    row("fetch <package>",            "fetch, build, and install a package");
    row("fetch <user/repo>",          "GitHub shorthand  (auto-detect build)");
    row("fetch <https://...>",        "full URL  (auto-detect build)");
    row("remove <package>",           "remove an installed binary from bin/");
    row("list",                       "list all packages in index.json");
    row("search <query>",             "search by name, URL or description");
    row("init",                       "initialise ~/hetrix/ workspace");
    row("index",                      "print path to index.json");
    row("index --edit",               "open index.json in micro (or $EDITOR)");
    row("which <package>",            "show installed binary path");
    row("clean",                      "remove all workspace build dirs");
    row("clean <package>",            "remove workspace dir for one package");
    row("help",                       "show this help");

    section("FETCH FLAGS");
    row("  --move",                   "skip prompt -- always move to bin");
    row("  --keep",                   "skip prompt -- always keep in place");
    row("  --no-clean",               "keep workspace after install");

    section("GLOBAL FLAGS");
    row("  --index <path>",           "use a custom index.json path");
    row("  --force",                  "overwrite index.json on init");

    section("PATHS  " CLR_DIM "(overridable via env vars)");
    pathRow("HETRIX_DIR",       Utils::hetrixDir(),    "base workspace  (default: ~/hetrix)");
    pathRow("HETRIX_BIN",       Utils::binDir(),       "installed binaries");
    pathRow("HETRIX_WORKSPACE", Utils::workspaceDir(), "temporary build dirs");
    pathRow("HETRIX_INDEX",     Utils::indexPath(),    "package index file");

    section("BUILD SYSTEMS  " CLR_DIM "(set \"build\": \"...\" in index.json)");
    std::cout << "\n  " CLR_DIM "C / C++\n" CLR_RESET;
    row("  gcc",                      "compile .c files with gcc");
    row("  g++",                      "compile .cpp files with g++");
    row("  cmake",                    "CMakeLists.txt  (calls make internally)");
    row("  make",                     "Makefile  (plain make -j)");
    row("  autoconf",                 "configure.ac / configure -> make");
    row("  meson",                    "meson.build  (calls ninja internally)");
    std::cout << "\n  " CLR_DIM "Compiled languages\n" CLR_RESET;
    row("  cargo",                    "Rust   (Cargo.toml)   -> target/release/");
    row("  go",                       "Go     (go.mod)       -> project root");
    row("  zig",                      "Zig    (build.zig)    -> zig-out/bin/");
    row("  dune",                     "OCaml  (dune-project) -> _build/default/");
    std::cout << "\n  " CLR_DIM "Scripted / bytecode\n" CLR_RESET;
    row("  python",                   "pip install  (pyproject.toml / setup.py)");
    row("  node",                     "npm install + npm run build");
    row("  ruby",                     "gem build/install or rake install");
    std::cout << "\n  " CLR_DIM "Special\n" CLR_RESET;
    row("  auto",                     "hetrix sniffs the repo and picks for you");

    section("INDEX.JSON FIELDS");
    frow("url",            "git repo, tarball URL, or direct binary URL");
    frow("pull_type",      "\"git\" | \"curl\" | \"auto\"  (default: auto)");
    frow("build",          "build system value from the list above");
    frow("args",           "extra compiler / cmake / cargo / go flags");
    frow("main",           "entry source file  (single-file gcc/g++ builds)");
    frow("sub_dir",        "cd into this subdir before building");
    frow("build_dir",      "override where to search for built binaries");
    frow("output",         "binary name to produce  (default: package name)");
    frow("version",        "git tag or branch to pin  (default: latest tag)");
    frow("auto_add_cmds",  "\"1\" prompt to move to bin (default), \"0\" keep in workspace");
    frow("description",    "shown in hetrix list");
    frow("install_cmd",    "custom shell command  (runs in src dir)");

    section("TOOLS");
    row("configgy.sh",               "interactive index.json manager");
    row("  add / remove / edit",     "add, remove, or edit a package entry");
    row("  list / show / search",    "browse the index");
    row("  validate / format",       "check and pretty-print index.json");
    row("hetrix.sh",                 "build and install hetrix itself");

    std::cout << "\n";
}

//init
int cmdInit(const Opts& opts) {
    Logger::banner("initialising hetrix workspace");
    initWorkspace();
    Logger::ok("workspace: " + Utils::hetrixDir());
    Logger::blank();

    ConfigManager cm;
    if (!opts.customIndex.empty()) cm.setIndexPath(opts.customIndex);

    if (opts.forceInit && Utils::exists(cm.getIndexPath())) {
        Logger::warn("--force set -- removing existing index.json.");
        Utils::rmrf(cm.getIndexPath());
    }

    if (!cm.initIndex()) { Logger::err("failed to set up index.json."); return 1; }
    Logger::blank();
    Logger::info("index:     " + cm.getIndexPath());
    Logger::info("bin:       " + Utils::binDir());
    Logger::info("workspace: " + Utils::workspaceDir());
    Logger::info("repos:     " + Utils::reposDir());
    Logger::blank();
    Logger::ok("done. edit your packages:");
    Logger::info("  micro " + cm.getIndexPath());
    return 0;
}

//list
int cmdList(const Opts& opts) {
    ConfigManager cm;
    if (!opts.customIndex.empty()) cm.setIndexPath(opts.customIndex);
    auto idx = cm.load();
    if (idx.empty()) { Logger::warn("No packages. Edit: " + cm.getIndexPath()); return 0; }

    Logger::blank();
    std::cout << CLR_BOLD "  packages in index.json  (" << idx.size() << ")\n" CLR_RESET;
    Logger::divider();

    for (auto& [name, e] : idx) {
        std::cout << "  " CLR_BGREEN << name << CLR_RESET;
        if (!e.description.empty())
            std::cout << CLR_DIM "  -- " << e.description << CLR_RESET;
        std::cout << "\n";
        std::cout << "    " CLR_DIM "url   " CLR_RESET << (e.url.empty() ? "(local repo)" : e.url) << "\n";
        std::cout << "    " CLR_DIM "build " CLR_RESET << e.build;
        if (!e.pull_type.empty() && e.pull_type != "auto")
            std::cout << "  " CLR_DIM "pull=" CLR_RESET << e.pull_type;
        if (!e.version.empty())
            std::cout << "  " CLR_DIM "ver=" CLR_RESET << e.version;
        if (!e.args.empty())
            std::cout << "  " CLR_DIM "args=" CLR_RESET << e.args;
        std::cout << "\n";
        std::string binPath = Utils::installDir() + "/" + e.output;
        if (Utils::exists(binPath))
            std::cout << "    " CLR_BGREEN NF_OK " installed" CLR_RESET " -> " CLR_DIM << binPath << CLR_RESET "\n";
        std::cout << "\n";
    }
    return 0;
}

//search
int cmdSearch(const Opts& opts) {
    if (opts.arg.empty()) { Logger::err("Usage: hetrix search <query>"); return 1; }
    ConfigManager cm;
    if (!opts.customIndex.empty()) cm.setIndexPath(opts.customIndex);
    auto idx = cm.load();
    int found = 0;
    Logger::blank();
    std::cout << CLR_BOLD "  search results for '" << opts.arg << "':\n" CLR_RESET;
    Logger::divider();
    for (auto& [name, e] : idx) {
        bool hit = name.find(opts.arg) != std::string::npos
                || e.url.find(opts.arg) != std::string::npos
                || e.description.find(opts.arg) != std::string::npos;
        if (hit) {
            std::cout << "  " CLR_BGREEN << name << CLR_RESET
                      << "  " CLR_DIM << e.url << CLR_RESET "\n";
            if (!e.description.empty())
                std::cout << "    " << e.description << "\n";
            ++found;
        }
    }
    if (found == 0) Logger::warn("No matches for '" + opts.arg + "'.");
    std::cout << "\n";
    return 0;
}

//which
int cmdWhich(const Opts& opts) {
    if (opts.arg.empty()) { Logger::err("Usage: hetrix which <package>"); return 1; }
    std::string target = Utils::installDir() + "/" + opts.arg;
    if (Utils::exists(target)) { std::cout << target << "\n"; return 0; }
    Logger::warn(opts.arg + " not found in " + Utils::installDir());
    return 1;
}

//remove
int cmdRemove(const Opts& opts) {
    if (opts.arg.empty()) { Logger::err("Usage: hetrix remove <package>"); return 1; }

    Logger::banner("removing: " + opts.arg);

    std::string binDir  = Utils::installDir();
    std::string removed;

    //look up the package in index.json to find the real binary name
    ConfigManager cm;
    if (!opts.customIndex.empty()) cm.setIndexPath(opts.customIndex);

    auto found = cm.find(opts.arg);
    std::string binName = found ? found->output : opts.arg;
    if (binName.empty()) binName = opts.arg;

    std::string target = binDir + "/" + binName;

    if (Utils::exists(target)) {
        Logger::cleaning("removing " + target + " ...");
        try {
            fs::remove(target);
            Logger::ok("removed: " + target);
            removed = target;
        } catch (const std::exception& ex) {
            Logger::err("failed to remove " + target + ": " + ex.what());
            return 1;
        }
    } else {
        //try the arg itself as the binary name (user may pass binary name directly)
        std::string alt = binDir + "/" + opts.arg;
        if (Utils::exists(alt)) {
            Logger::cleaning("removing " + alt + " ...");
            try {
                fs::remove(alt);
                Logger::ok("removed: " + alt);
                removed = alt;
            } catch (const std::exception& ex) {
                Logger::err("failed to remove " + alt + ": " + ex.what());
                return 1;
            }
        } else {
            Logger::warn("'" + opts.arg + "' not found in " + binDir);
            Logger::info("Use: hetrix which " + opts.arg + "  to check where it's installed.");
            return 1;
        }
    }

    Logger::blank();
    Logger::info("workspace source (if any) is untouched.");
    Logger::info("To also clean workspace: hetrix clean " + opts.arg);
    return 0;
}

//index. holy shit this is just a git wrapper with a json config.
int cmdIndex(const Opts& opts) {
    ConfigManager cm;
    if (!opts.customIndex.empty()) cm.setIndexPath(opts.customIndex);
    std::string path = cm.getIndexPath();
    std::cout << path << "\n";
    if (!opts.editIndex) return 0;

    std::string editor;
    for (auto& e : {"micro", "hx", "helix", "vim", "nvim", "nano", "vi", "emacs"}) {
        if (Utils::commandExists(e)) { editor = e; break; }
    }
    const char* envEd = std::getenv("EDITOR");
    if (envEd && envEd[0] != '\0') editor = envEd;
    const char* envVi = std::getenv("VISUAL");
    if (envVi && envVi[0] != '\0') editor = envVi;

    if (editor.empty()) {
        Logger::err("No editor found. Install micro or set $EDITOR(lol tedd was here)");
        Logger::info("Edit manually: " + path);
        return 1;
    }
    Logger::info("opening with: " + editor);
    return std::system((editor + " " + path).c_str());
}

//clean
int cmdClean(const Opts& opts) {
    std::string ws = Utils::workspaceDir();

    if (!opts.arg.empty()) {
        std::string target = ws + "/" + opts.arg;
        Logger::banner("cleaning workspace: " + opts.arg);
        if (!Utils::exists(target)) {
            Logger::info("Nothing to clean for '" + opts.arg + "'.");
            return 0;
        }
        Logger::cleaning("removing " + target + " ...");
        if (Utils::rmrf(target)) Logger::ok("cleaned!");
        else { Logger::err("failed to remove " + target); return 1; }
        return 0;
    }

    Logger::banner("cleaning entire workspace");
    if (!Utils::exists(ws)) { Logger::info("Workspace already clean: " + ws); return 0; }

    int count = 0;
    try {
        for (auto& entry : fs::directory_iterator(ws)) {
            Logger::cleaning("removing " + entry.path().string() + " ...");
            Utils::rmrf(entry.path().string());
            ++count;
        }
    } catch (const std::exception& ex) {
        Logger::err("clean failed: " + std::string(ex.what())); return 1;
    }

    Logger::blank();
    if (count == 0) Logger::info("workspace was already empty.");
    else Logger::ok("workspace cleaned!");
    return 0;
}

//fetch
//sudo cd / && fastfetch | sudo tee *
int cmdFetch(const Opts& opts) {
    if (opts.arg.empty()) {
        Logger::err("Usage: hetrix fetch <package | user/repo | https://...>");
        return 1;
    }

    initWorkspace();

    const std::string& pkgArg = opts.arg;
    std::string ws  = Utils::workspaceDir();
    std::string bin = Utils::installDir();

    PackageEntry entry;
    entry.name          = pkgArg;
    entry.build         = "auto";
    entry.output        = pkgArg;
    entry.pull_type     = "auto";
    entry.auto_add_cmds = "1";

    ConfigManager cm;
    if (!opts.customIndex.empty()) cm.setIndexPath(opts.customIndex);

    Logger::blank();
    std::cout << CLR_DIM "  searching index.json...\n" CLR_RESET;

    auto found = cm.find(pkgArg);
    if (found) {
        entry = *found;
        Logger::ok("found " CLR_BGREEN + entry.name + CLR_RESET " in index.json");
        if (!entry.description.empty()) Logger::detail(entry.description);
    } else {
        Logger::warn("'" + pkgArg + "' not in index.json -- treating as URL.");
        if (pkgArg.find('/') != std::string::npos && pkgArg.find("://") == std::string::npos) {
            entry.url  = "https://github.com/" + pkgArg;
            entry.name = fs::path(pkgArg).filename().string();
            entry.output = entry.name;
            Logger::info("GitHub shorthand -> " + entry.url);
        } else if (pkgArg.find("://") != std::string::npos) {
            entry.url    = pkgArg;
            entry.name   = fs::path(pkgArg).stem().string();
            entry.output = entry.name;
        } else {
            Logger::err("Cannot resolve '" + pkgArg + "'. Add to index.json or supply a URL.");
            return 1;
        }
    }

    Logger::blank();

    //step 1: Fetch. ts aint a lego build vro
    Logger::banner("fetching source");
    Fetcher fetcher(entry, ws);
    std::string srcDir = fetcher.fetch();
    if (srcDir.empty()) { Logger::err("fetch failed. aborting."); return 1; }
    Logger::ok("source ready: " + srcDir);
    Logger::blank();

    //Step 2: Build
    Logger::banner("building");
    Logger::info("running...");
    Logger::blank();

    Builder builder(srcDir, entry);
    if (!builder.build()) {
        Logger::blank();
        Logger::err("build failed. source left at: " + srcDir);
        Logger::info("inspect and build manually if needed.");
        return 1;
    }

    //sTep 3: Install
    Logger::banner("install");

    bool moved = false;

    if (!entry.install_cmd.empty()) {
        Installer inst(entry, bin);
        inst.runCustomInstall(entry.install_cmd, srcDir);
        moved = true;
    } else {
        auto bins = builder.findBuiltBinaries();
        Logger::info("found " + std::to_string(bins.size()) + " executable(s).");

        Installer inst(entry, bin);
        moved = inst.promptAndInstall(bins, srcDir, opts.forceMove, opts.forceKeep);
        if (moved) inst.printPathHint();
    }

    //stPp 4: Cleanup. corrupted ahh :sob:
    if (moved && !opts.noClean) {
        Logger::blank();
        Logger::banner("cleaning up");
        Logger::cleaning("removing workspace: " + srcDir + " ...");
        if (Utils::rmrf(srcDir)) Logger::ok("workspace cleaned!");
        else Logger::warn("could not fully clean workspace: " + srcDir);
    } else if (opts.noClean) {
        Logger::blank();
        Logger::info("--no-clean: workspace kept at " + srcDir);
    }

    Logger::blank();
    std::cout << CLR_BGREEN "  " NF_OK " done!\n" CLR_RESET;
    Logger::blank();
    return 0;
}

//shitty main
int main(int argc, char* argv[]) {
    Logger::printMainBanner();

    Opts opts = parseArgs(argc, argv);

    if (opts.command.empty() || opts.command == "help"
            || opts.command == "--help" || opts.command == "-h") {
        printHelp(argv[0]);
        return 0;
    }

    if (opts.command == "init")    return cmdInit(opts);
    if (opts.command == "list")    return cmdList(opts);
    if (opts.command == "search")  return cmdSearch(opts);
    if (opts.command == "which")   return cmdWhich(opts);
    if (opts.command == "remove")  return cmdRemove(opts);
    if (opts.command == "index")   return cmdIndex(opts);
    if (opts.command == "clean")   return cmdClean(opts);
    if (opts.command == "fetch")   return cmdFetch(opts);

    Logger::err("Unknown command: " + opts.command);
    Logger::info("Run: hetrix help");
    return 1;
}
//ITS OVER!!!!

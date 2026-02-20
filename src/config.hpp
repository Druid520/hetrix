#pragma once
#include <string>
#include <map>

//a single package entry from index.json
struct PackageEntry {
    std::string name;

    //source
    std::string url;            //git URL, tarball URL, or direct binary URL
    std::string pull_type;      //"git" | "curl" | "auto"  (default: auto)
    std::string version;        //git tag / branch to pin

    //build
    std::string build;          //cmake|make|gcc|g++|autoconf|meson|
                                //cargo|go|zig|dune|python|node|ruby|auto
    std::string args;           //extra flags for the compiler / build tool
    std::string main_file;      //single-file gcc/g++ entry point ("main" key)
    std::string sub_dir;        //cd into this subdir before building
    std::string build_dir;      //override where to look for built binaries

    //output / install
    std::string output;         //binary name to produce (default: package name)
    std::string install_cmd;    //shell command run instead of normal install flow
    std::string auto_add_cmds;  //"1" = prompt to move to bin (default)
                                //"0" = keep in workspace, skip prompt

    //metadata
    std::string description;
};

//the full index
using PackageIndex = std::map<std::string, PackageEntry>;
//was that the bite of 87?!

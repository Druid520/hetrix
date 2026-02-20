  HETRIX  --  source-based package manager  *  v1.1
  Fetches source code, builds it, installs the binary.

--------------------------------------------------------------------------------
  QUICK START
--------------------------------------------------------------------------------

  1.  Build and install hetrix:

        chmod +x hetrix.sh
        ./hetrix.sh

  2.  Packages are ready to install immediately -- index.json is seeded
      automatically. Browse what is available:

        hetrix list

  3.  Install something:

        hetrix fetch ripgrep
        hetrix fetch lazygit
        hetrix fetch shredder

  4.  Remove an installed binary:

        hetrix remove ripgrep

  5.  Add your own packages with configgy:

        chmod +x configgy.sh
        ./configgy.sh

--------------------------------------------------------------------------------
  INSTALL METHODS
--------------------------------------------------------------------------------

  Shell script (recommended):

    ./hetrix.sh                       build + install to /usr/local/bin
    ./hetrix.sh --prefix ~/.local     install without root
    ./hetrix.sh --no-install          build only  (output: ./hetrix)
    ./hetrix.sh --no-seed             skip copying index.json

  Make:

    make
    sudo make install                 installs to /usr/local/bin
    make install PREFIX=~/.local      user install

  CMake:

    mkdir build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make -j$(nproc)
    sudo make install

--------------------------------------------------------------------------------
  REQUIREMENTS
--------------------------------------------------------------------------------

  To BUILD hetrix itself:

    g++ >= 8  (C++17 filesystem support required)
    make

  To USE hetrix (install packages):

    git     -- for cloning repos
    curl    -- for downloading archives and binaries
    + the build tools for each package (see BUILD SYSTEMS below)

  Distro install commands:

    Debian / Ubuntu   sudo apt install g++ make git curl cmake
    Fedora / RHEL     sudo dnf install gcc-c++ make git curl cmake
    Arch Linux        sudo pacman -S gcc make git curl cmake
    Alpine Linux      sudo apk add g++ make git curl cmake
    Void Linux        sudo xbps-install -S gcc make git curl cmake

--------------------------------------------------------------------------------
  WORKSPACE LAYOUT
--------------------------------------------------------------------------------
    ~/hetrix/
    |-- index.json        package registry          <-- edit this
    |-- bin/              installed binaries         <-- add to PATH
    |-- workspace/        temporary build dirs       (auto-cleaned)
    |-- repos/            local source overrides     (optional)
    |-- src/              hetrix C++ source code
    |-- configgy.sh       interactive index manager
    |-- hetrix.sh         build + install script
    |-- Makefile
    |-- CMakeLists.txt
    `-- README.txt

  All paths are overridable via environment variables:

    HETRIX_DIR          base directory          (default: ~/hetrix)
    HETRIX_BIN          installed binaries       (default: ~/hetrix/bin)
    HETRIX_WORKSPACE    build workspace          (default: ~/hetrix/workspace)
    HETRIX_INDEX        package index file       (default: ~/hetrix/index.json)

  When run as root, binaries install to /usr/local/bin/ instead of ~/hetrix/bin.

--------------------------------------------------------------------------------
  COMMANDS
--------------------------------------------------------------------------------

  hetrix fetch <package>      fetch, build, and install a package
  hetrix fetch <user/repo>    GitHub shorthand  (auto-detect build)
  hetrix fetch <url>          full URL  (auto-detect build)
  hetrix remove <package>     remove an installed binary from bin/
  hetrix list                 list all packages + install status
  hetrix search <query>       search by name, URL, or description
  hetrix init                 initialise ~/hetrix/ workspace
  hetrix index                print path to index.json
  hetrix index --edit         open index.json in micro / $EDITOR
  hetrix which <package>      show installed binary path
  hetrix clean                wipe entire workspace
  hetrix clean <package>      wipe one package from workspace
  hetrix help                 full help with all flags and build systems

  Fetch flags:

    --move        skip prompt, always move binary to bin/
    --keep        skip prompt, always keep binary in workspace
    --no-clean    keep workspace after install (useful for debugging)

  Global flags:

    --index <path>    use a different index.json
    --force           overwrite existing index.json on init

--------------------------------------------------------------------------------
  BUILD SYSTEMS
--------------------------------------------------------------------------------

  Set "build" in index.json to one of these values:

  C / C++
    gcc           compile .c files with gcc
    g++           compile .cpp files with g++
    cmake         CMakeLists.txt  (calls make internally)
    make          Makefile  (plain make -j)
    autoconf      configure.ac / configure script -> make
    meson         meson.build  (calls ninja internally)

  Compiled languages
    cargo         Rust   (Cargo.toml)     binaries -> target/release/
    go            Go     (go.mod)         binary   -> project root
    zig           Zig    (build.zig)      binaries -> zig-out/bin/
    dune          OCaml  (dune-project)   binaries -> _build/default/

  Scripted / bytecode
    python        pip install --user  (pyproject.toml / setup.py / setup.cfg)
    node          npm install + npm run build
    ruby          gem build/install or rake install

  Special
    auto          hetrix inspects the repo and picks the right system

  Auto-detection order:
    CMakeLists.txt -> meson.build -> configure[.ac] -> Makefile ->
    Cargo.toml -> go.mod -> build.zig -> dune-project ->
    pyproject.toml / setup.py -> package.json -> *.gemspec ->
    *.cpp files -> *.c files

--------------------------------------------------------------------------------
  INDEX.JSON FORMAT
--------------------------------------------------------------------------------

  Location: ~/hetrix/index.json
  Edit with: hetrix index --edit   or   ./configgy.sh

  Full example entry (all fields):

    "myapp": {
      "url":           "https://github.com/user/myapp",
      "pull_type":     "git",
      "build":         "cmake",
      "args":          "-DCMAKE_BUILD_TYPE=Release -DWITH_FEATURE=ON",
      "main":          "",
      "sub_dir":       "cli",
      "build_dir":     "",
      "output":        "myapp",
      "version":       "v2.1.0",
      "auto_add_cmds": "1",
      "description":   "my cool app",
      "install_cmd":   ""
    }

  FIELDS:

    url             git repo, tarball URL, or direct binary URL  (required)
    pull_type       how to download the source:
                      "git"  -- always use git clone
                      "curl" -- always use curl (archive or direct binary)
                      "auto" -- hetrix decides based on URL  (default)
    build           build system  (see BUILD SYSTEMS above)
    args            extra flags passed to the compiler or build tool
                    IMPORTANT for Go: if args contains -o and a path,
                    they are used verbatim -- hetrix won't add another -o
    main            entry source file for single-file gcc/g++ builds
    sub_dir         cd into this subdirectory before building
                    useful for repos where source is not at the root
    build_dir       override the directory to scan for built binaries
    output          name of the binary to produce  (default: package name)
    version         git tag or branch to pin  (default: latest version tag)
    auto_add_cmds   "1" = prompt to move binary to bin/ after build  (default)
                    "0" = skip the prompt, always keep binary in workspace
    description     short description shown in hetrix list
    install_cmd     custom shell command run in the source directory
                    (overrides the normal build+install flow entirely)

  Keys starting with _ are ignored by hetrix (use them as comments):

    "_note": { "url": "", "description": "-- my packages below --" }

  Go args example  (the tricky case):
    If your repo has a cmd/ subdir, set args to point at it:

      "build": "go",
      "args":  "-o validator ./cmd/validator",
      "output": ""    <-- leave empty so hetrix doesn't add a second -o

    hetrix detects the -o and path in args and uses them verbatim.

--------------------------------------------------------------------------------
  PULL_TYPE FIELD
--------------------------------------------------------------------------------

  pull_type controls HOW hetrix downloads the source:

    "auto"  (default)
      Sniffs the URL -- github.com/gitlab.com/etc -> git, otherwise curl.

    "git"
      Always git clone. Use for any git repository.
      Supports: version detection, submodule cloning, branch/tag pinning.

    "curl"
      Always curl download. Use for:
        - Release tarballs  (.tar.gz, .zip, .tar.xz, ...)
        - Direct binary downloads (no archive needed)
      For direct binaries (no archive extension in URL), hetrix downloads
      the file, marks it executable, and treats it as the built binary.

  Examples:

    Git repo:
      "url": "https://github.com/user/repo",
      "pull_type": "git"

    Tarball release:
      "url": "https://example.com/myapp-1.0.tar.gz",
      "pull_type": "curl",
      "build": "autoconf"

    Pre-built binary (curl, no build step):
      "url": "https://example.com/releases/download/v1.0/myapp-linux",
      "pull_type": "curl",
      "build": "auto",
      "output": "myapp"

--------------------------------------------------------------------------------
  AUTO_ADD_CMDS FIELD
--------------------------------------------------------------------------------

  Controls whether hetrix prompts to move the binary to bin/ after building.

    "auto_add_cmds": "1"   (default)
      After building, hetrix shows the found executables and asks:
        move to commands path (1) or keep as executable in place (2)?

    "auto_add_cmds": "0"
      Skip the prompt entirely. Binary stays in the workspace.
      Useful for libraries, build tools you only want locally, or packages
      that install themselves via install_cmd.

  Can also be overridden per-command with --move or --keep flags.

--------------------------------------------------------------------------------
  CONFIGGY -- INDEX MANAGER
--------------------------------------------------------------------------------

  configgy.sh is an interactive tool for managing index.json
  without hand-editing JSON.

  Usage:

    ./configgy.sh                  open interactive menu
    ./configgy.sh add              add a package (interactive prompts)
    ./configgy.sh remove <name>    remove from index and/or delete from bin/
    ./configgy.sh uninstall <name> delete the installed binary from bin/
    ./configgy.sh edit <name>      edit a single field
    ./configgy.sh list             list all packages grouped by build system
    ./configgy.sh show <name>      show all fields of a package
    ./configgy.sh search <query>   search by name, URL, or description
    ./configgy.sh validate         check index.json for JSON syntax errors
    ./configgy.sh format           pretty-print index.json in place

  The add command prompts for all fields including pull_type and auto_add_cmds.
  The remove command offers: index only, or index + delete binary from bin/.

--------------------------------------------------------------------------------
  LOCAL REPO OVERRIDE
--------------------------------------------------------------------------------

  Drop source into ~/hetrix/repos/<name>/ and hetrix uses that instead
  of downloading from the URL -- useful for offline use or local projects:

    mkdir -p ~/hetrix/repos/myapp
    cp -r /path/to/source/* ~/hetrix/repos/myapp/
    hetrix fetch myapp

--------------------------------------------------------------------------------
  PATH SETUP
--------------------------------------------------------------------------------

  Add ~/hetrix/bin to your PATH so installed binaries are accessible:

    echo 'export PATH="$PATH:$HOME/hetrix/bin"' >> ~/.bashrc
    source ~/.bashrc

  hetrix will print a warning if the bin directory is not on your PATH
  after a successful install.

--------------------------------------------------------------------------------
  SOURCE CODE
--------------------------------------------------------------------------------

  src/
    main.cpp            entry point + all commands + CLI parser
    builder.hpp         build system detection + all build strategies
    fetcher.hpp         git clone / curl download / local repo copy
    installer.hpp       binary detection + interactive install prompt
    config_manager.hpp  JSON parser + index.json load/save
    config.hpp          PackageEntry struct + PackageIndex type
    logger.hpp          coloured output + Nerd Font icons
    utils.hpp           filesystem, shell, path, string helpers

  To recompile after modifying source:

    cd ~/hetrix
    ./hetrix.sh --no-seed          (rebuild only, don't touch index.json)
    # or
    make

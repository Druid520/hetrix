#!/usr/bin/env bash
# hetrix.sh — compile and install hetrix
# Usage: ./hetrix.sh [--prefix DIR] [--no-install] [--no-seed]
set -e

# colours
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[1;36m'; BOLD='\033[1m'; DIM='\033[2m'; NC='\033[0m' #holy shit my fingers hurt

info()  { echo -e "${CYAN}  ï¡${NC} $*"; }
ok()    { echo -e "${GREEN}  ï ${NC} $*"; }
warn()  { echo -e "${YELLOW}  ï± ${NC} $*"; }
err()   { echo -e "${RED}  ï ${NC} $*"; } #aaaaaaaaaaa
step()  { echo -e "${BOLD}  >> ${NC} $*"; }
banner(){ echo -e "\n${CYAN}  +== ${BOLD}$* ${NC}${CYAN}==+${NC}\n"; }
#lets fucking never do that again
#banner
echo -e "${CYAN}"
cat << 'ASCII'
  ██╗  ██╗███████╗████████╗██████╗ ██╗██╗  ██╗
  ██║  ██║██╔════╝╚══██╔══╝██╔══██╗██║╚██╗██╔╝
  ███████║█████╗     ██║   ██████╔╝██║ ╚███╔╝ 
  ██╔══██║██╔══╝     ██║   ██╔══██╗██║ ██╔██╗ 
  ██║  ██║███████╗   ██║   ██║  ██║██║██╔╝ ██╗
  ╚═╝  ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝╚═╝╚═╝  ╚═╝
ASCII
echo -e "${NC}${DIM}  hetrix build script  *  v1.1${NC}\n"

#arg parsing
PREFIX="/usr/local"
NO_INSTALL=0
NO_SEED=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --prefix)     PREFIX="$2"; shift 2 ;;
        --prefix=*)   PREFIX="${1#*=}"; shift ;;
        --no-install) NO_INSTALL=1; shift ;;
        --no-seed)    NO_SEED=1; shift ;;
        --help|-h)
            echo "Usage: $0 [--prefix DIR] [--no-install] [--no-seed]"
            echo "  --prefix DIR   install to DIR/bin  (default: /usr/local)"
            echo "  --no-install   build only, don't install"
            echo "  --no-seed      don't copy index.json to ~/hetrix/"
            exit 0 ;;
        *) warn "Unknown argument: $1"; shift ;;
    esac
done

#fuck off dependencys
banner "checking build dependencies"
MISSING=()
for tool in g++ make; do
    if command -v "$tool" &>/dev/null; then
        ok "found: $tool"
    else
        warn "missing: $tool"
        MISSING+=("$tool")
    fi
done

if [[ ${#MISSING[@]} -gt 0 ]]; then
    err "Missing tools: ${MISSING[*]}"
    echo ""
    echo "  Install with your package manager:"
    echo "    Debian/Ubuntu  →  sudo apt install g++ make"
    echo "    Fedora/RHEL    →  sudo dnf install gcc-c++ make"
    echo "    Arch           →  sudo pacman -S gcc make"
    echo "    Alpine         →  sudo apk add g++ make"
    exit 1
fi

#its probin time!
banner "checking C++17 support"
TMP=$(mktemp /tmp/hetrix_probe_XXXXXX.cpp)
cat > "$TMP" << 'EOF'
#include <filesystem>
int main(){ return std::filesystem::exists("/") ? 0 : 1; }
EOF

EXTRA_LD=""
if g++ -std=c++17 -o /tmp/hetrix_probe "$TMP" -lstdc++fs 2>/dev/null; then
    EXTRA_LD="-lstdc++fs"
    ok "C++17 filesystem OK  (with -lstdc++fs)"
elif g++ -std=c++17 -o /tmp/hetrix_probe "$TMP" 2>/dev/null; then
    ok "C++17 filesystem OK"
else
    err "g++ does not support C++17 filesystem. Upgrade to g++ ≥ 8."
    rm -f "$TMP" /tmp/hetrix_probe
    exit 1
fi
rm -f "$TMP" /tmp/hetrix_probe

#compile
banner "compiling hetrix"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

CXXFLAGS="-std=c++17 -O2 -Wall -Wextra -Isrc"
LDFLAGS="-lpthread $EXTRA_LD"
CMD="g++ $CXXFLAGS src/main.cpp -o hetrix $LDFLAGS"
step "$CMD"
if eval "$CMD"; then
    ok "compiled: $SCRIPT_DIR/hetrix"
else
    err "compilation failed!"
    exit 1
fi

#shitty error stuff
if [[ "$NO_INSTALL" -eq 1 ]]; then
    warn "--no-install: skipping install."
    echo ""
    ok "binary at: $SCRIPT_DIR/hetrix"
    echo "  run:  ./hetrix help"
    exit 0
fi

#install binary
banner "installing hetrix"
mkdir -p "$PREFIX/bin"
if [[ ! -w "$PREFIX/bin" ]]; then
    warn "No write permission to $PREFIX/bin — trying sudo..."
    sudo install -Dm755 hetrix "$PREFIX/bin/hetrix"
else
    install -Dm755 hetrix "$PREFIX/bin/hetrix"
fi
ok "installed: $PREFIX/bin/hetrix"

#seed index.json into ~/hetrix/
banner "seeding package index"
HETRIX_DIR="${HETRIX_DIR:-$HOME/hetrix}"
mkdir -p "$HETRIX_DIR"

if [[ "$NO_SEED" -eq 1 ]]; then
    warn "--no-seed: skipping index.json copy."
elif [[ -f "$HETRIX_DIR/index.json" ]]; then
    info "index.json already exists at $HETRIX_DIR/index.json — left untouched."
elif [[ -f "$SCRIPT_DIR/index.json" ]]; then
    cp "$SCRIPT_DIR/index.json" "$HETRIX_DIR/index.json"
    ok "seeded:    $HETRIX_DIR/index.json"
else
    warn "No index.json found next to hetrix.sh — hetrix will write a template on first init."
fi

#also create the other dirs
mkdir -p "$HETRIX_DIR/bin"
mkdir -p "$HETRIX_DIR/workspace"
mkdir -p "$HETRIX_DIR/repos"
ok "workspace ready: $HETRIX_DIR/"

#PATH hint
echo ""
if [[ ":$PATH:" != *":$PREFIX/bin:"* ]]; then
    warn "$PREFIX/bin not in PATH. Add to ~/.bashrc:"
    echo "    export PATH=\"\$PATH:$PREFIX/bin\""
fi
if [[ ":$PATH:" != *":$HETRIX_DIR/bin:"* ]]; then
    warn "$HETRIX_DIR/bin not in PATH. Add to ~/.bashrc:"
    echo "    export PATH=\"\$PATH:$HETRIX_DIR/bin\""
fi

echo ""
echo -e "${GREEN}${BOLD}  ï hetrix installed!${NC}"
echo ""
echo "  hetrix help           show all commands"
echo "  hetrix list           list packages"
echo "  hetrix index --edit   edit your index.json"
echo "  hetrix fetch shredder install a package"
echo ""

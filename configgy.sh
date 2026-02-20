#!/usr/bin/env bash
#configgy.sh -- interactive index.json manager for hetrix
#Usage: ./configgy.sh [command] [args]
set -euo pipefail

#colours + nerd font icons
RED='\033[0;31m';    BRED='\033[1;31m' #holy shit not this again
GREEN='\033[0;32m';  BGREEN='\033[1;32m'
YELLOW='\033[1;33m'; CYAN='\033[1;36m'
BLUE='\033[1;34m';   MAGENTA='\033[1;35m'
BOLD='\033[1m';      DIM='\033[2m';  NC='\033[0m'

NF_OK='\xef\x80\x8c'      # nf-fa-check
NF_ERR='\xef\x80\x8d'     # nf-fa-times
NF_WARN='\xef\x81\xb1'    # nf-fa-warning
NF_ARROW='\xef\x81\xa1'   # nf-fa-angle-right
NF_TRASH='\xef\x87\xb8'   # nf-fa-trash

ok()     { echo -e "${BGREEN}  ${NF_OK}  ${NC}${GREEN}$*${NC}"; }
err()    { echo -e "${BRED}  ${NF_ERR}  ${NC}${RED}$*${NC}"; } #im gonna deal with the devil
warn()   { echo -e "${YELLOW}  ${NF_WARN}  ${NC}${YELLOW}$*${NC}"; }
info()   { echo -e "${CYAN}  ${NF_ARROW} ${NC}$*"; }
step()   { echo -e "${MAGENTA}  >> ${NC}${BOLD}$*${NC}"; }
blank()  { echo ""; }
divider(){ echo -e "${DIM}  $(printf '%0.s-' {1..54})${NC}"; }
#im getting arthritis
banner() {
    local msg="$1"
    local w=$(( ${#msg} + 4 ))
    local line; line=$(printf '%0.s=' $(seq 1 $w))
    blank
    echo -e "${BLUE}  +${line}+${NC}"
    echo -e "${BLUE}  |  ${BOLD}${msg}${NC}${BLUE}  |${NC}"
    echo -e "${BLUE}  +${line}+${NC}"
    blank
}
#shitty art
#man up and half guess your index
print_banner() {
    echo -e "${CYAN}"
    cat << 'ASCII'
   ___  ___  ____  ____  ____  ___  ____  __ _
  / __)/ _ \(  _ \(  __)(  __)/ __)(  __)(  ( \
 ( (__(  O ) ) / / ) _)  ) _)( (_ \ ) _) /    /
  \___)\___/(__\_)(__)  (__)  \___/(____)(__)__)
                         by hetrix  *  index manager
ASCII
    echo -e "${NC}"
}

#resolve paths
INDEX="${HETRIX_INDEX:-${HETRIX_DIR:-$HOME/hetrix}/index.json}"
BIN_DIR="${HETRIX_BIN:-${HETRIX_DIR:-$HOME/hetrix}/bin}"

check_index() {
    if [[ ! -f "$INDEX" ]]; then
        err "index.json not found at: $INDEX"
        info "Run: hetrix init"
        exit 1
    fi
}

validate_json() {
    if command -v python3 &>/dev/null; then
        if ! python3 -m json.tool "$INDEX" > /dev/null 2>&1; then
            err "index.json has a JSON syntax error!"
            info "Run: python3 -m json.tool $INDEX"
            return 1
        fi
    fi
    return 0
}

#pretty-print one package
#pink is the manliest color
show_package() {
    local name="$1"
    python3 - "$INDEX" "$name" << 'PY'
import sys, json, os
idx = json.load(open(sys.argv[1]))
name = sys.argv[2]
if name not in idx:
    print("  NOT FOUND")
    sys.exit(0)
p = idx[name]

NC   = "\033[0m"; DIM = "\033[2m"; BOLD = "\033[1m"
GREEN = "\033[1;32m"; CYAN = "\033[36m"

bin_dir = os.path.expanduser(
    os.environ.get("HETRIX_BIN",
    os.path.join(os.environ.get("HETRIX_DIR", os.path.expanduser("~/hetrix")), "bin")))
installed = os.path.exists(os.path.join(bin_dir, p.get("output", name)))
tick = f"  {GREEN}[installed]{NC}" if installed else ""

print(f"  {BOLD}{GREEN}{name}{NC}{tick}")
for k, v in p.items():
    if v:
        print(f"    {DIM}{k:<16}{NC} {v}")
PY
}

#add a package
cmd_add() {
    check_index
    banner "add package"

    read -rp "$(echo -e "  ${CYAN}package name   ${NC}: ")" PKG_NAME
    [[ -z "$PKG_NAME" ]] && { err "Name cannot be empty."; exit 1; }

    if python3 -c "import json,sys; d=json.load(open('$INDEX')); sys.exit(0 if '$PKG_NAME' not in d else 1)" 2>/dev/null; then
        : #please hope this code doesnt break
    else
        warn "Package '$PKG_NAME' already exists."
        read -rp "$(echo -e "  ${YELLOW}overwrite? [y/N]${NC}: ")" OVR
        [[ "${OVR,,}" != "y" ]] && { info "Aborted."; exit 0; }
    fi

    read -rp "$(echo -e "  ${CYAN}url            ${NC}: ")" PKG_URL
    [[ -z "$PKG_URL" ]] && { err "URL cannot be empty."; exit 1; }

    echo -e "  ${DIM}pull_type: git | curl | auto (default: auto)${NC}"
    read -rp "$(echo -e "  ${CYAN}pull_type      ${NC}: ")" PKG_PULL
    PKG_PULL="${PKG_PULL:-auto}"

    echo -e "  ${DIM}build: cmake|make|gcc|g++|autoconf|meson|cargo|go|zig|python|node|ruby|dune|auto${NC}"
    read -rp "$(echo -e "  ${CYAN}build          ${NC}: ")" PKG_BUILD
    PKG_BUILD="${PKG_BUILD:-auto}"

    read -rp "$(echo -e "  ${CYAN}output binary  ${NC}: ")" PKG_OUTPUT
    PKG_OUTPUT="${PKG_OUTPUT:-$PKG_NAME}"

    read -rp "$(echo -e "  ${CYAN}args (flags)   ${NC}: ")" PKG_ARGS
    read -rp "$(echo -e "  ${CYAN}main file      ${NC}: ")" PKG_MAIN
    read -rp "$(echo -e "  ${CYAN}sub_dir        ${NC}: ")" PKG_SUBDIR
    read -rp "$(echo -e "  ${CYAN}version/tag    ${NC}: ")" PKG_VERSION

    echo -e "  ${DIM}auto_add_cmds: 1=prompt to move to bin (default), 0=keep in workspace${NC}"
    read -rp "$(echo -e "  ${CYAN}auto_add_cmds  ${NC}: ")" PKG_AUTO
    PKG_AUTO="${PKG_AUTO:-1}"

    read -rp "$(echo -e "  ${CYAN}description    ${NC}: ")" PKG_DESC

    blank
    step "writing to $INDEX ..."

    python3 - << PY
import json
#baguette
path = "$INDEX"
with open(path) as f:
    idx = json.load(f)

entry = {}
for k, v in [
    ("url",           "$PKG_URL"),
    ("pull_type",     "$PKG_PULL"),
    ("build",         "$PKG_BUILD"),
    ("args",          "$PKG_ARGS"),
    ("main",          "$PKG_MAIN"),
    ("sub_dir",       "$PKG_SUBDIR"),
    ("output",        "$PKG_OUTPUT"),
    ("version",       "$PKG_VERSION"),
    ("auto_add_cmds", "$PKG_AUTO"),
    ("description",   "$PKG_DESC"),
]:
    if v and v not in ("auto", "1") or k in ("url", "build", "output"):
        entry[k] = v

idx["$PKG_NAME"] = entry

with open(path, "w") as f:
    json.dump(idx, f, indent=2)
    f.write("\n")
PY

    ok "Package '$PKG_NAME' added."
    blank
    show_package "$PKG_NAME"
}

#remove a package from index.json
cmd_remove() {
    check_index
    local name="${1:-}"
    [[ -z "$name" ]] && read -rp "$(echo -e "  ${CYAN}package to remove${NC}: ")" name
    [[ -z "$name" ]] && { err "No name given."; exit 1; }

    banner "remove: $name"
    show_package "$name"
    blank

    echo -e "  ${BOLD}  1)${NC} remove from index.json only"
    echo -e "  ${BOLD}  2)${NC} remove from index.json + delete binary from bin/"
    echo -e "  ${BOLD}  q)${NC} cancel"
    blank
    read -rp "$(echo -e "  ${BRED}choice${NC}: ")" CHOICE

    case "$CHOICE" in
        1)
            python3 - << PY
import json
path = "$INDEX"
with open(path) as f: idx = json.load(f)
idx.pop("$name", None)
with open(path, "w") as f:
    json.dump(idx, f, indent=2); f.write("\n")
PY
            ok "Removed '$name' from index.json."
            ;;
        2)
            #get binary name from index, how does this work? just does.
            BIN_NAME=$(python3 -c "
import json, sys
idx = json.load(open('$INDEX'))
pkg = idx.get('$name', {})
print(pkg.get('output', '$name'))
" 2>/dev/null)
            BIN_PATH="$BIN_DIR/$BIN_NAME"

            python3 - << PY
import json
path = "$INDEX"
with open(path) as f: idx = json.load(f)
idx.pop("$name", None)
with open(path, "w") as f:
    json.dump(idx, f, indent=2); f.write("\n")
PY
            ok "Removed '$name' from index.json."

            if [[ -f "$BIN_PATH" ]]; then
                rm -f "$BIN_PATH"
                ok "Deleted binary: $BIN_PATH"
            else
                warn "Binary not found at: $BIN_PATH  (may not have been installed)"
            fi
            ;;
        q|Q|"")
            info "Cancelled."
            ;;
        *)
            warn "Unknown choice -- cancelled."
            ;;
    esac
}

#edit a single field
cmd_edit() {
    check_index
    local name="${1:-}"
    [[ -z "$name" ]] && read -rp "$(echo -e "  ${CYAN}package to edit${NC}: ")" name
    [[ -z "$name" ]] && { err "No name given."; exit 1; }

    banner "edit: $name"
    show_package "$name"

    echo -e "  ${DIM}fields: url pull_type build args main sub_dir build_dir output version auto_add_cmds description install_cmd${NC}"
    read -rp "$(echo -e "  ${CYAN}field to edit  ${NC}: ")" FIELD
    [[ -z "$FIELD" ]] && { err "No field given."; exit 1; }

    CURRENT=$(python3 -c "
import json
idx = json.load(open('$INDEX'))
print(idx.get('$name', {}).get('$FIELD', ''))
" 2>/dev/null)
    echo -e "  current        : ${DIM}${CURRENT:-<empty>}${NC}"
    read -rp "$(echo -e "  ${CYAN}new value      ${NC}: ")" NEW_VAL

    python3 - << PY
import json
path = "$INDEX"
with open(path) as f: idx = json.load(f)
if "$name" not in idx:
    print("Package not found"); exit(1)
if "$NEW_VAL":
    idx["$name"]["$FIELD"] = "$NEW_VAL"
else:
    idx["$name"].pop("$FIELD", None)
with open(path, "w") as f:
    json.dump(idx, f, indent=2); f.write("\n")
PY

    ok "Updated $name.$FIELD"
    show_package "$name"
}

# list all packages (forgive my shitty comments)
cmd_list() {
    check_index
    banner "packages in index.json"

    python3 - "$INDEX" "$BIN_DIR" << 'PY'
import json, os, sys

idx      = json.load(open(sys.argv[1]))
bin_dir  = sys.argv[2]
pkgs     = {k:v for k,v in idx.items() if not k.startswith("_")}

NC    = "\033[0m"; DIM = "\033[2m"; BOLD = "\033[1m"
GREEN = "\033[1;32m"; BLUE = "\033[1;34m"

build_order = ["gcc","g++","cmake","make","autoconf","meson",
               "cargo","go","zig","python","node","ruby","dune","auto","bash"]

groups = {}
for name, entry in pkgs.items():
    b = entry.get("build", "auto")
    groups.setdefault(b, []).append((name, entry))

for b in build_order:
    if b not in groups: continue
    print(f"\n  {BLUE}[ {b} ]{NC}")
    print(f"  {DIM}  {'--'*27}{NC}")
    for name, entry in sorted(groups[b]):
        binname = entry.get("output", name)
        installed = os.path.exists(os.path.join(bin_dir, binname))
        tick = f"{GREEN}*{NC}" if installed else " "
        desc = entry.get("description", "")
        pull = entry.get("pull_type","")
        pull_str = f"  {DIM}[{pull}]{NC}" if pull and pull != "auto" else ""
        print(f"  {tick} {BOLD}{name:<22}{NC}{DIM}{desc}{NC}{pull_str}")
        print(f"      {DIM}{entry.get('url','')}{NC}")

print(f"\n  {DIM}total: {len(pkgs)} packages{NC}\n")
PY
}

#show one package
cmd_show() {
    check_index
    local name="${1:-}"
    [[ -z "$name" ]] && read -rp "$(echo -e "  ${CYAN}package name${NC}: ")" name
    [[ -z "$name" ]] && { err "No name given."; exit 1; }
    banner "package: $name"
    show_package "$name"
    blank
}

#remove binary from bin
cmd_uninstall() {
    local name="${1:-}"
    [[ -z "$name" ]] && read -rp "$(echo -e "  ${CYAN}package name${NC}: ")" name
    [[ -z "$name" ]] && { err "No name given."; exit 1; }

    banner "uninstall binary: $name"

    BIN_NAME=$(python3 -c "
import json, sys
idx = json.load(open('$INDEX'))
print(idx.get('$name', {}).get('output', '$name'))
" 2>/dev/null)
    BIN_PATH="$BIN_DIR/$BIN_NAME"

    if [[ -f "$BIN_PATH" ]]; then
        read -rp "$(echo -e "  ${BRED}delete $BIN_PATH? [y/N]${NC}: ")" CONFIRM
        [[ "${CONFIRM,,}" != "y" ]] && { info "Cancelled."; return; }
        rm -f "$BIN_PATH"
        ok "Deleted: $BIN_PATH"
    else
        warn "Binary not found: $BIN_PATH"
    fi
}

#search cause yes
cmd_search() {
    check_index
    local query="${1:-}"
    [[ -z "$query" ]] && read -rp "$(echo -e "  ${CYAN}search query${NC}: ")" query
    [[ -z "$query" ]] && { err "No query."; exit 1; }
    banner "search: $query"

    python3 - "$INDEX" "$query" << 'PY'
import json, sys
idx   = json.load(open(sys.argv[1]))
q     = sys.argv[2].lower()
NC    = "\033[0m"; BOLD = "\033[1m"
GREEN = "\033[1;32m"; DIM = "\033[2m"
found = 0
for name, entry in idx.items():
    if name.startswith("_"): continue
    hay = (name + entry.get("url","") + entry.get("description","")).lower()
    if q in hay:
        print(f"  {BOLD}{GREEN}{name}{NC}  {DIM}{entry.get('build','?')}{NC}")
        print(f"    {DIM}{entry.get('url','')}{NC}")
        if entry.get("description"):
            print(f"    {entry['description']}")
        print()
        found += 1
if found == 0:
    print(f"  no packages matching '{q}'")
PY
}

#validate
cmd_validate() {
    check_index
    banner "validating index.json"
    info "path: $INDEX"
    blank
    if python3 -m json.tool "$INDEX" > /dev/null 2>&1; then
        ok "index.json is valid JSON!"
        COUNT=$(python3 -c "import json; d=json.load(open('$INDEX')); print(len([k for k in d if not k.startswith('_')]))")
        info "$COUNT packages found."
    else
        err "index.json has a syntax error!"
        blank
        python3 -m json.tool "$INDEX" 2>&1 | head -20 || true
        exit 1
    fi
}

#format
cmd_format() {
    check_index
    banner "formatting index.json"
    if ! validate_json; then exit 1; fi
    python3 - << PY
import json
path = "$INDEX"
with open(path) as f: idx = json.load(f)
with open(path, "w") as f:
    json.dump(idx, f, indent=2); f.write("\n")
PY
    ok "index.json formatted cleanly."
}

#fuck ass men
cmd_menu() {
    while true; do
        clear
        print_banner
        echo -e "  ${BLUE}index:${NC} ${DIM}$INDEX${NC}"
        echo -e "  ${BLUE}bin:  ${NC} ${DIM}$BIN_DIR${NC}"
        blank
        echo -e "  ${BOLD}  1)${NC} add package"
        echo -e "  ${BOLD}  2)${NC} remove package (from index and/or bin)"
        echo -e "  ${BOLD}  3)${NC} edit package field"
        echo -e "  ${BOLD}  4)${NC} list all packages"
        echo -e "  ${BOLD}  5)${NC} show package details"
        echo -e "  ${BOLD}  6)${NC} search"
        echo -e "  ${BOLD}  7)${NC} uninstall binary from bin/"
        echo -e "  ${BOLD}  8)${NC} validate JSON"
        echo -e "  ${BOLD}  9)${NC} format / pretty-print"
        echo -e "  ${BOLD}  q)${NC} quit"
        blank
        read -rp "$(echo -e "  ${CYAN}choice${NC}: ")" CHOICE
        blank
        case "$CHOICE" in
            1) cmd_add       ;;
            2) cmd_remove    ;;
            3) cmd_edit      ;;
            4) cmd_list      ;;
            5) cmd_show      ;;
            6) cmd_search    ;;
            7) cmd_uninstall ;;
            8) cmd_validate  ;;
            9) cmd_format    ;;
            q|Q) info "bye!"; exit 0 ;;
            *) warn "unknown option '$CHOICE'" ;;
        esac
        blank
        read -rp "$(echo -e "  ${DIM}press enter to continue...${NC}")" _
    done
}

#help
cmd_help() {
    print_banner
    echo -e "${BLUE}  COMMANDS${NC}"
    _row() { printf "  ${BOLD}%-24s${NC} %s\n" "$1" "$2"; }
    _row "add"                    "add a new package interactively"
    _row "remove <name>"          "remove from index and optionally from bin/"
    _row "uninstall <name>"       "delete the installed binary from bin/"
    _row "edit <name>"            "edit a single field of a package"
    _row "list"                   "list packages grouped by build system"
    _row "show <name>"            "show all fields of one package"
    _row "search <query>"         "search by name, URL, or description"
    _row "validate"               "check index.json for syntax errors"
    _row "format"                 "pretty-print index.json in place"
    _row "(no args)"              "open the interactive menu"
    _row "help"                   "show this help"
    blank
    echo -e "${BLUE}  ENVIRONMENT${NC}"
    printf "  ${CYAN}HETRIX_INDEX${NC}  = %s\n" "$INDEX"
    printf "  ${CYAN}HETRIX_BIN${NC}    = %s\n" "$BIN_DIR"
    printf "  ${CYAN}HETRIX_DIR${NC}    = %s\n" "${HETRIX_DIR:-$HOME/hetrix}"
    blank
    echo -e "${BLUE}  INDEX.JSON FIELDS${NC}"
    _frow() { printf "  ${GREEN}%-18s${NC} %s\n" "$1" "$2"; }
    _frow "url"            "git repo, tarball URL, or direct binary URL"
    _frow "pull_type"      "\"git\" | \"curl\" | \"auto\"  (default: auto)"
    _frow "build"          "cmake|make|gcc|g++|autoconf|meson|cargo|go|zig|python|node|ruby|dune|auto"
    _frow "args"           "extra compiler / build tool flags"
    _frow "main"           "entry source file (single-file gcc/g++ builds)"
    _frow "sub_dir"        "cd into this subdir before building"
    _frow "build_dir"      "override where to search for built binaries"
    _frow "output"         "binary name to produce  (default: package name)"
    _frow "version"        "git tag or branch to pin  (default: latest tag)"
    _frow "auto_add_cmds"  "\"1\" prompt to install to bin (default)  \"0\" keep in workspace"
    _frow "description"    "short description shown in list"
    _frow "install_cmd"    "custom shell command run in src dir"
    blank
}

#ultracase
print_banner

case "${1:-}" in
    add)            cmd_add "${@:2}"      ;;
    remove|rm)      cmd_remove "${2:-}"   ;;
    uninstall)      cmd_uninstall "${2:-}";;
    edit)           cmd_edit "${2:-}"     ;;
    list|ls)        cmd_list              ;;
    show)           cmd_show "${2:-}"     ;;
    search)         cmd_search "${2:-}"   ;;
    validate)       cmd_validate          ;;
    format|fmt)     cmd_format            ;;
    help|--help|-h) cmd_help             ;;
    "")             cmd_menu              ;;
    *)
        err "unknown command: $1"
        info "run: ./configgy.sh help"
        exit 1
        ;;
esac
#finally over
#im lowk terry davis

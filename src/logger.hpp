#pragma once
#include <iostream>
#include <string>

//NO GOD PLEASE NO, NOOOOO
#define CLR_RESET    "\033[0m"
#define CLR_BOLD     "\033[1m"
#define CLR_DIM      "\033[2m"
#define CLR_CYAN     "\033[36m"
#define CLR_GREEN    "\033[32m"
#define CLR_YELLOW   "\033[33m"
#define CLR_RED      "\033[31m"
#define CLR_MAGENTA  "\033[35m"
#define CLR_BLUE     "\033[34m"
#define CLR_WHITE    "\033[97m"
#define CLR_BCYAN    "\033[1;36m"
#define CLR_BGREEN   "\033[1;32m"
#define CLR_BYELLOW  "\033[1;33m"
#define CLR_BRED     "\033[1;31m"
#define CLR_BBLUE    "\033[1;34m"
#define CLR_BMAGENTA "\033[1;35m"

//nerd
#define NF_OK      "\xef\x80\x8c"   //  nf-fa-check
#define NF_ERR     "\xef\x80\x8d"   //  nf-fa-times
#define NF_WARN    "\xef\x81\xb1"   //  nf-fa-warning
#define NF_DL      "\xef\x80\x99"   //  nf-fa-download
#define NF_COG     "\xef\x80\x93"   //  nf-fa-cog
#define NF_UP      "\xef\x82\x93"   //  nf-fa-upload
#define NF_TRASH   "\xef\x87\xb8"   //  nf-fa-trash
#define NF_ARROW   "\xef\x81\xa1"   //  nf-fa-angle-right
#define NF_SEARCH  "\xef\x80\x82"   //  nf-fa-search
#define NF_BOX     "\xef\x81\xa6"   //  nf-fa-cube
#define NF_LINK    "\xef\x83\x82"   //  nf-fa-chain

namespace Logger {

inline void info(const std::string& msg) {
    std::cout << CLR_CYAN "  " NF_ARROW " " CLR_RESET << msg << "\n";
}

inline void ok(const std::string& msg) {
    std::cout << CLR_BGREEN "  " NF_OK "  " CLR_RESET CLR_GREEN << msg << CLR_RESET "\n";
}

inline void warn(const std::string& msg) {
    std::cout << CLR_BYELLOW "  " NF_WARN "  " CLR_RESET CLR_YELLOW << msg << CLR_RESET "\n";
}

inline void err(const std::string& msg) {
    std::cerr << CLR_BRED "  " NF_ERR "  " CLR_RESET CLR_RED << msg << CLR_RESET "\n";
}

inline void step(const std::string& msg) {
    std::cout << CLR_BMAGENTA "  >> " CLR_RESET CLR_BOLD << msg << CLR_RESET "\n";
}

inline void detail(const std::string& msg) {
    std::cout << CLR_DIM "      " << msg << CLR_RESET "\n";
}

inline void blank() {
    std::cout << "\n";
}

inline void divider() {
    std::string line = "  " + std::string(54, '-');
    std::cout << CLR_DIM << line << CLR_RESET "\n";
}

//section banner with plain ASCII box
inline void banner(const std::string& msg) {
    int w = (int)msg.size() + 4;
    std::string top    = "  +" + std::string(w, '=') + "+";
    std::string middle = "  |  " + msg + "  |";
    std::string bot    = "  +" + std::string(w, '=') + "+";
    blank();
    std::cout << CLR_BBLUE << top    << CLR_RESET "\n";
    std::cout << CLR_BBLUE << middle << CLR_RESET "\n";
    std::cout << CLR_BBLUE << bot    << CLR_RESET "\n";
    blank();
}

inline void printMainBanner() {
    std::cout << CLR_BCYAN R"(
  ██╗  ██╗███████╗████████╗██████╗ ██╗██╗  ██╗
  ██║  ██║██╔════╝╚══██╔══╝██╔══██╗██║╚██╗██╔╝
  ███████║█████╗     ██║   ██████╔╝██║ ╚███╔╝ 
  ██╔══██║██╔══╝     ██║   ██╔══██╗██║ ██╔██╗ 
  ██║  ██║███████╗   ██║   ██║  ██║██║██╔╝ ██╗
  ╚═╝  ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝╚═╝╚═╝  ╚═╝
)" CLR_RESET;
    std::cout << CLR_DIM "  source-based package manager  *  v1.1\n" CLR_RESET;
    blank();
}

//specialised log types used during fetch/build/install/clean
inline void fetching(const std::string& msg) {
    std::cout << CLR_BBLUE "  " NF_DL "  " CLR_RESET << msg << "\n";
}

inline void building(const std::string& msg) {
    std::cout << CLR_BMAGENTA "  " NF_COG "  " CLR_RESET << msg << "\n";
}

inline void installing(const std::string& msg) {
    std::cout << CLR_BGREEN "  " NF_UP "  " CLR_RESET << msg << "\n";
}

inline void cleaning(const std::string& msg) {
    std::cout << CLR_BYELLOW "  " NF_TRASH "  " CLR_RESET << msg << "\n";
}

} //namespace Logger. where?

//
//  player-basic.hpp
//  CMD-Media-Player
//
//  Created by Robert He on 2024/9/1.
//

#ifndef player_basic_hpp
#define player_basic_hpp

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <readline/history.h>
#include <readline/readline.h>
#include <sstream>
#include <vector>

struct CLIOptions {
    std::vector<std::string> arguments;
    std::map<std::string, std::string> options;
};

template <typename T>
void printVector(const std::vector<T> &vec) {
    for (const auto &element : vec) {
        std::cout << element << "\n";
    }
    std::cout << std::endl;
}

template <typename K, typename V>
void printMap(const std::map<K, V> &m) {
    for (const auto &pair : m) {
        std::cout << pair.first << ": " << pair.second << std::endl;
    }
}

extern const std::string VERSION; // Declare the version variable

std::string format_time(int64_t seconds);
void get_terminal_size(int &width, int &height);
std::string get_system_type();
void save_default_options_to_file(std::map<std::string, std::string> &default_options);
void load_default_options_from_file(std::map<std::string, std::string> &default_options);
void show_interface();
void show_help_prompt();
void show_help(bool full_version = false);
void clear_screen();
std::vector<std::string> argv_to_vector(int argc, const char *argv[]);
std::vector<std::string> parseCommandLine(const std::string &str);
CLIOptions parseArguments(const std::vector<std::string> &args, std::map<std::string, std::string> defaultOptions, const char *self_name);
void print_error(std::string error_name, std::string error_detail = "");

#endif /* player_basic_hpp */

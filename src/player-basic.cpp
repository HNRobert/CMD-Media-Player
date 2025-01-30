//
//  player-basic.cpp
//  CMD-Media-Player
//
//  Created by Robert He on 2024/9/1.
//

#include "cmd-media-player/player-basic.hpp"
#include <iostream>

std::string format_time(int64_t seconds) {
    int64_t hours = seconds / 3600;
    int64_t minutes = (seconds % 3600) / 60;
    int64_t secs = seconds % 60;
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << hours << ":"
       << std::setfill('0') << std::setw(2) << minutes << ":"
       << std::setfill('0') << std::setw(2) << secs;
    return ss.str();
}

#ifdef _WIN32
#include <windows.h>

void get_terminal_size(int &width, int &height) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        width = csbi.dwSize.X;
        height = csbi.dwSize.Y;
    } else {
        // When failed to get:
        width = 80;  // Default width
        height = 25; // Default height
    }
}

#else
#include <sys/ioctl.h>
#include <unistd.h>

void get_terminal_size(int &width, int &height) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        width = ws.ws_col;
        height = ws.ws_row;
    } else {
        // When failed to get:
        width = 80;  // Default width
        height = 24; // default height
    }
}

#endif

const std::string SYS_TYPE = get_system_type();

std::string get_system_type() {
#ifdef _WIN32
    return "Windows\n";
#elif __linux__
    return "Linux\n";
#elif __APPLE__
    return "macOS\n";
#endif
}

std::string get_config_file_path() {
    const char *home_dir = getenv("HOME");
    if (!home_dir) {
        home_dir = ".";
    }
    return std::string(home_dir) + "/.config/CMD-Media-Player/config.txt";
}

void save_default_options_to_file(std::map<std::string, std::string> &default_options) {
    std::string config_file_path = get_config_file_path();
    std::filesystem::path config_dir = std::filesystem::path(config_file_path).parent_path();

    // Check if the directory exists, if not then create one
    if (!std::filesystem::exists(config_dir)) {
        if (!std::filesystem::create_directories(config_dir)) {
            std::cerr << "Error: Could not create config directory: " << config_dir << std::endl;
            return;
        }
    }

    // Load the config file and write in
    std::ofstream config_file(config_file_path);
    if (!config_file.is_open()) {
        std::cerr << "Error: Could not open config file for writing: " << config_file_path << std::endl;
        return;
    }

    // Write in the default option
    for (const auto &option : default_options) {
        config_file << option.first << "=" << option.second << std::endl;
    }

    config_file.close();
    std::cout << "Default options saved to " << config_file_path << std::endl;
}

void load_default_options_from_file(std::map<std::string, std::string> &default_options) {
    std::string config_file_path = get_config_file_path();

    if (!std::filesystem::exists(config_file_path)) {
        std::cout << "No config file found. Skipping loading defaults." << std::endl;
        return;
    }

    std::ifstream config_file(config_file_path);
    if (!config_file.is_open()) {
        std::cerr << "Error: Could not open config file for reading: " << config_file_path << std::endl;
        return;
    }

    std::string line;
    while (std::getline(config_file, line)) {
        std::size_t equals_pos = line.find('=');
        if (equals_pos != std::string::npos) {
            std::string key = line.substr(0, equals_pos);
            std::string value = line.substr(equals_pos + 1);
            default_options[key] = value;
        }
    }

    config_file.close();
    // std::cout << "Default options loaded from " << config_file_path << std::endl;
}

void show_interface() {
    std::cout << R"(
  ____ __  __ ____     __  __          _ _       
 / ___|  \/  |  _ \   |  \/  | ___  __| (_) __ _ 
| |   | |\/| | | | |  | |\/| |/ _ \/ _` | |/ _` |
| |___| |  | | |_| |  | |  | |  __/ (_| | | (_| |
 \____|_|  |_|____/   |_|  |_|\___|\__,_|_|\__,_|

 ____  _                       
|  _ \| | __ _ _   _  ___ _ __ 
| |_) | |/ _` | | | |/ _ \ '__|
|  __/| | (_| | |_| |  __/ |        - by HNRobert
|_|   |_|\__,_|\__, |\___|_|   
               |___/ 
)";
}

void show_help_prompt() {
    std::cout << R"(
-------- Type "help" and return for help --------
)";
}

extern const std::string VERSION; // Declare the version variable

void show_help(bool show_full) {
    std::cout << R"(
Usage:
  [command] [-m /path/to/media] [-st|-dy] [-s|-l] [-c "@%#*+=-:. "] /
  [/path/to/media] [-st|-dy] [-s|-l] [-c "@%#*+=-:. "] 

)";
    if (show_full) {
        std::cout << R"(Commands:
  play                 Start playing media in this terminal window
  set                  Set default options (e.g., media path, contrast mode)
  reset                Reset the default options to the initial state
  save                 Save the default options to a configuration file
  help                 Show this help message
  exit                 Exit the program

Options:
  -m /path/to/media    Specify the media file to play
  -st                  Use static contrast (default)
  -dy                  Use dynamic contrast 
                        Scaling the contrast dynamically 
                        based on each frame
  -s                   Use short character set "@#*+-:. " (default)
  -l                   Use long character set "@%#*+=^~-;:,'.` "
  -c "sequence"        Set a custom character sequence for ASCII art 
                        (prior to -s and -l)
                        Example: "@%#*+=-:. "
  --version            Show the version of the program
  -h, --help           Show this help message

While playing:
  [Space]              Pause/Resume
  [Left/Right Arrow]   Fast rewind/forward
  [Up/Down Arrow]      Increase/Decrease volume
  =                    Increase character set length
  -                    Decrease character set length
  Ctrl+C/Esc           Quit

Examples:
  play -m video.mp4 -dy -l
      Play 'video.mp4' using dynamic contrast and long character set 
      for ASCII art.
  play -m 'a video.mp4' -c "@#&*+=-:. "
      Play 'a video.mp4' with a custom character sequence for ASCII art.
      (add quotation marks on both sides if the path contains space)
      (if quotation marks included in the seq, use backslash to escape)
  set -m 'default.mp4'
      Set a default media path to 'default.mp4'
      for future playback commands.
  set -dy
      Set dynamic contrast as the default mode 
      for future playback commands.
  reset -m
      Reset the default media path to the initial state.

Version: )" << VERSION
                  << R"(
Homepage: https://github.com/HNRobert/CMD-Media-Player

)";
    }
}

void clear_screen() {
    if (SYS_TYPE == "Windows")
        system("cls");
    else
        system("clear");
}

std::vector<std::string> argv_to_vector(int argc, const char *argv[]) {
    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.find(' ') != std::string::npos) {
            arg = "\"" + arg + "\"";
        }
        args.push_back(arg);
    }
    return args;
}

std::vector<std::string> parseCommandLine(const std::string &str) {
    std::vector<std::string> result;
    std::string currentArg;
    bool inQuotes = false; // if we're between "s or 's
    char quoteChar = '\0'; // to track which quote character is used (' or ")

    for (std::size_t i = 0; i < str.length(); ++i) {
        char ch = str[i];

        if (ch == '\"' || ch == '\'') {
            // Toggle inQuotes state if matching quote is found
            if (!inQuotes) {
                inQuotes = true;
                quoteChar = ch; // Set the current quote character
            } else if (quoteChar == ch) {
                inQuotes = false;
            }
        } else if (std::isspace(ch) && !inQuotes) {
            // Finish the argument if not inside quotes
            if (!currentArg.empty()) {
                result.push_back(currentArg);
                currentArg.clear();
            }
        } else if (ch == '\\' && i + 1 < str.length()) {
            // Handle escaped characters
            char nextChar = str[i + 1];
            if (nextChar == '\"' || nextChar == '\'' || nextChar == '\\') {
                currentArg += nextChar; // Add escaped character
                ++i;                    // Skip the next character
            } else {
                currentArg += ch;
            }
        } else {
            // Add other characters directly
            currentArg += ch;
        }
    }

    // Add the last argument if any
    if (!currentArg.empty()) {
        result.push_back(currentArg);
    }

    return result;
}

CLIOptions parseArguments(const std::vector<std::string> &args,
                          std::map<std::string, std::string> defaultOptions,
                          const char *self_name) {
    CLIOptions cmdOptions;
    for (size_t i = 0; i < args.size(); ++i) {
        std::string arg = args[i];
        if (arg == self_name)
            continue;
        if (arg[0] == '-') { // option starts with '-'
            if (i + 1 < args.size() && args[i + 1][0] != '-') {
                cmdOptions.options[arg] = args[++i];
            } else {
                cmdOptions.options[arg] = ""; // option without value
            }
        } else {
            cmdOptions.arguments.push_back(arg); // normal arg
        }
    }
    // Add default options if they do not exist in cmdOptions
    for (const auto &defaultOption : defaultOptions) {
        if (cmdOptions.options.count(defaultOption.first) == 0) {
            cmdOptions.options[defaultOption.first] = defaultOption.second;
        }
    }
    return cmdOptions;
}

void print_error(std::string error_name, std::string error_detail) {
    std::cerr << error_name;
    if (error_detail.length())
        std::cout << ": " << error_detail;
    std::cout << std::endl
              << "Press any key to continue...";
    getchar();
}

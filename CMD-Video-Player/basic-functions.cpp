//
//  basic-functions.cpp
//  CMD-Video-Player
//
//  Created by Robert He on 2024/9/1.
//

#include "basic-functions.hpp"

#ifdef _WIN32
#include <windows.h>

void get_terminal_size(int &width, int &height) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        width = csbi.dwSize.X;
        height = csbi.dwSize.Y;
    } else {
        // 获取失败时的处理
        width = 80;  // 默认宽度
        height = 25; // 默认高度
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
        // 获取失败时的处理
        width = 80;  // 默认宽度
        height = 24; // 默认高度
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
    const char* home_dir = getenv("HOME");
    if (!home_dir) {
        home_dir = ".";
    }
    return std::string(home_dir) + "/.config/CMD-Video-Player/config.txt";
}

void save_default_options_to_file(std::map<std::string, std::string> &default_options) {
    std::string config_file_path = get_config_file_path();
    std::filesystem::path config_dir = std::filesystem::path(config_file_path).parent_path();

    // 检查目录是否存在，如果不存在则创建
    if (!std::filesystem::exists(config_dir)) {
        if (!std::filesystem::create_directories(config_dir)) {
            std::cerr << "Error: Could not create config directory: " << config_dir << std::endl;
            return;
        }
    }

    // 打开配置文件进行写入
    std::ofstream config_file(config_file_path);
    if (!config_file.is_open()) {
        std::cerr << "Error: Could not open config file for writing: " << config_file_path << std::endl;
        return;
    }

    // 写入默认选项
    for (const auto& option : default_options) {
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
        size_t equals_pos = line.find('=');
        if (equals_pos != std::string::npos) {
            std::string key = line.substr(0, equals_pos);
            std::string value = line.substr(equals_pos + 1);
            default_options[key] = value;
        }
    }
    
    config_file.close();
    std::cout << "Default options loaded from " << config_file_path << std::endl;
}

void show_interface() {
    std::cout << R"(
  ____ __  __ ____   __     ___     _            
 / ___|  \/  |  _ \  \ \   / (_) __| | ___  ___  
| |   | |\/| | | | |  \ \ / /| |/ _` |/ _ \/ _ \ 
| |___| |  | | |_| |   \ V / | | (_| |  __/ (_) |
 \____|_|  |_|____/     \_/  |_|\__,_|\___|\___/ 
                                                 
 ____  _                       
|  _ \| | __ _ _   _  ___ _ __ 
| |_) | |/ _` | | | |/ _ \ '__|
|  __/| | (_| | |_| |  __/ |        - by HNRobert
|_|   |_|\__,_|\__, |\___|_|   
               |___/            
)";
}

void show_help(bool show_full) {
    std::cout<<R"(
-------- Type "help" and return for help --------
)";

    if (show_full) {
        std::cout << R"(
Usage:
  play -v /path/to/video [-ct st/dy] [-c s/l] [-chars "@%#*+=-:. "]

Options:
  -v /path/to/video    Specify the video file to play
  -ct [st|dy]          Choose the contrast mode for ASCII art generation
                        st: Static contrast (default)
                        dy: Dynamic contrast, scales the contrast dynamically 
                            based on the video
  -c [s|l]             Choose the character set for ASCII art
                        s: Short character set "@#*+-:. " (default)
                        l: Long character set "@%#*+=^~-;:,'.` "
  -chars "sequence"    Set a custom character sequence for ASCII art 
                       (perior to -c)
                        Example: "@%#*+=-:. "

Examples:
  play -v video.mp4 -ct dy -c l
      Play 'video.mp4' using dynamic contrast and long character set 
      for ASCII art.
  play -v 'a video.mp4' -chars "@#&*+=-:. "
      Play 'a video.mp4' with a custom character sequence for ASCII art.
      (add quotation marks on both sides if the path contains space)
  set -v 'default.mp4'
      Set a default video path to 'default.mp4'
      for future playback commands.
  set -ct dy
      Set dynamic contrast as the default mode 
      for future playback commands.

Additional commands:
  help               Show this help message
  exit               Exit the program
  set                Set default options (e.g., video path, contrast mode)
  save               Save the default options to a configuration file

Other support? Visit https://github.com/HNRobert/CMD-Video-Player
)";
    }
}

void clear_screen() {
    if (SYS_TYPE == "Windows")
        system("cls");
    else
        system("clear");
}

std::pair<int, const char **> parseCommandLine(const std::string &str) {
    std::vector<std::string> result;
    std::string currentArg;
    bool inQuotes = false; // if we're between "s or 's
    char quoteChar = '\0'; // to track which quote character is used (' or ")

    for (size_t i = 0; i < str.length(); ++i) {
        char ch = str[i];

        if (ch == '"' || ch == '\'') {
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
            if (nextChar == '"' || nextChar == '\'' || nextChar == '\\') {
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

    // Add the final argument if it's not empty
    if (!currentArg.empty()) {
        result.push_back(currentArg);
    }

    // Convert std::vector<std::string> to const char* argv[]
    int argc = static_cast<int>(result.size());
    const char **argv = new const char *[argc + 1]; // +1 for the NULL terminator

    for (int i = 0; i < argc; ++i) {
        // Allocate memory for each string and copy its content
        argv[i] = new char[result[i].size() + 1];               // +1 for null terminator
        strcpy(const_cast<char *>(argv[i]), result[i].c_str()); // Copy string content
    }
    argv[argc] = nullptr; // NULL terminator for argv

    return std::make_pair(argc, argv);
}

cmdOptions parseArguments(const std::pair<int, const char **> &args, const char *self_name) {
    cmdOptions cmdOptions;
    int argc = args.first;
    const char **argv = args.second;
    for (int i = 0; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == self_name)
            continue;
        if (arg[0] == '-') { // option starts with '-'
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                // Check if there's a value following or not
                cmdOptions.options[arg] = argv[++i];
            } else {
                cmdOptions.options[arg] = ""; // option without value
            }
        } else {
            cmdOptions.arguments.push_back(arg); // normal arg
        }
    }

    return cmdOptions;
}

void print_error(std::string error_name, std::string error_detail) {
    std::cerr << error_name;
    if (error_detail.length())
        std::cout << ": " << error_detail;
    std::cout << std::endl
              << "Press any key to continue..." << std::endl;
    getchar();
}

//
//  main.cpp
//  CMD-Media-Player
//
//  Created by Robert He on 2024/9/1.
//

#include "cmd-media-player/basic-functions.hpp"
#include "cmd-media-player/media-player.hpp"

const std::string VERSION = "1.1.0";
const std::string UPDATE_DATE = "Jan 17th 2025";

const char *SELF_FILE_NAME;
std::map<std::string, std::string> default_options;

void get_command(std::string input = "$DEFAULT") {
    bool exit_next = input != "$DEFAULT";
    std::string next_step = exit_next ? "exit" : "$DEFAULT";
    if (!exit_next) {
        char *line = readline("\nYour command >> ");
        if (line) {
            if (*line) {
                add_history(line);
            }
            input = std::string(line);
            free(line);
        }
    }

    CLIOptions cmdOpts = parseArguments(parseCommandLine(input),
                                        default_options,
                                        SELF_FILE_NAME);

    // printVector(parseCommandLine(input));
    // printVector(cmdOpts.arguments);
    // printMap(cmdOpts.options);

    if (cmdOpts.options.count("--version")) {
        std::cout << "CMD-Media-Player version " << VERSION << "\nUpdated on: " << UPDATE_DATE << std::endl
                  << std::endl;
        get_command(next_step);
        return;
    }
    if (cmdOpts.options.count("-h") || cmdOpts.options.count("--help")) {
        show_help(true);
        get_command(next_step);
        return;
    }

    if (cmdOpts.arguments.size() == 0 && cmdOpts.options.size() == 0) {
        get_command(next_step);
        return;
    } else if (cmdOpts.arguments.size() == 0) {
        print_error("Arguments Error", "Please insert your argument");
        show_help();
        show_help_prompt();
        get_command(next_step);
        return;
    } else if (cmdOpts.arguments.size() > 1) {
        print_error("Arguments Error", "Only ONE argument is allowed!");
        show_help();
        show_help_prompt();
        get_command(next_step);
        return;
    }

    if (cmdOpts.arguments[0] == "help") {
        show_help(true);
        get_command(next_step);
        return;
    }

    if (cmdOpts.arguments[0] == "set") {
        // Save the options and apply them as default
        for (const auto &option : cmdOpts.options) {
            default_options[option.first] = option.second;
        }
        std::cout << "Settings updated successfully." << std::endl;
        if (exit_next) {
            save_default_options_to_file(default_options);
        }
        get_command(next_step);
        return;
    }

    if (cmdOpts.arguments[0] == "reset") {
        for (const auto &option : cmdOpts.options) {
            if (default_options.count(option.first)) {
                default_options.erase(option.first);
            }
        }
        std::cout << "Settings reset to default." << std::endl;
        if (exit_next) {
            save_default_options_to_file(default_options);
        }
        get_command(next_step);
        return;
    }

    if (cmdOpts.arguments[0] == "save") {
        save_default_options_to_file(default_options);
        get_command(next_step);
        return;
    }

    if (cmdOpts.arguments[0] == "play") {
        play_media(cmdOpts.options);

        show_interface();
        get_command(next_step);
        return;
    }

    if (cmdOpts.arguments[0] == "exit") {
        return;
    }

    if (std::filesystem::exists(cmdOpts.arguments[0])) {
        std::map<std::string, std::string> opt = {};
        for (const auto &option : cmdOpts.options) {
            opt[option.first] = option.second;
        }
        opt["-m"] = cmdOpts.arguments[0];
        play_media(opt);
    }

    get_command(next_step);
}

void start_ui() {
    show_interface();
    show_help_prompt();
    get_command();
}

int main(int argc, const char *argv[]) {
    SELF_FILE_NAME = argv[0];
    auto args = argv_to_vector(argc, argv);

    load_default_options_from_file(default_options);

    if (args.size() == 1) {
        clear_screen();
        start_ui();
    } else {
        std::string combined_args;
        for (size_t i = 1; i < args.size(); ++i) {
            combined_args += args[i] + " ";
        }
        get_command(combined_args);
    }

    return 0;
}

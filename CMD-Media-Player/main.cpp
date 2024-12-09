//
//  main.cpp
//  CMD-Media-Player
//
//  Created by Robert He on 2024/9/1.
//

#include "basic-functions.hpp"
#include "video-player.hpp"

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

    cmdOptions cmdOpts = parseArguments(parseCommandLine(input),
                                        default_options,
                                        SELF_FILE_NAME);

    
    printVector(parseCommandLine(input));
    printVector(cmdOpts.arguments);
    printMap(cmdOpts.options);
    
    
    if (cmdOpts.arguments.size() == 0) {
        print_error("Arguments Error", "Please insert your argument");
        show_help();
        get_command(next_step);
        return;
    } else if (cmdOpts.arguments.size() > 1) {
        print_error("Arguments Error", "Only ONE argument is allowed!");
        show_help();
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
        for (const auto& option : cmdOpts.options) {
            default_options[option.first] = option.second;
        }
        std::cout << "Settings updated successfully." << std::endl;
        get_command(next_step);
        return;
    }
    
    if (cmdOpts.arguments[0] == "save") {
        save_default_options_to_file(default_options);
        get_command(next_step);
        return;
    }

    if (cmdOpts.arguments[0] == "play") {
        // If any option not provided, use the default oftion
        // if (!params_include(cmdOpts.options, "-v") && params_include(default_options, "-v")) {
        //     cmdOpts.options["-v"] = default_options["-v"];
        // }
        // if (!params_include(cmdOpts.options, "-ct") && params_include(default_options, "-ct")) {
        //     cmdOpts.options["-ct"] = default_options["-ct"];
        // }
        // if (!params_include(cmdOpts.options, "-c") && params_include(default_options, "-c")) {
        //     cmdOpts.options["-c"] = default_options["-c"];
        // }
        // if (!params_include(cmdOpts.options, "-chars") && params_include(default_options, "-chars")) {
        //     cmdOpts.options["-chars"] = default_options["-chars"];
        // }
        
        play_video(cmdOpts.options);

        show_interface();
        get_command(next_step);
        return;
    }

    if (std::filesystem::exists(cmdOpts.arguments[0])) {
        std::map<std::string, std::string> opt = {{"-v", cmdOpts.arguments[0]}};
        play_video(opt);
    }

    if (cmdOpts.arguments[0] == "exit") {
        return;
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

    clear_screen();

    if (args.size() == 1) {
        start_ui();
    } else {
        // if (args.size() > 1)
        play_video(parseArguments(args, default_options, SELF_FILE_NAME).options);
    }
    
    return 0;
}

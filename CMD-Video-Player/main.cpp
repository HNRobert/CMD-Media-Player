//
//  main.cpp
//  CMD-Video-Player
//
//  Created by Robert He on 2024/9/1.
//

#include "basic-functions.hpp"
#include "video-player.hpp"

const char *SELF_FILE_NAME;

void get_command(std::string input = "$DEFAULT") {
    if (input == "$DEFAULT") {
        std::cout << std::endl
                  << "Your command >> ";
        std::getline(std::cin, input);
    }

    cmdOptions cmdOpts = parseArguments(parseCommandLine(input),
                                        SELF_FILE_NAME);

    /*
    printVector(parseCommandLine(input));
    printMap(cmdOpts.options);
    printVector(cmdOpts.arguments);
     */
    if (cmdOpts.arguments.size() == 0) {
        print_error("Arguments Error", "Please insert your argument");
        show_help(true);
        get_command();
        return;
    } else if (cmdOpts.arguments.size() > 1) {
        print_error("Arguments Error", "Only ONE argument is allowed!");
        show_help(true);
        get_command();
        return;
    }

    if (cmdOpts.arguments[0] == "help") {
        show_help(true);
        get_command();
        return;
    } else if (cmdOpts.arguments[0] == "play") {
        play_video(cmdOpts.options);
        show_interface();
        get_command();
        return;
    }

    if (std::filesystem::exists(cmdOpts.arguments[0])) {
        std::map<std::string, std::string> opt = {{"-v", cmdOpts.arguments[0]}};
        play_video(opt);
    }

    if (cmdOpts.arguments[0] == "exit") {
        return;
    }

    get_command();
}

void start_ui() {
    show_interface();
    show_help();
    get_command();
}

int main(int argc, const char *argv[]) {
    clear_screen();
    SELF_FILE_NAME = argv[0];
    switch (argc) {
        case 1:
            start_ui();
            break;
        case 2:
            play_video(parseArguments(std::make_pair(argc, argv), SELF_FILE_NAME).options);
            break;
        default:
            start_ui();
    }
    return 0;
}

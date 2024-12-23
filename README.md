# An app which plays image, video and audio in your console through C++, OpenCV, SDL2 and FFmpeg

## Installation

### Homebrew(arm64)

```sh
brew tap hnrobert/cmd-media-player && brew install cmd-media-player
```

## Function Description

![main-page](https://github.com/user-attachments/assets/398396d0-523d-4071-be3c-bf444629a939)

```txt
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

Usage:
  [command] [-v /path/to/video] [-st|-dy] [-s|-l] [-c "@%#*+=-:. "] /
  [/path/to/video] [-st|-dy] [-s|-l] [-c "@%#*+=-:. "] 

Options:
  -v /path/to/video    Specify the video file to play
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


Additional commands:
  help               Show this help message
  exit               Exit the program
  set                Set default options (e.g., video path, contrast mode)
  reset              Reset the default options to the initial state
  save               Save the default options to a configuration file

Examples:
  play -v video.mp4 -dy -l
      Play 'video.mp4' using dynamic contrast and long character set 
      for ASCII art.
  play -v 'a video.mp4' -c "@#&*+=-:. "
      Play 'a video.mp4' with a custom character sequence for ASCII art.
      (add quotation marks on both sides if the path contains space)
      (if quotation marks included in the seq, use backslash to escape)
  set -v 'default.mp4'
      Set a default video path to 'default.mp4'
      for future playback commands.
  set -dy
      Set dynamic contrast as the default mode 
      for future playback commands.
  reset -v
      Reset the default video path to the initial state.

```

## Effect

![kk1](https://github.com/user-attachments/assets/e284168c-05bf-4312-ab50-fcc3b8e3759c)

> You can use command +/- to scale the characters, then scale the terminal window to change the resolution

![kk2](https://github.com/user-attachments/assets/9ecb18d5-c500-4da0-abb0-dc8082dda450)

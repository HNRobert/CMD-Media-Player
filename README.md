# An app which plays image, video and audio in your console through C++, OpenCV, SDL2 and FFmpeg

![main-page](https://github.com/user-attachments/assets/a1988f68-5e57-49c5-920a-b7a8d57c9f11)

## Installation

### Homebrew(macOS)

```sh
brew tap hnrobert/cmdp && brew install cmdp
```

## Description

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
  [command] [-m /path/to/media] [-st|-dy] [-s|-l] [-c "@%#*+=-:. "] /
  [/path/to/media] [-st|-dy] [-s|-l] [-c "@%#*+=-:. "] 

Commands:
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
  [Ctrl+C]/[Esc]       Quit

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

```

## Demo

### Video

![kk1](https://github.com/user-attachments/assets/6c2b3a48-f8ac-4748-9b28-0794eebf66ea)

- You can use command +/- to scale the characters' size, then scale the terminal window to change the resolution
- While playing, you may press up/down arrow key to adjust volume, and use left/right arrow to fast rewind/forward
- You can also press -/= key directly to switch between different character sets

### Audio

> The Album cover would be shown if exists. (e.g. the one below belongs to Eagles - Hotel California.mp3)

![kk2](https://github.com/user-attachments/assets/6d5519f2-7bf7-43b1-9c01-cb421c8c4ea4)

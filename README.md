# An app which plays image, video and audio in your console through C++, OpenCV, SDL2 and FFmpeg

## Installation

### Homebrew(arm64)

```sh
brew tap hnrobert/cmd-video-player && brew install cmd-video-player
```

## Function Description:

<img width="697" alt="image" src="https://github.com/user-attachments/assets/75527a63-4e69-4a9a-a5a6-351450d97931">

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
  set                Set default options (e.g. video path, contrast mode)
  save               Save the default options to a configuration file

```

<img width="1075" alt="Screenshot 2024-09-08 at 17 52 12" src="https://github.com/user-attachments/assets/a219cb31-0a63-4b8f-bdba-4febac3b264e">
<img width="1131" alt="Screenshot 2024-09-08 at 17 51 00" src="https://github.com/user-attachments/assets/e8198745-13e8-4a57-b648-ac1539e6fd22">

It only supports macOS on Apple Silicon currently. If needed, other versions would be rolled out as long as you contact me.

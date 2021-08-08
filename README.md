# Pacman

This is a Pac-Man clone that I wrote in C(99) for Windows and Linux. This is a from-scratch approach and doesn't make use of any game engines or frameworks. Therefore there are no external dependencies outside of the stuff that ships with the platforms themselves. One thing to note is that Xlib is used on Linux for window and OpenGL context creation, so it might not work for users who are using a non-X11 display server on Linux.

The renderer is done in software and OpenGL is used to draw a single-textured quad to the screen.

## Build steps

Building should be quite straightforward and you won't really need anything outside of a C compiler and a command prompt/terminal to run the build scripts.

### Windows

Make sure you have a version of the MSVC compiler installed on your machine (I've used the version of MSVC that comes with Visual Studio 2019) and also ensure that you're able to use **cl** on the command prompt. If you get a warning that cl is not recognized, then you can either run the **vcvarsall.bat** script (with the x64 flag) that comes with your installation of Visual Studio or simply open the **x64 Native Tools Command Prompt for VS 2019** (or whatever version is applicable to your environment).

Once you've made sure everything is working correctly, run the following commands inside your command prompt:
```
> cd path\to\game_repo
> build.bat
> pacman.exe
```

### Linux

Make sure you have clang installed on your machine (or update the build.sh script if you want to make use of GCC), open up a terminal and execute the following commands:
```
$ cd path/to/game_repo
$ ./build.sh
$ ./pacman
```
>Note: **build.sh** may not have the correct permissions set to enable the file to be executed. If it fails to run, you need to first run `chmod +x ./build.sh` before you can execute the build script.

# Ping Pong Game

A simple ping pong game written in C using raylib.

## Build from source
>![!WARNING]
>This way of building works only on Linux for now!
You may need to install the following tools for raylib to work.
```console
sudo apt install libasound2-dev libx11-dev libxrandr-dev libxi-dev libgl1-mesa-dev libglu1-mesa-dev libxcursor-dev libxinerama-dev
```

Then compile build.c with any C compiler (for the first time)
```console
cc -o build.out build.c
```

And the any time you want to compile or recompile the project simply run:
```console
./build.out 
```
or 
```console
./build.out run
```
to compile and run.

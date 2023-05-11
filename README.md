# CHIP8-Emulator
CHIP8 is an interpreted programing language developed in the 70s for use on certain microcomputers. It was designed for simple games and applications on these systems. It offers simple graphics and input routines. Some popular CHIP8 games include Tetris, Space Invaders and Pong.

This emulator allows you to play games and applications designed for the CHIP8 platform

*Written in C using SDL*

**Currently a work in progress**

# Build

### Dependencies:

*libsdl2-dev*  
sudo apt-get install libsdl2-dev

*make*  
sudo apt-get install make

*gcc*  
sudo apt-get install gcc

### Build:  

execute **make** in the root folder

# Usage
./chip8 rom_file [**scale_factor**]

**scale_factor:** integer scaling factor of the original 64x32 CHIP8 resolution  
scale_factor: 20 (1280x640)

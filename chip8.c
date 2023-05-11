#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#include "SDL2/SDL.h"

#define RAM_SIZE 4096
#define ROM_START_ADDRESS 0x200
#define ROM_MAX_SIZE (RAM_SIZE - ROM_START_ADDRESS)

// SDL container object
typedef struct {
	SDL_Window *window;
	SDL_Renderer *renderer;
} sdl_display_t;

// Emulator confguration object
typedef struct {
	uint32_t window_width; // SDL window width
	uint32_t window_height;  // SDL window height
	uint32_t fg_color;		// Foreground color RGBA8888
	uint32_t bg_color;		// Background color RGBA8888	
	uint32_t scale_factor;		// Amount to scale a CHIP8 pixel by e.g. 20 will be 20 times larger
								// uses integer-scaling	
} config_t;

// Emulator states
typedef enum {
	STATE_QUIT,
	STATE_RUNNING,
	STATE_PAUSE,
} emulator_state_t;

// CHIP8 instructions format
typedef struct {
	uint16_t opcode; 
	uint16_t NNN;	// 12 bit address/constant
	uint8_t NN; 	// 8 bit constant
	uint8_t N;		// 4 bit constant
	uint8_t X;		// 4 bit register identifier
	uint8_t Y;		// 4 bit register identifer
} chip8_instruction_t;

// CHIP8 machine object
typedef struct {
	emulator_state_t state;
	uint8_t ram[RAM_SIZE];
	bool display[64*32];	// Emulate original CHIP8 resolution pixels
	uint16_t stack[12];		// Subroutine stack
	uint16_t stack_ptr;     // Ptr to first empty stack element
	uint8_t V[16];   		// V0-VF data registers
	uint16_t I;				// I memory register
	uint16_t PC;			// PC Program counter
	uint8_t delay_timer;	// Decrements at 60hz when >0
	uint8_t sound_timer;	// Decrements at 60hz when >0; beeps when >0
	bool keypad[16];		// 16 0x0-0xF keypad; true when pressed
	const char *rom_name;			// File name of currently running ROM
	chip8_instruction_t inst; 		// Currently executing instruction
} chip8_t;


bool init_sdl(sdl_display_t *sdl, const config_t config){
	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO) != 0) {
		SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
			return false;
	}
	// SDL_CreateWindow(title, x, y, w, h, flags)
	sdl->window = SDL_CreateWindow("CHIP8 emu", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 											   config.window_width * config.scale_factor, 																   		   config.window_height * config.scale_factor, 0);
	if(!sdl->window){
		SDL_Log("Could not create an SDL window %s\n", SDL_GetError());
		return false;
	}	

	sdl->renderer = SDL_CreateRenderer(sdl->window, -1, SDL_RENDERER_ACCELERATED);
		
	if(!sdl->renderer){
		SDL_Log("Could not create an SDL renderer %s\n", SDL_GetError());
	return false;
	}

	return true; // Success
}

bool init_chip8(chip8_t *chip8, const char *rom_name) {
	const uint8_t font[] = {
		0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
		0x20, 0x60, 0x20, 0x20, 0x70, // 1
		0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
		0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
		0x90, 0x90, 0xF0, 0x10, 0x10, // 4
		0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
		0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
		0xF0, 0x10, 0x20, 0x40, 0x40, // 7
		0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
		0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
		0xF0, 0x90, 0xF0, 0x90, 0x90, // A
		0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
		0xF0, 0x80, 0x80, 0x80, 0xF0, // C
		0xE0, 0x90, 0x90, 0x90, 0xE0, // D
		0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
		0xF0, 0x80, 0xF0, 0x80, 0x80  // F
	};
	
	// Load font
	memcpy(&chip8->ram[0], font, sizeof(font));

	// Load ROM
	FILE *rom = fopen(rom_name, "rb");
	if (!rom) {
		SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR, "ROM file %s could not be opened.\n", rom_name);
		return false;
	}

	// Read the ROM data into RAM
	fseek(rom, 0L, SEEK_END); // Seek end of the rom
	long rom_size = ftell(rom); // Get current position, which is the ROM size in bytes
	if (rom_size > ROM_MAX_SIZE) {			
		SDL_Log("ROM file %s is too large (%ld bytes); max size allowed: %d bytes\n", rom_name, rom_size, ROM_MAX_SIZE);
		return false;
	}
	rewind(rom);
	
	if(fread(&chip8->ram[ROM_START_ADDRESS], rom_size, 1, rom) !=1){
		SDL_Log("ROM file %s could not be read into CHIP8 memory\n", rom_name);
		return false;
	}

	fclose(rom);

	// Set CHIP8 machine defaults
	chip8->state = STATE_RUNNING; // Default machine state to on/running
	chip8->PC = ROM_START_ADDRESS;	// Initialise PC at ROM entry-point
	chip8->rom_name = rom_name;	
	chip8->stack_ptr = *chip8->stack;
		
	return true; // Success
}


// Set up emulator config from passed args
bool set_config_from_args(config_t *config, int argc, char **argv){	
	// set defaults
	*config = (config_t){
		.window_width = 64,  // CHIP8 original X resolution
		.window_height = 32, // CHIP8 original Y resolution	
		.fg_color = 0xFFFFFFFF, // WHITE 
		.bg_color = 0x000000FF,  // BLACK
		.scale_factor = 20,	// Default resolution: 1280x640
};
(void)argv;
(void)argc;
	return true;
}

void cleanup_sdl(const sdl_display_t sdl) {
	SDL_DestroyRenderer(sdl.renderer);
	SDL_DestroyWindow(sdl.window);
	SDL_Quit();
}

// Clear screen / SDL Window to background color
void clear_screen(const sdl_display_t sdl, const config_t config) {
	const uint8_t r = (config.bg_color >> 24) & 0xFF;
	const uint8_t g = (config.bg_color >> 16) & 0xFF;
	const uint8_t b = (config.bg_color >> 8) & 0xFF;
	const uint8_t a = (config.bg_color >> 0) & 0xFF;

SDL_SetRenderDrawColor(sdl.renderer, r, g, b, a);
	SDL_RenderClear(sdl.renderer);
}

void set_draw_color_fg(const sdl_display_t sdl, const config_t config)
{
	const uint8_t r = (config.fg_color >> 24) & 0xFF;
    const uint8_t g = (config.fg_color >> 16) & 0xFF;
    const uint8_t b = (config.fg_color >> 8) & 0xFF;
    const uint8_t a = (config.fg_color >> 0) & 0xFF;

	SDL_SetRenderDrawColor(sdl.renderer, r, g, b, a);
}

// Update window with any changes
void update_screen(const sdl_display_t sdl) {
	SDL_RenderPresent(sdl.renderer);
}

void handle_input(chip8_t *chip8) {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT:
				// Exit window; end program
				chip8->state = STATE_QUIT;
				return;
		
			// KEYDOWN events	
			case SDL_KEYDOWN:
				switch (event.key.keysym.sym){
					// KEYS
					case SDLK_ESCAPE:
						// Escape key -> Exit window; End program
						chip8->state = STATE_QUIT;
						return;
					case SDLK_SPACE:
						// Space bar -> Pause
						if(chip8->state == STATE_RUNNING){
							chip8->state = STATE_PAUSE;
							puts("CHIP8 paused");
						}
						else {	
							chip8->state = STATE_RUNNING;
							puts("CHIP8 resumed");
						return;
					default: 
						break;
					}
				}
				break;
			// KEYUP events
			case SDL_KEYUP:
				break;
			default: 
				break;			
		}
	}
					
}

#ifdef DEBUG
void print_debug_info(chip8_t *chip8, const sdl_display_t sdl, const config_t config) {
	printf("Addres: 0x%04X, Opcode: 0x%04X, Desc: ",
				   	chip8->PC-2, chip8->inst.opcode);
	switch ((chip8->inst.opcode >> 12) & 0xF) {
			case 0x0:
				if (chip8->inst.NN == 0xE0) {
					// 0x00E0: Clear the screen
					printf("Clear screen\n");
				} else if (chip8->inst.NN == 0xEE) {
					// 0x00EE: Return from subroutine
					// Decrement stack ptr, get last address
					// Set last address to PC, clear stack pointer
					printf("Return from subroutine to address 0x%04X\n",(chip8->stack_ptr - 1));	

				}
				else {
					printf("(Unimplemented Opcode) \n");
				}
				break;
			case 0x1:
				// 0x1NNN: Jump to address NNN
				printf("Jump to address 0x%04X\n", chip8->inst.NNN);
				chip8->PC = chip8->inst.NNN;
				break;
			case 0x2:
				printf("Call subroutine at address: 0x%04X\n", chip8->inst.NNN);
				// 0x2NNN: Call subroutine at NNN
				// Set current address to top of subroutine stack to return to
				chip8->stack_ptr = chip8->PC;

				// Set PC to subroutine
				// Next instruction executed will be at the PC address (the subroutine)
				chip8->PC = chip8->inst.NNN;
				
				chip8->stack_ptr++;
				break;
			case 0x6:
				printf("Set VX to address: 0x%04X\n", chip8->inst.NN);
				// 0x6XNN: Set VX to NN
				chip8->V[chip8->inst.X] = chip8->inst.NN;
				break; 
			case 0x7:
				// 0x7XNN: Add NN to VX
				printf("Add %d to V%d\n", chip8->inst.NN, chip8->inst.X);
				chip8->V[chip8->inst.X] = chip8->inst.NN;
				break;
			case 0xA:
				// 0xANNN: Sets I (memory register) to the address NNN
				printf("Set I (memory address) to address: 0x%04X\n", chip8->inst.NNN);
				chip8->I = chip8->inst.NNN;
				break;
			case 0xD:
				// 0xDXYN: Draw sprite at XY, N rows 
				printf("Draw sprite at x: %d, y: %d, n rows: %d \n", chip8->V[chip8->inst.X], chip8->V[chip8->inst.Y], chip8->inst.N);
				// Get X and Y by accessing VX and VY
				uint8_t X = chip8->V[chip8->inst.X];
				uint8_t Y = chip8->V[chip8->inst.Y];
				uint8_t N = chip8->inst.N;

				// For loop for N rows
				for (int i = 0; i <= N; i++) {
					// Get 8-bit data in ram of location I+i (i iterates N)
					uint8_t sprite_data = chip8->ram[chip8->I+i];
					for (int w = 7; w <= 0; w--) {
						// Get bit value corresponding to current pixel with respect to width
						// To do this: pad zeroes on the least significant end and AND
						// to obtain the desired bit
						int pixel_value = (sprite_data & (1 << w));
						
						if (pixel_value) {
							set_draw_color_fg(sdl, config);	
							SDL_Rect rect;
							// Set the rectangle x and y position relative to
							// the current width and height position
							rect.x = X+w;
							rect.y = Y+i;
							rect.w = 1;
							rect.h = 1;
							SDL_RenderFillRect(sdl.renderer, &rect);
						}
					}
				}
				break;
			default: 
				printf("Unimplemented opcode\n"); 
				break;		// Unimplemented or invalid opcode
	}

}
#endif

// Emulate 1 CHIP8 instruction
void emulate_instruction(chip8_t *chip8, const sdl_display_t sdl, const config_t config) {
	// Get next opcode from ram
			
	// Convert the big-endian opcode in memory to little-endian format.
	// This involves shifting the upper byte into the corresponding little endian position
	// and OR-ing it with the lower little-endian byte
	chip8->inst.opcode = (chip8->ram[chip8->PC] << 8) | chip8->ram[chip8->PC+1];
	
	// Increment the program counter to point to the next opcode
	chip8->PC += 2; 

	// Fill out instruction format	
	chip8->inst.NNN = chip8->inst.opcode & 0x0FFF;
	chip8->inst.NN = chip8->inst.opcode & 0x0FF;
	chip8->inst.N = chip8->inst.opcode & 0x0F;
	chip8->inst.X = (chip8->inst.opcode >> 8) & 0x0F;
	chip8->inst.Y = (chip8->inst.opcode >> 4) & 0x0F;

#ifdef DEBUG
	print_debug_info(chip8, sdl, config);
#endif	

	// Emulate opcode
	switch ((chip8->inst.opcode >> 12) & 0xF) {
			case 0x0:
				if (chip8->inst.NN == 0xE0) {
					// 0x00E0: Clear the screen
					memset(&chip8->display[0], false, sizeof(chip8->display)); 
				} else if (chip8->inst.NN == 0xEE) {
					// 0x00EE: Return from subroutine
					// Decrement stack ptr, get last address
					// Set last address to PC, clear stack pointer
					chip8->stack_ptr--; 

					chip8->PC = chip8->stack_ptr;

					chip8->stack_ptr = 0;
				}
				break;
			case 0x2:
				// 0x2NNN: Call subroutine at NNN
				// Set current address to top of subroutine stack to return to
				chip8->stack_ptr = chip8->PC;

				// Set PC to subroutine
				// Next instruction executed will be at the PC address (the subroutine)
				chip8->PC = chip8->inst.NNN;
					
				chip8->stack_ptr++;
				break;
			case 0xA:
				// 0xANNN: Sets I (memory register) to the address NNN
				chip8->I = chip8->inst.NNN;
				break;

			default: 
				break;		// Unimplemented or invalid opcode
	}

}




int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s rom_file [scale_factor]\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	// init emulator config/options
	config_t config = {0};
	if(!set_config_from_args(&config, argc, argv)) exit(EXIT_FAILURE); 

	// init sdl
    sdl_display_t sdl = {0};
    if (!init_sdl(&sdl, config)) exit(EXIT_FAILURE);

	// Initialise CHIP8 machine
	chip8_t chip8 = {0};
	const char *rom_name = argv[1];
	if (!init_chip8(&chip8, rom_name)) exit(EXIT_FAILURE);

	// initial screen clear to background color
	clear_screen(sdl, config);
	
	// Main emulator loop
	while (chip8.state != STATE_QUIT) {
		//Get_time ();

		//Emulate CHIP8 instructions
		
		handle_input(&chip8);
		
		if(chip8.state == STATE_PAUSE) continue;

		//Get_time(); since last Get_time();

		emulate_instruction(&chip8, sdl, config);

		//Delay for approx. 60hz/60fps (16.67ms)
		SDL_Delay(16);
		
		//Update with with changes each iteration	
		update_screen(sdl);
	}


	// init cleanup
	cleanup_sdl(sdl);
	exit(EXIT_SUCCESS);
}


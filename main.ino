#include <stdint.h>

#define DISP_SIZE 256 		/* 32 * 8*/
#define MEM_BASE 0x200
#define MEM_SIZE 700
#define BAUD_RATE 115200

/* TODO: 
	Check RAM sizes and usage

*/

uint8_t memory[MEM_SIZE];
uint8_t V[16];           		/* All general purpose registers */
uint16_t I;              		/* Register to store memory addr */
uint16_t pc;             		/* Program counter */
uint8_t display[DISP_SIZE]; /* 256 bytes or 256*8 = 2048 bits*/
uint8_t sp;              		/* Stack pointer points to next free slot in the stack */
uint16_t stack[16];      		/* The stack grtows upwards here */

uint8_t delay_timer;
uint8_t sound_timer;

bool is_running = 0;
bool draw_flag = 0;


const int8_t chip8_fontset[80] PROGMEM = {
	0xF0, 0x90, 0x90, 0x90, 0xF0,  // 0
	0x20, 0x60, 0x20, 0x20, 0x70,  // 1
	0xF0, 0x10, 0xF0, 0x80, 0xF0,  // 2
	0xF0, 0x10, 0xF0, 0x10, 0xF0,  // 3
	0x90, 0x90, 0xF0, 0x10, 0x10,  // 4
	0xF0, 0x80, 0xF0, 0x10, 0xF0,  // 5
	0xF0, 0x80, 0xF0, 0x90, 0xF0,  // 6
	0xF0, 0x10, 0x20, 0x40, 0x40,  // 7
	0xF0, 0x90, 0xF0, 0x90, 0xF0,  // 8
	0xF0, 0x90, 0xF0, 0x10, 0xF0,  // 9
	0xF0, 0x90, 0xF0, 0x90, 0x90,  // A
	0xE0, 0x90, 0xE0, 0x90, 0xE0,  // B
	0xF0, 0x80, 0x80, 0x80, 0xF0,  // C
	0xE0, 0x90, 0x90, 0x90, 0xE0,  // D
	0xF0, 0x80, 0xF0, 0x80, 0xF0,  // E
	0xF0, 0x80, 0xF0, 0x80, 0x80   // F
};


const uint8_t ibm_ch8[] PROGMEM = {
	0x00, 0xe0, 0xa2, 0x2a, 0x60, 0x0c, 0x61, 0x08, 0xd0, 0x1f, 0x70, 0x09,
	0xa2, 0x39, 0xd0, 0x1f, 0xa2, 0x48, 0x70, 0x08, 0xd0, 0x1f, 0x70, 0x04,
	0xa2, 0x57, 0xd0, 0x1f, 0x70, 0x08, 0xa2, 0x66, 0xd0, 0x1f, 0x70, 0x08,
	0xa2, 0x75, 0xd0, 0x1f, 0x12, 0x28, 0xff, 0x00, 0xff, 0x00, 0x3c, 0x00,
	0x3c, 0x00, 0x3c, 0x00, 0x3c, 0x00, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff,
	0x00, 0x38, 0x00, 0x3f, 0x00, 0x3f, 0x00, 0x38, 0x00, 0xff, 0x00, 0xff,
	0x80, 0x00, 0xe0, 0x00, 0xe0, 0x00, 0x80, 0x00, 0x80, 0x00, 0xe0, 0x00,
	0xe0, 0x00, 0x80, 0xf8, 0x00, 0xfc, 0x00, 0x3e, 0x00, 0x3f, 0x00, 0x3b,
	0x00, 0x39, 0x00, 0xf8, 0x00, 0xf8, 0x03, 0x00, 0x07, 0x00, 0x0f, 0x00,
	0xbf, 0x00, 0xfb, 0x00, 0xf3, 0x00, 0xe3, 0x00, 0x43, 0xe0, 0x00, 0xe0,
	0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0xe0, 0x00, 0xe0
};

static uint8_t memRead(uint16_t addr){	/* Helper function to read memory*/
	if (addr < 0x050)	/* It is a font */
		return pgm_read_byte(&chip8_fontset[addr]);

	if ((addr < (MEM_SIZE + MEM_BASE)) && (addr >= MEM_BASE))
		return memory[addr - MEM_BASE];		/* Address translation */
}

static void memWrite(uint16_t addr, uint8_t val){		/* Helper function to write into memory*/
	if (addr < (MEM_SIZE + MEM_BASE) && (addr >= MEM_BASE))
		memory[addr - MEM_BASE] = val;
}

void setup() {
	Serial.begin(BAUD_RATE);
	while (!Serial); 	/* Waits for serial port to connect */

	Serial.write(27);					/* ESC */
	Serial.print(F("[2J"));   /* clear the whole screen once */

	memset(memory, 0, sizeof(MEM_SIZE));

	for (int i = 0; i < sizeof(ibm_ch8); i++) {     /* Loads game ROM to ram at mem start */
		memory[i] = pgm_read_byte(&ibm_ch8[i]);
	}

	pc = MEM_BASE; /* Start of game */
	is_running = 1;

	Serial.println(sizeof(ibm_ch8));
}

/* MAIN LOOP */
void loop() {
	if (!is_running)
		return;
	for (int i = 0; i < 10; i++)
		emulate_cycle();
	if (draw_flag)
		draw();		/* Also un-sets draw flag */
}

void draw()
{
  Serial.write(27);          // ESC
  Serial.print(F("[H"));     // move cursor to top-left, so the frame overwrites the last one

  for (int y = 0; y < 32; y++) {
    for (int x = 0; x < 64; x++) {
      if (((display[y * 8 + x / 8] >> (7 - (x % 8))) & 1) == 1){
        Serial.print('#');
			}
      else{
        Serial.print('.');
			}
    }
    Serial.println();
  }
  draw_flag = 0;
}


void emulate_cycle() {
	uint16_t opcode = memRead(pc) << 8 | memRead(pc + 1);
	if (pc >= sizeof(memory))
		is_running = 0;

	pc += 2;

	switch (opcode & 0xF000) {
		case (0x0000):
			switch (opcode & 0x00FF) {
				case (0x00E0):
					memset(display, 0, sizeof(display));
					draw_flag = 1;
					break;

				case (0x00EE):
					sp--;
					pc = stack[sp];
					break;
			}
			break;

		case (0x2000):
			stack[sp] = pc;
			sp++;
			pc = opcode & 0x0FFF;
			break;

		case (0x4000):
			if (V[(opcode & 0x0F00) >> 8] != (opcode & 0x00FF))
				pc += 2;
			break;

		case (0x5000):
			if (V[(opcode & 0x0F00) >> 8] == V[(opcode & 0x00F0) >> 4])
				pc += 2;
			break;

		case (0xA000):
			I = opcode & 0x0FFF;
			break;

		case (0x6000):
			V[(opcode & 0x0F00) >> 8] = opcode & 0x00FF;
			break;

		case (0x1000):
			pc = opcode & 0x0FFF;
			break;

		case (0x7000):
			V[(opcode & 0x0F00) >> 8] += (opcode & 0x00FF);
			break;

		case (0x8000):
			switch (opcode & 0x000F) {
				case (0x0000):

				case (0x0001):
					V[(opcode & 0x0F00) >> 8] |= V[(opcode & 0x00F0) >> 4];
					break;
				case (0x0002):
					V[(opcode & 0x0F00) >> 8] &= V[(opcode & 0x00F0) >> 4];
					break;
				case (0x0003):
					V[(opcode & 0x0F00) >> 8] ^= V[(opcode & 0x00F0) >> 4];
					break;
				case (0x0004):{
					uint16_t result = V[(opcode & 0x0F00) >> 8] + V[(opcode & 0x00F0) >> 4];
					if (result > 255)
						V[0xF] = 1;
					else
						V[0xF] = 0;
					V[(opcode & 0x0F00) >> 8] = result & 0x00FF;
					break;
				}
				case (0x0005):
					if (V[(opcode & 0x0F00) >> 8] > V[(opcode & 0x00F0) >> 4])
						V[0xF] = 1;
					else
						V[0xF] = 0;
					V[(opcode & 0x0F00) >> 8] -= V[(opcode & 0x00F0) >> 4];
					break;
				case (0x0006):
					if (V[(opcode & 0x0F00) >> 8] & 0x01 == 1)
						V[0xF] = 1;
					else
						V[0xF] = 0;
					V[(opcode & 0x0F00) >> 8] >>= 1;
					break;
				case (0x0007):
					if (V[(opcode & 0x0F00) >> 8] < V[(opcode & 0x00F0) >> 4])
						V[0xF] = 1;
					else
						V[0xF] = 0;
					V[(opcode & 0x0F00) >> 8] = V[(opcode & 0x00F0) >> 4] - V[(opcode & 0x0F00) >> 8];
					break;
				case (0x000E):
					if (V[(opcode & 0x0F00) >> 8] & 0x80 == 0x80)
						V[0xF] = 1;
					else
						V[0xF] = 0;
					V[(opcode & 0x0F00) >> 8] << 1;
					break;
			}
			break;

		case (0x9000):
			if (V[(opcode & 0x0F00) >> 8] != V[(opcode & 0x00F0) >> 4])
				pc += 2;
			break;

		case (0xB000):
			pc = (opcode & 0x0FFF) + V[0];
			break;

		case (0xC000):{
			uint8_t random = rand() & 0xFF;
			V[(opcode & 0x0F00) >> 8] = random & (opcode & 0x00FF);
			break;
		}
		case (0x3000):
			if ((V[(opcode & 0x0F00) >> 8]) == (opcode & 0x00FF))
				pc += 2;
			break;

		case (0xE000): /* KEY CHECKS */
			switch (opcode & 0x00FF) 
			{
				/* Can reduce code lines by putting keycheck here */
				case (0x009E):
					/*
					uint8_t key_check = c->V[(opcode & 0x0F00) >> 8];	
					if(c->keys[key_check] == 1)
						c->pc += 2;
					break;
				 */
				case (0x00A1):
					/*
					uint8_t key_check = c->V[(opcode & 0x0F00) >> 8];
					if(c->keys[key_check] == 0)
						c->pc += 2;		
					*/
					break;
			}
			break;

		case (0xF000):
			switch (opcode & 0x00FF) {
				case (0x0007):
					V[(opcode & 0x0F00) >> 8] = delay_timer;
					break;
				case (0x000A): /*
					int key_pressed = 0;
					uint8_t pressed_key;
					for(uint8_t i = 0; i < 16 ; i++){
						if(c->keys[i] != 0){
							key_pressed = 1;		
							pressed_key = i;
						}
					}
					if(key_pressed == 1){
						c->V[(opcode & 0x0F00) >> 8] = pressed_key;
					}
					else{
						c->pc -= 2;	
					}
					*/
					break; 
				case (0x0015):
					delay_timer = V[(opcode & 0x0F00) >> 8];
					break;
				case (0x0018):
					sound_timer = V[(opcode & 0x0F00) >> 8];
					break;
				case (0x001E):
					I += V[(opcode & 0x0F00) >> 8];
					break;
				case (0x0029):{
					uint8_t digit = V[(opcode & 0x0F00) >> 8];
					I = digit * 5; /* Each font is 5 bytes */
					break;
				}
				case (0x0033):{
					uint32_t value = V[(opcode & 0x0F00) >> 8];
					memWrite(I, value / 100);
					memWrite(I + 1, (value / 10) % 10);
					memWrite(I + 2, value % 10);
					break;
				}
				case (0x0055):
					{
						uint8_t x = (opcode & 0x0F00) >> 8;
						for (int i = 0; i < x + 1; i++)
							memWrite(I + 1, V[i]);
						break;
					}
				case (0x0065):
					{
						uint8_t x = (opcode & 0x0F00) >> 8;
						for (int i = 0; i < x + 1; i++)
							V[i] = memRead(I + i);
						break;
					}
			}
			break;

		case (0xD000):{
			uint8_t n = opcode & 0x000F;	/* n byte long sprite*/
			V[0xF] = 0;
			uint8_t x_cords = V[(opcode & 0x0F00) >> 8];
			uint8_t y_cords = V[(opcode & 0x00F0) >> 4];
			for (uint8_t i = 0; i < n; i++) {
				uint8_t byte = memRead(I + i);
				for (uint8_t j = 0; j < 8; j++) {		/* To draw each sprite */
					uint8_t pixel = byte & (0x80 >> j);
					if (pixel) {
						uint32_t x_display = (x_cords + j) % 64;
						uint32_t y_display = (y_cords + i) % 32;
						uint32_t index = (y_display * 8) + (x_display / 8);

						uint8_t mask = 0x80 >> (x_display % 8);	
						display[index] ^= mask;
						if ((display[index] & mask) == 0){
							V[0xF] = 1;
						} 
					}
				}
			}
			draw_flag = 1;
			break;
		}
	}
}

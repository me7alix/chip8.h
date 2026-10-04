#include <stdio.h>
#include <stdlib.h>
#include <raylib.h>
#include <string.h>
#include <time.h>

#define CHIP8_IMPLEMENTATION
#include "chip8.h"

unsigned char key_mapping[255];

void init_key_mapping(void) {
	memset(key_mapping, 0xFF, sizeof(key_mapping));
	key_mapping[KEY_ONE]   = 0x1;
	key_mapping[KEY_TWO]   = 0x2;
	key_mapping[KEY_THREE] = 0x3;
	key_mapping[KEY_FOUR]  = 0xC;
	key_mapping[KEY_Q]     = 0x4;
	key_mapping[KEY_W]     = 0x5;
	key_mapping[KEY_E]     = 0x6;
	key_mapping[KEY_R]     = 0xD;
	key_mapping[KEY_A]     = 0x7;
	key_mapping[KEY_S]     = 0x8;
	key_mapping[KEY_D]     = 0x9;
	key_mapping[KEY_F]     = 0xE;
	key_mapping[KEY_Z]     = 0xA;
	key_mapping[KEY_X]     = 0x0;
	key_mapping[KEY_C]     = 0xB;
	key_mapping[KEY_V]     = 0xF;
}

const float frequency = 440.0f;
const float sampleRate = 44100.0f;
const float volume = 0.01f;
static float phase = 0.0f;
static int beep = 0;

void audio_callback(void *data, unsigned frames) {
	float *buffer = data;

	for (unsigned i = 0; i < frames; i++) {
		if (beep) {
			buffer[i] = (phase < 0.5f) ? volume : -volume;
		} else {
			buffer[i] = 0.0f;
		}

		phase += frequency / sampleRate;

		if (phase >= 1.0f) {
			phase -= 1.0f;
		}
	}
}

int load_program(Chip8 *m, char *filename) {
	FILE *f = fopen(filename, "rb");
	if (!f) return 0;

	fseek(f, 0, SEEK_END);

	size_t size = ftell(f);
	fseek(f, 0, SEEK_SET);

	unsigned char prog[size];
	fread(prog, 1, size, f);

	*m = chip8_init(prog, size);
	fclose(f);

	return 1;
}

void print_usage() {
    fprintf(stderr,
        "Usage: chip8 [options] <rom>\n"
        "Options:\n"
        "  -bg   Set the background color (default: 0x000000FF)\n"
        "  -fg   Set the foreground color (default: 0x00FF00FF)\n"
        "  -rs   Set the CPU refresh rate multiplier (default: 14)\n"
        "  -ss   Set the screen scale factor (default: 12)\n"
        "  -shb  Use alternate SHL/SHR behavior\n"
    );
}

void check_index(size_t i, size_t cnt) {
	if (i >= cnt) {
		fprintf(stderr, "error parsing last option\n");
		exit(1);
	}
}

int main(int argc, char *argv[]) {
	unsigned char shb = 0;
	unsigned bg = 0x000000FF;
	unsigned fg = 0x00FF00FF;
	unsigned rs = 14;
	unsigned ss = 12;
	char *rom = NULL;

	for (size_t i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-bg") == 0) {
			check_index(++i, argc);
			bg = strtoul(argv[i], NULL, 0);
	 	} else if (strcmp(argv[i], "-fg") == 0) {
			check_index(++i, argc);
			fg = strtoul(argv[i], NULL, 0);
	 	} else if (strcmp(argv[i], "-ss") == 0) {
			check_index(++i, argc);
			ss = strtoul(argv[i], NULL, 0);
	 	} else if (strcmp(argv[i], "-rs") == 0) {
			check_index(++i, argc);
			rs = strtoul(argv[i], NULL, 0);
	 	} else if (strcmp(argv[i], "-shb") == 0) {
			shb = 1;
	 	} else {
			if (!rom) {
				rom = argv[i];
			} else {
				print_usage();
				return 1;
			}
		}
	}

	if (!rom) {
		print_usage();
		return 1;
	}

	Chip8 m;
	/* Load ROM */ {
		if (!load_program(&m, rom)) {
			fprintf(stderr, "failed to load rom\n");
			return 1;
		}

		m.sh_vx_vy = shb;
	}

	init_key_mapping();

	Color BG = GetColor(bg);
	Color FG = GetColor(fg);

	int width  = CHIP8_WIDTH  * ss;
	int height = CHIP8_HEIGHT * ss;

	/* Setup audio */
	InitAudioDevice();
    AudioStream stream = LoadAudioStream(sampleRate, 32, 1);
    SetAudioStreamCallback(stream, audio_callback);
    PlayAudioStream(stream);

	/* Setup window */
	InitWindow(width, height, "Chip8");
	SetTargetFPS(60);

	while (!WindowShouldClose()) {
		BeginDrawing();
		ClearBackground(BG);

		/* Keyboard input */ {
			memset(m.key_down, 0, sizeof m.key_down);

			for (int key = 0; key < 256; key++) {
				unsigned char code = key_mapping[key];
				if (code != 0xFF) {
					if (IsKeyDown(key)) {
						m.key_down[code] = 1;
					}
				}
			}
		}

		/* Execution */ {
			for (size_t i = 0; i < rs; i++) {
				chip8_exec(&m);
			}
		}

		/* Timers */ {
			if (m.DT > 0) m.DT--;
			if (m.ST > 0) m.ST--;
			beep = m.ST;
		}

		/* Draw */ {
			for (size_t i = 0; i < CHIP8_HEIGHT; i++) {
				for (size_t j = 0; j < CHIP8_WIDTH; j++) {
					if (m.FB[i * CHIP8_WIDTH + j]) {
						DrawRectangle(j * ss, i * ss, ss, ss, FG);
					}
				}
			}
		}

		EndDrawing();
	}

	UnloadAudioStream(stream);
	CloseAudioDevice();
	CloseWindow();
	return 0;
}

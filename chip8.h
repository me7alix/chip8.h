#ifndef CHIP8_H_
#define CHIP8_H_

#define CHIP8_WIDTH  64
#define CHIP8_HEIGHT 32

typedef struct {
	unsigned short I, PC;
	unsigned char DT, ST;
	unsigned char V[16];
	unsigned char FB[64 * 32];
	unsigned char RAM[0x1000];
	unsigned short stack[64];
	unsigned char stack_ptr;
	unsigned char key_down[16];
	unsigned long long rnd_seed;
	unsigned char sh_vx_vy;
} Chip8;

static unsigned char chip8_font[] = {
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

Chip8 chip8_init(unsigned char *prog, size_t cnt);
void chip8_exec(Chip8 *m);

#endif // CHIP8_H_

#ifdef CHIP8_IMPLEMENTATION

static unsigned char chip8_rand(Chip8 *m) {
	m->rnd_seed = 6364136223846793005ULL * m->rnd_seed + 1;
	return m->rnd_seed >> 33;
}

static void chip8_memcpy(void *dst, void *src, unsigned size) {
	for (unsigned i = 0; i < size; i++) {
		((char*)dst)[i] = ((char*)src)[i];
	}
}

Chip8 chip8_init(unsigned char *prog, size_t cnt) {
	Chip8 m = {0};
	m.PC = 0x200;
	m.stack_ptr = 0;
	m.sh_vx_vy = 0;
	chip8_memcpy(m.RAM, chip8_font, sizeof chip8_font);
	chip8_memcpy(m.RAM + 0x200, prog, cnt);
	return m;
}

void chip8_exec(Chip8 *m) {
	unsigned short I = (m->RAM[m->PC] << 8) + m->RAM[m->PC + 1];

	switch (I >> (3 * 4)) {
	case 0x0:
		if ((I & 0x0F00) >> (2 * 4) == 0) {
			switch (I & 0x00FF) {
			case 0xE0: /* 00E0 - CLS */
				for (unsigned i = 0; i < sizeof(m->FB); i++)
					m->FB[i] = 0;
				m->PC += 2;
				break;
			case 0xEE: /* 00EE - RET */
				m->PC = m->stack[--m->stack_ptr];
			}
		} break;

	case 0x1: /* 1nnn - JP addr */
		m->PC = I & 0xFFF;
		break;

	case 0x2: /* 2nnn - CALL addr */
		m->stack[m->stack_ptr++] = m->PC + 2;
		m->PC = I & 0xFFF;
		break;

	case 0x3: { /* 3xkk - SE Vx, byte */
		unsigned char x = (I & 0x0F00) >> 8;
		unsigned char byte = I & 0x00FF;
		if (m->V[x] == byte) m->PC += 4;
		else m->PC += 2;
	} break;

	case 0x4: { /* 4xkk - SNE Vx, byte */
		unsigned char x = (I & 0x0F00) >> 8;
		unsigned char byte = I & 0x00FF;
		if (m->V[x] != byte) m->PC += 4;
		else m->PC += 2;
	} break;

	case 0x5: { /* 5xy0 - SE Vx, Vy */
		unsigned char x = (I & 0x0F00) >> 8;
		unsigned char y = (I & 0x00F0) >> 4;
		if (m->V[x] == m->V[y]) m->PC += 4;
		else m->PC += 2;
	} break;

	case 0x6: { /* 6xkk - LD Vx, byte */
		unsigned char x = (I & 0x0F00) >> 8;
		unsigned char byte = I & 0x00FF;
		m->V[x] = byte;
		m->PC += 2;
	} break;

	case 0x7: { /* 7xkk - ADD Vx, byte */
		unsigned char x = (I & 0x0F00) >> 8;
		unsigned char byte = I & 0x00FF;
		m->V[x] += byte;
		m->PC += 2;
	} break;

	case 0x8: {
		unsigned char x = (I & 0x0F00) >> 8;
		unsigned char y = (I & 0x00F0) >> 4;

		switch (I & 0x000F) {
		case 0x0: /* 8xy0 - LD Vx, Vy */
			m->V[x] = m->V[y];
			break;

		case 0x1:
			/* 8xy1 - OR Vx, Vy */
			m->V[x] |= m->V[y];
			break;

		case 0x2:
			/* 8xy2 - AND Vx, Vy */
			m->V[x] &= m->V[y];
			break;

		case 0x3: /* 8xy3 - XOR Vx, Vy */
			m->V[x] ^= m->V[y];
			break;

		case 0x4: { /* 8xy4 - ADD Vx, Vy */
			int cr = ((unsigned short)m->V[x] + m->V[y]) > 255;
			m->V[x] += m->V[y];
			m->V[0xF] = cr;
		} break;

		case 0x5: { /* 8xy5 - SUB Vx, Vy */
			int cr = m->V[x] >= m->V[y];
			m->V[x] -= m->V[y];
			m->V[0xF] = cr;
		} break;

		case 0x6: { /* 8xy6 - SHR Vx {, Vy} */
			unsigned char old = m->V[x];
			m->V[0xF] = old & 1;
			if (m->sh_vx_vy) {
				unsigned char shift = m->V[y];
				m->V[x] = old >> shift;
			} else {
				m->V[x] = old >> 1;
			}
		} break;

		case 0x7: { /* 8xy7 - SUBN Vx, Vy */
			int cr = m->V[y] >= m->V[x];
			m->V[x] = m->V[y] - m->V[x];
			m->V[0xF] = cr;
		} break;

		case 0xE: /* 8xy8 - SHL Vx {, Vy} */
			unsigned char old = m->V[x];
			m->V[0xF] = (old >> 7) & 1;
			if (m->sh_vx_vy) {
				unsigned char shift = m->V[y];
				m->V[x] = old << shift;
			} else {
				m->V[x] = old << 1;
			}
		}

		m->PC += 2;
	} break;

	case 0x9: { /* 9xy0 - SNE Vx, Vy */
		unsigned char x = (I & 0x0F00) >> 8;
		unsigned char y = (I & 0x00F0) >> 4;
		if (m->V[x] != m->V[y]) m->PC += 4;
		else m->PC += 2;
	} break;

	case 0xA: /* Annn - LD I, addr*/
		m->I = I & 0xFFF;
		m->PC += 2;
		break;

	case 0xB: /* Bnnn - JP V0, addr */
		m->PC = (I & 0x0FFF) + m->V[0];
		break;

	case 0xC: { /* Cxkk - RND Vx, byte */
		unsigned char x = (I & 0x0F00) >> 8;
		unsigned char byte = I & 0x00FF;
		m->V[x] = chip8_rand(m) & byte;
		m->PC += 2;
	} break;

	case 0xD: { /* Dxyn - DRW Vx, Vy, nibble */
		unsigned char x = (I & 0x0F00) >> 8;
		unsigned char y = (I & 0x00F0) >> 4;
		unsigned char n = (I & 0x000F);
		int flp = 0;

		unsigned char vx = m->V[x];
		unsigned char vy = m->V[y];

		for (size_t i = 0; i < n; i++) {
			for (size_t j = 0; j < 8; j++) {
				unsigned char rx = (j + vx) % CHIP8_WIDTH;
				unsigned char ry = (i + vy) % CHIP8_HEIGHT;
				unsigned char np = (m->RAM[m->I + i] >> (7 - j)) & 1;
				unsigned char *pp = m->FB + (ry * 64 + rx);
				if (*pp && np) flp = 1;
				*pp ^= np;
			}
		}

		m->V[0xF] = flp;
		m->PC += 2;
	} break;

	case 0xE: {
		unsigned char x = (I >> 8) & 0xF;
		switch (I & 0x00FF) {
		case 0x9E: /* Ex9E - SKP Vx */
			m->PC += m->key_down[m->V[x] & 0xF] ? 4 : 2;
			break;
		case 0xA1: /* ExA1 - SKNP Vx */
			m->PC += !m->key_down[m->V[x] & 0xF] ? 4 : 2;
		}
	} break;

	case 0xF:
		unsigned char x = (I >> 8) & 0xF;

		switch (I & 0xFF) {
		case 0x07: /* Fx07 - LD Vx, DT */
			m->V[x] = m->DT;
			m->PC += 2;
			break;

		case 0x0A: /* Fx0A - LD Vx, K */
			for (unsigned char i = 0; i < 16; i++) {
				if (m->key_down[i]) {
					m->V[x] = i;
					m->PC += 2;
					break;
				}
			} break;

		case 0x15: /* Fx15 - LD DT, Vx */
			m->DT = m->V[x];
			m->PC += 2;
			break;

		case 0x18: /* Fx18 - LD ST, Vx */
			m->ST = m->V[x];
			m->PC += 2;
			break;

		case 0x1E: /* Fx1E - ADD I, Vx */
			m->I += m->V[x];
			m->PC += 2;
			break;

		case 0x29: /* Fx29 - LD F, Vx */
			m->I = (m->V[x] & 0xF) * 5;
			m->PC += 2;
			break;

		case 0x33: /* Fx33 - LD B, Vx */
			unsigned char val = m->V[x];
			m->RAM[m->I + 0] = val / 100;
			val %= 100; m->RAM[m->I + 1] = val / 10;
			val %= 10;
			m->RAM[m->I + 2] = val / 1;
			m->PC += 2;
			break;

		case 0x55: /* Fx55 - LD [I], Vx */
			for (unsigned char i = 0; i <= x; i++)
				m->RAM[m->I + i] = m->V[i];
			m->PC += 2;
			break;

		case 0x65: /* Fx65 - LD Vx, [I] */
			for (unsigned char i = 0; i <= x; i++)
				m->V[i] = m->RAM[m->I + i];
			m->PC += 2;
		}
	}
}

#endif // CHIP8_IMPLEMENTATION

/*
 * Helper debugger tool based on simavr for RobCmp
 * Copyright 2023 Thiago Borges de Oliveira <thborges@gmail.com>
 */

#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <string.h>
#include <stdbool.h>

#include <sim_avr.h>
#include <sim_core.h>
#include <sim_elf.h>
#include <sim_gdb.h>
#include <avr_ioport.h>
#include <avr_twi.h>

#if __APPLE__
#define GL_SILENCE_DEPRECATION
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include <pthread.h>

#include <ssd1306_glut.h>
#include <ssd1306_virt.h>
#include <hd44780_glut.h>

avr_t * avr = NULL;
elf_firmware_t f = {{0}};

ssd1306_t ssd1306;
unsigned win_width, win_height;
int window_identifier;

hd44780_t hd44780;

uint8_t	builtin_led_state = 0;	// current port B, 5

int screen_columns = 0;
int screen_rows = 0;
const char *screen_title = "";

static void *avr_run_thread (void * ignore) {
	for (;;) {
		int state = avr_run(avr);
		if (state == cpu_Done || state == cpu_Crashed)
			break;
	}
	return NULL;
}

void load_firmware(const char *fname, const char *mcu, int32_t frequency) {
	if (fname) {
		if (elf_read_firmware(fname, &f) == -1) {
			fprintf(stderr, "Unable to load firmware from file %s\n", fname);
			exit(1);
		}
		printf("Firmware %s f=%d mmcu=%s\n", fname, (int)f.frequency, f.mmcu);
	}

	if (strlen(mcu) > 0) {
		strcpy(f.mmcu, mcu);
	}
	f.frequency = frequency;

	avr = avr_make_mcu_by_name(f.mmcu);
	if (!avr) {
		fprintf(stderr, "AVR mcu '%s' not known\n", f.mmcu);
		exit(1);
	}
	else
		printf("MCU used was: %s\n", f.mmcu);

	avr_init(avr);
	avr->log = LOG_DEBUG;
	avr_load_firmware(avr, &f);
	
}

void builtin_led_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
	builtin_led_state = value;
	//(builtin_led_state & ~(1 << irq->irq)) | (value << irq->irq);
}

/* Called on a key press */
void keyCB (unsigned char key, int x, int y) {
	switch (key) {
		case 'q':
			exit (0);
			break;
	}
}

// gl timer. if the lcd is dirty, refresh display
void timerCB(int i) {
	// restart timer
	glutTimerFunc (1000 / 64, timerCB, 0);
	glutPostRedisplay ();
}

/* Function called whenever redisplay needed */
void displayCB_ssd1306 (void) {
	const uint8_t seg_remap_default = ssd1306_get_flag (
					&ssd1306, SSD1306_FLAG_SEGMENT_REMAP_0);
	const uint8_t seg_comscan_default = ssd1306_get_flag (
					&ssd1306, SSD1306_FLAG_COM_SCAN_NORMAL);

	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Set up projection matrix
	glMatrixMode (GL_PROJECTION);
	// Start with an identity matrix
	glLoadIdentity ();
	glOrtho (0, win_width, 0, win_height, 0, 10);
	// Apply vertical and horizontal display mirroring
	glScalef (seg_remap_default ? 1 : -1, seg_comscan_default ? -1 : 1, 1);
	glTranslatef (seg_remap_default ? 0 : -win_width, seg_comscan_default ? -win_height : 0, 0);

	// Select modelview matrix
	glMatrixMode (GL_MODELVIEW);
	glPushMatrix ();
	// Start with an identity matrix
	glLoadIdentity ();
	ssd1306_gl_draw (&ssd1306);
	glPopMatrix ();
	glutSwapBuffers ();
}

int initGL_ssd1306(float pix_size) {
	win_width = screen_columns * pix_size;
	win_height = screen_rows * pix_size;

	// Double buffered, RGB disp mode.
	glutInitDisplayMode (GLUT_RGB | GLUT_DOUBLE);
	glutInitWindowSize (win_width, win_height);
	window_identifier = glutCreateWindow (screen_title);

	// Set window's display callback
	glutDisplayFunc (displayCB_ssd1306);
	// Set window's key callback
	glutKeyboardFunc (keyCB);

	glutTimerFunc (1000 / 24, timerCB, 0);

	ssd1306_gl_init (pix_size, SSD1306_GL_WHITE);

	return 1;
}


void displayCB_hd44780 (void) {
	static int color = 0;
	const uint32_t colors[][4] = {
		{ 0x00aa00ff, 0x00cc00ff, 0x000000ff, 0x00000055 },	// fluo green
		{ 0xaa0000ff, 0xcc0000ff, 0x000000ff, 0x00000055 },	// red
	};

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW); // Select modelview matrix
	glPushMatrix();
	glLoadIdentity(); // Start with an identity matrix
	glScalef(3, 3, 1);

	hd44780_gl_draw(
		&hd44780,
			colors[color][0], /* background */
			colors[color][1], /* character background */
			colors[color][2], /* text */
			colors[color][3] /* shadow */ );
	glPopMatrix();
	glutSwapBuffers();
}

int initGL_hd44780() {
	int pixsize = 3;
	screen_columns = (5 + hd44780.w * 6) * pixsize;
	screen_rows = (5 + hd44780.h * 8) * pixsize;
	screen_title = "HD44780 LCD";

	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
	glutInitWindowSize(screen_columns, screen_rows);
	window_identifier = glutCreateWindow(screen_title);

	// Set up projection matrix
	glMatrixMode(GL_PROJECTION); // Select projection matrix
	glLoadIdentity(); // Start with an identity matrix
	glOrtho(0, screen_columns, 0, screen_rows, 0, 10);
	glScalef(1,-1,1);
	glTranslatef(0, -1 * screen_rows, 0);

	glutDisplayFunc(displayCB_hd44780);		/* set window's display callback */
	glutKeyboardFunc(keyCB);		/* set window's key callback */
	glutTimerFunc(1000 / 24, timerCB, 0);

	glEnable(GL_TEXTURE_2D);
	glShadeModel(GL_SMOOTH);

	glClearColor(0.8f, 0.8f, 0.8f, 1.0f);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	hd44780_gl_init();

	return 1;
}

void displayCB_builtin_led (void) {
	// OpenGL rendering goes here...
	glClear(GL_COLOR_BUFFER_BIT);

	// Set up modelview matrix
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	float grid = 64;
	float size = grid * 0.8;
	glBegin(GL_QUADS);
	glColor3f(0,0,1);

	if (builtin_led_state) {
		float x = 7;
		float y = 7;
		glVertex2f(x + size, y + size);
		glVertex2f(x, y + size);
		glVertex2f(x, y);
		glVertex2f(x + size, y);
	}

	glEnd();
	glutSwapBuffers();
	//glFlush();				/* Complete any pending operations */
}

void initGL_builtin_led() {
	const int pixsize = 64;
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
	glutInitWindowSize(1 * pixsize, 1 * pixsize);
	window_identifier = glutCreateWindow("Built-in LED");

	// Set up projection matrix
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 1 * pixsize, 0, 1 * pixsize, 0, 10);
	glScalef(1,-1,1);
	glTranslatef(0, -1 * pixsize, 0);

	glutDisplayFunc(displayCB_builtin_led);
	glutKeyboardFunc(keyCB);
	glutTimerFunc(1000 / 24, timerCB, 0);
}

void init_ssd1306() {
	ssd1306_init (avr, &ssd1306, 128, 64);
	screen_columns = 128;
	screen_rows = 64;
	screen_title = "SSD1306 128x64 OLED";

	// SSD1306 wired to the SPI bus, with the following additional pins:
	ssd1306_wiring_t wiring =
	{
		.chip_select.port = 'B',
		.chip_select.pin = 4,
		.data_instruction.port = 'B',
		.data_instruction.pin = 1,
		.reset.port = 'B',
		.reset.pin = 3,
	};

	ssd1306_connect (&ssd1306, &wiring);
	
	initGL_ssd1306(2.0);
}

void init_hd44780() {
	hd44780_init(avr, &hd44780, 20, 4);

	/* Connect Data Lines to Port B, 0-3 */
	/* These are bidirectional too */
	for (int i = 0; i < 4; i++) {
		avr_irq_t * iavr = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), i);
		avr_irq_t * ilcd = hd44780.irq + IRQ_HD44780_D4 + i;
		// AVR -> LCD
		avr_connect_irq(iavr, ilcd);
		// LCD -> AVR
		avr_connect_irq(ilcd, iavr);
	}
	avr_connect_irq(
			avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 4),
			hd44780.irq + IRQ_HD44780_RS);
	avr_connect_irq(
			avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 5),
			hd44780.irq + IRQ_HD44780_E);
	avr_connect_irq(
			avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 6),
			hd44780.irq + IRQ_HD44780_RW);

	initGL_hd44780();
}

void init_builtin_led() {
	avr_irq_register_notify(
		avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 5),
		builtin_led_hook, 
		NULL);
	initGL_builtin_led();
}

int main(int argc, char *argv[])
{
	if (argc <= 1) {
		printf("%s -f firmware.elf -c 16000000 -m mcu -hw {hd44780,ssd1306,led}\n", argv[0]);
		exit(1);
	}

	char *mcu = NULL;
	char *firmware = NULL;
	bool hd44780 = false;
	bool ssd1306 = false;
	bool builtin_led = false;
	bool debug = false;
	int32_t frequency = 16000000;

	int i = 1;
	while (i < argc) {
		if (strncmp("-c", argv[i], 2) == 0) {
			i++;
			frequency = (i < argc) ? atoi(argv[i]) : 1600000;
		} else if (strncmp("-f", argv[i], 2) == 0) {
			i++;
			firmware = (i < argc) ? argv[i] : "";
		} else if (strncmp("-m", argv[i], 2) == 0) {
			i++;
			mcu = (i < argc) ? argv[i] : "";
		} else if (strncmp("-g", argv[i], 2) == 0) {
			debug = true;
		} else if (strncmp("-hw", argv[i], 3) == 0) {
			i++;
			char *hw = (i < argc) ? argv[i] : "";
			if (strncmp("hd44780", hw, 7) == 0) {
				hd44780 = true;
			}
			else if (strncmp("ssd1306", hw, 7) == 0) {
				ssd1306 = true;
			}
			else if (strncmp("led", hw, 3) == 0) {
			   builtin_led = true;
			}
			else {
				printf("Unknown hardware: %s\n", hw);
				exit(1);
			}
		} else
			printf("Unknown argment %s\n", argv[i]);
		i++;
	}

	if (!firmware) {
		printf("Firmware must be provided using -f.\n");
		exit(1);
	}

	bool use_hardware = hd44780 || ssd1306 || builtin_led;
	
	// Start interface
	if (use_hardware)
		glutInit(&argc, argv);

	// Start avr
	load_firmware(firmware, mcu, frequency);

	if (debug) {
		printf("Starting debugger at port 1234...\n");
		avr->gdb_port = 1234;
		avr->state = cpu_Stopped;
		avr_gdb_init(avr);
	}

	printf("Launching avr firmware...\n");
	pthread_t run;
	pthread_create(&run, NULL, avr_run_thread, NULL);

	// Start hardware
	if (hd44780)
		init_hd44780();
	if (ssd1306)
		init_ssd1306();
	if (builtin_led)
		init_builtin_led();

	if (use_hardware) {
		printf("Starting hardware...\n");
		glutMainLoop();
	} else {
		pthread_join(run, NULL);
	}

	/* Return the result of the last function executed.
	 * This is used on test cases of robcmp.
	 * After main returns, interruptions are disabled and
	 * the mcu is put to sleep.
	 */

	// assuming, by call convention, that r24 (LSB) and r25 (MSB) has the return code
	// See https://gcc.gnu.org/wiki/avr-gcc, Calling Convention
	uint16_t ret = avr->data[24] | (avr->data[25] << 8);
	printf("Return value %d\n", ret);

	avr_terminate(avr);
	return ret;
}

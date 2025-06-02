/* See COPYRIGHT for copyright information. */

#ifndef _CONSOLE_H_
#define _CONSOLE_H_
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#include <inc/types.h>

#define MONO_BASE	0x3B4
#define MONO_BUF	0xB0000
#define CGA_BASE	0x3D4
#define CGA_BUF		0xB8000

#define CRT_ROWS	25
#define CRT_COLS	80
#define CRT_SIZE	(CRT_ROWS * CRT_COLS)

/*
15		12 11	   8 7				0
[backcolor][forcolor][	character	]

The [backcolor][forcolor] is refered to as the attribute byte
and the [ character ] is refered to as the character byte.

The character byte holds the ASCII value of the character.

The structure of the attribute byte:
	Bit 76543210
	    ||||||||
		|||||^^^-fore colour
	    ||||^----fore colour bright bit
	    |^^^-----back colour
	    ^--------back colour bright bit OR enables blinking Text
*/

#define CRT_COLOR_BLACK			0
#define CRT_COLOR_BLUE			1
#define CRT_COLOR_GREEN			2
#define CRT_COLOR_CYAN			3
#define CRT_COLOR_RED			4
#define CRT_COLOR_MAGENTA		5
#define CRT_COLOR_BROWN			6
#define CRT_COLOR_LIGHT_GREY 	7
#define CRT_COLOR_DARK_GREY 	8
#define CRT_COLOR_LIGHT_BLUE	9
#define CRT_COLOR_LIGHT_GREEN	10
#define CRT_COLOR_LIGHT_CYAN	11
#define CRT_COLOR_LIGHT_RED		12
#define CRT_COLOR_LIGHT_MAGENTA	13
#define CRT_COLOR_LIGHT_BROWN	14
#define CRT_COLOR_WHITE			15

#define CRT_COLOR_FG_DEFAULT CRT_COLOR_LIGHT_GREY
#define CRT_COLOR_BG_DEFAULT CRT_COLOR_BLACK

#define RED_STR(s) ("\x1b[31m" s "\x1b[0m")
#define BLUE_STR(s) ("\x1b[34m" s "\x1b[0m")
#define BLUE_STR(s) ("\x1b[34m" s "\x1b[0m")
#define MAGENTA_STR(s) ("\x1b[35m" s "\x1b[0m")
#define GREEN_STR(s) ("\x1b[32m" s "\x1b[0m")

void cons_init(void);
int cons_getc(void);

void kbd_intr(void); // irq 1
void serial_intr(void); // irq 4

#endif /* _CONSOLE_H_ */

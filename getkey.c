#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/time.h>
#include "term.h"

#define	SENSEPERSEC	50
#define	WAITKEYPAD	360		/* msec */


main()
{
	unsigned char ch;
	int i;

	inittty(0);
	nonl2();
	raw2();
	noecho2();
	getterment();
	putterm(t_keypad);
	tflush();
	i = 0;
	for (;;) {
		if (!i) while (!kbhit2(1000000L / SENSEPERSEC));
		if ((ch = getch2()) == '\003') break;
		else if (ch == '\033') fprintf(stderr, "\r\n");
		i = kbhit2(WAITKEYPAD * 1000L);
		fprintf(stderr, "%02xh(%c),%1d  ",
			ch, isprint(ch) ? ch : '?', i);
	}

	putterm(t_nokeypad);
	cputs("\r\n");
	exit2(0);
}

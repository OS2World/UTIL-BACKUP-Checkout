/* program to talk to the GETTIME daemon */

#define PIPE "\\\\SERVER\\pipe\\gettime.ctl"

#define INCL_DOSNMPIPES
#define INCL_DOSPROCESS

#include <os2.h>
#include <stdio.h>

void cdecl main(void);

static unsigned char buffer[80];

void cdecl main()
{
	unsigned char op;
	HPIPE hp;
	USHORT n,sofar;
	DATETIME time;

	printf("\r\nGETTIME daemon control program.\r\n\r\n");
	while (TRUE) {
	tryagain:
		printf("\r\nChoose an operation:\r\n\r\n  1) Get the last received time "
			"from the NIST.\r\n  2) Force GETTIME to connect to the NIST.\r\n  "
			"3) Force GETTIME to terminate.\r\n  4) Quit.\r\n\r\n(1-4)?");
		gets(buffer);
		op = (unsigned char)(buffer[0]-'1');
		if (op > 3) {
			printf("\007\r\nBad operation.\r\n");
			goto tryagain; 
		}
		if (op == 3) break;
		printf("\r\n");
		if (DosOpen(PIPE,&hp,&n,0L,FILE_NORMAL,FILE_OPEN,
			OPEN_ACCESS_READWRITE|OPEN_SHARE_DENYNONE|OPEN_FLAGS_WRITE_THROUGH,
			0L)) goto abort1;
		if (DosWrite(hp,&op,1,&n)) goto abort;
		if (n != 1) goto abort;
		sofar = 0;
		do {
			if (DosRead(hp,(char far *)(&time)+sofar,sizeof(DATETIME)-sofar,
				&n)) goto abort;
			if (!n) goto abort;
			sofar += n;
		} while (sofar < sizeof(DATETIME));
		DosClose(hp);
		if (!op) printf("Last received time was on %d/%d/%d at %d:%02d:%02d.\r\n",
			time.month,time.day,time.year-1900,time.hours,time.minutes,time.seconds);
		else printf("Command sent.\r\n");
		continue;
	abort:
		DosClose(hp);
	abort1:
		printf("\007Can't connect with the GETTIME daemon.\r\n");
	}
}

/* program to talk to the GETTIME daemon */

#define PIPE "\\\\SERVER\\pipe\\gettime.ctl"

#define INCL_DOSNMPIPES
#define INCL_DOSPROCESS

#include <os2.h>
#include <stdio.h>

int cdecl main(int, char **);

int cdecl main(int argc, char **argv)
{
	unsigned char op;
	HPIPE hp;
	USHORT n,sofar;
	DATETIME time;

	printf("\r\nSYNCTIME server control program.\r\n\r\n");
	if (DosOpen(PIPE,&hp,&n,0L,FILE_NORMAL,FILE_OPEN,
		OPEN_ACCESS_READWRITE|OPEN_SHARE_DENYNONE|OPEN_FLAGS_WRITE_THROUGH,
		0L)) goto abort1;
	op = (unsigned char)(argc <= 1);
	if (DosWrite(hp,&op,1,&n) || n != 1) goto abort;
	sofar = 0;
	do {
		if (DosRead(hp,(char far *)(&time)+sofar,sizeof(DATETIME)-sofar,
			&n)) goto abort;
		if (!n) goto abort;
		sofar += n;
	} while (sofar < sizeof(DATETIME));
	DosClose(hp);
	printf("Last received time was on %d/%d/%d at %d:%02d:%02d.\r\n",
		time.month,time.day,time.year-1900,time.hours,time.minutes,time.seconds);
	if (op) printf("Connection to NIST started.\r\n");
	return 0;
abort:
	DosClose(hp);
abort1:
	printf("\007Can't communicate with the server.\r\n");
	return 1;
}

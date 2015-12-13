/* program to talk to the DOLABEL daemon */

#define PIPE "\\\\SERVER\\pipe\\netlabel.ctl"

#define INCL_DOSNMPIPES

#include <os2.h>
#include <stdio.h>
#include <string.h>

int cdecl main(int, char **);

int cdecl main(int argc, char **argv)
{
	char oplabel[12];
	HPIPE hp;
	USHORT n;

	if (argc < 2) goto abort1;
	_fstrncpy(oplabel,argv[1],11);
	oplabel[11] = 0;
	if (DosOpen(PIPE,&hp,&n,0L,FILE_NORMAL,FILE_OPEN,
		OPEN_ACCESS_READWRITE|OPEN_SHARE_DENYNONE|OPEN_FLAGS_WRITE_THROUGH,
		0L)) goto abort1;
	if (DosWrite(hp,oplabel,12,&n) || n != 12) goto abort;
	if (DosRead(hp,oplabel,1,&n)) goto abort;
	if (!n) goto abort;
	DosClose(hp);
	return 0;
abort:
	DosClose(hp);
abort1:
	puts("\007Can't label optical disk.\r\n");
	return 1;
}

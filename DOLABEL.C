/* label the optical disk */

#define OPTDRIVE 5
#define PIPENAME "netlabel.ctl"
#define PIPESIZE 1024

#define INCL_DOSNMPIPES
#define INCL_DOSFILEMGR

#include <os2.h>
#include <string.h>

void cdecl main(void);

void cdecl main()
{
	char name[12];
	HPIPE hp;
	USHORT n,sofar;
	VOLUMELABEL vollab;

	DosMakeNmPipe("\\pipe\\" PIPENAME,&hp,
		NP_ACCESS_DUPLEX|NP_NOINHERIT|NP_NOWRITEBEHIND,
		1|NP_WAIT|NP_READMODE_BYTE|NP_TYPE_BYTE,
		PIPESIZE,PIPESIZE,100000000);
	while (1) {
		if (DosConnectNmPipe(hp)) goto abort;
		sofar = 0;
		do {
			if (DosRead(hp,name+sofar,sizeof(name)-sofar,&n)) goto abort;
			if (!n) goto abort;
			sofar += n;
		} while (sofar < sizeof(name));
		if (DosWrite(hp,name,1,&n)) goto abort;
		if (n != 1) goto abort;
		vollab.cch = (BYTE)_fstrlen(name);
		_fstrcpy(vollab.szVolLabel,name);
		DosSetFSInfo(OPTDRIVE,FSIL_VOLSER,(PBYTE)&vollab,sizeof(VOLUMELABEL));
		DosRead(hp,&name,1,&n);
	abort:
		DosDisConnectNmPipe(hp);
	}
}

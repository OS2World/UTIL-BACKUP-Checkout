/* server daemon for the megamake program */

#define NAMES "D:\\lan\\megamake\\*.*"
#define MASTER ".MAS"
#define SLAVE ".SLV"
#define BUFFERSIZE 4096
#define PIPESIZE 4096

#define INCL_DOSNMPIPES
#define INCL_DOSPROCESS

#include <os2.h>
#include <string.h>

void cdecl main(void);
static USHORT copystring(HPIPE, HPIPE, char *);
static USHORT readdata(HPIPE, void *, unsigned int);
static USHORT copydata(HPIPE, HPIPE, char *);
static void far switchboard(void);

static char near username[13];
static ULONG semi = 0;

void cdecl main()
{
	FILEFINDBUF fbuf;
	SEL sel;
	TID tid;
	USHORT err;
	HDIR hdir = HDIR_SYSTEM;
	USHORT n = 1;

	DosSetMaxFH(100);
	if (DosFindFirst(NAMES,&hdir,FILE_DIRECTORY,&fbuf,sizeof(FILEFINDBUF),
		&n,0L)) DosExit(EXIT_PROCESS,1);
	do {
		strcpy(username,fbuf.achName);
		n = 1;
		err = DosFindNext(hdir,&fbuf,sizeof(FILEFINDBUF),&n);
		if (!err && username[0] != '.') {
			DosSemSet(&semi);
			if (DosAllocSeg(2048+BUFFERSIZE,&sel,SEG_NONSHARED))
				DosExit(EXIT_PROCESS,1);
			if (DosCreateThread(switchboard,&tid,MAKEP(sel,2046+BUFFERSIZE)))
				DosExit(EXIT_PROCESS,1);
			DosSemWait(&semi,SEM_INDEFINITE_WAIT);
		}
	} while (!err);
	DosFindClose(hdir);
	switchboard();
}

static void far switchboard()
{
	char buffer[BUFFERSIZE];
	char *pnt;
	USHORT n,dummy;
	HPIPE mhp,shp;

	strcpy(buffer,"\\pipe\\");
	strcat(buffer,username);
	DosSemClear(&semi);
	if (pnt = strchr(buffer,'.')) *pnt = 0;
	strcat(buffer,MASTER);
	DosMakeNmPipe(buffer,&mhp,
		NP_ACCESS_DUPLEX|NP_NOINHERIT|NP_NOWRITEBEHIND,
		1|NP_WAIT|NP_READMODE_BYTE|NP_TYPE_BYTE,
		PIPESIZE,PIPESIZE,100000000);
	if (pnt = strchr(buffer,'.')) *pnt = 0;
	strcat(buffer,SLAVE);
	DosMakeNmPipe(buffer,&shp,
		NP_ACCESS_DUPLEX|NP_NOINHERIT|NP_NOWRITEBEHIND,
		1|NP_WAIT|NP_READMODE_BYTE|NP_TYPE_BYTE,
		PIPESIZE,PIPESIZE,100000000);
	while (TRUE) {
		if (DosConnectNmPipe(shp)) goto abort;
		if (DosConnectNmPipe(mhp)) goto abort;
		if (copystring(mhp,shp,buffer)) goto abort;
		if (copystring(mhp,shp,buffer)) goto abort;
		if (copystring(mhp,shp,buffer)) goto abort;
		if (copydata(mhp,shp,buffer)) goto abort;
		if (copystring(mhp,shp,buffer)) goto abort;
		if (copydata(shp,mhp,buffer)) goto abort;
		if (readdata(shp,&n,2)) goto abort;
		if (DosWrite(mhp,&n,2,&dummy)) goto abort;
		if (!n) copydata(shp,mhp,buffer);
		DosRead(mhp,&n,1,&dummy);
	abort:
		DosDisConnectNmPipe(mhp);
		DosDisConnectNmPipe(shp);
	}
}

static USHORT readdata(hp,buffer,len)
HPIPE hp;
void *buffer;
unsigned int len;
{
	USHORT n,num;
	unsigned int sofar = 0;

	do {
		if (n = DosRead(hp,(char far *)buffer+sofar,len-sofar,&num)) return n;
		if (!num) return 12345;
		sofar += num;
	} while (sofar < len);
	return 0;
}

static USHORT copystring(fhf,thf,buffer)
HPIPE fhf,thf;
char *buffer;
{
	USHORT n,num,num1,len;

	if (n = readdata(fhf,&len,2)) return n;
	if (n = DosWrite(thf,&len,2,&num)) return n;
	do {
		if (len > BUFFERSIZE) num1 = BUFFERSIZE;
		else num1 = len;
		if (n = DosRead(fhf,buffer,num1,&num)) return n;
		if (!num) return 12345;
		if (n = DosWrite(thf,buffer,num,&num1)) return n;
		len -= num;
	} while (len);
	return 0;
}

static USHORT copydata(fhf,thf,buffer)
HPIPE fhf,thf;
char *buffer;
{
	USHORT n,num,num1;
	long len;

	if (n = readdata(fhf,&len,4)) return n;
	if (n = DosWrite(thf,&len,4,&num)) return n;
	do {
		if (len > BUFFERSIZE) num1 = BUFFERSIZE;
		else num1 = (USHORT)len;
		if (n = DosRead(fhf,buffer,num1,&num)) return n;
		if (!num) return 12345;
		if (n = DosWrite(thf,buffer,num,&num1)) return n;
		len -= num;
	} while (len);
	return 0;
}

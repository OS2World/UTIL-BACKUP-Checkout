/* get time from modem daemon */

#define PHONE "9,4944774"
#define TIME 5
#define BAUD 1200
#define COM "com2"
#define PIPENAME "gettime.ctl"
#define RETRY 10
#define TIMEZONE -7
#define PIPESIZE 1024
#define STACKSIZE 4096

#define INCL_DOSDEVICES
#define INCL_DOSNMPIPES
#define INCL_DOSPROCESS

#include <os2.h>
#include <string.h>
#include <stdlib.h>

void cdecl main(void);
static BOOL getline(char *, BOOL);
static void far listener(void);

static char str[256];
static HFILE file;
static ULONG semi = 0;
static USHORT retry = RETRY;
static DATETIME lasttime = { 0, 0, 0, 0, 0, 0, 1900, -1, 0 };

void cdecl main()
{
	int hour;
	SEL sel;
	TID tid;
	DATETIME date,temptime;
	DCBINFO dcb;
	LINECONTROL lct;
	RXQUEUE rq;
	PIDINFO pid;
	USHORT dummy,i,pos,err;
	UCHAR day = 0;
	USHORT baud = BAUD;

	DosSemSet(&semi);
	if (DosAllocSeg(STACKSIZE,&sel,SEG_NONSHARED))
		DosExit(EXIT_PROCESS,1);
	if (DosCreateThread(listener,&tid,MAKEP(sel,STACKSIZE-2)))
		DosExit(EXIT_PROCESS,1);
	while (retry) {
		if (!(err = DosSemWait(&semi,3300000))) DosSemSet(&semi);
		DosGetDateTime(&date);
		if (!err || (date.hours == TIME && date.day != day)) {
			day = date.day;
			DosGetPID(&pid);
			DosSetPrty(PRTYS_PROCESS,PRTYC_TIMECRITICAL,0,pid.pid);
			for (i = 0; i < retry; i++) {
				if (DosOpen(COM,&file,&dummy,0,FILE_NORMAL,FILE_OPEN,
					OPEN_ACCESS_READWRITE|OPEN_SHARE_DENYNONE,0)) goto retry1;
				DosDevIOCtl(&dcb,0L,0x0073,0x0001,file);
				dcb.fbCtlHndShake = MODE_DTR_CONTROL;
				dcb.fbFlowReplace = MODE_RTS_CONTROL;
				dcb.fbTimeout = MODE_NO_WRITE_TIMEOUT|MODE_WAIT_READ_TIMEOUT;
				DosDevIOCtl(0L,&dcb,0x0053,0x0001,file);
				DosDevIOCtl(0L,&baud,0x0041,0x0001,file);
				lct.bDataBits = 8;
				lct.bParity = 0;
				lct.bStopBits = 0;
				DosDevIOCtl(0L,&lct,0x0042,0x0001,file);
				DosSleep(250);
				DosWrite(file,"\r",1,&dummy);
				DosSleep(500);
				DosDevIOCtl(&rq,0L,0x0068,0x0001,file);
				while (rq.cch) {
					DosRead(file,str,sizeof(str),&dummy);
					rq.cch -= dummy;
				}
				DosWrite(file,"ATV1\r",5,&dummy);
				DosSleep(500);
				DosDevIOCtl(&rq,0L,0x0068,0x0001,file);
				if (!rq.cch || rq.cch > 255) goto abort;
				DosRead(file,str,sizeof(str)-1,&dummy);
				str[dummy] = 0;
				if (!strstr(str,"OK")) goto abort;
				strcpy(str,"ATDT");
				strcat(str,PHONE);
				strcat(str,"\r");
				DosWrite(file,str,strlen(str),&dummy);
				while (TRUE) {
					if (getline(str,FALSE)) goto abort;
					if (strstr(str,"ERROR")) goto abort;
					if (strstr(str,"NO DIAL TONE")) goto abort;
					if (strstr(str,"BUSY")) goto abort;
					if (strstr(str,"NO ANSWER")) goto abort;
					if (strstr(str,"CONNECT")) break;
				}
				while (TRUE) {
					if (getline(str,FALSE)) goto abort;
					if ((pos = strcspn(str,"-")) == strlen(str)) continue;
					if (pos < 2 || strlen(str)-pos+2 < 22) continue;
					if (str[pos+3] == '-' && str[pos+6] == ' ' &&
						str[pos+9] == ':' && str[pos+12] == ':' &&
						str[pos+15] == ' ' && str[pos+18] == ' ') break;
				}
				temptime = lasttime;
				while (_fmemcmp(&date,&temptime,sizeof(DATETIME))) {
					if (getline(str,FALSE)) goto abort;
					temptime = date;
					temptime.seconds++;
					if (getline(str,TRUE)) goto abort;
					if ((pos = strcspn(str,"-")) == strlen(str)) goto abort;
					if (pos < 2 || strlen(str)-pos+2 < 22) goto abort;
					if (str[pos+3] != '-' || str[pos+6] != ' ' ||
						str[pos+9] != ':' || str[pos+12] != ':' ||
						str[pos+15] != ' ' || str[pos+18] != ' ') goto abort;
					str[pos] = 0;
					if ((date.year = 1900+atoi(str+pos-2)) < 1990) date.year += 100;
					str[pos+3] = 0;
					if ((date.month = (UCHAR)atoi(str+pos+1)) > 12) goto abort;
					if (!date.month) date.month = 12;
					str[pos+6] = 0;
					if ((date.day = (UCHAR)atoi(str+pos+4)) < 1 || date.day > 31) goto abort;
					str[pos+9] = 0;
					if ((hour = atoi(str+pos+7)) > 23) goto abort;
					str[pos+12] = 0;
					if ((date.minutes = (UCHAR)atoi(str+pos+10)) > 59) goto abort;
					str[pos+15] = 0;
					if ((date.seconds = (UCHAR)atoi(str+pos+13)) > 59) goto abort;
					str[pos+18] = 0;
					dummy = atoi(str+pos+16);
					if (dummy >= 2 && dummy <= 51) hour++;
					hour += TIMEZONE;
					if (hour < 0) {
						hour += 24;
						date.day--;
					}
					if (hour > 23) {
						hour -= 24;
						date.day++;
					}
					date.hours = (UCHAR)hour;
					date.hundredths = 0;
				}
				DosSetDateTime(&date);
				lasttime = date;
				i = retry;
			abort:
				DosClose(file);
			retry1:
				DosSleep(2000);
			}
			DosSetPrty(PRTYS_PROCESS,PRTYC_REGULAR,0,pid.pid);
		}
	}
	DosExit(EXIT_PROCESS,0);
}

static BOOL getline(char *str,BOOL marker)
{
	USHORT dummy;
	int i = 0;

	while (TRUE) {
		DosRead(file,str+i,1,&dummy);
		if (!dummy || str[i] == '\n') continue;
		if (str[i] == '\r') break;
		if (marker && (str[i] == '*' || str[i] == '#')) break;
		if (++i > 255) return TRUE;
	}
	str[i] = 0;
	return (BOOL)strstr(str,"NO CARRIER");
}

static void far listener()
{
	HPIPE hp;
	UCHAR n;
	USHORT dummy;

	DosMakeNmPipe("\\pipe\\" PIPENAME,&hp,
		NP_ACCESS_DUPLEX|NP_NOINHERIT|NP_NOWRITEBEHIND,
		1|NP_WAIT|NP_READMODE_BYTE|NP_TYPE_BYTE,
		PIPESIZE,PIPESIZE,100000000);
	while (retry) {
		if (DosConnectNmPipe(hp)) goto abort;
		if (DosRead(hp,&n,1,&dummy)) goto abort;
		if (dummy != 1) goto abort;
		if (DosWrite(hp,&lasttime,sizeof(DATETIME),&dummy)) goto abort;
		if (dummy != sizeof(DATETIME)) goto abort;
		DosRead(hp,&n,1,&dummy);
		if (n == 1 || n == 2) {
			if (n == 2) {
				retry = 0;
				DosEnterCritSec();
			}
			DosSemClear(&semi);
		}
	abort:
		DosDisConnectNmPipe(hp);
	}
}

/* windowed workstation daemon for the megamake program */

#define ENVIRONMENT "TMP"
#define PLACE "megamake"
#define PIPE "\\\\SERVER\\pipe\\"
#define SLAVE ".SLV"
#define USER "C:\\lanman\\username.txt"
#define TEMPNAME "$$mega.tmp"
#define HOSTMARK "megamake\\"
#define BUFFERSIZE 4096
#define RETRYRATE 3000

#define INCL_DOSERRORS
#define INCL_DOSMISC
#define INCL_DOSQUEUES
#define INCL_DOSNMPIPES
#define INCL_DOSPROCESS
#define INCL_WINFRAMEMGR
#define INCL_WININPUT
#define INCL_WINTRACKRECT
#define INCL_WINSYS

#include <os2.h>
#include <string.h>

void cdecl main(void);
MRESULT EXPENTRY _export daemonproc(HWND, USHORT, MPARAM, MPARAM);
static void far thread(void);
static void showstatus(char *);
static USHORT readstring(HPIPE, char far *, USHORT);
static USHORT readdata(HPIPE, void far *, unsigned int);
static USHORT copy(HFILE, HFILE, long);

static char far *bigbuf;
static char name1[128];
static char name2[64];
static char pipename[64];
static char hostname[13];
static HWND hwndframe;
static HWND hwndclient;
static long semi = 0;

void cdecl main()
{
	SHORT n;
	HFILE hf;
	HAB hab;
	HMQ hmq;
	QMSG qmsg;
	TID tid;
	SEL sel;
	RECTL rcl;
	BOOL trytime = TRUE;
	ULONG frameflags = FCF_BORDER;

	hab = WinInitialize(0);
	hmq = WinCreateMsgQueue(hab,DEFAULT_QUEUE_SIZE);
	strcpy(hostname,"IDLE");
	WinRegisterClass(hab,"mdaemon",daemonproc,0L,0);
	hwndframe = WinCreateStdWindow(HWND_DESKTOP,0L,&frameflags,"mdaemon",
		"",0L,0,666,&hwndclient);
	if (!DosScanEnv(ENVIRONMENT,&bigbuf) && bigbuf[1] == ':' &&
		!DosSelectDisk((bigbuf[0]&0xdf)-'@') && !DosChDir(bigbuf+2,0L)) {
		DosMkDir(PLACE,0L);
		if (DosChDir(PLACE,0L)) goto error;
		if (DosOpen(USER,&hf,&n,0L,FILE_NORMAL,FILE_OPEN,
			OPEN_ACCESS_READONLY|OPEN_SHARE_DENYWRITE,0L)) goto error;
		if (DosRead(hf,name2,8,&n)) {
			DosClose(hf);
			goto error;
		}
		DosClose(hf);
		name2[n] = 0;
		rcl.yTop = WinQuerySysValue(HWND_DESKTOP,SV_CYSCREEN)-77;
		rcl.yBottom = rcl.yTop-18;
		rcl.xRight = WinQuerySysValue(HWND_DESKTOP,SV_CXSCREEN)-2;
		rcl.xLeft = rcl.xRight-117;
		WinCalcFrameRect(hwndframe,&rcl,FALSE);
		WinSetWindowPos(hwndframe,NULL,(SHORT)rcl.xLeft,(SHORT)rcl.yBottom,
			(SHORT)(rcl.xRight-rcl.xLeft),(SHORT)(rcl.yTop-rcl.yBottom),
			SWP_SIZE|SWP_MOVE|SWP_SHOW);
		if (DosAllocSeg(2048,&sel,SEG_NONSHARED)) goto error;
		if (DosCreateThread(thread,&tid,MAKEP(sel,2046))) goto error;
		DosSetPrty(PRTYS_THREAD,PRTYC_REGULAR,PRTYD_MINIMUM,tid);
		while(WinGetMsg(hab,&qmsg,NULL,0,0)) WinDispatchMsg(hab,&qmsg);
	}
	else {
	error:
		WinMessageBox(HWND_DESKTOP,hwndframe,
			"Megamake daemon can not be started.",
			"Uh Oh!",0,MB_CANCEL|MB_ICONHAND);
	}
	WinDestroyWindow(hwndframe);
	WinDestroyMsgQueue(hmq);
	WinTerminate(hab);
}

MRESULT EXPENTRY _export daemonproc(hwnd,msg,mp1,mp2)
HWND hwnd;
USHORT msg;
MPARAM mp1,mp2;
{
	HPS hps;
	RECTL rectl;

	switch (msg) {
	case WM_PAINT:
		hps = WinBeginPaint(hwnd,NULL,NULL);
		WinQueryWindowRect(hwnd,&rectl);
		DosSemRequest(&semi,SEM_INDEFINITE_WAIT);
		WinDrawText(hps,0xffff,hostname,&rectl,CLR_NEUTRAL,CLR_BACKGROUND,
			DT_CENTER|DT_VCENTER|DT_ERASERECT);
		DosSemClear(&semi);
		WinEndPaint(hps);
		return FALSE;
	case WM_BUTTON1DOWN:
		return WinSendMsg(hwndframe,WM_TRACKFRAME,(MPARAM)TF_MOVE,0);
	case WM_BUTTON1DBLCLK:
		if (WinMessageBox(HWND_DESKTOP,hwnd,"(c) 1989 Quark Incorporated\n"
			"by John Ridges\n\n\nTerminate the daemon?","Megamake Daemon",
			0,MB_YESNO|MB_NOICON|MB_DEFBUTTON2|MB_APPLMODAL) == MBID_YES)
			WinPostMsg(hwnd,WM_QUIT,0,0);
		return MRFROMSHORT(TRUE);
	default:
		return WinDefWindowProc(hwnd,msg,mp1,mp2);
	}
}

static void far thread()
{
	char progname[13];
	char infile[13];
	char outfile[13];
	char *pnt;
	HFILE hf,thf,oldstdout,oldstderr;
	HPIPE hp;
	FILESTATUS fs;
	FILEFINDBUF fbuf;
	USHORT n,err;
	ULONG l;
	RESULTCODES result;
	HDIR hdir;
	SEL sel;
	HFILE thestdout = 1;
	HFILE thestderr = 2;

	strupr(name2);
	strcpy(pipename,PIPE);
	strcat(pipename,name2);
	strcat(pipename,SLAVE);
	while (TRUE) {
	tryagain:
		if (err = DosOpen(pipename,&hp,&n,0L,FILE_NORMAL,FILE_OPEN,
			OPEN_ACCESS_READWRITE|OPEN_SHARE_DENYNONE|OPEN_FLAGS_WRITE_THROUGH,0L)) {
//			if (err == ERROR_PIPE_BUSY) DosWaitNmPipe(pipename,(long)RETRYRATE);
//			else
				DosSleep(RETRYRATE);
			goto tryagain;
		}
		if (readstring(hp,progname,13)) goto abort1;
		if (readstring(hp,name1,128)) goto abort1;
		if (pnt = strstr(name1+strlen(name1)+1,HOSTMARK))
			showstatus(pnt+strlen(HOSTMARK));
		else showstatus("BUSY");
		if (readstring(hp,infile,13)) goto abort1;
		if (DosAllocSeg(BUFFERSIZE,&sel,SEG_NONSHARED)) goto abort1;
		bigbuf = MAKEP(sel,0);
		if (readdata(hp,&l,4)) goto abort2;
		if (DosOpen(infile,&hf,&n,l,FILE_NORMAL,FILE_TRUNCATE|FILE_CREATE,
			OPEN_ACCESS_WRITEONLY|OPEN_SHARE_DENYREADWRITE,0L)) goto abort2;
		if (copy(hp,hf,l)) goto abort3;
		DosClose(hf);
		if (readstring(hp,outfile,13)) goto abort4;
		if (DosOpen(TEMPNAME,&hf,&n,0L,FILE_NORMAL,FILE_TRUNCATE|FILE_CREATE,
			OPEN_ACCESS_READWRITE|OPEN_SHARE_DENYREADWRITE|OPEN_FLAGS_NOINHERIT,
			0L)) goto abort4;
		DosOpen("NUL",&thf,&n,0L,FILE_NORMAL,FILE_OPEN,
			OPEN_ACCESS_WRITEONLY|OPEN_SHARE_DENYNONE|OPEN_FLAGS_NOINHERIT,0L);
		oldstdout = oldstderr = 0xFFFF;
		DosDupHandle(thestdout,&oldstdout);
		DosDupHandle(thestderr,&oldstderr);
		DosDupHandle(hf,&thestdout);
		DosDupHandle(thf,&thestderr);
		err = DosExecPgm(name2,63,EXEC_SYNC,name1,NULL,&result,progname);
		DosClose(thf);
		DosDupHandle(oldstdout,&thestdout);
		DosDupHandle(oldstderr,&thestderr);
		DosClose(oldstdout);
		DosClose(oldstderr);
		if (err || result.codeTerminate != TC_EXIT) goto abort3;
		DosChgFilePtr(hf,0L,FILE_BEGIN,&l);
		if (DosQFileInfo(hf,1,&fs,sizeof(FILESTATUS))) goto abort3;
		if (DosWrite(hp,&(fs.cbFile),4,&n)) goto abort3;
		if (copy(hf,hp,fs.cbFile)) goto abort3;
		DosClose(hf);
		if (DosWrite(hp,&(result.codeResult),2,&n)) goto abort4;
		if (!result.codeResult) {
			if (DosOpen(outfile,&hf,&n,0L,FILE_NORMAL,FILE_OPEN,
				OPEN_ACCESS_READONLY|OPEN_SHARE_DENYWRITE,0L)) goto abort4;
			if (DosQFileInfo(hf,1,&fs,sizeof(FILESTATUS))) goto abort3;
			if (DosWrite(hp,&(fs.cbFile),4,&n)) goto abort3;
			if (copy(hf,hp,fs.cbFile)) goto abort3;
		}
		DosRead(hp,&n,1,&err);
	abort3:
		DosClose(hf);
	abort4:
		hdir = HDIR_SYSTEM;
		n = 1;
		if (!DosFindFirst("*.*",&hdir,FILE_NORMAL,&fbuf,sizeof(FILEFINDBUF),
			&n,0L)) do {
			DosDelete(fbuf.achName,0L);
			n = 1;
			err = DosFindNext(hdir,&fbuf,sizeof(FILEFINDBUF),&n);
		} while (!err);
		DosFindClose(hdir);
	abort2:
		DosFreeSeg(sel);
	abort1:
		DosClose(hp);
		showstatus("IDLE");
	}
}

static void showstatus(str)
char *str;
{
	char *pnt;

	DosSemRequest(&semi,SEM_INDEFINITE_WAIT);
	strncpy(hostname,str,12);
	hostname[12] = 0;
	if (pnt = strchr(hostname,' ')) *pnt = 0;
	DosSemClear(&semi);
	WinInvalidateRect(hwndclient,NULL,FALSE);
}

static USHORT readdata(hp,buffer,len)
HPIPE hp;
void far *buffer;
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

static USHORT readstring(hp,buffer,max)
HPIPE hp;
char far *buffer;
USHORT max;
{
	USHORT n,len;

	if (n = readdata(hp,&len,2)) return n;
	if (len > max) return 12345;
	if (n = readdata(hp,buffer,len)) return n;
	buffer[len] = 0;
	return 0;
}

static USHORT copy(fhf,thf,len)
HFILE fhf,thf;
long len;
{
	USHORT n,num,num1;

	do {
		if (len > BUFFERSIZE) num1 = BUFFERSIZE;
		else num1 = (USHORT)len;
		if (n = DosRead(fhf,bigbuf,num1,&num)) return n;
		if (!num) return 12345;
		if (n = DosWrite(thf,bigbuf,num,&num1)) return n;
		len -= num;
	} while (len);
	return 0;
}

/* networked make program */

#define SERVER "\\\\server\\disc\\megamake\\"
#define USER "C:\\lanman\\username.txt"
#define MASTER ".MAS"
#define PIPE "\\\\SERVER\\pipe\\"
#define TEMPNAME "$$mega.tmp"
#define MAXFILES 250
#define RETRYRATE 5000
#define IDLERATE 5000
#define BUFFERSIZE 4096

#define INCL_DOSERRORS
#define INCL_DOSNMPIPES
#define INCL_DOSPROCESS

#include <os2.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void cdecl main(int, char **);
static void far switchboard(void);
static USHORT readdata(HPIPE, void *, unsigned int);
static USHORT writestring(HPIPE, char *);
static USHORT copydata(HFILE, HFILE, long, char *);
static BOOL copy(char *, char *);

static FILEFINDBUF near fbuf;
static char near name1[128];
static char near username[128];
static unsigned int newlen = 0;
static long semi = 0;
static long sync = 0;
static long done = 0;
static BOOL volatile error = FALSE;

struct {
	char line[128];
	char name[10];
	char volatile flag;
	char volatile owner;
} *commands;

void cdecl main(argc,argv)
int argc;
char **argv;
{
	USHORT n,err,dummy;
	char *pnt,*pnt2;
	char name2[128];
	char makename[128];
	FILE *fl;
	TID tid;
	SEL newsel,sel;
	HFILE hf,thf;
	RESULTCODES result;
	HDIR hdir;
	HFILE oldstdout = 0xFFFF;
	HFILE oldstderr = 0xFFFF;
	HFILE thestdout = 1;
	HFILE thestderr = 2;
	BOOL madeit = FALSE;

	DosSetMaxFH(200);
	if (argc > 1) {
		strcpy(makename,"\"DEBS=");
		for (n = 1; n < (USHORT)argc; n++) {
			if (n != 1) strcat(makename," ");
			strcat(makename,argv[n]);
			strcat(makename,".deb");
		}
		strcat(makename,"\"");
	}
	else makename[0] = 0;
	/* get our name */
	if (DosOpen(USER,&hf,&dummy,0L,FILE_NORMAL,FILE_OPEN,
		OPEN_ACCESS_READONLY|OPEN_SHARE_DENYWRITE,0L) ||
		DosRead(hf,username,127,&n)) {
		DosClose(hf);
		puts("\007Owner could not be determined");
		DosExit(EXIT_PROCESS,1);
	}
	username[n] = 0;
	DosClose(hf);
	strupr(username);
	/* get space for new update date file */
	if (DosAllocSeg(sizeof(commands[0])*MAXFILES,&newsel,SEG_NONSHARED)) {
		puts("\007Out of memory");
		DosExit(EXIT_PROCESS,1);
	}
	commands = MAKEP(newsel,0);
	/* invoke the NMAKE command */
	strcpy(name1,"NMAKE");
	pnt = name1+strlen(name1)+1;
	strcpy(pnt,"/n ");
	strcat(pnt,makename);
	pnt[strlen(pnt)+1] = 0;
	if (DosOpen(TEMPNAME,&hf,&n,0L,FILE_NORMAL,FILE_TRUNCATE|FILE_CREATE,
		OPEN_ACCESS_READWRITE|OPEN_SHARE_DENYREADWRITE,0L)) {
		puts("\007Can't open temporary file");
		DosExit(EXIT_PROCESS,1);
	}
	DosOpen("NUL",&thf,&n,0L,FILE_NORMAL,FILE_OPEN,
		OPEN_ACCESS_WRITEONLY|OPEN_SHARE_DENYNONE,0L);
	DosDupHandle(thestdout,&oldstdout);
	DosDupHandle(thestderr,&oldstderr);
	DosDupHandle(hf,&thestdout);
	DosDupHandle(thf,&thestderr);
	err = DosExecPgm(name2,127,EXEC_SYNC,name1,NULL,&result,"NMAKE.EXE");
	DosClose(hf);
	DosClose(thf);
	DosDupHandle(oldstdout,&thestdout);
	DosDupHandle(oldstderr,&thestderr);
	DosClose(oldstdout);
	DosClose(oldstderr);
	if (err) {
		puts("\007NMAKE could not be invoked");
		DosExit(EXIT_PROCESS,1);
	}
	if (result.codeTerminate != TC_EXIT) {
		puts("\007NMAKE terminated with an error");
		DosExit(EXIT_PROCESS,1);
	}
	if (result.codeResult) {
		puts("\007Can't find makefile");
		DosExit(EXIT_PROCESS,1);
	}
	if (!(fl = fopen(TEMPNAME,"rt"))) {
		puts("\007Can't use temporary file");
		DosExit(EXIT_PROCESS,1);
	}
	while(fgets(name1,128,fl)) {
		if (pnt = strrchr(name1,'>')) *pnt = 0;
		pnt = name1+strspn(strrev(name1),"; \t\n\r");
		strcpy(name2,pnt);
		strcat(name2," ");
		name2[strcspn(name2,"<> @,")] = 0;
		strrev(name2);
		if (pnt2 = strchr(name2,'.')) *pnt2 = 0;
		strrev(pnt);
		pnt += strspn(pnt," \t\n\r");
		if (toupper(pnt[0]) == 'C' && toupper(pnt[1]) == 'L') {
			strcpy(commands[newlen].line,"CL");
			pnt2 = commands[newlen].line+3;
			strcpy(pnt2,"/I");
			strcat(pnt2,SERVER);
			strcat(pnt2,username);
			strcat(pnt2,pnt+2);
			pnt2[strlen(pnt2)+1] = 0;
			for (n = 0; n < newlen; n++) if (!strcmp(name2,commands[n].name)) break;
			if (n != newlen) continue;
			strcpy(commands[newlen].name,name2);
			commands[newlen].owner = (char)!newlen;
			commands[newlen++].flag = 0;
		}
	}
	fclose(fl);
	DosDelete(TEMPNAME,0L);
	if (newlen > 1) {
		hdir = HDIR_SYSTEM;
		n = 1;
		if (!DosFindFirst("*.*",&hdir,FILE_NORMAL,&fbuf,sizeof(FILEFINDBUF),
			&n,0L)) do {
			pnt = strchr(fbuf.achName,'.');
			if (pnt && (!strcmp(pnt,".H") || !strcmp(pnt,".P") ||
				!strcmp(pnt,".INC"))) {
				strcpy(name1,SERVER);
				strcat(name1,username);
				strcat(name1,"\\");
				strcat(name1,fbuf.achName);
				if (copy(fbuf.achName,name1)) {
					printf("\007Could not copy the file %s\r\n",fbuf.achName);
					DosExit(EXIT_PROCESS,1);
				}
			}
			n = 1;
			err = DosFindNext(hdir,&fbuf,sizeof(FILEFINDBUF),&n);
		} while (!err);
		DosFindClose(hdir);
		DosSemSet(&done);
		n = 1;
		hdir = HDIR_SYSTEM;
		strcpy(name1,SERVER);
		strcat(name1,"*.*");
		if (!DosFindFirst(name1,&hdir,FILE_DIRECTORY,&fbuf,sizeof(FILEFINDBUF),
			&n,0L)) {
			do {
				strcpy(name1,fbuf.achName);
				if (name1[0] != '.') {
					DosSemSet(&semi);
					if (DosAllocSeg(BUFFERSIZE+2048,&sel,SEG_NONSHARED)) {
						puts("\007Out of memory");
						DosExit(EXIT_PROCESS,1);
					}
					if (DosCreateThread(switchboard,&tid,MAKEP(sel,BUFFERSIZE+2046))) {
						puts("\007Can't start thread");
						DosExit(EXIT_PROCESS,1);
					}
					madeit = TRUE;
					DosSemWait(&semi,SEM_INDEFINITE_WAIT);
				}
				n = 1;
				err = DosFindNext(hdir,&fbuf,sizeof(FILEFINDBUF),&n);
			} while (!err);
			DosFindClose(hdir);
		}
		if (!madeit) goto keepgoing;	
		DosSemWait(&done,SEM_INDEFINITE_WAIT);
		hdir = HDIR_SYSTEM;
		n = 1;
		strcpy(name1,SERVER);
		strcat(name1,username);
		strcat(name1,"\\*.*");
		if (!DosFindFirst(name1,&hdir,FILE_NORMAL,&fbuf,sizeof(FILEFINDBUF),
			&n,0L)) {
			do {
				strcpy(name1,SERVER);
				strcat(name1,username);
				strcat(name1,"\\");
				strcat(name1,fbuf.achName);
				DosDelete(name1,0L);
				n = 1;
				err = DosFindNext(hdir,&fbuf,sizeof(FILEFINDBUF),&n);
			} while (!err);
			DosFindClose(hdir);
		}
		makename[0] = 0;
	}
	else DosFreeSeg(newsel);
keepgoing:
	/* invoke the NMAKE command again */
	if (error) {
		printf("\r\nThere were errors, NMAKE not continued\r\n");
		result.codeResult = 1;
	}
	else {
		strcpy(name1,"NMAKE");
		pnt2 = name1+strlen(name1)+1;
		strcpy(pnt2,makename);
		pnt2[strlen(pnt2)+1] = 0;
		DosExecPgm(name2,127,EXEC_SYNC,name1,NULL,&result,"NMAKE.EXE");
	}
	if (result.codeResult) {
		for (n = 0; n < 19; n++) {
			DosBeep(100,30);
			DosSleep(30L);
		}
		DosBeep(100,30);
	}
	else {
		for (n = 0; n < 2; n++) {
			DosBeep(800,50);
			DosSleep(50L);
		}
		DosBeep(800,50);
	}
	DosExit(EXIT_PROCESS,result.codeResult);
}

static void far switchboard()
{
	char buffer[BUFFERSIZE];
	char name[128];
	char compname[13];
	char *pnt;
	BOOL doneall,err,nonowner;
	USHORT n,m,dummy;
	ULONG l;
	HPIPE hp;
	HFILE hf;
	FILESTATUS fs;
	BOOL needclr = TRUE;

	if (pnt = strchr(name1,'.')) *pnt = 0;
	strcpy(compname,name1);
	nonowner = strcmp(username,strupr(compname));
	strcpy(name,PIPE);
	strcat(name,compname);
	strcat(name,MASTER);
	do {
		DosSemRequest(&sync,SEM_INDEFINITE_WAIT);
		doneall = TRUE;
		for (n = 0; n < newlen && (commands[n].flag || (commands[n].owner &&
			nonowner)); n++) if (commands[n].flag != 2) doneall = FALSE;
		if (n != newlen) {
			commands[n].flag = 1;
			commands[n].owner = 0;
			doneall = FALSE;
		}
		else if (doneall) DosSemClear(&done);
		DosSemClear(&sync);
		if (needclr) {
			DosSemClear(&semi);
			needclr = FALSE;
		}
		err = FALSE;
		if (n != newlen) {
			err = TRUE;
			if (m = DosOpen(name,&hp,&dummy,0L,FILE_NORMAL,FILE_OPEN,
				OPEN_ACCESS_READWRITE|OPEN_SHARE_DENYNONE|OPEN_FLAGS_WRITE_THROUGH,0L)) {
//				if (m == ERROR_PIPE_BUSY) {
//					commands[n].flag = 0;
//					DosWaitNmPipe(name,(long)RETRYRATE);
//					continue;
//				}
//				else
					goto abort1;
			}
			if (writestring(hp,"CL.EXE")) goto abort2;
			m = 4+strlen(commands[n].line+3);
			if (DosWrite(hp,&m,2,&dummy)) goto abort2;
			if (DosWrite(hp,commands[n].line,m,&dummy)) goto abort2;
			strcpy(buffer,commands[n].name);
			strcat(buffer,".C");
			if (writestring(hp,buffer)) goto abort2;
			if (DosOpen(buffer,&hf,&dummy,0L,FILE_NORMAL,FILE_OPEN,
				OPEN_ACCESS_READONLY|OPEN_SHARE_DENYNONE,0L)) goto abort2;
			if (DosQFileInfo(hf,1,&fs,sizeof(FILESTATUS))) goto abort3;
			if (DosWrite(hp,&(fs.cbFile),4,&dummy)) goto abort3;
			if (copydata(hf,hp,fs.cbFile,buffer)) goto abort3;
			DosClose(hf);
			strcpy(buffer,commands[n].name);
			strcat(buffer,".OBJ");
			if (writestring(hp,buffer)) goto abort2;
			if (readdata(hp,&l,4)) goto abort2;
			strcpy(buffer,commands[n].name);
			strcat(buffer,".ERR");
			if (DosOpen(buffer,&hf,&dummy,l,FILE_NORMAL,FILE_TRUNCATE|FILE_CREATE,
				OPEN_ACCESS_WRITEONLY|OPEN_SHARE_DENYNONE,0L)) goto abort2;
			if (copydata(hp,hf,l,buffer)) goto abort3;
			if (readdata(hp,&m,2)) goto abort3;
			if (!m) {
				DosClose(hf);
				if (readdata(hp,&l,4)) goto abort2;
				strcpy(buffer,commands[n].name);
				strcat(buffer,".OBJ");
				if (DosOpen(buffer,&hf,&dummy,l,FILE_NORMAL,FILE_TRUNCATE|FILE_CREATE,
					OPEN_ACCESS_WRITEONLY|OPEN_SHARE_DENYNONE,0L)) goto abort2;
				if (copydata(hp,hf,l,buffer)) goto abort3;
			}
			err = FALSE;
			DosSemRequest(&sync,SEM_INDEFINITE_WAIT);
			commands[n].flag = 2;
			DosWrite(1,commands[n].name,strlen(commands[n].name),&dummy);
			if (m) {
				error = TRUE;
				DosWrite(1,".c had an ERROR on ",19,&dummy);
			}
			else DosWrite(1,".c compiled with no errors on ",30,&dummy);
			DosWrite(1,compname,strlen(compname),&dummy);
			DosWrite(1,"\r\n",2,&dummy);
			DosSemClear(&sync);
		abort3:
			DosClose(hf);
		abort2:
			DosClose(hp);
		}
		else DosSleep((long)IDLERATE);
	abort1:
		if (err) {
			commands[n].flag = 0;
			DosSleep((long)RETRYRATE);
		}
	} while (!doneall);
}

static USHORT readdata(hp,buffer,len)
HPIPE hp;
void *buffer;
unsigned int len;
{
	USHORT n,num;
	unsigned int sofar = 0;

	do {
		if (n = DosRead(hp,(char *)buffer+sofar,len-sofar,&num)) return n;
		if (!num) return 12345;
		sofar += num;
	} while (sofar < len);
	return 0;
}

static USHORT writestring(hp,buffer)
HPIPE hp;
char *buffer;
{
	USHORT n,len,dummy;

	len = strlen(buffer);
	if (n = DosWrite(hp,&len,2,&dummy)) return n;
	return DosWrite(hp,buffer,len,&dummy);
}

static USHORT copydata(fhf,thf,len,buffer)
HFILE fhf,thf;
long len;
char *buffer;
{
	USHORT n,num,num1;

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

static USHORT copy(from,to)
char *from,*to;
{
	char far *buffer;
	HFILE fhf,thf;
	USHORT n,err,dummy;
	FILESTATUS fsts;
	SEL sel;

	if ((err = DosAllocSeg(0x8000,&sel,SEG_NONSHARED))) goto err1;
	buffer = MAKEP(sel,0);
	if ((err = DosOpen(from,&fhf,&dummy,0L,FILE_NORMAL,FILE_OPEN,
		OPEN_ACCESS_READONLY|OPEN_SHARE_DENYWRITE,0L))) goto err2;
	if ((err = DosQFileInfo(fhf,1,&fsts,sizeof(FILESTATUS)))) goto err3;
	if ((err = DosOpen(to,&thf,&dummy,fsts.cbFile,FILE_NORMAL,
		FILE_TRUNCATE|FILE_CREATE,
		OPEN_ACCESS_WRITEONLY|OPEN_SHARE_DENYREADWRITE,0L))) goto err3;
	do {
		if ((err = DosRead(fhf,buffer,0x8000,&n))) goto err4;
		if ((err = DosWrite(thf,buffer,n,&dummy))) goto err4;
	} while (n);
	if ((err = DosSetFileInfo(thf,1,(PBYTE)&fsts,sizeof(FILESTATUS)))) goto err4;
	err = DosClose(thf);
	goto err3;
err4:
	DosClose(thf);
err3:
	DosClose(fhf);
err2:
	DosFreeSeg(sel);
err1:
	return err;
}


/* directory backup daemon */

#define INCL_DOSPROCESS

#include <os2.h>
#include <string.h>

void cdecl main(void);
static void backup(char *);
static USHORT copy(char *, char *);

void cdecl main()
{
	DATETIME date;
	UCHAR day = 0;

	while (TRUE) {
		DosSleep(3300000);
		DosGetDateTime(&date);
		if (date.hours == 0 && date.day != day) {
			day = date.day;
			if (date.weekday > 1) backup("DAILY");
			else if (date.weekday == 1) backup("WEEKLY");
		}
	}
}

static void backup(place)
char *place;
{
	USHORT n,err;
	HDIR hdir;
	FILEFINDBUF fbuf;
	char name[64];

	hdir = HDIR_SYSTEM;
	n = 1;
	strcpy(name,place);
	strcat(name,"\\*.*");
	if (!DosFindFirst(name,&hdir,FILE_NORMAL,&fbuf,sizeof(FILEFINDBUF),
		&n,0L)) do {
		strcpy(name,place);
		strcat(name,"\\");
		strcat(name,fbuf.achName);
		DosDelete(name,0L);
		n = 1;
		err = DosFindNext(hdir,&fbuf,sizeof(FILEFINDBUF),&n);
	} while (!err);
	DosFindClose(hdir);
	hdir = HDIR_SYSTEM;
	n = 1;
	if (!DosFindFirst("*.*",&hdir,FILE_NORMAL,&fbuf,sizeof(FILEFINDBUF),
		&n,0L)) do {
		strcpy(name,place);
		strcat(name,"\\");
		strcat(name,fbuf.achName);
		copy(fbuf.achName,name);
		n = 1;
		err = DosFindNext(hdir,&fbuf,sizeof(FILEFINDBUF),&n);
	} while (!err);
	DosFindClose(hdir);
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


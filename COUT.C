/* source control network check-out program */

#define SERVER "\\\\server\\disk"
#define MERGE "merge\\"
#define USER "C:\\util\\username.txt"
#define LIBCARD "owners\\"
#define BACKUP "backup\\"

#define INCL_DOSERRORS

#include <os2.h>
#include <string.h>
#include <stdio.h>
#include <conio.h>
#include <stdlib.h>

void cdecl main(int, char **);
static BOOL copy(char *, char *, BOOL);
static USHORT myDosQPathInfo(PSZ, USHORT, PBYTE, USHORT, ULONG);

void cdecl main(argc,argv)
int argc;
char **argv;
{
	USHORT n,drv;
	LONG l;
	char *pnt,*pnt2;
	char drive[3];
	char path[128];
	char file[9];
	char extension[5];
	char name1[128];
	char name2[128];
	char name3[128];
	HFILE hf;
	FILESTATUS fbuf1,fbuf2;

	if (argc < 2) {
		cprintf("\007No file specified\r\n");
		DosExit(EXIT_PROCESS,1);
	}
	pnt = argv[1];
	DosQCurDisk(&drv,&l);
	if (pnt[1] == ':') {
		drive[0] = pnt[0];
		drive[1] = pnt[1];
		pnt += 2;
	}
	else {
		drive[0] = (char)('@'+drv);
		drive[1] = ':';
	}
	drive[2] = 0;
	if (pnt[0] == '\\') path[0] = 0;
	else {
		path[0] = '\\';
		n = 127;
		DosQCurDir(drv,path+1,&n);
		strcat(path,"\\");
	}
	if ((pnt2 = strrchr(pnt,'\\'))) {
		strncat(path,pnt,pnt2-pnt+1);
		pnt = pnt2+1;
	}
	if ((pnt2 = strchr(pnt,'.'))) {
		strncpy(file,pnt,n = min(8,pnt2-pnt));
		file[n] = 0;
		strncpy(extension,pnt2,4);
		extension[4] = 0;
	}
	else {
		strncpy(file,pnt,8);
		file[8] = 0;
		extension[0] = '.';
		extension[1] = 0;
	}
	/* append X onto filename in server directory */
	strcpy(name1,SERVER);
	strcat(name1,path);
	strcat(name1,file);
	pnt = name2+strlen(name1);
	strcat(name1,extension);
	if (!myDosQPathInfo(argv[1],FIL_STANDARD,(PBYTE)&fbuf1,sizeof(FILESTATUS),0L) &&
		!myDosQPathInfo(name1,FIL_STANDARD,(PBYTE)&fbuf2,sizeof(FILESTATUS),0L) &&
		(fbuf1.fdateLastWrite.day != fbuf2.fdateLastWrite.day ||
		fbuf1.fdateLastWrite.month != fbuf2.fdateLastWrite.month ||
		fbuf1.fdateLastWrite.year != fbuf2.fdateLastWrite.year ||
		fbuf1.ftimeLastWrite.twosecs != fbuf2.ftimeLastWrite.twosecs ||
		fbuf1.ftimeLastWrite.minutes != fbuf2.ftimeLastWrite.minutes ||
		fbuf1.ftimeLastWrite.hours != fbuf2.ftimeLastWrite.hours)) {
		cprintf("\007Your copy of %s is different than the network copy\r\n",argv[1]);
		cprintf("Do you want to check out the file anyway (y/n)? ");
		if (strupr(gets(name2))[0] != 'Y') {
			cprintf("File not checked out\r\n");
			DosExit(EXIT_PROCESS,0);
		}
	}
	strcpy(name2,name1);
	if (strlen(pnt) == 4) pnt[3] = 'X';
	else strcat(name2,"X");
	if (DosMove(name1,name2,0L)) {
		strcpy(name3,SERVER);
		strcat(name3,path);
		strcat(name3,LIBCARD);
		strcat(name3,file);
		strcat(name3,extension);
		if (DosOpen(name3,&hf,&n,0L,FILE_NORMAL,FILE_OPEN,
			OPEN_ACCESS_READONLY|OPEN_SHARE_DENYNONE,0L)) {
			cprintf("\007File not found\r\n");
			DosExit(EXIT_PROCESS,1);
		}
		else {
			n = 0;
			DosRead(hf,name3,127,&n);
			DosClose(hf);
			name3[n] = 0;
			cprintf("\007File already checked out by %s\r\n",name3);
			DosExit(EXIT_PROCESS,1);
		}
	}
	/* copy users name to library card file */
	strcpy(name3,SERVER);
	strcat(name3,path);
	strcat(name3,LIBCARD);
	strcat(name3,file);
	strcat(name3,extension);
	if (copy(USER,name3,FALSE)) cprintf("\007Warning! Owner could not be assigned\r\n");
	/* copy server file to backup directory */
	strcpy(name3,SERVER);
	strcat(name3,path);
	strcat(name3,BACKUP);
	strcat(name3,file);
	strcat(name3,extension);
	if (copy(name2,name3,TRUE))
		cprintf("\007Warning! Backup file could not be created\r\n");
	/* delete the merge file */
	strcpy(name3,drive);
	strcat(name3,path);
	strcat(name3,MERGE);
	strcat(name3,file);
	strcat(name3,extension);
	DosDelete(name3,0L);
	/* move the users file to the merge directory */
	if ((n = DosMove(argv[1],name3,0L)) && n != ERROR_FILE_NOT_FOUND) {
		cprintf("\007Your local file could not be moved to merge directory\r\n");
		if (DosMove(name2,name1,0L))
			cprintf("\007BIG TROUBLE! File could not be checked back in\r\n");
		DosExit(EXIT_PROCESS,1);
	}
	/* copy the file from the network */
	if (copy(name2,argv[1],TRUE)) {
		cprintf("\007File could not be copied from the network\r\n");
		if (DosMove(name2,name1,0L))
			cprintf("\007BIG TROUBLE! File could not be checked back in\r\n");
		DosExit(EXIT_PROCESS,1);
	}
}

static USHORT copy(from,to,olddate)
char *from,*to;
BOOL olddate;
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
	if (olddate && (err = DosSetFileInfo(thf,FIL_STANDARD,(PBYTE)&fsts,sizeof(FILESTATUS))))
		goto err4;
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

static USHORT myDosQPathInfo(PSZ pszPath, USHORT usInfoLevel, PBYTE pInfoBuf,
	USHORT cbInfoBuf, ULONG ulReserved)
{
	HFILE hf;
	USHORT n,err;

	if (err = DosOpen(pszPath,&hf,&n,0L,FILE_NORMAL,FILE_OPEN,
		OPEN_ACCESS_READONLY|OPEN_SHARE_DENYNONE,0L)) return err;
	err = DosQFileInfo(hf,1,(FILESTATUS FAR *)pInfoBuf,sizeof(FILESTATUS));
	DosClose(hf);
	return err;
}

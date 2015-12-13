/* source control network check-in program */

#define SERVER "\\\\server\\disk"
#define USER "C:\\util\\username.txt"
#define LIBCARD "owners\\"
#define UPDATE "update\\update.dat"
#define BACKUP "backup\\"
#define MAXFILES 1000

#define INCL_DOSERRORS

#include <os2.h>
#include <string.h>
#include <stdio.h>
#include <conio.h>
#include <stdlib.h>

typedef struct {
	FDATE	fdateCreation;
	FTIME	ftimeCreation;
	FDATE	fdateLastAccess;
	FTIME	ftimeLastAccess;
	FDATE	fdateLastWrite;
	FTIME	ftimeLastWrite;
	ULONG	cbFile;
	ULONG	cbFileAlloc;
	USHORT	attrFile;
	UCHAR	cchName;
	CHAR	achName[13];
} OLDFILEFINDBUF;

void cdecl main(int, char **);
static BOOL copy(char *, char *, BOOL, FILESTATUS *);
static USHORT myDosQPathInfo(PSZ, USHORT, PBYTE, USHORT, ULONG);

void cdecl main(argc,argv)
int argc;
char **argv;
{
	USHORT n,oldn,drv,err,oldlen;
	LONG l;
	char *pnt,*pnt2;
	char drive[3];
	char path[128];
	char file[9];
	char extension[5];
	char search[128];
	char username[128];
	char name1[128];
	char name2[128];
	char name3[128];
	HFILE hf;
	SEL oldsel;
	FILESTATUS fs,fbuf1,fbuf2;
	OLDFILEFINDBUF fbuf,*old;
	HDIR hdir = HDIR_SYSTEM;
	BOOL many = FALSE;
	BOOL anyfiles = FALSE;
	BOOL blind = FALSE;

	if (argc < 2) {
		cprintf("\007No file specified\r\n");
		DosExit(EXIT_PROCESS,1);
	}
	strcpy(search,argv[1]);
	pnt = strupr(search);
	if (!strcmp(pnt,"ALL")) {
		strcpy(search,"*.*");
		many = TRUE;
	}
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
	if (DosOpen(USER,&hf,&n,0L,FILE_NORMAL,FILE_OPEN,
		OPEN_ACCESS_READONLY|OPEN_SHARE_DENYWRITE,0L) ||
		DosRead(hf,username,127,&n)) {
		DosClose(hf);
		cprintf("\007Owner could not be determined\r\n");
		DosExit(EXIT_PROCESS,1);
	}
	username[n] = 0;
	DosClose(hf);
	/* read the old update date file */
	strcpy(name3,drive);
	strcat(name3,path);
	strcat(name3,UPDATE);
	if (DosOpen(name3,&hf,&n,0L,FILE_NORMAL,FILE_OPEN|FILE_CREATE,
		OPEN_ACCESS_READWRITE|OPEN_SHARE_DENYREADWRITE,0L) ||
		DosQFileInfo(hf,1,&fs,sizeof(FILESTATUS)) ||
		DosAllocSeg(sizeof(OLDFILEFINDBUF)*MAXFILES,&oldsel,SEG_NONSHARED) ||
		DosRead(hf,old = MAKEP(oldsel,0),(USHORT)fs.cbFile,&n)) {
		fs.cbFile = 0;
		cprintf("\007WARNING! Could not read previous file update dates\r\n");
	}
	DosClose(hf);
	oldlen = (USHORT)(fs.cbFile/sizeof(OLDFILEFINDBUF));
	if (many) {
		strcpy(name3,SERVER);
		strcat(name3,path);
		strcat(name3,LIBCARD);
		strcat(name3,"*.*");
		n = 1;
		if (DosFindFirst(name3,&hdir,FILE_NORMAL,(PFILEFINDBUF)&fbuf,
			sizeof(OLDFILEFINDBUF),&n,0L)) {
			cprintf("\007No files found to check in\r\n");
			DosExit(EXIT_PROCESS,0);
		}
	}
	do {
		if (many) pnt = fbuf.achName;
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
		/* make sure that we checked the file out */
		strcpy(name3,SERVER);
		strcat(name3,path);
		strcat(name3,file);
		strcat(name3,extension);
		if ((err = DosQFileMode(name3,&n,0L)) != ERROR_FILE_NOT_FOUND) {
			if (many) goto nextfile;
			strcpy(name2,drive);
			strcat(name2,path);
			strcat(name2,file);
			strcat(name2,extension);
			if (err ||
				(!myDosQPathInfo(name3,FIL_STANDARD,(PBYTE)&fbuf1,sizeof(FILESTATUS),0L) &&
				!myDosQPathInfo(name2,FIL_STANDARD,(PBYTE)&fbuf2,sizeof(FILESTATUS),0L) &&
				fbuf1.fdateLastWrite.day == fbuf2.fdateLastWrite.day &&
				fbuf1.fdateLastWrite.month == fbuf2.fdateLastWrite.month &&
				fbuf1.fdateLastWrite.year == fbuf2.fdateLastWrite.year &&
				fbuf1.ftimeLastWrite.twosecs == fbuf2.ftimeLastWrite.twosecs &&
				fbuf1.ftimeLastWrite.minutes == fbuf2.ftimeLastWrite.minutes &&
				fbuf1.ftimeLastWrite.hours == fbuf2.ftimeLastWrite.hours)) {
				cprintf("\007File is not checked out\r\n");
				DosExit(EXIT_PROCESS,1);
			}
			blind = TRUE;
		}
		else {
			strcpy(name3,SERVER);
			strcat(name3,path);
			strcat(name3,LIBCARD);
			strcat(name3,file);
			strcat(name3,extension);
			err = DosOpen(name3,&hf,&n,0L,FILE_NORMAL,FILE_OPEN,
				OPEN_ACCESS_READONLY|OPEN_SHARE_DENYNONE,0L);
			if (!err && !DosRead(hf,name2,127,&n)) name2[n] = 0;
			else name2[0] = 0;
			if (!err) DosClose(hf);
			if (strcmp(username,name2)) {
				if (many) goto nextfile;
				else if (strlen(name2)) {
					cprintf("\007File was checked out by %s\r\nDo you want to check "
						"in your copy (y/n)? ",name2);
					if (strupr(gets(name1))[0] != 'Y') DosExit(EXIT_PROCESS,0);
					cprintf("Are you sure (y/n)? ");
					if (strupr(gets(name1))[0] != 'Y') DosExit(EXIT_PROCESS,0);
					cprintf("Are you really, really sure (y/n)? ");
					if (strupr(gets(name1))[0] != 'Y') DosExit(EXIT_PROCESS,0);
				}
			}
		}
		/* copy file to the server directory */
		strcpy(name1,SERVER);
		strcat(name1,path);
		strcat(name1,file);
		pnt2 = name2+strlen(name1);
		strcat(name1,extension);
		strcpy(name2,name1);
		if (strlen(pnt2) == 4) pnt2[3] = 'X';
		else strcat(name2,"X");
		for (oldn = 0; oldn < oldlen && strcmp(pnt,old[oldn].achName); oldn++);
		if (blind) {
			if (DosMove(name1,name2,0L) ||
				DosOpen(name2,&hf,&n,0L,FILE_NORMAL,FILE_OPEN,
					OPEN_ACCESS_READONLY|OPEN_SHARE_DENYNONE,0L) ||
				DosQFileInfo(hf,1,&fs,sizeof(FILESTATUS))) {
				cprintf("\007File could not be checked in\r\n");
				DosExit(EXIT_PROCESS,1);
			}
			DosClose(hf);
			if (oldn == oldlen ||
				fs.fdateLastWrite.day != old[oldn].fdateLastWrite.day ||
				fs.fdateLastWrite.month != old[oldn].fdateLastWrite.month ||
				fs.fdateLastWrite.year != old[oldn].fdateLastWrite.year ||
				fs.ftimeLastWrite.twosecs != old[oldn].ftimeLastWrite.twosecs ||
				fs.ftimeLastWrite.minutes != old[oldn].ftimeLastWrite.minutes ||
				fs.ftimeLastWrite.hours != old[oldn].ftimeLastWrite.hours) {
				cprintf("\007Can not check in file because it has"
					" changed since your last update\r\n");
				if (DosMove(name2,name1,0L))
					cprintf("\007BIG TROUBLE! Check-in could not be completed\r\n");
				DosExit(EXIT_PROCESS,1);
			}
			/* copy users name to library card file */
			strcpy(name3,SERVER);
			strcat(name3,path);
			strcat(name3,LIBCARD);
			strcat(name3,file);
			strcat(name3,extension);
			if (copy(USER,name3,FALSE,&fs))
				cprintf("\007Warning! Owner could not be assigned\r\n");
			/* copy server file to backup directory */
			strcpy(name3,SERVER);
			strcat(name3,path);
			strcat(name3,BACKUP);
			strcat(name3,file);
			strcat(name3,extension);
			if (copy(name2,name3,TRUE,&fs))
				cprintf("\007Warning! Backup file could not be created\r\n");
		}
		strcpy(name3,drive);
		strcat(name3,path);
		strcat(name3,file);
		strcat(name3,extension);
		if (copy(name3,name2,TRUE,&fs)) {
			if (many) cprintf("\007%s%s could not be checked in\r\n",
				file,extension);
			else if (err) {
				cprintf("\007File not found\r\n");
				DosExit(EXIT_PROCESS,1);
			}
			else {
				cprintf("\007File could not be checked in\r\n");
				DosExit(EXIT_PROCESS,1);
			}
		}
		/* rename the file */
		else if (DosMove(name2,name1,0L)) {
			cprintf("\007BIG TROUBLE! Check-in could not be completed\r\n");
			DosExit(EXIT_PROCESS,1);
		}
		else {
			anyfiles = TRUE;
			memcpy(old+oldn,&fs,sizeof(FILESTATUS));
			if (oldn == oldlen) {
				oldlen++;
				old[oldn].cchName = 0;
				strcpy(old[oldn].achName,pnt);
			}
			if (many) cprintf("%s%s checked in\r\n",file,extension);
		}
	nextfile:
		if (many) {
			n = 1;
			err = DosFindNext(hdir,(PFILEFINDBUF)&fbuf,sizeof(OLDFILEFINDBUF),&n);
		}
	} while (many && !err);
	if (many) {
		DosFindClose(hdir);
		if(!anyfiles) cprintf("\007No files found to check in\r\n");
	}
	/* write the new update date file */
	if (anyfiles) {
		strcpy(name3,drive);
		strcat(name3,path);
		strcat(name3,UPDATE);
		if (DosOpen(name3,&hf,&n,(long)(oldlen*sizeof(OLDFILEFINDBUF)),
			FILE_NORMAL,FILE_TRUNCATE|FILE_CREATE,
			OPEN_ACCESS_WRITEONLY|OPEN_SHARE_DENYREADWRITE,0L) ||
			DosWrite(hf,old,oldlen*sizeof(OLDFILEFINDBUF),&n)) {
			cprintf("\007WARNING! Could not write file update information\r\n");
		}
		DosClose(hf);
	}
	DosFreeSeg(oldsel);
}

static USHORT copy(from,to,olddate,fsts)
char *from,*to;
BOOL olddate;
FILESTATUS *fsts;
{
	char far *buffer;
	HFILE fhf,thf;
	USHORT n,err,dummy;
	SEL sel;

	if ((err = DosAllocSeg(0x8000,&sel,SEG_NONSHARED))) goto err1;
	buffer = MAKEP(sel,0);
	if ((err = DosOpen(from,&fhf,&dummy,0L,FILE_NORMAL,FILE_OPEN,
		OPEN_ACCESS_READONLY|OPEN_SHARE_DENYWRITE,0L))) goto err2;
	if ((err = DosQFileInfo(fhf,1,fsts,sizeof(FILESTATUS)))) goto err3;
	if ((err = DosOpen(to,&thf,&dummy,fsts->cbFile,FILE_NORMAL,
		FILE_TRUNCATE|FILE_CREATE,
		OPEN_ACCESS_WRITEONLY|OPEN_SHARE_DENYREADWRITE,0L))) goto err3;
	do {
		if ((err = DosRead(fhf,buffer,0x8000,&n))) goto err4;
		if ((err = DosWrite(thf,buffer,n,&dummy))) goto err4;
	} while (n);
	if (olddate && (err = DosSetFileInfo(thf,FIL_STANDARD,(PBYTE)fsts,sizeof(FILESTATUS))))
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

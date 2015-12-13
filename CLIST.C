/* source control network check-out status program */

#define SERVER "\\\\server\\disk"
#define USER "C:\\util\\username.txt"
#define LIBCARD "owners\\"
#define UPDATE "update\\update.dat"
#define MAXFILES 1800

#define INCL_DOSERRORS

#include <os2.h>
#include <string.h>
#include <stdio.h>
#include <conio.h>

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

void cdecl main(argc,argv)
int argc;
char **argv;
{
	USHORT oldn,n,i,j,drv,err,oldlen;
	LONG l;
	char *pnt;
	char drive[3];
	char path[128];
	char username[128];
	char name1[128];
	HFILE hf;
	OLDFILEFINDBUF fbuf,*old,*fbufs;
	FILESTATUS fs;
	SEL oldsel,bufsel;
	HDIR hdir;
	BOOL out,us;
	BOOL anyfiles = FALSE;

	argv;
	path[0] = '\\';
	n = 127;
	DosQCurDisk(&drv,&l);
	DosQCurDir(drv,path+1,&n);
	strcat(path,"\\");
	/* read the old update date file */
	drive[0] = (char)('@'+drv);
	drive[1] = ':';
	drive[2] = 0;
	if (DosOpen(USER,&hf,&n,0L,FILE_NORMAL,FILE_OPEN,
		OPEN_ACCESS_READONLY|OPEN_SHARE_DENYWRITE,0L) ||
		DosRead(hf,username,127,&n)) {
		DosClose(hf);
		cprintf("\007Owner could not be determined\r\n");
		DosExit(EXIT_PROCESS,1);
	}
	username[n] = 0;
	DosClose(hf);
	strcpy(name1,drive);
	strcat(name1,path);
	strcat(name1,UPDATE);
	if (DosOpen(name1,&hf,&n,0L,FILE_NORMAL,FILE_OPEN|FILE_CREATE,
		OPEN_ACCESS_READWRITE|OPEN_SHARE_DENYREADWRITE,0L) ||
		DosQFileInfo(hf,1,&fs,sizeof(FILESTATUS)) ||
		DosAllocSeg((USHORT)fs.cbFile,&oldsel,SEG_NONSHARED) ||
		DosRead(hf,old = MAKEP(oldsel,0),(USHORT)fs.cbFile,&n)) {
		fs.cbFile = 0;
		cprintf("\007WARNING! Could not read previous file update dates\r\n");
	}
	DosClose(hf);
	oldlen = (int)(fs.cbFile/sizeof(OLDFILEFINDBUF));
	if (DosAllocSeg(sizeof(OLDFILEFINDBUF)*MAXFILES,&bufsel,SEG_NONSHARED)) {
		cprintf("\007Can't allocate memory\r\n");
		DosExit(EXIT_PROCESS,1);
	}
	fbufs = MAKEP(bufsel,0);
	strcpy(name1,SERVER);
	strcat(name1,path);
	strcat(name1,"*.*");
	hdir = HDIR_SYSTEM;
	n = 1;
	i = 0;
	if (err = DosFindFirst(name1,&hdir,FILE_NORMAL,(PFILEFINDBUF)&fbufs[i],
		sizeof(OLDFILEFINDBUF),&n,0L)) {
		if (err == ERROR_NO_MORE_FILES) cprintf("No files are checked out\r\n");
		else if (err == ERROR_PATH_NOT_FOUND) cprintf("\007No server directory\r\n");
		else cprintf("\007Server down\r\n");
		DosExit(EXIT_PROCESS,1);
	}
	do {
		i++;
		n = 1;
   } while (i < MAXFILES && !DosFindNext(hdir,(PFILEFINDBUF)&fbufs[i],
		sizeof(OLDFILEFINDBUF),&n));
	DosFindClose(hdir);
	for (j = 0; j < i; j++) {
		pnt = fbufs[j].achName;
		out = pnt[strlen(pnt)-1] == 'X';
		if (out || argc > 1) {
			strcpy(name1,SERVER);
			strcat(name1,path);
			strcat(name1,LIBCARD);
			strcat(name1,pnt);
			if (out) name1[strlen(name1)-1] = '?';
			hdir = HDIR_SYSTEM;
			n = 1;
			if (!DosFindFirst(name1,&hdir,FILE_NORMAL,(PFILEFINDBUF)&fbuf,
				sizeof(OLDFILEFINDBUF),&n,0L) || argc > 1) {
				DosFindClose(hdir);
				if (out) {
					pnt = fbuf.achName;
					strcpy(name1,SERVER);
					strcat(name1,path);
					strcat(name1,LIBCARD);
					strcat(name1,pnt);
				}
				err = DosOpen(name1,&hf,&n,0L,FILE_NORMAL,FILE_OPEN,
					OPEN_ACCESS_READONLY|OPEN_SHARE_DENYNONE,0L);
				if (!err && !DosRead(hf,name1,127,&n)) name1[n] = 0;
				else name1[0] = 0;
				if (!err) DosClose(hf);
				if (out) {
					cprintf("%s",pnt);
					anyfiles = TRUE;
				}
				else {
					if (name1[0]) cprintf("%s last",pnt);
					else cprintf("%s has never been checked out\r\n",pnt);
				}
				if (argc < 2 || name1[0]) {
					n = fbuf.ftimeLastWrite.hours%12;
					cprintf(" checked out by %s on %d/%d/%d at %d:%02d%cM\r\n",name1,
						fbuf.fdateLastWrite.month,fbuf.fdateLastWrite.day,
						fbuf.fdateLastWrite.year+80,n+(n ? 0 : 12),
						fbuf.ftimeLastWrite.minutes,
						fbuf.ftimeLastWrite.hours/12 ? 'P' : 'A');
				}
			}
			else cprintf("\007%s is orphaned\r\n",fbufs[j].achName);
		}
   }
	if(!anyfiles) cprintf("No files are checked out\r\n");
	strcpy(name1,SERVER);
	strcat(name1,path);
	strcat(name1,"*.*");
	for (j = 0; j < i; j++) {
		pnt = fbufs[j].achName;
		out = pnt[strlen(pnt)-1] == 'X';
		if (out) {
			strcpy(name1,SERVER);
			strcat(name1,path);
			strcat(name1,LIBCARD);
			strcat(name1,pnt);
			name1[strlen(name1)-1] = '?';
			hdir = HDIR_SYSTEM;
			n = 1;
			if (!DosFindFirst(name1,&hdir,FILE_NORMAL,(PFILEFINDBUF)&fbuf,
				sizeof(OLDFILEFINDBUF),&n,0L)) {
				DosFindClose(hdir);
				pnt = fbuf.achName;
				strcpy(name1,SERVER);
				strcat(name1,path);
				strcat(name1,LIBCARD);
				strcat(name1,pnt);
				err = DosOpen(name1,&hf,&n,0L,FILE_NORMAL,FILE_OPEN,
					OPEN_ACCESS_READONLY|OPEN_SHARE_DENYNONE,0L);
				if (!err && !DosRead(hf,name1,127,&n)) name1[n] = 0;
				else name1[0] = 0;
				if (!err) DosClose(hf);
			}
			else pnt = NULL;
		}
		if (pnt) {
			us = out && !strcmpi(name1,username);
			for (oldn = 0; oldn < oldlen && strcmp(pnt,old[oldn].achName); oldn++);
			strcpy(name1,drive);
			strcat(name1,path);
			strcat(name1,pnt);
			if (!DosOpen(name1,&hf,&n,0L,FILE_NORMAL,FILE_OPEN,
					OPEN_ACCESS_READONLY|OPEN_SHARE_DENYNONE,0L) &&
				!DosQFileInfo(hf,1,&fs,sizeof(FILESTATUS))) {
				if (us) {
					cprintf("Your copy of %s needs to be ",pnt);
					if (fs.fdateLastWrite.day != fbufs[j].fdateLastWrite.day ||
						fs.fdateLastWrite.month != fbufs[j].fdateLastWrite.month ||
						fs.fdateLastWrite.year != fbufs[j].fdateLastWrite.year ||
						fs.ftimeLastWrite.twosecs != fbufs[j].ftimeLastWrite.twosecs ||
						fs.ftimeLastWrite.minutes != fbufs[j].ftimeLastWrite.minutes ||
						fs.ftimeLastWrite.hours != fbufs[j].ftimeLastWrite.hours)
						cprintf("checked in\r\n");
					else cprintf("released\r\n");
				}
				else if (oldn != oldlen &&
					(fs.fdateLastWrite.day != old[oldn].fdateLastWrite.day ||
					fs.fdateLastWrite.month != old[oldn].fdateLastWrite.month ||
					fs.fdateLastWrite.year != old[oldn].fdateLastWrite.year ||
					fs.ftimeLastWrite.twosecs != old[oldn].ftimeLastWrite.twosecs ||
					fs.ftimeLastWrite.minutes != old[oldn].ftimeLastWrite.minutes ||
					fs.ftimeLastWrite.hours != old[oldn].ftimeLastWrite.hours)) {
					cprintf("Your copy of %s ",pnt);
					if (fbufs[j].fdateLastWrite.day == old[oldn].fdateLastWrite.day &&
						fbufs[j].fdateLastWrite.month == old[oldn].fdateLastWrite.month &&
						fbufs[j].fdateLastWrite.year == old[oldn].fdateLastWrite.year &&
						fbufs[j].ftimeLastWrite.twosecs == old[oldn].ftimeLastWrite.twosecs &&
						fbufs[j].ftimeLastWrite.minutes == old[oldn].ftimeLastWrite.minutes &&
						fbufs[j].ftimeLastWrite.hours == old[oldn].ftimeLastWrite.hours) {
						if (out) cprintf("will need to be checked in or merged\r\n");
						else cprintf("needs to be checked in\r\n");
					}
					else {
						if (out) cprintf("will need to be merged\r\n");
						else cprintf("needs to be merged\r\n");
					}
				}
				DosClose(hf);
			}
		}
   }
	DosFreeSeg(bufsel);
	DosFreeSeg(oldsel);
}

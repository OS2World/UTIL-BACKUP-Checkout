/* source control network updating program */

#define SERVER "\\\\server\\disk"
#define USER "C:\\util\\username.txt"
#define LIBCARD "owners\\"
#define UPDATE "update\\update.dat"
#define TEMPNAME "$$update.tmp"
#define MAXFILES 1000

#define INCL_DOSPROCESS

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
	USHORT n,i,j,drv,err,dummy;
	LONG l;
	int oldlen;
	char *pnt,*pnt2,*buffer;
	char drive[3];
	char path[128];
	char search[128];
	char username[128];
	char name1[128];
	char name2[128];
	char name3[128];
	FILE *fl;
	FILESTATUS fs;
	SEL oldsel,newsel,sel,bufsel;
	HFILE hf,fhf,thf;
	OLDFILEFINDBUF fbuf,*fbufs,*old,*new;
	RESULTCODES result;
	BOOL chngd;
	HDIR hdir = HDIR_SYSTEM;
	HFILE oldstdout = 0xFFFF;
	HFILE oldstderr = 0xFFFF;
	HFILE thestdout = 1;
	HFILE thestderr = 2;
	BOOL anyfiles = FALSE;
	BOOL check = FALSE;
	BOOL partial = FALSE;
	int newlen = 0;

	strcpy(search,"*.*");
	for (n = 1; (int)n < argc; n++) {
		if (argv[n][0] == '/') check = TRUE;
		else {
			strcpy(search,argv[n]);
			partial = TRUE;
		}
	}
	pnt = search;
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
	if (pnt2 = strrchr(pnt,'\\')) {
		strncat(path,pnt,pnt2-pnt+1);
		pnt = pnt2+1;
	}
	/* get our name */
	if (DosOpen(USER,&hf,&dummy,0L,FILE_NORMAL,FILE_OPEN,
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
	if (DosOpen(name3,&hf,&dummy,0L,FILE_NORMAL,FILE_OPEN|FILE_CREATE,
		OPEN_ACCESS_READWRITE|OPEN_SHARE_DENYREADWRITE,0L) ||
		DosQFileInfo(hf,1,&fs,sizeof(FILESTATUS)) ||
		DosAllocSeg((USHORT)fs.cbFile,&oldsel,SEG_NONSHARED) ||
		DosRead(hf,old = MAKEP(oldsel,0),(USHORT)fs.cbFile,&dummy)) {
		fs.cbFile = 0;
		cprintf("\007WARNING! Could not read previous file update dates\r\n");
	}
	DosClose(hf);
	oldlen = (int)(fs.cbFile/sizeof(OLDFILEFINDBUF));
	/* get space for new update date file */
	if (DosAllocSeg(sizeof(OLDFILEFINDBUF)*MAXFILES,&newsel,SEG_NONSHARED)) {
		cprintf("\007Out of memory\r\n");
		DosExit(EXIT_PROCESS,1);
	}
	new = MAKEP(newsel,0);
	if (DosAllocSeg(sizeof(OLDFILEFINDBUF)*MAXFILES,&bufsel,SEG_NONSHARED)) {
		cprintf("\007Can't allocate memory\r\n");
		DosExit(EXIT_PROCESS,1);
	}
	fbufs = MAKEP(bufsel,0);
	strcpy(name3,SERVER);
	strcat(name3,path);
	strcat(name3,pnt);
	n = 1;
	i = 0;
	if (DosFindFirst(name3,&hdir,FILE_NORMAL,(PFILEFINDBUF)&fbufs[i],
			sizeof(OLDFILEFINDBUF),&n,0L)) {
		cprintf("\007No files found to update\r\n");
		DosExit(EXIT_PROCESS,0);
	}
	do {
		i++;
		n = 1;
	} while (i < MAXFILES && !DosFindNext(hdir,(PFILEFINDBUF)&fbufs[i],
		sizeof(OLDFILEFINDBUF),&n));
	DosFindClose(hdir);
	for (j = 0; j < i; j++) {
		pnt = fbufs[j].achName;
		name3[0] = 0;
		/* process a checked out file */
		if (pnt[strlen(pnt)-1] == 'X') {
			strcpy(name3,SERVER);
			strcat(name3,path);
			strcat(name3,LIBCARD);
			strcat(name3,pnt);
			name3[strlen(name3)-1] = '?';
			hdir = HDIR_SYSTEM;
			n = 1;
			if (DosFindFirst(name3,&hdir,FILE_NORMAL,(PFILEFINDBUF)&fbuf,
				sizeof(OLDFILEFINDBUF),&n,0L)) {
				cprintf("\007Can't update %s\r\n",pnt);
				continue;
			}
			DosFindClose(hdir);
			strcpy(name3,SERVER);
			strcat(name3,path);
			strcat(name3,LIBCARD);
			strcat(name3,fbuf.achName);
			if (DosOpen(name3,&hf,&dummy,0L,FILE_NORMAL,FILE_OPEN,
				OPEN_ACCESS_READONLY|OPEN_SHARE_DENYWRITE,0L) ||
				DosRead(hf,name3,127,&n)) {
				DosClose(hf);
				cprintf("\007Can't update %s\r\n",fbuf.achName);
				continue;
			}
			name3[n] = 0;
			DosClose(hf);
			pnt = fbuf.achName;
		}
		/* get the date of our copy of that file */
		for (n = 0; (int)n < oldlen && strcmp(pnt,old[n].achName); n++);
		if ((int)n < oldlen) old[n].cchName = 2;
		strcpy(name2,drive);
		strcat(name2,path);
		strcat(name2,pnt);
		err = DosOpen(name2,&hf,&dummy,0L,FILE_NORMAL,FILE_OPEN,
			OPEN_ACCESS_READONLY|OPEN_SHARE_DENYNONE,0L);
		if (!err) {
			if (DosQFileInfo(hf,1,(FILESTATUS *)(new+newlen),sizeof(FILESTATUS))) {
				cprintf("\007Can't get the date of %s",pnt);
				DosExit(EXIT_PROCESS,1);
			}
			new[newlen].cchName = 0;
			strcpy(new[newlen++].achName,pnt);
			DosClose(hf);
			if (!strcmp(name3,username)) {
				if (check) cprintf("%s is checked out by you, file would not be copied\r\n",pnt);
				else cprintf("%s is checked out by you, file not copied\r\n",pnt);
				continue;
			}
			if (new[newlen-1].fdateLastWrite.day == fbufs[j].fdateLastWrite.day &&
				new[newlen-1].fdateLastWrite.month == fbufs[j].fdateLastWrite.month &&
				new[newlen-1].fdateLastWrite.year == fbufs[j].fdateLastWrite.year &&
				new[newlen-1].ftimeLastWrite.twosecs == fbufs[j].ftimeLastWrite.twosecs &&
				new[newlen-1].ftimeLastWrite.minutes == fbufs[j].ftimeLastWrite.minutes &&
				new[newlen-1].ftimeLastWrite.hours == fbufs[j].ftimeLastWrite.hours)
				continue;
			if ((int)n == oldlen ||
				new[newlen-1].fdateLastWrite.day != old[n].fdateLastWrite.day ||
				new[newlen-1].fdateLastWrite.month != old[n].fdateLastWrite.month ||
				new[newlen-1].fdateLastWrite.year != old[n].fdateLastWrite.year ||
				new[newlen-1].ftimeLastWrite.twosecs != old[n].ftimeLastWrite.twosecs ||
				new[newlen-1].ftimeLastWrite.minutes != old[n].ftimeLastWrite.minutes ||
				new[newlen-1].ftimeLastWrite.hours != old[n].ftimeLastWrite.hours) {
				chngd = ((int)n == oldlen ||
					fbufs[j].fdateLastWrite.day != old[n].fdateLastWrite.day ||
					fbufs[j].fdateLastWrite.month != old[n].fdateLastWrite.month ||
					fbufs[j].fdateLastWrite.year != old[n].fdateLastWrite.year ||
					fbufs[j].ftimeLastWrite.twosecs != old[n].ftimeLastWrite.twosecs ||
					fbufs[j].ftimeLastWrite.minutes != old[n].ftimeLastWrite.minutes ||
					fbufs[j].ftimeLastWrite.hours != old[n].ftimeLastWrite.hours);
				if (check) {
					if (chngd) cprintf("Your copy and the network copy of %s have changed\r\n",pnt);
					else cprintf("Your copy of %s has changed\r\n",pnt);
					continue;
				}
				else {
					cprintf("\007Your copy of %s has changed since you last updated\r\n",pnt);
					if (chngd) cprintf("and the network copy has also changed.\r\n");
					cprintf("Do you want to get a copy from the network anyway (y/n)? ");
					if (strupr(gets(name1))[0] != 'Y') {
						if ((int)n == oldlen) newlen--;
						else memcpy(new+newlen-1,old+n,sizeof(FILESTATUS));
						continue;
					}
				}
			}
			newlen--;
		}
		/* copy the file */
		if (err) cprintf("New file ");
		if (name3[0]) {
			if (check) cprintf("%s is checked out by %s, backup would be copied\r\n",
				pnt,name3);
			else cprintf("%s is checked out by %s, copying backup\r\n",
				pnt,name3);
		}
		else if (check) cprintf("%s would be copied\r\n",pnt);
		else cprintf("%s is being copied\r\n",pnt);
		if (!check) {
			strcpy(name1,SERVER);
			strcat(name1,path);
			strcat(name1,fbufs[j].achName);
			if ((err = DosAllocSeg(0x8000,&sel,SEG_NONSHARED))) goto err1;
			buffer = MAKEP(sel,0);
			if ((err = DosOpen(name1,&fhf,&dummy,0L,FILE_NORMAL,FILE_OPEN,
				OPEN_ACCESS_READONLY|OPEN_SHARE_DENYWRITE,0L))) goto err2;
			if ((err = DosQFileInfo(fhf,1,&fs,sizeof(FILESTATUS)))) goto err3;
			if ((err = DosOpen(name2,&thf,&dummy,fs.cbFile,FILE_NORMAL,
				FILE_TRUNCATE|FILE_CREATE,
				OPEN_ACCESS_WRITEONLY|OPEN_SHARE_DENYREADWRITE,0L))) goto err3;
			do {
				if ((err = DosRead(fhf,buffer,0x8000,&n))) goto err4;
				if ((err = DosWrite(thf,buffer,n,&dummy))) goto err4;
			} while (n);
			memcpy(new+newlen,&fs,sizeof(FILESTATUS));
			new[newlen].cchName = 1;
			strcpy(new[newlen++].achName,pnt);
			fs.fdateLastWrite.year += 15;
			if ((err = DosSetFileInfo(thf,FIL_STANDARD,(PBYTE)&fs,sizeof(FILESTATUS)))) goto err4;
			err = DosClose(thf);
			goto err3;
		err4:
			DosClose(thf);
		err3:
			DosClose(fhf);
		err2:
			DosFreeSeg(sel);
		err1:
			if (err) {
				cprintf("\007%s could not be copied\r\n",pnt);
				DosExit(EXIT_PROCESS,1);
			}
		}
		anyfiles = TRUE;
	}
	/* write the new update date file */
	if (!check) {
		if (partial) for (n = 0; (int)n < oldlen; n++) if (old[n].cchName != 2) {
			new[newlen] = old[n];
			new[newlen++].cchName = 0;
		}
		strcpy(name3,drive);
		strcat(name3,path);
		strcat(name3,UPDATE);
		if (DosOpen(name3,&hf,&dummy,(long)(newlen*sizeof(OLDFILEFINDBUF)),
			FILE_NORMAL,FILE_TRUNCATE|FILE_CREATE,
			OPEN_ACCESS_WRITEONLY|OPEN_SHARE_DENYREADWRITE,0L) ||
			DosWrite(hf,new,newlen*sizeof(OLDFILEFINDBUF),&dummy)) {
			cprintf("\007WARNING! Could not write file update information\r\n");
		}
		DosClose(hf);
	}
	DosFreeSeg(oldsel);
	if (!anyfiles) {
		if (check) cprintf("No files would be updated\r\n");
		else cprintf("No files were updated\r\n");
	}
	else if (!check) {
		cprintf("Modifying files for NMAKE\r\n");
		n = 1;
		strcpy(name3,drive);
		strcat(name3,path);
		strcat(name3,"*.MAK");
		hdir = HDIR_SYSTEM;
		if (!DosFindFirst(name3,&hdir,FILE_NORMAL,(PFILEFINDBUF)&fbuf,
			sizeof(OLDFILEFINDBUF),&n,0L)) do {
			strcpy(name1,"NMAKER");
			pnt2 = name1+strlen(name1)+1;
			strcpy(pnt2,fbuf.achName);
			strcat(pnt2," /N");
			pnt2[strlen(pnt2)+1] = 0;
			if (DosOpen(TEMPNAME,&hf,&n,0L,FILE_NORMAL,FILE_TRUNCATE|FILE_CREATE,
				OPEN_ACCESS_READWRITE|OPEN_SHARE_DENYREADWRITE,0L))
				cprintf("\007Can't open temporary file\r\n");
			else {
				DosOpen("NUL",&thf,&n,0L,FILE_NORMAL,FILE_OPEN,
					OPEN_ACCESS_WRITEONLY|OPEN_SHARE_DENYNONE,0L);
				DosDupHandle(thestdout,&oldstdout);
				DosDupHandle(thestderr,&oldstderr);
				DosDupHandle(hf,&thestdout);
				DosDupHandle(thf,&thestderr);
				err = DosExecPgm(name3,127,EXEC_SYNC,name1,NULL,&result,"NMAKER.EXE");
				DosClose(hf);
				DosClose(thf);
				DosDupHandle(oldstdout,&thestdout);
				DosDupHandle(oldstderr,&thestderr);
				DosClose(oldstdout);
				DosClose(oldstderr);
				if (err) cprintf("\007NMAKE could not be invoked\r\n");
				else if (result.codeTerminate != TC_EXIT)
					cprintf("\007NMAKE terminated with an error\r\n");
				else if (result.codeResult)
					cprintf("\007Can't find makefile\r\n");
				else {
					if (!(fl = fopen(TEMPNAME,"rt")))
						cprintf("\007Can't use temporary file\r\n");
					else while(fgets(name1,128,fl)) {
						pnt = name1+strspn(strrev(name1),"; \t\n\r");
						strcpy(name3,pnt);
						strcat(name3," ");
						name3[strcspn(name3,"<> @,")] = 0;
						strrev(name3);
						if (pnt2 = strchr(name3,'.')) *pnt2 = 0;
						strrev(pnt);
						pnt += strspn(pnt," \t\n\r");
						pnt[2] = 0;
						if (!strcmp(strupr(pnt),"CL")) strcat(name3,".OBJ");
						else if (!strcmp(pnt,"ML")) strcat(name3,".OBJ");
						else if (!strcmp(pnt,"MA")) strcat(name3,".OBJ");
						else if (!strcmp(pnt,"RC")) strcat(name3,".RES");
						else if (!strcmp(pnt,"LI")) strcat(name3,".EXE");
						else if (!strcmp(pnt,"H2")) strcat(name3,".INC");
						else if (!strcmp(pnt,"LO")) strcat(name3,".IMG");
						else continue;
						if (!DosOpen(name3,&hf,&dummy,0L,FILE_NORMAL,FILE_OPEN,
							OPEN_ACCESS_READWRITE|OPEN_SHARE_DENYWRITE,0L)) {
							DosQFileInfo(hf,1,&fs,sizeof(FILESTATUS));
							fs.fdateLastWrite.year = 0;
							DosSetFileInfo(hf,FIL_STANDARD,(PBYTE)&fs,sizeof(FILESTATUS));
							DosClose(hf);
						}
					}
					fclose(fl);
				}
			}
			DosDelete(TEMPNAME,0L);
			n = 1;
		} while (!DosFindNext(hdir,(PFILEFINDBUF)&fbuf,sizeof(OLDFILEFINDBUF),&n));
		DosFindClose(hdir);
		for (n = 0; (int)n < newlen; n++) if (new[n].cchName) {
			strcpy(name2,drive);
			strcat(name2,path);
			strcat(name2,new[n].achName);
			if (DosOpen(name2,&hf,&dummy,0L,FILE_NORMAL,FILE_OPEN,
				OPEN_ACCESS_READWRITE|OPEN_SHARE_DENYWRITE,0L) ||
				DosSetFileInfo(hf,FIL_STANDARD,(PBYTE)(new+n),sizeof(FILESTATUS)))
				cprintf("\007WARNING! Can't restore date of file %s\r\n",
					new[n].achName);
			DosClose(hf);
		}
	}
	DosFreeSeg(bufsel);
	DosFreeSeg(newsel);
}

/* source control network release program */

#define SERVER "\\\\server\\disk"

#include <os2.h>
#include <string.h>
#include <stdio.h>
#include <conio.h>
#include <stdlib.h>

void cdecl main(int, char **);

void cdecl main(argc,argv)
int argc;
char **argv;
{
	USHORT n,drv;
	LONG l;
	char *pnt,*pnt2;
	char path[128];
	char file[9];
	char extension[5];
	char name1[128];
	char name2[128];

	if (argc < 2) {
		cprintf("\007No file specified\r\n");
		DosExit(EXIT_PROCESS,1);
	}
	pnt = argv[1];
	if (pnt[1] == ':') pnt += 2;
	if (pnt[0] == '\\') path[0] = 0;
	else {
		path[0] = '\\';
		n = 127;
		DosQCurDisk(&drv,&l);
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
	/* rename file back to normal in server directory */
	strcpy(name1,SERVER);
	strcat(name1,path);
	strcat(name1,file);
	pnt = name2+strlen(name1);
	strcat(name1,extension);
	strcpy(name2,name1);
	if (strlen(pnt) == 4) pnt[3] = 'X';
	else strcat(name2,"X");
	if (DosMove(name2,name1,0L)) {
		cprintf("\007File could not be released\r\n");
		DosExit(EXIT_PROCESS,1);
	}
}

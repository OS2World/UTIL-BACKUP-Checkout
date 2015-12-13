/* OS/2 mode clock synchronization program */

#define SERVER "\\\\SERVER"

#define INCL_DOSPROCESS
#define INCL_NETREMUTIL

#include <os2.h>
#include <lan.h>
#include <stdio.h>

void cdecl main(void);

void cdecl main()
{
	struct time_of_day_info srvdate;
	PIDINFO pid;
	DATETIME dosdate;

	DosGetPID(&pid);
	DosSetPrty(PRTYS_PROCESS,PRTYC_TIMECRITICAL,0,pid.pid);
	if (!NetRemoteTOD(SERVER,(char *)&srvdate,sizeof(srvdate))) {
		dosdate.hours = srvdate.tod_hours;
		dosdate.minutes = srvdate.tod_mins;
		dosdate.seconds = srvdate.tod_secs;
		dosdate.hundredths = srvdate.tod_hunds;
		dosdate.day = srvdate.tod_day;
		dosdate.month = srvdate.tod_month;
		dosdate.year = srvdate.tod_year;
		dosdate.timezone = srvdate.tod_timezone;
		dosdate.weekday = srvdate.tod_weekday;
		DosSetDateTime(&dosdate);
	}
	else puts("\007Can't get remote time.\n");
}

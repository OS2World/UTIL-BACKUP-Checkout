/* DOS mode clock synchronization program */

#define SERVER "\\\\SERVER"
#define DISK "\\DISK\\$$TIME.TMP"

#define INCL_NETREMUTIL

#include <lan.h>
#include <stdio.h>
#include <dos.h>
#include <direct.h>

typedef struct {
	char reserved[21];
	char attrib;
	unsigned int tod_secs2:5;
	unsigned int tod_mins:6;
	unsigned int tod_hours:5;
	unsigned int tod_day:5;
	unsigned int tod_month:4;
	unsigned int tod_year:7;
	long size;
	char name[13];
} MYFIND_T;

void cdecl main(void);

void cdecl main()
{
	int err;
	MYFIND_T mysrvdate;
	struct time_of_day_info srvdate;
	struct dosdate_t dosdate;
	struct dostime_t dostime;

	if (!(err = NetRemoteTOD(SERVER,(char *)&srvdate,sizeof(srvdate)))) {
		dostime.hour = srvdate.tod_hours;
		dostime.minute = srvdate.tod_mins;
		dostime.second = srvdate.tod_secs;
		dostime.hsecond = srvdate.tod_hunds;
		dosdate.day = srvdate.tod_day;
		dosdate.month = srvdate.tod_month;
		dosdate.year = srvdate.tod_year;
		_dos_setdate(&dosdate);
		_dos_settime(&dostime);
	}
	else {
		if (!(err = mkdir(SERVER DISK))) {
			if (!(err = _dos_findfirst(SERVER DISK,_A_SUBDIR,(struct find_t *)&mysrvdate))) {
				dostime.hour = (unsigned char)mysrvdate.tod_hours;
				dostime.minute = (unsigned char)mysrvdate.tod_mins;
				dostime.second = (unsigned char)(mysrvdate.tod_secs2*2);
				dostime.hsecond = 0;
				dosdate.day = (unsigned char)mysrvdate.tod_day;
				dosdate.month = (unsigned char)mysrvdate.tod_month;
				dosdate.year = mysrvdate.tod_year+1980;
				_dos_setdate(&dosdate);
				_dos_settime(&dostime);
			}
		}
		rmdir(SERVER DISK);
	}
	if (err) puts("\007Can't get remote time.\n");
}

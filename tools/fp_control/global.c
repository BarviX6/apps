/*
 * global.c
 *
 * (c) 2009 dagobert/donald@teamducktales
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */
#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <sys/ioctl.h>

#include "global.h"

#define E2TIMERSXML "/etc/enigma2/timers.xml"
#define E2WAKEUPTIME "/proc/stb/fp/wakeup_time"

#define NEUTRINO_TIMERS "/var/tuxbox/config/timerd.conf"

#define CONFIG "/etc/vdstandby.cfg"
char *sDisplayStd = "%a %d %H:%M:%S";

#define WAKEUPFILE "/var/wakeup"
#define WAS_TIMER_WAKEUP "/proc/stb/fp/was_timer_wakeup"

#define E2_WAKEUP_TIME_PROC
int verbose = 0; //verbose is off by default

static Model_t *AvailableModels[] =
{
	&Ufs910_1W_model,
	&Ufs910_14W_model,
	&UFS922_model,
	&Fortis_model,
	&HL101_model,
	&VIP2_model,
	&Hs5101_model,
	&UFS912_model,
	&UFC960_model,
	&Spark_model,
	&Adb_Box_model,
	&Cuberevo_model,
	&CNBOX_model,
	&Vitamin_model,
	NULL
};

#if 0
static time_t read_e2_wakeup(time_t curTime)
{
	char line[12];
	time_t recordTime = LONG_MAX;
	FILE *fd = fopen(E2WAKEUPTIME, "r");
	printf("Getting Enigma2 wakeup time");
	if (fd > 0)
	{
		fgets(line, 11, fd);
		sscanf(line, "%ld", &recordTime);
		if (recordTime <= curTime)
		{
			recordTime = LONG_MAX;
			printf(" - Got time, but in the past (ignoring)\n");
		}
		else
			printf(" - Done\n");
		fclose(fd);
	}
	else
	{
		printf(" - Error reading %s\n", E2WAKEUPTIME);
	}
	return recordTime;
}
#else
static time_t read_e2_timers(time_t curTime)
{
	char recordString[12];
	char line[1000];
	time_t recordTime = LONG_MAX;
	FILE *fd = fopen(E2TIMERSXML, "r");
	printf("Getting 1st Enigma2 timer");
	if (fd > 0)
	{
		while (fgets(line, 999, fd) != NULL)
		{
			line[999] = '\0';
			if (!strncmp("<timer begin=\"", line, 14))
			{
				unsigned long int tmp = 0;
				strncpy(recordString, line + 14, 10);
				recordString[11] = '\0';
				tmp = atol(recordString);
				recordTime = (tmp < recordTime && tmp > curTime ? tmp : recordTime);
			}
		}
		if (recordTime == LONG_MAX)
		{
			printf(" (none set)");
			recordTime = -1;
		}
		else
		{
			recordTime -= Vwakeup;
		}
		printf(" - Done\n");
		fclose(fd);
	}
	else
	{
		printf(" - Error reading %s\n", E2TIMERSXML);
	}
	return recordTime;
}
#endif

static time_t read_neutrino_timers(time_t curTime)
{
	char line[1000];
	time_t recordTime = LONG_MAX;
	FILE *fd = fopen(NEUTRINO_TIMERS, "r");
	printf("Getting 1st neutrino timer");
	if (fd > 0)
	{
		while (fgets(line, 999, fd) != NULL)
		{
			line[999] = '\0';
			if (strstr(line, "ALARM_TIME_") != NULL)
			{
				time_t tmp = 0;
				char *str;
				str = strstr(line, "=");
				if (str != NULL)
				{
					tmp = atol(str + 1);
					recordTime = (tmp < recordTime && tmp > curTime ? tmp : recordTime);
				}
			}
		}
		printf(" - Done\n");
		fclose(fd);
	}
	else
		printf(" - Error reading %s\n", NEUTRINO_TIMERS);
	if (recordTime != LONG_MAX)
	{
		int wakeupDecrement = Vwakeup;
		int platzhalter;
		char *dummy;

		checkConfig(&platzhalter, &platzhalter, &dummy, &wakeupDecrement);
		recordTime -= wakeupDecrement;
	}
	return recordTime;
}

// Write the wakeup time to a file to allow detection of wakeup cause
// in case fp does not support wakeup cause
static void write_wakeup_file(time_t wakeupTime)
{
	FILE *wakeupFile;
	wakeupFile = fopen(WAKEUPFILE, "w");
	if (wakeupFile)
	{
		fprintf(wakeupFile, "%ld", wakeupTime);
		fclose(wakeupFile);
		system("sync");
	}
}

static time_t read_wakeup_file()
{
	time_t wakeupTime = LONG_MAX;
	FILE  *wakeupFile;
	wakeupFile = fopen(WAKEUPFILE, "r");
	if (wakeupFile)
	{
		fscanf(wakeupFile, "%ld", &wakeupTime);
		fclose(wakeupFile);
	}
	return wakeupTime;
}

#define FIVE_MIN 300

// Important: system has to have a valid current time
// This is a little bit tricky, we can only detect if the time is valid
// and this check happens +-5min arround the timer
int getWakeupReasonPseudo(eWakeupReason *reason)
{
	time_t curTime    = 0;
	time_t wakeupTime = LONG_MAX;

	printf("%s: IMPORTANT: Valid Linux System Time is mandatory\n", __func__);

	time(&curTime);
	wakeupTime = read_wakeup_file();
	if ((curTime - FIVE_MIN) < wakeupTime && (curTime + FIVE_MIN) > wakeupTime)
	{
		*reason = TIMER;
	}
	else
	{
		*reason = NONE;
	}
	return 0;
}

int syncWasTimerWakeup(eWakeupReason reason)
{
	FILE *wasTimerWakeupProc = fopen(WAS_TIMER_WAKEUP, "w");
	if (wasTimerWakeupProc == NULL)
	{
		fprintf(stderr, "setWakeupReason failed to open %s\n", WAS_TIMER_WAKEUP);
		return -1;
	}
	if (reason == TIMER)
	{
		fwrite("1\n", 2, 1, wasTimerWakeupProc);
	}
	else
	{
		fwrite("0\n", 2, 1, wasTimerWakeupProc);
	}
	fclose(wasTimerWakeupProc);
	return 0;
}

// This function tries to read the wakeup time from enigma2 and neutrino
// The wakeup time will also be written to file for wakeupcause detection
// If no wakeup time can be found LONG_MAX will be returned
time_t read_timers_utc(time_t curTime)
{
	time_t wakeupTime = LONG_MAX;  // flag no timer read (yet)
	wakeupTime = read_e2_timers(curTime);  // get next e2timer
	if (wakeupTime == LONG_MAX) // if none
	{
		wakeupTime = read_neutrino_timers(curTime);  // try neutrino timer
	}
	write_wakeup_file(wakeupTime);
	return wakeupTime;
}

// This function returns a time in the future
time_t read_fake_timer_utc(time_t curTime)
{
	struct tm tsWake;
	struct tm *ts;
	time_t wakeupTime = LONG_MAX;

	ts = gmtime(&curTime);
	tsWake.tm_hour = ts->tm_hour;
	tsWake.tm_min  = ts->tm_min;
	tsWake.tm_sec  = ts->tm_sec;
	tsWake.tm_mday = ts->tm_mday;
	tsWake.tm_mon  = ts->tm_mon;
	tsWake.tm_year = ts->tm_year + 1;
	wakeupTime = mktime(&tsWake);
	return wakeupTime;
}
/* ******************************************** */

double modJulianDate(struct tm *theTime)
{ // struct tm (date) -> MJD since epoch
	double date;
	int month;
	int day;
	int year;

	year  = theTime->tm_year + 1900;
	month = theTime->tm_mon + 1;
	day   = theTime->tm_mday;
	date = day - 32076 +
		   1461 * (year + 4800 + (month - 14) / 12) / 4 +
		   367 * (month - 2 - (month - 14) / 12 * 12) / 12 -
		   3 * ((year + 4900 + (month - 14) / 12) / 100) / 4;
	date += (theTime->tm_hour + 12.0) / 24.0;
	date += (theTime->tm_min) / 1440.0;
	date += (theTime->tm_sec) / 86400.0;
	date -= 2400000.5;
	return date;
}

int get_GMT_offset(struct tm theTime)
{
	time_t theoffsetTime;
	time_t theinputTime;
	int gmt_offset;

	// Calculate time_t of input time theTime
//	theinputTime = (((int)modJulianDate(&theTime) & 0xffff) - 40587) * 86400;  // mjd starts on midnight 17-11-1858 which is 40587 days before unix epoch
	theinputTime = ((int)modJulianDate(&theTime) - 40587) * 86400;  // mjd starts on midnight 17-11-1858 which is 40587 days before unix epoch
	theinputTime += theTime.tm_hour * 3600;
	theinputTime += theTime.tm_min * 60;
	theinputTime += theTime.tm_sec;

	// Get time_t of input time theTime minus GMT offset
	theTime.tm_isdst = -1; /* say mktime that we do not know */
	theoffsetTime = mktime(&theTime);

	gmt_offset = theinputTime - theoffsetTime;
	return gmt_offset;
}

#if 0
#define LEAPYEAR(year) (!((year) % 4) && (((year) % 100) || !((year) % 400)))
#define YEARSIZE(year) (LEAPYEAR(year) ? 366 : 365)
static const int _ytab[2][12] =
{
	{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
	{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

int get_ndays(struct tm *theTime)
{ // struct tm (date) -> number of days since linux epoch
	int ndays = 0;
	int year;
	int i;

	year  = theTime->tm_year - 1; // do not count current year
	while (year >= 70)
	{
		ndays += 365;
		if (LEAPYEAR(year))
		{
			ndays++;
		}
		year--;
	}
	for (i = 0; i < theTime->tm_mon; i++)
	{
		ndays += _ytab[0][i];
	}
	if ((LEAPYEAR(theTime->tm_year)) && (theTime->tm_mon > 2))
	{
		ndays++;
	}
	ndays += theTime->tm_mday;
	printf("%s ndays: %d\n", __func__, ndays);
	printf("%s MJD: %d\n", __func__, ndays + 40587);
	return ndays;
}
#endif

/* ********************************************** */

int searchModel(Context_t *context, eBoxType type)
{
	int i;
	for (i = 0; AvailableModels[i] != NULL; i++)
		if (AvailableModels[i]->Type == type)
		{
			context->m = AvailableModels[i];
			return 0;
		}
	return -1;
}

int checkConfig(int *display, int *display_custom, char **timeFormat, int *wakeup)
{
	const int MAX = 100;
	char buffer[MAX];
	*display = 0;
	*display_custom = 0;
	*timeFormat = "Unknown";
	*wakeup = 5 * 60;
	FILE *fd_config = fopen(CONFIG, "r");  //read box /etc/vdstandby.cfg

	if (fd_config == NULL)
	{
		Vwakeup = 5 * 60; //default wakeupdecrement is 5 minutes

		printf("Config file (%s) not found,\nusing standard config:", CONFIG);
		printf("Config:\nDisplay: %d              Time format: %d\n", *display, *display_custom);
		printf("Displaycustom: %s  Wakeupdecrement: %d minutes %d seconds\n", *timeFormat, *wakeup/60, *wakeup%60);
		return -1;
	}
	while (fgets(buffer, MAX, fd_config))
	{
		if (!strncmp("DISPLAY=", buffer, 8))
		{
			char *option = &buffer[8];
			if (!strncmp("TRUE", option, 4))
			{
				*display = 1;
			}
			else
			{
				*display = 0;
			}
		}
		else if (!strncmp("DISPLAYCUSTOM=", buffer, 14))
		{
			char *option = &buffer[14];  //get buffer from character 14 on
			*display_custom = 1;
			*timeFormat = strdup(option);
		}
		else if (!strncmp("WAKEUPDECREMENT=", buffer, 16))
		{
			char *option = &buffer[16];
			*wakeup = atoi(option);
		}
	}
	if (*timeFormat == NULL)
	{
		*timeFormat = "?";
	}
	if (disp)
	{
		printf("Configuration of receiver:\n");
		printf("Display      : %d  Time format: %s", *display, *timeFormat);
		printf("Displaycustom: %d  Wakeupdecrement: %d minute(s)", *display_custom, *wakeup / 60);
		if (*wakeup % 60 != 0)
		{
			printf(" %d second(s)\n", *wakeup % 60);
		}
		else
		{
			printf("\n");
		}
	}
	Vdisplay = *display;
	VtimeFormat = *timeFormat;
	Vdisplay_custom = *display_custom;
	Vwakeup = *wakeup;
	fclose(fd_config);
	return 0;
}

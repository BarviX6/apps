/*
 * Spark.c
 *
 * (c) 2010 duckbox project
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
 ****************************************************************************
 *
 * Changes
 *
 * Date     By              Description
 * --------------------------------------------------------------------------
 * 20130909 Audioniek       Get wake up reason made functional.
 * 20130910 Audioniek       Set timer polished.
 * 20130911 Audioniek       Shutdown polished.
 * 20130911 Audioniek       Reboot built.
 * 20150404 Audioniek       -tm added for DVFD models.
 * 20150410 Audioniek       Awareness of VFD or DVFD front panel versions
 *                          added, including time mode on DVFD.
 *
 ****************************************************************************/

/* ******************* includes ************************ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <sys/ioctl.h>

#include "global.h"
#include "Spark.h"

static int Spark_setText(Context_t *context, char *theText);
int res;
unsigned int fp_type;
unsigned int time_mode;

/* ******************* constants ************************ */

#define cVFD_DEVICE "/dev/vfd"
#define cRTC_OFFSET_FILE "/proc/stb/fp/rtc_offset"
#define cMAXCharsSpark 8

typedef struct
{
	char    *arg;
	char    *arg_long;
	char    *arg_description;
} tArgs;

tArgs vHArgs[] =
{
	{ "-e", "  --setTimer           ", "Args: [time date] (format: HH:MM:SS dd-mm-YYYY)" },
	{ "", "                         ", "      No arg:     Set the most recent timer from e2 or neutrino" },
	{ "", "                         ", "                  to the frontcontroller and shutdown" },
	{ "", "                         ", "      Arg time date: Set frontcontroller wake-up time to" },
	{ "", "                         ", "                  time, shutdown, and wake up at given time" },
	{ "-d", "  --shutdown           ", "Args: [time date] (format: HH:MM:SS dd-mm-YYYY)" },
	{ "", "                         ", "      No arg:     Shut down immediately" },
	{ "", "                         ", "      Arg time date: Shut down at given time/date" },
	{ "-r", "  --reboot             ", "Args: [time date] (format: HH:MM:SS dd-mm-YYYY)" },
	{ "", "                         ", "      No arg:     Reboot immediately (= -e current time+5 date today)" },
	{ "", "                         ", "      Arg time date: Reboot at given time/date (= -e time date)" },
	{ "-g", "  --getTime            ", "Args: None        Display currently set frontprocessor time" },
	{ "-gs", " --getTimeAndSet      ", "Args: None        Set system time to current frontprocessor time" },
	{ "-gt", " --getWTime           ", "Args: None        Get the current frontcontroller wake up time" },
	{ "-s", "  --setTime            ", "Args: time date   Format: HH:MM:SS dd-mm-YYYY" },
	{ "", "                         ", "                  Set the frontprocessor time (date ignored)" },
	{ "", "                         ", "      WARNING:    front panel date will be 01-01-1970!" },
	{ "-st", " --setWTime           ", "Args: time date   Format: HH:MM:SS dd-mm-YYYY" },
	{ "", "                         ", "                  Set the frontprocessor wake up time" },
	{ "-sst", "--setSystemTime      ", "Args: None        Set frontprocessor time to current system time" },
//	{ "-p", "  --sleep              ", "Args: time date   Format: HH:MM:SS dd-mm-YYYY\n\t\tReboot receiver via fp at given time" },
	{ "-t", "  --settext            ", "Arg : text        Show text in front panel display" },
	{ "-l", "  --setLed             ", "Args: LED# 0|1|2  LED#: off, on or blink" },
	{ "-i", "  --setIcon            ", "Args: icon# 0|1   Set an icon off or on" },
	{ "-b", "  --setBrightness      ", "Arg : 0..7        Set display brightness (VFD/DVFD only)" },
	{ "-w", "  --getWakeupReason    ", "Args: None        Get the wake up reason" },
	{ "-tm", " --time_mode          ", "Arg : 0|1         Clock display off|on (DVFD only)" },
	{ "-L", "  --setLight           ", "Arg : 0|1         Set display off|on" },
	{ "-c", "  --clear              ", "Args: None        Clear display, all icons and LEDs off" },
	{ "-v", "  --version            ", "Args: None        Get version info from frontprocessor" },
	{ "-V", "  --verbose            ", "Args: None        Verbose operation" },
#if defined MODEL_SPECIFIC
	{ "-ms ", "--set_model_specific ", "Args: long long   Model specific set function" },
#endif
	{ NULL, NULL, NULL }
};

typedef struct
{
	int display;
	int display_custom;
	char *timeFormat;

	time_t wakeupTime;
	int wakeupDecrement;
} tSparkPrivate;

/* ******************* helper/misc functions ****************** */

/* Calculate the time value which we can pass to
    * the aotom fp. it is a mjd time (mjd=modified
    * julian date). mjd is relative to gmt so theGMTTime
    * must be in GMT/UTC.
    */
void Spark_calcAotomTime(time_t theGMTTime, char *destString)
{
	/* from u-boot aotom */
	struct tm *now_tm;
	now_tm = localtime(&theGMTTime);

	double mjd = modJulianDate(now_tm);
	int mjd_int = mjd;

	printf(" mjd %d\n", mjd_int);

	destString[0] = ((mjd_int & 0xff00) >> 8);
	destString[1] = (mjd_int & 0xff);
	destString[2] = now_tm->tm_hour;
	destString[3] = now_tm->tm_min;
	destString[4] = now_tm->tm_sec;
}

#if 0 //not used anywhere
unsigned long Spark_getAotomTime(char *aotomTimeString)
{
	unsigned int  mjd   = ((aotomTimeString[1] & 0xff) * 256) + (aotomTimeString[2] & 0xff);
	unsigned long epoch = ((mjd - 40587) * 86400); //01-01-1970
	unsigned int  hour  = aotomTimeString[3] & 0xff;
	unsigned int  min   = aotomTimeString[4] & 0xff;
	unsigned int  sec   = aotomTimeString[5] & 0xff;
	epoch += (hour * 3600 + min * 60 + sec);
//	printf("MJD = %d epoch = %ld, time = %02d:%02d:%02d\n", mjd, epoch, hour, min, sec);
	return epoch;
}
#endif
/* ******************* driver functions ****************** */

static int Spark_init(Context_t *context)
{
	tSparkPrivate *private = malloc(sizeof(tSparkPrivate));
	int vFd;
	int t_mode;
	unsigned char strVersion[20];
	const char *dp_type[9] = { "Unknown", "VFD", "LCD", "DVFD", "LED", "?", "?", "?", "LBD" };
	const char *tm_type[2] = { "off", "on" };

	vFd = open(cVFD_DEVICE, O_RDWR);
	if (vFd < 0)
	{
		fprintf(stderr, "Cannot open %s\n", cVFD_DEVICE);
		perror("");
	}

	((Model_t *)context->m)->private = private;
	memset(private, 0, sizeof(tSparkPrivate));
	checkConfig(&private->display, &private->display_custom, &private->timeFormat, &private->wakeupDecrement);

	if (ioctl(vFd, VFDGETVERSION, &strVersion) < 0) // get version info (1x u32 4x u8)
	{
		perror("Get version info");
		return -1;
	}
	if (strVersion[0] != '\0')  /* if the version info is OK */
	{
		fp_type = strVersion[4];
	}
	else
	{
		fprintf(stderr, "Error reading version from fp\n");
	}

	if (ioctl(vFd, VFDGETDISPLAYTIME, &t_mode) < 0)
	{
		time_mode = 1; //if no support from aotom, assume clock on
	}
	else
	{
		time_mode = t_mode;
	}
	if (disp)
	{
		printf("FP type is   : %s", dp_type[fp_type]);
		if (fp_type == 3)
		{
			printf(", time mode %s\n\n", tm_type[time_mode]);
		}
		else
		{
			printf("\n\n");
		}

	}
	return vFd;
}

static int Spark_usage(Context_t *context, char *prg_name, char *cmd_name)
{
	int i;

	fprintf(stderr, "Usage:\n\n");
	fprintf(stderr, "%s argument [optarg1] [optarg2]\n", prg_name);

	for (i = 0; ; i++)
	{
		if (vHArgs[i].arg == NULL)
		{
			break;
		}
		if ((cmd_name == NULL) || (strcmp(cmd_name, vHArgs[i].arg) == 0) || (strstr(vHArgs[i].arg_long, cmd_name) != NULL))
		{
			fprintf(stderr, "%s   %s   %s\n", vHArgs[i].arg, vHArgs[i].arg_long, vHArgs[i].arg_description);
		}
	}
	return 0;
}

static int Spark_setTimer(Context_t *context, time_t *timerTime)
{
	//-e command, rewritten, tested on Spark7162
	struct aotom_ioctl_data vData;
	time_t curTime;
	time_t iTime;
	time_t wakeupTime;
	struct tm *ts;
	struct tm *tss;
	unsigned long diff;
	int sday, smonth, syear;

	time(&curTime);  //get system time (UTC)
	tss = localtime(&curTime); //save it for the date
	sday = tss->tm_mday;
	smonth = tss->tm_mon + 1;
	syear = tss->tm_year + 1900;
	printf("Current system time: %02d:%02d:%02d %02d-%02d-%04d\n", tss->tm_hour, tss->tm_min, tss->tm_sec, tss->tm_mday, tss->tm_mon + 1, tss->tm_year + 1900);
	if (timerTime == NULL) // -e no argument = shutdown on next e2/neutrino timer
	{
		wakeupTime = read_timers_utc(curTime); //get current 1st timer
	}
	else
	{
		wakeupTime = *timerTime; //get specified time
	}
	if ((wakeupTime == LONG_MAX) || (wakeupTime == -1)) // if no timers set
	{
		return 0;
	}
	else
	{
		/* get front controller time */
		if (ioctl(context->fd, VFDGETTIME, &iTime) < 0)
		{
			perror("Get current fp time");
			return -1;
		}
		/* difference from now to wake up */
		diff = (unsigned long int) wakeupTime - curTime;
		if (iTime != '\0')
		{
			/* use current front controller time */
			curTime = iTime;
			ts = gmtime(&curTime);
			printf("Current fp");
		}
		else
		{
			printf("Error reading current fp time... assuming local system time\n");
			ts = localtime(&curTime);
			printf("Current system");
		}
		wakeupTime = curTime + diff;
		printf(" time: %02d:%02d:%02d %02d-%02d-%04d\n", ts->tm_hour, ts->tm_min, ts->tm_sec, sday, smonth, syear);
		if (curTime > wakeupTime)
		{
			printf("Wake up time in the past!\n");
			return 0;
		}
		ts = gmtime(&wakeupTime);
		printf("Wake up time: %02d:%02d:%02d", ts->tm_hour, ts->tm_min, ts->tm_sec);
//		if (diff > 86399)
		if ((tss->tm_mday == ts->tm_mday)
		&& (tss->tm_mon == ts->tm_mon)
		&& (tss->tm_year == ts->tm_year)) //today
		{
			printf(" (today)\n");
		}
		else
		{
			printf("\non          : %02d-%02d-%04d\n", sday, smonth, syear);
		}
//		if (tss->tm_mday > 1)
//		{
//			printf(" in %d day(s)", tss->tm_mday - 1);
//		}
//		if (tss->tm_mon != 0)
//		{
//			printf(", %d month(s)", tss->tm_mon);
//		}
//		if (tss->tm_year != 70)
//		{
//			printf(", %d year(s)", tss->tm_year - 70);
//		}
//		printf("\n");
		Spark_calcAotomTime(wakeupTime, vData.u.standby.time);
		if (ioctl(context->fd, VFDSTANDBY, &wakeupTime) < 0)
		{
			perror("Shut down until wake up time");
			return -1;
		}
	}
	return 0;
}

static int Spark_shutdown(Context_t *context, time_t *shutdownTimeGMT)
{
	//-d command, partially rewritten
	struct aotom_ioctl_data vData;
	time_t curTime;
	time_t wakeupTime;
	struct tm *ts;

	/* shutdown immediately */
	if (*shutdownTimeGMT == -1)
	{
		if (ioctl(context->fd, VFDPOWEROFF, &vData) < 0)
		{
			perror("Shut down immediately");
			return -1;
		}
		return 0;
	}
	/* shutdown at given time */
	wakeupTime = *shutdownTimeGMT;
	ts = localtime(&wakeupTime);
	printf("Shut down time: %02d:%02d:%02d %02d-%02d-%04d\n",
		   ts->tm_hour, ts->tm_min, ts->tm_sec, ts->tm_mday, ts->tm_mon + 1, ts->tm_year + 1900);
	time(&curTime);
	ts = localtime(&curTime);
	if (curTime > *shutdownTimeGMT)
	{
		printf("Shut down time in the past!\n");
		return 0;
	}
	while (curTime < *shutdownTimeGMT)
	{
		time(&curTime);
		ts = localtime(&curTime);
		printf("Current time  : %02d:%02d:%02d %02d-%02d-%04d\r",
			   ts->tm_hour, ts->tm_min, ts->tm_sec, ts->tm_mday, ts->tm_mon + 1, ts->tm_year + 1900);
		usleep(20000);
	}
	printf("\n\n");
	/* shut down */
	if (ioctl(context->fd, VFDPOWEROFF, &vData) < 0)
	{
		perror("Shutdown at given time");
		return -1;
	}
	return 0;
}

static int Spark_reboot(Context_t *context, time_t *rebootTimeGMT)
{
	//-r command, partially rewritten
	// Note: aotom does not have a particular reboot command
	time_t curTime;

	/* reboot at a given time */
	if (*rebootTimeGMT != -1)
	{
		return (Spark_setTimer(context, rebootTimeGMT));
	}
	/* reboot immediately */
	time(&curTime);
	*rebootTimeGMT = curTime + 5;
	return (Spark_setTimer(context, rebootTimeGMT));
}

static int Spark_getTime(Context_t *context, time_t *theGMTTime)
{
	// -g and -gs commands, adapted for spark
	struct tm *g_tm;
	time_t iTime;

	/* get front controller time */
	if (ioctl(context->fd, VFDGETTIME, &iTime) < 0)
	{
		perror("Get time");
		return -1;
	}
	/* if we get the fp time */
	if (iTime != '\0')
	{
		/* current frontcontroller time */
		*theGMTTime = iTime;
		g_tm = gmtime(&iTime);
		printf("Frontprocessor time: %02d:%02d:%02d %02d-%02d-%04d\n", g_tm->tm_hour,
			g_tm->tm_min, g_tm->tm_sec, g_tm->tm_mday, g_tm->tm_mon + 1, g_tm->tm_year + 1900);
	}
	else
	{
		fprintf(stderr, "Error reading time from frontprocessor\n");
		*theGMTTime = 0;
		return -1;
	}
	return 1;
}

static int Spark_getWTime(Context_t *context, time_t *theGMTTime)
{
	//-gt command, note: VFDGETWAKEUPTIME not supported by older aotoms
	time_t iTime;

	/* front controller wake up time */
	if (ioctl(context->fd, VFDGETWAKEUPTIME, &iTime) < 0)
	{
		perror("Get wakeup time");
		return -1;
	}
	/* if we get the fp time */
	if (iTime != '\0')
	{
		/* current frontcontroller wake up time */
		*theGMTTime = iTime;
	}
	else
	{
		fprintf(stderr, "Error reading wake up time from frontprocessor\n");
		*theGMTTime = 0;
		return -1;
	}
	return 1;
}

static int Spark_setTime(Context_t *context, time_t *theGMTTime)
{
	//-s command
	time_t sTime;
	struct tm *s_tm;

	sTime = *theGMTTime;
	s_tm = localtime(&sTime); //get the time the user put in and the matching GMT offset

	sTime += s_tm->tm_gmtoff;
	if (ioctl(context->fd, VFDSETTIME2, &sTime) < 0)
	{
		perror("Set time");
		return -1;
	}
	fprintf(stderr, "Frontprocessor time set to: %02d:%02d:%02d %02d-%02d-%04d (local)\n", s_tm->tm_hour,
		s_tm->tm_min, s_tm->tm_sec, s_tm->tm_mday, s_tm->tm_mon + 1, s_tm->tm_year + 1900);
	return 1;
}

static int Spark_setWTime(Context_t *context, time_t *theGMTTime)
{
	//-st command
	struct tm *swtm;
	time_t iTime;
	int proc_fs;
	FILE *proc_fs_file;

	iTime = *theGMTTime;
	swtm = localtime(&iTime);
	fprintf(stderr, "Setting wake up time to %02d:%02d:%02d %02d-%02d-%04d\n", swtm->tm_hour,
		swtm->tm_min, swtm->tm_sec, swtm->tm_mday, swtm->tm_mon + 1, swtm->tm_year + 1900);

	iTime += swtm->tm_gmtoff;
	if (ioctl(context->fd, VFDSETPOWERONTIME, &iTime) < 0)
	{
		perror("Set wake up time");
		return -1;

		// write UTC offset to /proc/stb/fp/rtc_offset
		proc_fs_file = fopen(cRTC_OFFSET_FILE, "w");
		if (proc_fs_file == NULL)
		{
			perror("Open rtc_offset");
			return -1;
		}
		proc_fs = fprintf(proc_fs_file, "%d", (int)swtm->tm_gmtoff);
		if (proc_fs < 0)
		{
			perror("Write rtc_offset");
			return -1;
		}
		fclose(proc_fs_file);
		fprintf(stderr, "/proc/stb/fp/rtc_offset set to: %+d seconds\n", (int)swtm->tm_gmtoff);
	}
	return 0;
}

static int Spark_setSTime(Context_t *context, time_t *theGMTTime)
{
	//-sst command
	time_t systemTime = time(NULL);
	struct tm *sst;
	int proc_fs;
	FILE *proc_fs_file;

	sst = localtime(&systemTime);
	if (sst->tm_year == 100)
	{
		fprintf(stderr, "Problem: system time not set.\n");
	}
	else
	{
		systemTime += sst->tm_gmtoff;
		// set front panel clock to local time
		if (ioctl(context->fd, VFDSETTIME2, &systemTime) < 0)
		{
			perror("Set FP time to system time");
			return -1;
		}
		fprintf(stderr, "Frontprocessor time set to: %02d:%02d:%02d %02d-%02d-%04d\n", sst->tm_hour,
			 sst->tm_min, sst->tm_sec, sst->tm_mday, sst->tm_mon + 1, sst->tm_year + 1900);

		// write UTC offset to /proc/stb/fp/rtc_offset
		proc_fs_file = fopen(cRTC_OFFSET_FILE, "w");
		if (proc_fs_file == NULL)
		{
			perror("Open rtc_offset");
			return -1;
		}
		proc_fs = fprintf(proc_fs_file, "%d", (int)sst->tm_gmtoff);
		if (proc_fs < 0)
		{
			perror("Write rtc_offset");
			return -1;
		}
		fclose(proc_fs_file);
		fprintf(stderr, "/proc/stb/fp/rtc_offset set to: %+d seconds\n", (int)sst->tm_gmtoff);
	}
	return 0;
}

static int Spark_setText(Context_t *context, char *theText)
{
	//-t command
	char text[cMAXCharsSpark + 1];
	int disp_size;

	switch (fp_type)
	{
		case 3: //DVFD
		{
			disp_size = (time_mode ? 10 : 16);
			break;
		}
		case 4: //LED
		{
			if (strlen(theText) > 2 //handle period, comma and colon
			&& (theText[2] == 0x2e || theText[2] == 0x2c || theText[2] == 0x3a))
			{
				disp_size = 5;
			}
			else
			{
				disp_size = 4;
			}
			break;
		}
		default: //VFD and others
		{
			disp_size = 8;
			break;
		}
	}
	strncpy(text, theText, disp_size);
	text[disp_size] = 0;
	write(context->fd, text, strlen(text));
	return 0;
}

static int Spark_setLed(Context_t *context, int which, int on)
{
	//-l command
	struct aotom_ioctl_data vData;

	if (on < 0 || on > 255)
	{
		printf("Illegal LED action %d (valid is 0..255)\n", on);
		return 0;
	}
	vData.u.led.led_nr = which;
	vData.u.led.on = on;
	res = (ioctl(context->fd, VFDSETLED, &vData));
	if (res < 0)
	{
		perror("Setled");
		return -1;
	}
	return 0;
}

static int Spark_setIcon(Context_t *context, int which, int on)
{
	//-i command
	int first, last;
	struct aotom_ioctl_data vData;

	if (fp_type == 3)
	{
		first = 48;
		last = 63;
	}
	else
	{
		first = 1;
		last = 47;
	}
	if ((which < first || which > last) 
	&& which != 46 //icons present on both VFD and DVFD
	&& which != 8
	&& which != 10
	&& which != 11
	&& which != 12
	&& which != 13
	&& which != 14
	&& which != 26)
	{
		return 0;
	}
	vData.u.icon.icon_nr = which;
	vData.u.icon.on = on;
	if (ioctl(context->fd, VFDICONDISPLAYONOFF, &vData) < 0)
	{
		perror("Set icon");
		return -1;
	}
	return 0;
}

static int Spark_setBrightness(Context_t *context, int brightness)
{
	//-b command
	struct aotom_ioctl_data vData;

	if (brightness < 0 || brightness > 7)
	{
		printf("Illegal brightness level %d (valid is 0..7)\n", brightness);
		return 0;
	}
	vData.u.brightness.level = brightness;
	if (ioctl(context->fd, VFDBRIGHTNESS, &vData) < 0)
	{
		perror("Set brightness");
		return -1;
	}
	return 0;
}

static int Spark_setLight(Context_t *context, int on)
{
	//-L command
	struct aotom_ioctl_data vData;
	if (on < 0 || on > 1)
	{
		printf("Illegal light value %d (valid is 0 | 1)\n", on);
		return 0;
	}
	vData.u.light.onoff = on;
	if (ioctl(context->fd, VFDDISPLAYWRITEONOFF, &vData) < 0)
	{
		perror("Set light");
		return -1;
	}
	return 0;
}

static int Spark_setDisplayTime(Context_t *context, int on)
{
	// -dt command
	time_t systemTime = time(NULL);
	struct tm *gmt;

	gmt = localtime(&systemTime); //get system time

	if ((gmt->tm_year + 1900) > 2016)
	{
		fprintf(stderr, "Option -dt is not supported on this receiver.\n");
		return -1;
	}

	fprintf(stderr, "Option -dt is deprecated in this receiver,\nplease use -sst or -s option in future.\n");

	if (gmt->tm_year == 100)
	{
		fprintf(stderr, "Problem: system time not set.\n");
	}
	else
	{
		fprintf(stderr, "Setting front panel clock to\ncurrent system time: %02d:%02d:%02d %02d-%02d-%04d\n",
				gmt->tm_hour, gmt->tm_min, gmt->tm_sec, gmt->tm_mday, gmt->tm_mon + 1, gmt->tm_year + 1900);
		systemTime += gmt->tm_gmtoff;
		if (ioctl(context->fd, VFDSETTIME2, &systemTime) < 0)
		{
			perror("Set FP time to system time");
			return -1;
		}
	}
	return 0;
}

static int Spark_setFan(Context_t *context, int on)
{
	// -sf command
	fprintf(stderr, "Set fan: not implemented,\nSparks do not have a fan!\n");
	return -1;
}

static int Spark_setRF(Context_t *context, int on)
{
	// -sr command
	fprintf(stderr, "Set RF: not implemented,\nSparks do not have an RF modulator!\n");
	return -1;
}

static int Spark_setLEDb(Context_t *context, int brightness)
{
	// -lb command
	fprintf(stderr, "Set LED brightness: not implemented,\nSparks cannot control LED brightness!\n");
	return -1;
}

static int Spark_clear(Context_t *context)
{
	//-c command
	struct aotom_ioctl_data vData;

	if (ioctl(context->fd, VFDDISPLAYCLR, &vData) < 0)
	{
		perror("Clear");
		return -1;
	}
	if (fp_type == 1 || fp_type == 3)
	{
		vData.u.icon.icon_nr = 46;
		vData.u.icon.on = 0;
		if (ioctl(context->fd, VFDICONDISPLAYONOFF, &vData) < 0)
		{
			perror("Set icon");
			return -1;
		}
	}
	vData.u.led.led_nr = 0;
	vData.u.led.on = 0;
	res = (ioctl(context->fd, VFDSETLED, &vData));
	if (res < 0)
	{
		perror("Setled (red)");
		return -1;
	}
	if (fp_type != 1)
	{
		vData.u.led.led_nr = 1;
		vData.u.led.on = 0;
		res = (ioctl(context->fd, VFDSETLED, &vData));
		if (res < 0)
		{
			perror("Setled (green)");
			return -1;
		}
	}
	return 0;
}

static int Spark_getWakeupReason(Context_t *context, eWakeupReason *reason)
{
	//-w command
	char mode[8];

	if (ioctl(context->fd, VFDGETSTARTUPSTATE, mode) < 0)
	{
		perror("Get wakeup reason");
		return -1;
	}
	if (mode[0] != '\0')  /* if we get the fp wake up reason */
	{
		*reason = mode[0] & 0xff; //get first byte
	}
	else
	{
		fprintf(stderr, "Error reading wakeup mode from frontprocessor\n");
		*reason = 0;  //echo unknown
	}
	return 0;
}

static int Spark_getVersion(Context_t *context, int *version)
{
	//-v command
	unsigned char strVersion[20];
	const char *CPU_type[3] = { "Unknown", "ATTING48", "ATTING88" };
	const char *Display_type[9] = { "Unknown", "VFD", "LCD", "DVFD", "LED", "?", "?", "?", "LBD" };

	if (ioctl(context->fd, VFDGETVERSION, &strVersion) < 0) // get version info (1x u32 4x u8)
	{
		perror("Get version info");
		return -1;
	}
	if (strVersion[0] != '\0')  /* if the version info is OK */
	{
		printf("Front processor version info:\n");
		printf("FP CPU type is   : %d (%s)\n", strVersion[0], CPU_type[strVersion[0]]);
		printf("Display type is  : %d (%s)\n", strVersion[4], Display_type[strVersion[4]]);
		printf("# of keys        : %d\n", strVersion[5]);
		printf("FP SW version is : %d.%d\n", strVersion[6], strVersion[7]);
		*version = strVersion[6] * 100 + strVersion[7];
	}
	else
	{
		fprintf(stderr, "Error reading version from fp\n");
		*version = -1;
	}
	return 0;
}

static int Spark_setTimeMode(Context_t *context, int on)
{
	// -tm command
	const char *timeMode_type[2] = { "off", "on" };
	struct aotom_ioctl_data vData;

	if (fp_type == 3)
	{
		if (on != 0)
		{
			on = 1;
		}
		vData.u.display_time.on = on;
		if (ioctl(context->fd, VFDSETDISPLAYTIME, &vData) < 0)
		{
			perror("Set time mode");
			return -1;
		}
		time_mode = on;
		if (disp)
		{
			printf("FP time mode is now %s.\n", timeMode_type[time_mode]);
		}
	}
	else
	{
		printf("Set time mode is only supported on DVFD displays.\n");
	}
	return 0;
}

static int Spark_exit(Context_t *context)
{
	tSparkPrivate *private = (tSparkPrivate *)((Model_t *)context->m)->private;

	if (context->fd > 0)
	{
		close(context->fd);
	}
	free(private);
	return 1;
}

Model_t Spark_model =
{
	.Name             = "Edision/Spark frontpanel control utility",
	.Type             = Spark,
	.Init             = Spark_init,
	.Clear            = Spark_clear,
	.Usage            = Spark_usage,
	.SetTime          = Spark_setTime,
	.SetSTime         = Spark_setSTime,
	.SetTimer         = Spark_setTimer,
	.GetTime          = Spark_getTime,
	.GetWTime         = Spark_getWTime,
	.SetWTime         = Spark_setWTime,
	.Shutdown         = Spark_shutdown,
	.Reboot           = Spark_reboot,
	.Sleep            = NULL,
	.SetText          = Spark_setText,
	.SetLed           = Spark_setLed,
	.SetIcon          = Spark_setIcon,
	.SetBrightness    = Spark_setBrightness,
	.GetWakeupReason  = Spark_getWakeupReason,
	.SetLight         = Spark_setLight,
	.SetLedBrightness = Spark_setLEDb,
	.GetVersion       = Spark_getVersion,
	.SetRF            = Spark_setRF,
	.SetFan           = Spark_setFan,
	.SetDisplayTime   = Spark_setDisplayTime,
	.SetTimeMode      = Spark_setTimeMode,
#if defined MODEL_SPECIFIC
	.ModelSpecific    = NULL,
#endif
	.Exit             = Spark_exit
};

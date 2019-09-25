/*
 * Vitamin_HD5000.c
 *
 * (c) 2009 dagobert@teamducktales
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
 *****************************************************************************
 *
 * Changes
 *
 * Date     By              Description
 * --------------------------------------------------------------------------
 * 20190210 Audioniek       CubeRevo specific usage added.
 * 20190212 Audioniek       Completely tested through and improved/expanded.
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
#include <linux/input.h>

#include "global.h"
#include "Vitamin_HD5000.h"

static int setText(Context_t *context, char *theText);
static int Clear(Context_t *context);
static int setIcon(Context_t *context, int which, int on);
//static int getVersion(Context_t *context, int *version);
//static int setDisplayTime(Context_t *context, int on);

/* ******************* constants ************************ */

#define cVFD_DEVICE "/dev/vfd"
#define cRTC_OFFSET_FILE "/proc/stb/fp/rtc_offset"
#define cEVENT_DEVICE "/dev/input/event0"

#define cMAXCharsVitamin 11

typedef struct
{
	char *arg;
	char *arg_long;
	char *arg_description;
} tArgs;

tArgs vVArgs[] =
{
	{ "-d", "  --shutdown         * ", "Args: None        Shut down immediately" },
	{ "-r", "  --reboot           * ", "Args: time date Format: HH:MM:SS dd-mm-YYYY" },
	{ "", "                         ", "      Reboot receiver via fc at given time" },
	{ "-s", "  --setTime          * ", "Args: time date   Format: HH:MM:SS dd-mm-YYYY" },
	{ "", "                         ", "      Set the frontprocessor time" },
	{ "-sst", "--setSystemTime    * ", "Args: None        Set front processor time to system time" },
	{ "-p", "  --sleep            * ", "Args: time date   Format: HH:MM:SS dd-mm-YYYY" },
	{ "", "                         ", "      Reboot receiver via fp at given time" },
	{ "-t", "  --settext            ", "Args: text        Set text to frontpanel" },
	{ "-l", "  --setLed             ", "Args: LED# int    LED#: int=on,off (0,1)" },
	{ "-i", "  --setIcon            ", "Args: icon# 1|0   Set an icon on or off" },
	{ "-b", "  --setBrightness      ", "Arg : 0..7        Set display brightness" },
	{ "-L", "  --setLight           ", "Arg : 0|1         Set display on/off" },
	{ "-c", "  --clear              ", "Args: None        Clear display, all icons and LEDs off" },
	{ "-V", "  --verbose            ", "Args: None        Verbose operation" },
#if defined MODEL_SPECIFIC
	{ "-ms", " --model_specific     ", "Args: int1 [int2] [int3] ... [int16]   (note: input in hex)" },
	{ "", "                         ", "                  Model specific test function" },
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
} tVitaminPrivate;

/* ******************* helper/misc functions ****************** */

static void setMode(int fd)
{
	struct micom_ioctl_data micom;

	micom.u.mode.compat = 1;
	if (ioctl(fd, VFDSETMODE, &micom) < 0)
	{
		perror("Set compatibility mode");
	}
}

/* Remark on times:
 *
 * System time is in UTC
 * RTC/FP time is local (required for correct time display in time display mode)
 * User supplied times on the command line are assumed to be local
 */

/* Calculate the time value which we can pass to
 * the micom fp.
 */
static void setMicomTime(time_t theGMTTime, char *destString, bool seconds)
{ // time_t -> micom string
	struct tm *now_tm;
	char tmpString[13];

	memset(tmpString, 0, sizeof(tmpString));
//	//printf("Time to set: %10d (time_t)\n", (int)theGMTTime);
	now_tm = gmtime(&theGMTTime);

	if (seconds)
	{
#if 1
		printf("Time to set: %02d:%02d:%02d %02d-%02d-20%02d\n", now_tm->tm_hour, now_tm->tm_min, now_tm->tm_sec, now_tm->tm_mday,
			now_tm->tm_mon + 1, now_tm->tm_year - 100);
#endif
		sprintf(tmpString, "%02d%02d%02d%02d%02d%02d",
			now_tm->tm_year - 100, now_tm->tm_mon + 1, now_tm->tm_mday, now_tm->tm_hour, now_tm->tm_min, now_tm->tm_sec);
		strncpy(destString, tmpString, 12);
	}
	else
	{
#if 1
		printf("Time to set: %02d:%02d:%02d %02d-%02d-20%02d (seconds ignored)\n", now_tm->tm_hour, now_tm->tm_min, now_tm->tm_sec, now_tm->tm_mday,
			now_tm->tm_mon + 1, now_tm->tm_year - 100);
#endif
		sprintf(tmpString, "%02d%02d%02d%02d%02d", 
			now_tm->tm_year - 100, now_tm->tm_mon + 1, now_tm->tm_mday, now_tm->tm_hour, now_tm->tm_min);
		strncpy(destString, tmpString, 10);
	}
}

#if 0
static time_t getMicomTime(char* TimeString)
{ // micomstring -> time_t
	char convertTime[128];
	unsigned int year, month, day;
	unsigned int hour, min, sec;
	struct tm the_tm;
	struct tm *tz_time_utc_tm;
	struct tm *tz_time_local_tm;
	time_t convertedTime;

	memset(convertTime, 0, sizeof(convertTime));
	sprintf(convertTime, "%02x %02x %02x %02x %02x %02x\n",
			TimeString[0], TimeString[1], TimeString[2],
			TimeString[3], TimeString[4], TimeString[5]);
	sscanf(convertTime, "%d %d %d %d %d %d", &sec, &min, &hour, &day, &month, &year);
	the_tm.tm_year = year + 100;
	the_tm.tm_mon  = month - 1;
	the_tm.tm_mday = day;
	the_tm.tm_hour = hour;
	the_tm.tm_min  = min;
	the_tm.tm_sec  = sec;
	the_tm.tm_isdst = -1; // struct tm in local time, convert to UTC
//	printf("Converted time: %02d:%02d:%02d %02d-%02d-20%02d (seconds ignored)\n", the_tm.tm_hour, the_tm.tm_min, the_tm.tm_sec, the_tm.tm_mday,
//		the_tm.tm_mon + 1, the_tm.tm_year - 100);
	convertedTime = mktime(&the_tm); // twice local due to mktime...
	tz_time_local_tm = localtime(&convertedTime);
	hour = tz_time_local_tm->tm_hour;
	if (hour == 0)
	{
		hour = 24;
	}
	tz_time_utc_tm = gmtime(&convertedTime);
//	printf("Converted time2: %02d:%02d:%02d %02d-%02d-20%02d (seconds ignored)\n", tz_time_utc_tm->tm_hour, tz_time_utc_tm->tm_min, tz_time_utc_tm->tm_sec, tz_time_utc_tm->tm_mday,
//		tz_time_utc_tm->tm_mon + 1, tz_time_utc_tm->tm_year - 100);
	convertedTime += ((hour - tz_time_utc_tm->tm_hour) * 3600); 
//	printf("Addition: %d\n",  ((hour - tz_time_utc_tm->tm_hour) * 3600));
	return convertedTime;
}
#endif

/* ******************* driver functions ****************** */

static int init(Context_t *context)
{
	tVitaminPrivate *private = malloc(sizeof(tVitaminPrivate));
	int vFd;

	vFd = open(cVFD_DEVICE, O_RDWR);
	if (vFd < 0)
	{
		fprintf(stderr, "Cannot open %s\n", cVFD_DEVICE);
		perror("");
	}
	((Model_t *)context->m)->private = private;
	memset(private, 0, sizeof(tVitaminPrivate));
	checkConfig(&private->display, &private->display_custom, &private->timeFormat, &private->wakeupDecrement);
	return vFd;
}

static int usage(Context_t *context, char *prg_name, char *cmd_name)
{
	int i;

	fprintf(stderr, "Usage:\n\n");
	fprintf(stderr, "%s argument [optarg1] [optarg2]\n", prg_name);
	for (i = 0; ; i++)
	{
		if (vVArgs[i].arg == NULL)
		{
			break;
		}
		if ((cmd_name == NULL) || (strcmp(cmd_name, vVArgs[i].arg) == 0) || (strstr(vVArgs[i].arg_long, cmd_name) != NULL))
		{
			fprintf(stderr, "%s   %s   %s\n", vVArgs[i].arg, vVArgs[i].arg_long, vVArgs[i].arg_description);
		}
	}
	fprintf(stderr, "Options marked * should be the only calling argument.\n");
	return 0;
}

static int setTime(Context_t *context, time_t *theGMTTime)
{
	// -s command
	struct micom_ioctl_data vData;

	setMicomTime(*theGMTTime, vData.u.time.time, 1); // with seconds

	if (ioctl(context->fd, VFDSETTIME, &vData) < 0)
	{
		perror("Set time");
		printf("%s < (-1)\n", __func__);
		return -1;
	}
	return 0;
}

#if 0
static int getTime(Context_t *context, time_t *theGMTTime)
{
	// -g command
	struct micom_ioctl_data vData;

	if (ioctl(context->fd, VFDGETTIME, &vData) < 0)
	{
		perror("getTime");
		//printf("%s <- -1\n", __func__);
		return -1;
	}
	*theGMTTime = (time_t)getMicomTime(vData.u.time.time);
#if 0
	printf("Got current FP time %02x:%02x:%02x %02x-%02x-20%02x\n",
		vData.u.time.time[2], vData.u.time.time[1], vData.u.time.time[0], vData.u.time.time[3], vData.u.time.time[4], vData.u.time.time[5]);
#endif
	return 0;
}
#endif

#if 0
static int setSTime(Context_t *context, time_t *theGMTTime)
{
	// -sst command
	time_t curTime;
	char fp_time[8];
	time_t curTimeFP;
	struct tm *ts_gmt;
	int gmt_offset;
	int proc_fs;
	FILE *proc_fs_file;

	time(&curTime); // get system time in UTC
	ts_gmt = gmtime(&curTime);
	gmt_offset = get_GMT_offset(*ts_gmt);
	printf("Current system time: %02d:%02d:%02d %02d-%02d-%04d (local)\n",
		ts_gmt->tm_hour + (gmt_offset / 3600), ts_gmt->tm_min, ts_gmt->tm_sec,
		ts_gmt->tm_mday, ts_gmt->tm_mon + 1, ts_gmt->tm_year + 1900);
	curTime += gmt_offset;
	curTime += 3506716800u; // start of MJD
	setTime(context, &curTime); // set fp clock to local time
	sleep(2); // allow FP time to process the new time

	/* Read fp time back */
	if (ioctl(context->fd, VFDGETTIME, &fp_time) < 0)
	{
		perror("Gettime");
		return -1;
	}
	curTimeFP = (time_t)getMicomTime(fp_time);
	ts_gmt = gmtime(&curTimeFP);
	printf("Front panel time set to: %02d:%02d:%02d %02d-%02d-%04d (local)\n", ts_gmt->tm_hour, ts_gmt->tm_min, ts_gmt->tm_sec,
		ts_gmt->tm_mday, ts_gmt->tm_mon + 1, ts_gmt->tm_year + 1900);

	// write UTC offset to /proc/stb/fp/rtc_offset
	proc_fs_file = fopen(cRTC_OFFSET_FILE, "w");
	if (proc_fs_file == NULL)
	{
		perror("Open rtc_offset");
		return -1;
	}
	proc_fs = fprintf(proc_fs_file, "%d", gmt_offset);
	if (proc_fs < 0)
	{
		perror("Write rtc_offset");
		return -1;
	}
	fclose(proc_fs_file);
	printf("Note: /proc/stb/fp/rtc_offset set to: %+d seconds.\n", gmt_offset);
	return 0; // ?? -> segmentation fault
}
#endif

#if 0
static int setTimer(Context_t *context, time_t *theGMTTime)
{
	// -e command
	time_t curTime    = 0;
	time_t curTimeFP  = 0;
	time_t wakeupTime = 0;
	struct tm *ts_gmt;
	int gmt_offset;
	struct tm *tsw;
	struct micom_ioctl_data vData;

	time(&curTime);// get system time in UTC
	ts_gmt = gmtime(&curTime);
	gmt_offset = get_GMT_offset(*ts_gmt);
	printf("Current system time: %02d:%02d:%02d %02d-%02d-%04d (local)\n",
		ts_gmt->tm_hour + (gmt_offset / 3600), ts_gmt->tm_min, ts_gmt->tm_sec,
		ts_gmt->tm_mday, ts_gmt->tm_mon + 1, ts_gmt->tm_year + 1900);

	if (theGMTTime == NULL) // -e no argument = shutdown until next e2/neutrino timer
	{
		wakeupTime = read_timers_utc(curTime); //get current 1st timer
		wakeupTime += gmt_offset; // timers are stored in UTC
	}
	else
	{
		wakeupTime = *theGMTTime; //get specified time (assumed local)
		wakeupTime -= 3506716800u; // start of MJD
		tsw = gmtime(&wakeupTime);
#if 0
		printf("Wake up time (arg): %02d:%02d:%02d %02d-%02d-%04d (local)\n", tsw->tm_hour, tsw->tm_min,
		tsw->tm_sec, tsw->tm_mday, tsw->tm_mon + 1, tsw->tm_year + 1900);
#endif
	}
	curTime += gmt_offset; // convert to local
	// check --> wakeupTime is set and larger than curTime and no larger than 300 days in the future
	// check --> there is no timer set
	if ((wakeupTime <= 0) || ((wakeupTime == LONG_MAX)) || (curTime > wakeupTime) || (curTime < (wakeupTime - 25920000)))
	{
		/* shut down immedately */
		printf("No timers set or 1st timer more than 300 days ahead,\nor all timer(s) in the past.\n");
//		wakeupTime = 946684800u; // 00:00:00 01-01-2000 -> timer icon off
		vData.u.wakeup_time.time[0] = '\0';
	}
	else // wake up time valid and in the coming 300 days
	{
		unsigned long diff;
		char fp_time[7];

		/* Determine difference between system time and wake up time */
		diff = (unsigned long int) wakeupTime - curTime;

		/* Check if front panel clock is set properly */
		memset(fp_time, 0, sizeof(fp_time));
		if (ioctl(context->fd, VFDGETTIME, &fp_time) < 0)
		{
			perror("Gettime");
			return -1;
		}
#if 0
		printf("Current FP time %02x:%02x:%02x %02x-%02x-20%02x\n",
			fp_time.u.time.time[2], fp_time.u.time.time[1], fp_time.u.time.time[0], fp_time.u.time.time[3], fp_time.u.time.time[4], fp_time.u.time.time[5]);
#endif
		sleep(1);
		if (fp_time[0] != '\0')
		{
			curTimeFP = (time_t)getMicomTime(fp_time);
			/* set FP-Time if time is more than 5 minutes off */
			if (abs((int)(curTimeFP - curTime)) > 300)
			{
				printf("Time difference between fp and system: %+d seconds.\n", (int)(curTimeFP - curTime));
				setTime(context, &curTime); // sync fp clock
				ts_gmt = gmtime(&curTimeFP);
				printf("Front panel time corrected, set to: %02d:%02d:%02d %02d-%02d-%04d (local)\n",
					ts_gmt->tm_hour, ts_gmt->tm_min, ts_gmt->tm_sec, ts_gmt->tm_mday, ts_gmt->tm_mon + 1, ts_gmt->tm_year + 1900);
			}
		}
		else
		{
			fprintf(stderr, "Error reading front panel time... using system time.\n");
			curTimeFP = curTime;
		}
		wakeupTime = curTimeFP + diff;
	}
	tsw = gmtime(&wakeupTime);
	printf("Wake up time: %02d:%02d:%02d %02d-%02d-%04d (local)\n", tsw->tm_hour, tsw->tm_min,
		tsw->tm_sec, tsw->tm_mday, tsw->tm_mon + 1, tsw->tm_year + 1900);

	setMicomTime(wakeupTime, vData.u.wakeup_time.time, 0); // ignore seconds
	fflush(stdout);
	fflush(stderr);
	sleep(2);
	if (ioctl(context->fd, VFDSTANDBY, &vData) < 0)
	{
		perror("Shut down");
		printf("%s <- -1\n", __func__);
		return -1;
	}
	return 0;
}
#endif

#if 0
static int getWakeupTime(Context_t *context, time_t *theGMTTime)
{
	// -gw command
	struct micom_ioctl_data vData;

	if (ioctl(context->fd, VFDGETWAKEUPTIME, &vData) < 0)
	{
		perror("getWakeupTime");
		return -1;
	}
	*theGMTTime = (time_t)getMicomTime(vData.u.wakeup_time.time);
	return 0;
}
#endif

#if 0
static int setWakeupTime(Context_t *context, time_t *theGMTTime)
{
	// -st command
	struct micom_ioctl_data vData;

	setMicomTime(*theGMTTime - 3506716800u, vData.u.wakeup_time.time, 0); // without seconds

	if (ioctl(context->fd, VFDSETWAKEUPTIME_CUB, &vData) < 0)
	{
		perror("Set wake up time");
		return -1;
	}
	return 0;
}
#endif

static int shutdown(Context_t *context, time_t *shutdownTimeGMT)
{
	// -d command, OK
	struct micom_ioctl_data vData;

	/* shutdown immediately */
	if (ioctl(context->fd, VFDSTANDBY, &vData) < 0)
	{
		perror("Shutdown");
		return -1;
	}
	return 0;
}

static int reboot(Context_t *context, time_t *rebootTimeGMT)
{
	//-r command
	time_t curTime;
	struct tm *ts_gmt;
	int gmt_offset;
	struct micom_ioctl_data vData;

	time(&curTime); // get system time in UTC
	ts_gmt = gmtime(&curTime);
	gmt_offset = get_GMT_offset(*ts_gmt);

	//printf("Waiting until reboot time (%10d)", (int)*rebootTimeGMT);
	while (1)
	{
		time(&curTime);
		curTime += gmt_offset;
		curTime += 3506716800u; // start of MJD
		//printf("time: %10d, reboot: %10d\n", (int)curTime, (int)*rebootTimeGMT);
		if (curTime >= *rebootTimeGMT)
		{
			if (ioctl(context->fd, VFDREBOOT, &vData) < 0)
			{
				perror("Reboot");
				return -1;
			}
//		printf(".");
		}
		sleep(1);
	}
	return 0;
}

static int Sleep(Context_t *context, time_t *wakeUpGMT)
{
	// -p command
	time_t curTime;
	int gmt_offset;
	int sleep_flag;
	int vFd;
	fd_set rfds;
	struct timeval tv;
	int retval, i, rd;
	struct tm *ts;
//	char output[cMAXCharsVitamin + 1];
	struct input_event ev[64];

//	tVitaminPrivate *private = (tVitaminPrivate *)((Model_t *)context->m)->private;
	vFd = open(cEVENT_DEVICE, O_RDWR);
	if (vFd < 0)
	{
		fprintf(stderr, "Cannot open %s\n", cEVENT_DEVICE);
		perror("");
		return -1;
	}
	Clear(context); /* clear display */
	sleep_flag = 1;

	while (sleep_flag)
	{
		time(&curTime);  // get system time (UTC)
		ts = gmtime(&curTime);
		gmt_offset = get_GMT_offset(*ts); // convert to local
		curTime += gmt_offset;
		curTime += 3506716800u; // start of MJD

		if (curTime >= *wakeUpGMT)
		{
			sleep_flag = 0;
		}
		else
		{
			FD_ZERO(&rfds);
			FD_SET(vFd, &rfds);
			tv.tv_sec = 0;
			tv.tv_usec = 100000;
			retval = select(vFd + 1, &rfds, NULL, NULL, &tv);
			if (retval > 0)
			{
				rd = read(vFd, ev, sizeof(struct input_event) * 64);
				if (rd < (int) sizeof(struct input_event))
				{
					continue;
				}
				for (i = 0; i < rd / sizeof(struct input_event); i++)
				{
					if (ev[i].type == EV_SYN)
					{
					}
					else if (ev[i].type == EV_MSC && (ev[i].code == MSC_RAW || ev[i].code == MSC_SCAN))
					{
					}
					else
					{
						if (ev[i].code == 116)
						{
							sleep_flag = 0;
						}
					}
				}
			}
		}
		sleep(1);
	}
	Clear(context); /* clear display */
	return 0;
}

static int setText(Context_t *context, char *theText)
{
	// -t command
	char vHelp[128];

	strncpy(vHelp, theText, 64);
	vHelp[64] = '\0';
	write(context->fd, vHelp, strlen(vHelp));
	return 0;
}

static int setLed(Context_t *context, int which, int state)
{
	// -l command
	struct micom_ioctl_data vData;
	unsigned int ioctlcmd = VFDSETLED;

	// states: 0 = off, 1 = on
	if (state < 0 || state > 1)
	{
		printf("Illegal LED state %d (valid is 0 or 1)\n", state);
		return -1;
	}
	switch (which)
	{
		case 0:  // blue/power)
		{
			ioctlcmd = VFDSETLED;
			break;
		}
		case 1:  // red
		{
			ioctlcmd = VFDSETREDLED;
			break;
		}
		case 2:  // green
		{
			ioctlcmd = VFDSETGREENLED;
			break;
		}
		default:
		{
			printf("Illegal LED number %d (valid is 0..2)\n", which);
			return -1;
		}
	}
	vData.u.led.on = state;
	setMode(context->fd);
	if (ioctl(context->fd, ioctlcmd, &vData) < 0)
	{
		perror("SetLED");
		return -1;
	}
	return 0;
}

static int setRFModulator(Context_t *context, int on)
{
	// -sr command
	printf("This model does not have an RF modulator.\n");
	return 0;
}

static int setFan(Context_t *context, int on)
{
	// -sf command
	printf("This model does not have a fan.\n");
	return 0;
}

static int setIcon(Context_t *context, int which, int on)
{
	// -i command, OK
	struct micom_ioctl_data vData;

	vData.u.icon.icon_nr = which;
	vData.u.icon.on = on;
	setMode(context->fd);
	if (ioctl(context->fd, VFDICONDISPLAYONOFF, &vData) < 0)
	{
		perror("setIcon");
		return -1;
	}
	return 0;
}

static int setBrightness(Context_t *context, int brightness)
{
	// -b command, OK
	struct micom_ioctl_data vData;

	if (brightness < 0 || brightness > 7)
	{
		return -1;
	}
	vData.u.brightness.level = brightness;
	setMode(context->fd);
	printf("%d\n", context->fd);
	if (ioctl(context->fd, VFDBRIGHTNESS, &vData) < 0)
	{
		perror("setBrightness");
		return -1;
	}
	return 0;
}

static int Clear(Context_t *context)
{
	// -c command
	int i;
	struct vfd_ioctl_data data;

	data.start = 0;
	if (ioctl(context->fd, VFDDISPLAYCLR, &data) < 0)
	{
		perror("Clear");
		return -1;
	}
	setLed(context, 0, 0); //blue led off
	for (i = 1; i < 18; i++) //all icons off
	{
		setIcon(context, i, 0);
	}
	return 0;
}

static int setLight(Context_t *context, int on)
{
	// -L command, OK
	struct micom_ioctl_data vData;

	vData.u.light.onoff = on;
//	setMode(context->fd);
	if (ioctl(context->fd, VFDDISPLAYWRITEONOFF, &vData) < 0)
	{
		perror("Set light");
		return -1;
	}
	return 0;
}

#if 0
static int getWakeupReason(Context_t *context, eWakeupReason *reason)
{
	//-w command
	struct micom_ioctl_data vData;

	/* front controller data */
	if (ioctl(context->fd, VFDGETWAKEUPMODE, &vData) < 0)
	{
		perror("Get wakeup reason");
		return -1;
	}
	if ((vData.u.status.status & 0xff) == 0x02)
	{
		*reason = TIMER;
	}
	else
	{
		*reason = NONE;
	}
	//printf("Reason = 0x%x\n", *reason);
	return 0;
}
#endif

#if 0
static int getVersion(Context_t *context, int *version)
{
	//-v command
	struct micom_ioctl_data micom;

	/* front controller version */
	if (ioctl(context->fd, VFDGETVERSION, &micom) < 0)
	{
		perror("getVersion");
		return -1;
	}
	*version = micom.u.version.version;
	printf("micom version = %d\n", micom.u.version.version);
	return 0;
}
#endif

#if 0
static int setTimeMode(Context_t *context, int twentyFour)
{
	// -tm command
	struct micom_ioctl_data vData;

	vData.u.timemode.twentyFour = twentyFour;
	setMode(context->fd);
	if (ioctl(context->fd, VFDSETTIMEMODE, &vData) < 0)
	{
		perror("setTimeMode");
		return -1;
	}
	return 0;
}
#endif

#if 0
static int setLedBrightness(Context_t *context, int brightness)
{
	// -led command, not tested
	struct micom_ioctl_data vData;
	int version;

	getVersion(context, &version);
	if (version >= 2)
	{
		printf("This model cannot control LED brightness.\n");
		return 0;
	}

	if (brightness < 0 || brightness > 0xff)
	{
		return -1;
	}
	vData.u.brightness.level = brightness;
	setMode(context->fd);

	if (ioctl(context->fd, VFDLEDBRIGHTNESS_CUB, &vData) < 0)
	{
		perror("setLedBrightness");
		return -1;
	}
	return 0;
}
#endif

#if defined MODEL_SPECIFIC
static int modelSpecific(Context_t *context, char len, unsigned char *data)
{
	//-ms command
	int i, res;
	unsigned char testdata[12];

#if 0
	printf("len = %02d\n", len);
	for (i = 0; i < len; i++)
	{
		printf("data[%02d] = 0x%02x\n", i, (int)data[i]);
	}
	printf("\n");
#endif	
	testdata[0] = len; // set length

	if (data[0] == 0xf5)  // CLASS_DISPLAY2 command
	{
		memset(testdata, ' ', sizeof(testdata));
//		testdata[0] = 11; // set length (always 11)
	}
	else
	{
		memset(testdata, 0, sizeof(testdata));
//		testdata[0] = 8; // set length (always 8)
	}
	printf("micom ioctl: VFDTEST (0x%08x) CMD=", VFDTEST);
	for (i = 1; i <= len; i++)
	{
		testdata[i] = data[i - 1] & 0xff;
		printf("0x%02x ", testdata[i]);
	}
	printf("\n");

	memset(data, 0, 18);  // clear return data

	setMode(context->fd); //set mode 1

	res = (ioctl(context->fd, VFDTEST, &testdata) < 0);

	if (res < 0)
	{
		perror("Model specific");
		return -1;
	}
	else
	{
		for (i = 0; i < ((testdata[1] == 1) ? 11 : 2); i++)
		{
			data[i] = testdata[i]; //return values
		}
	}
	return testdata[0];
}
#endif

static int Exit(Context_t *context)
{
	tVitaminPrivate *private = (tVitaminPrivate *)((Model_t *)context->m)->private;

	if (context->fd > 0)
	{
		close(context->fd);
	}
	free(private);
	return 1;
}

Model_t Vitamin_model =
{
	.Name             = "Vitamin_HD5000 frontpanel control utility",
	.Type             = Vitamin_HD5000,
	.Init             = init,
	.Clear            = Clear,
	.Usage            = usage,
	.SetTime          = setTime,
	.GetTime          = NULL,  // getTime,
	.SetTimer         = NULL,  // setTimer,
	.GetWTime         = NULL,  // getWakeupTime,
	.SetWTime         = NULL,  // setWakeupTime,
	.SetSTime         = NULL,  // setSTime,
	.Shutdown         = shutdown,
	.Reboot           = reboot,
	.Sleep            = Sleep,
	.SetText          = setText,
	.SetLed           = setLed,
	.SetIcon          = setIcon,
	.SetBrightness    = setBrightness,
	.GetWakeupReason  = NULL,  // getWakeupReason,
	.SetLight         = setLight,
	.SetLedBrightness = NULL,
	.GetVersion       = NULL,  // getVersion,
	.SetRF            = setRFModulator,
	.SetFan           = setFan,
	.SetDisplayTime   = NULL,
	.SetTimeMode      = NULL,
#if defined MODEL_SPECIFIC
	.ModelSpecific    = modelSpecific,
#endif
	.Exit             = Exit
};

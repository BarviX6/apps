/*
 * Ufs912.c
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
 */

/******************** includes ************************ */
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
#include "Ufs912.h"

static int setText(Context_t *context, char *theText);
static int Clear(Context_t *context);
static int setIcon(Context_t *context, int which, int on);

/******************** constants ************************ */

#define cVFD_DEVICE "/dev/vfd"
#define cEVENT_DEVICE "/dev/input/event0"

#define cMAXCharsUFS912 16

typedef struct
{
	char *arg;
	char *arg_long;
	char *arg_description;
} tArgs;

tArgs vKArgs[] =
{
	{ "-e", "  --setTimer           ", "Args: [time date]  Format: HH:MM:SS dd-mm-YYYY" },
	{ "", "                         ", "      No arg: Set the most recent timer from e2 or neutrino" },
	{ "", "                         ", "      to the frontcontroller and shutdown" },
	{ "", "                         ", "      Arg time date: Set frontcontroller wake up time to" },
	{ "", "                         ", "      time, shutdown, and wake up at given time" },
	{ "-d", "  --shutdown           ", "Args: None or [time date]  Format: HH:MM:SS dd-mm-YYYY" },
	{ "", "                         ", "      No arg: Shut down immediately" },
	{ "", "                         ", "      Arg time date: Shut down at given time/date" },
	{ "-r", "  --reboot             ", "Args: None" },
	{ "", "                         ", "      No arg: Reboot immediately" },
	{ "", "                         ", "      Arg time date: Reboot at given time/date" },
	{ "-g", "  --getTime            ", "Args: None        Display currently set frontprocessor time" },
//	{ "-gs", " --getTimeAndSet      ", "Args: None" },
//	{ "", "                         ", "      Set system time to current frontprocessor time" },
//	{ "", "                         ", "      WARNING: system date will be 01-01-1970!" },
//	{ "-gt", " --getWTime           ", "Args: None        Get the current frontcontroller wake up time" },
//	{ "-st", " --setWakeTime        ", "Args: time date   Format: HH:MM:SS dd-mm-YYYY" },
//	{ "", "                         ", "      Set the frontcontroller wake up time" },
	{ "-s", "  --setTime            ", "Args: time date   Format: HH:MM:SS dd-mm-YYYY" },
	{ "", "                         ", "      Set the frontprocessor time" },
//	{ "-sst", "--setSystemTime      ", "Args: None        Set front processor time to system time" },
	{ "-p", "  --sleep              ", "Args: time date   Format: HH:MM:SS dd-mm-YYYY" },
	{ "", "                         ", "      Reboot receiver via fp at given time" },
	{ "-t", "  --settext            ", "Args: text        Set text to frontpanel" },
	{ "-l", "  --setLed             ", "Args: LED# 1|0    Set an LED on or off" },
	{ "-led", "--setLedBrightness   ", "Args: int         int=brightness (0..255)" },
	{ "-i", "  --setIcon            ", "Args: icon# 1|0   Set an icon on or off" },
	{ "-b", "  --setBrightness      ", "Arg : 0..7        Set display brightness" },
	{ "-w", "  --getWakeupReason    ", "Args: None        Get the wake up reason" },
	{ "-L", "  --setLight           ", "Arg : 0|1         Set display on/off" },
	{ "-c", "  --clear              ", "Args: None        Clear display, all icons and LEDs off" },
	{ "-v", "  --version            ", "Args: None        Get version info from frontprocessor" },
//	{ "-tm", " --time_mode          ", "Args: 0/1         Set time mode" },
#if defined MODEL_SPECIFIC
//	{ "-ms", " --model_specific     ", "Args: int1 [int2] [int3] ... [int16]   (note: input in hex)" },
//	{ "", "                         ", "                  Model specific test function" },
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
} tUFS912Private;

/* ******************* helper/misc functions ****************** */

static void setMode(int fd)
{
	struct micom_ioctl_data micom;
	micom.u.mode.compat = 1;
	if (ioctl(fd, VFDSETMODE, &micom) < 0)
	{
		perror("setMode: ");
	}
}

/* calculate the time value which we can pass to
 * the micom fp. its a mjd time (mjd=modified
 * julian date). mjd is relativ to gmt so theGMTTime
 * must be in GMT/UTC.
 */
static void setMicomTime(time_t theGMTTime, char *destString)
{
	/* from u-boot micom */
	struct tm *now_tm;

	now_tm = gmtime(&theGMTTime);
	//printf("Set Time (UTC): %02d:%02d:%02d %02d-%02d-%04d\n",
	// now_tm->tm_hour, now_tm->tm_min, now_tm->tm_sec, now_tm->tm_mday, now_tm->tm_mon+1, now_tm->tm_year+1900);
	double mjd = modJulianDate(now_tm);
	int mjd_int = mjd;
	destString[0] = (mjd_int >> 8);
	destString[1] = (mjd_int & 0xff);
	destString[2] = now_tm->tm_hour;
	destString[3] = now_tm->tm_min;
	destString[4] = now_tm->tm_sec;
}

static unsigned long getMicomTime(char *micomTimeString)
{
	unsigned int    mjd     = ((micomTimeString[1] & 0xFF) * 256) + (micomTimeString[2] & 0xFF);
	unsigned long   epoch   = ((mjd - 40587) * 86400);
	unsigned int    hour    = micomTimeString[3] & 0xFF;
	unsigned int    min     = micomTimeString[4] & 0xFF;
	unsigned int    sec     = micomTimeString[5] & 0xFF;
	epoch += (hour * 3600 + min * 60 + sec);
	//printf( "MJD = %d epoch = %ld, time = %02d:%02d:%02d\n", mjd,
	//  epoch, hour, min, sec );
	return epoch;
}

/* ******************* driver functions ****************** */

static int init(Context_t *context)
{
	tUFS912Private *private = malloc(sizeof(tUFS912Private));
	int vFd;
//	printf("%s\n", __func__);
	vFd = open(cVFD_DEVICE, O_RDWR);
	if (vFd < 0)
	{
		fprintf(stderr, "Cannot open %s\n", cVFD_DEVICE);
		perror("");
	}
	((Model_t *)context->m)->private = private;
	memset(private, 0, sizeof(tUFS912Private));
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
		if (vKArgs[i].arg == NULL)
		{
			break;
		}
		if ((cmd_name == NULL) || (strcmp(cmd_name, vKArgs[i].arg) == 0) || (strstr(vKArgs[i].arg_long, cmd_name) != NULL))
		{
			fprintf(stderr, "%s   %s   %s\n", vKArgs[i].arg, vKArgs[i].arg_long, vKArgs[i].arg_description);
		}
	}
	return 0;
}

static int setTime(Context_t *context, time_t *theGMTTime)
{  // -s command
	struct micom_ioctl_data vData;

	setMicomTime(*theGMTTime, vData.u.time.time);
	fprintf(stderr, "Setting Current FP Time to %d (MJD) %02d:%02d:%02d\n",
			vData.u.standby.time[0] & 0xff, ((vData.u.standby.time[1] & 0xff) * 256+ vData.u.standby.time[2]) & 0xff,
			vData.u.standby.time[3] & 0xff, vData.u.standby.time[4] & 0xff);
	if (ioctl(context->fd, VFDSETTIME, &vData) < 0)
	{
		perror("settime: ");
		return -1;
	}
	return 0;
}

static int getTime(Context_t *context, time_t *theGMTTime)
{  // -g command
	char fp_time[8];

	/* front controller time */
	if (ioctl(context->fd, VFDGETTIME, &fp_time) < 0)
	{
		perror("gettime: ");
		return -1;
	}
	/* if we get the fp time */
	if (fp_time[0] != '\0')
	{
		/* current front controller time */
		*theGMTTime = (time_t) getMicomTime(fp_time);
//		fprintf(stderr, "Current Fp Time:     %02d:%02d:%02d MJD=%d (UTC)\n",
//			fp_time[3], fp_time[4], fp_time[5], (fp_time[1] * 256) + fp_time[2]);
	}
	else
	{
		fprintf(stderr, "error reading time from fp\n");
		*theGMTTime = 0;
	}
	return 0;
}

static int setTimer(Context_t *context, time_t *theGMTTime)
{  // -e command
	struct micom_ioctl_data vData;
	time_t curTime    = 0;
	time_t curTimeFp  = 0;
	time_t wakeupTime = 0;
	struct tm *ts;
	struct tm *tsFp;
	struct tm *tsWakeupTime;

//	printf("%s ->\n", __func__);
	// Get current Frontpanel time
	getTime(context, &curTimeFp);
	tsFp = gmtime(&curTimeFp);
	fprintf(stderr, "Current Fp Time   : %02d:%02d:%02d %02d-%02d-%04d (UTC)\n",
			tsFp->tm_hour, tsFp->tm_min, tsFp->tm_sec, tsFp->tm_mday, tsFp->tm_mon + 1, tsFp->tm_year + 1900);
	// Get current Linux time
	time(&curTime);
	ts = gmtime(&curTime);
	fprintf(stderr, "Current Linux Time: %02d:%02d:%02d %02d-%02d-%04d (UTC), offset = %d\n",
			ts->tm_hour, ts->tm_min, ts->tm_sec, ts->tm_mday, ts->tm_mon + 1, ts->tm_year + 1900, 0);
	// Set current Linux time as new current Frontpanel time
	setTime(context, &curTime);
	if (theGMTTime == NULL)
	{
		wakeupTime = read_timers_utc(curTime);
	}
	else
	{
		wakeupTime = *theGMTTime;
	}
	if ((wakeupTime <= 0) || (wakeupTime == LONG_MAX))
	{
		/* clear timer */
		vData.u.standby.time[0] = '\0';
	}
	else
	{
		// Print wakeup time
		tsWakeupTime = gmtime(&wakeupTime);
		fprintf(stderr, "Planned Wakeup Time: %02d:%02d:%02d %02d-%02d-%04d (UTC)\n",
				tsWakeupTime->tm_hour, tsWakeupTime->tm_min, tsWakeupTime->tm_sec, tsWakeupTime->tm_mday, tsWakeupTime->tm_mon + 1, tsWakeupTime->tm_year + 1900);
		setMicomTime(wakeupTime, vData.u.standby.time);
		fprintf(stderr, "Setting Planned Fp Wakeup Time to = %02X%02X %d %d %d (mtime)\n",
			vData.u.standby.time[0], vData.u.standby.time[1], vData.u.standby.time[2], 
			vData.u.standby.time[3], vData.u.standby.time[4]);
	}
	fprintf(stderr, "Entering Deep Standby. ... good bye ...\n");
	fflush(stdout);
	fflush(stderr);
	sleep(2);
	if (ioctl(context->fd, VFDSTANDBY, &vData) < 0)
	{
		perror("standby: ");
		return -1;
	}
	return 0;
}

static int getWTime(Context_t *context, time_t *theGMTTime)
{  // -gw command
	fprintf(stderr, "%s: not implemented\n", __func__);
	return -1;
}

static int shutdown(Context_t *context, time_t *shutdownTimeGMT)
{  // -d command
	time_t curTime;

	/* shutdown immediately */
	if (*shutdownTimeGMT == -1)
	{
		return (setTimer(context, NULL));
	}
	while (1)
	{
		time(&curTime);
		/*printf("curTime = %d, shutdown %d\n", curTime, *shutdownTimeGMT);*/
		if (curTime >= *shutdownTimeGMT)
		{
			/* set most recent timer and bye bye */
			return (setTimer(context, NULL));
		}
		usleep(100000);
	}
	return -1;
}

static int reboot(Context_t *context, time_t *rebootTimeGMT)
{  // -r command
	time_t curTime;
	struct micom_ioctl_data vData;

	while (1)
	{
		time(&curTime);
		if (curTime >= *rebootTimeGMT)
		{
			if (ioctl(context->fd, VFDREBOOT, &vData) < 0)
			{
				perror("reboot: ");
				return -1;
			}
		}
		usleep(100000);
	}
	return 0;
}

static int Sleep(Context_t *context, time_t *wakeUpGMT)
{  // -p command
	time_t curTime;
	int sleep = 1;
	int vFd;
	fd_set rfds;
	struct timeval tv;
	int retval, i, rd;
	struct tm *ts;
	char output[cMAXCharsUFS912 + 1];
	struct input_event ev[64];
	tUFS912Private *private = (tUFS912Private *)((Model_t *)context->m)->private;

	vFd = open(cEVENT_DEVICE, O_RDWR);
	if (vFd < 0)
	{
		fprintf(stderr, "cannot open %s\n", cEVENT_DEVICE);
		perror("");
		return -1;
	}
	printf("%s 1\n", __func__);
	while (sleep)
	{
		time(&curTime);
		ts = localtime(&curTime);
		if (curTime >= *wakeUpGMT)
		{
			sleep = 0;
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
							sleep = 0;
						}
					}
				}
			}
		}
		if (private->display)
		{
			strftime(output, cMAXCharsUFS912 + 1, private->timeFormat, ts);
			setText(context, output);
		}
	}
	return 0;
}

static int setText(Context_t *context, char *theText)
{  // -t command
	char vHelp[128];

	strncpy(vHelp, theText, cMAXCharsUFS912);
	vHelp[cMAXCharsUFS912] = '\0';
	write(context->fd, vHelp, strlen(vHelp));
	return 0;
}

static int setLed(Context_t *context, int which, int on)
{  // -l command
	struct micom_ioctl_data vData;

	vData.u.led.led_nr = which;
	vData.u.led.on = on;
	setMode(context->fd);
	if (ioctl(context->fd, VFDSETLED, &vData) < 0)
	{
		perror("setLed: ");
		return -1;
	}
	return 0;
}

static int setIcon(Context_t *context, int which, int on)
{  // -i command
	struct micom_ioctl_data vData;

	vData.u.icon.icon_nr = which;
	vData.u.icon.on = on;
	setMode(context->fd);
	if (ioctl(context->fd, VFDICONDISPLAYONOFF, &vData) < 0)
	{
		perror("setIcon: ");
		return -1;
	}
	return 0;
}

static int setBrightness(Context_t *context, int brightness)
{  // -b command
	struct micom_ioctl_data vData;

	if (brightness < 0 || brightness > 7)
	{
		return -1;
	}
	vData.u.brightness.level = brightness;
	setMode(context->fd);
//	printf("%d\n", context->fd);
	if (ioctl(context->fd, VFDBRIGHTNESS, &vData) < 0)
	{
		perror("setBrightness: ");
		return -1;
	}
	return 0;
}

static int setLight(Context_t *context, int on)
{  // -L command
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

/* 0xc1 = rcu (from standby)
 * 0xc2 = front (from standby)
 * 0xc3 = time (timer)
 * 0xc4 = ac (power on)
 */
static int getWakeupReason(Context_t *context, eWakeupReason *reason)
{
	unsigned char mode[8];
	/* front controller time */
	if (ioctl(context->fd, VFDGETWAKEUPMODE, &mode) < 0)
	{
		perror("getWakeupReason: ");
		return -1;
	}
	/* if we get the reason */
	if (mode[0] != '\0')
	{
		switch (mode[1])
		{
			case 0xc4:  // power on
			{
				*reason = 1;
				break;
			}
			case 0xc1:  // from deep standby, remote
			case 0xc2:  // from deep standby, front panel
			{
				*reason = 2;
				break;
			}
			case 0xc3:  // timer
			{
				*reason = 3;
				break;
			}
			default:
			{
				*reason = 0;
				break;
			}
		}
	}
	else
	{
		fprintf(stderr, "Error reading wakeupmode from fp\n");
		*reason = 0;
	}
	return 0;
}

static int getVersion(Context_t *context, int *version)
{  // -v command
	char strVersion[8];
	/* front controller version */
	if (ioctl(context->fd, VFDGETVERSION, &strVersion) < 0)
	{
		perror("getVersion: ");
		return -1;
	}
	/* if we get the fp time */
	if (strVersion[0] != '\0')
	{
		*version = strVersion[1] * 10 | strVersion[2];
	}
	else
	{
		fprintf(stderr, "Error reading version info from fp\n");
		*version = 0;
	}
	return 0;
}

static int Clear(Context_t *context)
{  // -c command
	char string[17];
	struct micom_ioctl_data vData;

	memset(string, 0x20, sizeof(string));
	string[cMAXCharsUFS912] = '\0';
	/* printf("%s, %d\n", vHelp, strlen(vHelp));*/
	write(context->fd, string, strlen(string));

	vData.u.icon.icon_nr = 17;
	vData.u.icon.on = 0;
	setMode(context->fd);
	if (ioctl(context->fd, VFDICONDISPLAYONOFF, &vData) < 0)
	{
		perror("Clear: ");
		return -1;
	}
	return 0;
}

static int setLedBrightness(Context_t *context, int brightness)
{  // -led command
	struct micom_ioctl_data vData;

	if (brightness < 0 || brightness > 0xff)
	{
		return -1;
	}
	vData.u.brightness.level = brightness;
	setMode(context->fd);
	if (ioctl(context->fd, VFDLEDBRIGHTNESS, &vData) < 0)
	{
		perror("setLedBrightness: ");
		return -1;
	}
	return 0;
}

static int Exit(Context_t *context)
{
	tUFS912Private *private = (tUFS912Private *)((Model_t *)context->m)->private;
	if (context->fd > 0)
	{
		close(context->fd);
	}
	free(private);
	return 1;
}

Model_t UFS912_model =
{
	.Name             = "Kathrein UFS912 frontpanel control utility",
	.Type             = Ufs912,
	.Init             = init,
	.Clear            = Clear,
	.Usage            = usage,
	.SetTime          = setTime,
	.GetTime          = getTime,
	.SetTimer         = setTimer,
	.GetWTime         = getWTime,
	.SetWTime         = NULL,
	.Shutdown         = shutdown,
	.Reboot           = reboot,
	.Sleep            = Sleep,
	.SetText          = setText,
	.SetLed           = setLed,
	.SetIcon          = setIcon,
	.SetBrightness    = setBrightness,
	.GetWakeupReason  = getWakeupReason,
	.SetLight         = setLight,
	.SetLedBrightness = setLedBrightness,
	.GetVersion       = getVersion,
	.SetRF            = NULL,
	.SetFan           = NULL,
	.SetDisplayTime   = NULL,
	.SetTimeMode      = NULL,
#if defined MODEL_SPECIFIC
	.ModelSpecific    = NULL,
#endif
	.Exit             = Exit
};


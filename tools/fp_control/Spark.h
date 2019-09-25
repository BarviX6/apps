#ifndef __Spark__
#define __Spark__

#define VFDGETVERSION     0xc0425af7
#define VFDSETTIME2       0xc0425afd
#define VFDGETWAKEUPTIME  0xc0425b03
#define VFDSETDISPLAYTIME 0xc0425b04 // added by Audioniek (Cuberevo uses 0xc0425b02)
#define VFDGETDISPLAYTIME 0xc0425b05 /* Spark specific */

/* this setups the mode temporarily (for one ioctl)
 * to the desired mode. currently the "normal" mode
 * is the compatible vfd mode
 */
struct set_mode_s
{
	int compat; /* 0 = compatibility mode to vfd driver; 1 = nuvoton mode */
};

struct set_brightness_s
{
	int level;
};

struct set_icon_s
{
	int icon_nr;
	int on;
};

struct set_led_s
{
	int led_nr;
	int on;
};

struct set_light_s
{
	int onoff;
};

struct set_display_time_s
{
	int on;
};

/* time must be given as follows:
 * time[0] & time[1] = mjd ???
 * time[2] = hour
 * time[3] = min
 * time[4] = sec
 */
struct set_standby_s
{
	char time[5];
};

struct set_time_s
{
	char time[5];
};

struct get_version_s
{
	unsigned char CpuType;
	unsigned char DisplayInfo;
	unsigned char scankeyNum;
	unsigned char swMajorVersion;
	unsigned char swSubVersion;
};

/* YYMMDDhhmmss */
struct get_wakeuptime
{
	char time[12];
};

struct set_modelspecific_s
{
	long one;
	long two;
};

struct get_modelspecific_s
{
	long key_nr;
	long key_data;
};

struct aotom_ioctl_data
{
	union
	{
		struct set_icon_s icon;
		struct set_led_s led;
		struct set_light_s light;
		struct set_brightness_s brightness;
		struct set_display_time_s display_time;
		struct set_mode_s mode;
		struct set_standby_s standby;
		struct set_time_s time;
		struct get_version_s versionp;
		struct get_wakeuptime wakeup_time;
		struct get_modelspecific_s getmodelspecific;
		struct set_modelspecific_s setmodelspecific;
	} u;
};

#endif

#ifndef __vitamin_hd5000__
#define __vitamin_hd5000__

#if 0 // moved to global.h
/* ioctl numbers ->hacky */
#define VFDBRIGHTNESS        0xc0425a03
#define VFDDRIVERINIT        0xc0425a08
#define VFDICONDISPLAYONOFF  0xc0425a0a
#define VFDDISPLAYWRITEONOFF 0xc0425a05
#define VFDDISPLAYCHARS      0xc0425a00

#define VFDGETVERSION        0xc0425af7
#define VFDLEDBRIGHTNESS     0xc0425af8
#define VFDGETWAKEUPMODE     0xc0425af9
#define VFDGETTIME           0xc0425afa
#define VFDSETTIME           0xc0425afb
#define VFDSTANDBY           0xc0425afc
#define VFDREBOOT            0xc0425afd

#define VFDSETLED            0xc0425afe // Blue LED (power)
#define VFDSETMODE           0xc0425aff

#define VFDSETREDLED         0xc0425af6
#define VFDSETGREENLED       0xc0425af5
#define VFDDEEPSTANDBY       0xc0425a81
#endif

#define VFDSETGREENLED       0xc0425af5  /* Vitamin/micom specific */
#define VFDSETREDLED         0xc0425af6  /* Vitamin/micom specific */

struct set_test_s
{
	unsigned char data[14];
};

struct set_brightness_s
{
	int level;
};

struct set_led_s
{
//	int led_nr;
	int on;
};

struct set_icon_s
{
	int icon_nr;
	int on;
};

struct set_light_s
{
	int onoff;
};

/* YMDhms */
struct get_time_s
{
	char time[6];
};

/* YYMMDDhhmm */
struct set_standby_s
{
	char time[10];
};

/* YMDhms */
struct set_time_s
{
	char time[6];
};

#if 0
struct get_wakeupstatus_s
{
	char status;
};
#endif

/* time must be given as follows:
 * time[0] & time[1] = mjd ???
 * time[2] = hour
 * time[3] = min
 * time[4] = sec
 */
//struct set_standby_s
//{
//	char time[5];
//};

//struct set_time_s
//{
//	char time[5];
//};

#if 0
struct get_version_s
{
	int version;
};
#endif

/* This will set the mode temporarily (for one ioctl)
 * to the desired mode. Currently the "normal" mode
 * is the compatible vfd mode
 */
struct set_mode_s
{
	int compat; /* 0 = compatibility mode to vfd driver; 1 = micom mode */
};

#if defined MODEL_SPECIFIC
struct modelspecific_s
{
	char data[19]; //the bytes to send, and returned
};
#endif

struct micom_ioctl_data
{
	union
	{
		struct set_test_s test;
		struct set_icon_s icon;
		struct set_led_s led;
		struct set_brightness_s brightness;
		struct set_light_s light;
		struct set_mode_s mode;
		struct set_standby_s standby;
		struct set_time_s time;
//		struct get_time_s get_time;
//		struct get_wakeupstatus_s status;
//		struct get_time_s get_wakeup_time;
//		struct set_standby_s wakeup_time;
//		struct get_version_s version;
#if defined MODEL_SPECIFIC
		struct modelspecific_s modelspecific;
#endif
	} u;
};
#endif  // vitamin_hd5000
// vim:ts=4

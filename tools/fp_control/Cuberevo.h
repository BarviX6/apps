#ifndef __cuberevo__
#define __cuberevo__

#if 0 // moved to global.h
/* ioctl numbers ->hacky */
#define VFDBRIGHTNESS         0xc0425a03
#define VFDPWRLED             0xc0425a04 // deprecated, use VFDSETLED (0xc0425afe) instead.
#define VFDDRIVERINIT         0xc0425a08
#define VFDICONDISPLAYONOFF   0xc0425a0a
#define VFDDISPLAYWRITEONOFF  0xc0425a05
#define VFDDISPLAYCHARS       0xc0425a00

#define VFDCLEARICONS         0xc0425af6
#define VFDSETRF              0xc0425af7
#define VFDSETFAN             0xc0425af8
#define VFDGETWAKEUPMODE      0xc0425af9
#define VFDGETTIME            0xc0425afa
#define VFDSETTIME            0xc0425afb
#define VFDSTANDBY            0xc0425afc
#define VFDREBOOT             0xc0425afd
#define VFDSETLED             0xc0425afe
#define VFDSETMODE            0xc0425aff

#define VFDGETWAKEUPTIME      0xc0425b00
#define VFDGETVERSION_1       0xc0425b01
#define VFDSETDISPLAYTIME     0xc0425b02
#define VFDSETTIMEMODE        0xc0425b03
#endif

#define VFDSETRF_CUB          0xc0425af7 /* Cuberevo/micom specific */
#define VFDSETFAN             0xc0425af8 /* Cuberevo/micom specific */
#define VFDGETWAKEUPTIME_CUB  0xc0425b00 /* Cuberevo/micom specific */
#define VFDGETVERSION_CUB     0xc0425b01 /* Cuberevo/micom specific */
#define VFDSETDISPLAYTIME_CUB 0xc0425b02 /* Cuberevo/micom specific */
#define VFDSETTIMEMODE        0xc0425b03 /* Cuberevo/micom specific */
#define VFDSETWAKEUPTIME_CUB  0xc0425b04 /* Cuberevo/micom specific */
#define VFDLEDBRIGHTNESS_CUB  0xc0425b05 /* Cuberevo/micom specific */

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
	int state; 	/* state: 0 = off, 1 = on, 2 = slow, 3 = fast */
};

struct set_fan_s
{
	int on;
};

struct set_rf_s
{
	int on;
};

struct set_display_time_s
{
	int on;
};

/* YYMMDDhhmm */
struct time_10_s
{
	char time[10];
};

/* YYMMDDhhmmss */
struct time_12_s
{
	char time[12];
};

/* this sets the mode temporarily (for one ioctl)
 * to the desired mode. currently the "normal" mode
 * is the compatible vfd mode
 */
struct set_mode_s
{
	int compat; /* 0 = compatibility mode to vfd driver; 1 = micom mode */
};

struct set_timemode_s
{
	int twentyFour;
};

struct get_wakeupstatus_s
{
	char status;
};

/* 0 = 12dot 12seg, 1 = 13grid, 2 = 12 dot 14seg, 3 = 7seg */
struct get_version_s
{
	int version;
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
		struct set_icon_s icon;
		struct set_led_s led;
		struct set_fan_s fan;
		struct set_rf_s rf;
		struct set_brightness_s brightness;
		struct set_mode_s mode;
		struct time_10_s wakeup_time;
		struct time_12_s time;
		struct get_wakeupstatus_s status;
		struct set_display_time_s display_time;
		struct get_version_s version;
		struct set_timemode_s timemode;
#if defined MODEL_SPECIFIC
		struct modelspecific_s modelspecific;
#endif
	} u;
};
#endif
// vim:ts=4

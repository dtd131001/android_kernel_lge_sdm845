/* touch_core.h
 *
 * Copyright (C) 2015 LGE.
 *
 * Author: hoyeon.jang@lge.com
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef LGE_TOUCH_CORE_H
#define LGE_TOUCH_CORE_H

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#if defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#elif defined(CONFIG_DRM) && defined(CONFIG_FB)
#include <linux/msm_drm_notify.h>
#elif defined(CONFIG_FB)
#include <linux/fb.h>
#endif
#include <linux/notifier.h>
#include <linux/atomic.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <touch_common.h>
#include <touch_hwif.h>
#include <linux/input/lge_touch_notify.h>
#include <linux/lge_panel_notify.h>

#if defined(CONFIG_SECURE_TOUCH)
#include <linux/completion.h>
#include <linux/atomic.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#endif

#define LGE_TOUCH_NAME			"lge_touch"
#define LGE_TOUCH_DRIVER_NAME		"lge_touch_driver"
#define MAX_FINGER			10
#define MAX_LPWG_CODE			128
#define EUPGRADE			140
#define EHWRESET			141
#define ESWRESET			142
#define EGLOBALRESET			143

enum TOUCH_DEBUG {
	_NONE                      = 0,
	BASE_INFO                 = (1U << 0),    /* 1 */
	TRACE                     = (1U << 1),    /* 2 */
	GET_DATA                  = (1U << 2),    /* 4 */
	ABS                       = (1U << 3),    /* 8 */
	BUTTON                    = (1U << 4),    /* 16*/
	FW_UPGRADE                = (1U << 5),    /* 32 */
	GHOST                     = (1U << 6),    /* 64 */
	IRQ_HANDLE                = (1U << 7),    /* 128 */
	POWER                     = (1U << 8),    /* 256 */
	JITTER                    = (1U << 9),    /* 512 */
	ACCURACY                  = (1U << 10),   /* 1024 */
	BOUNCING                  = (1U << 11),   /* 2048 */
	GRIP                      = (1U << 12),   /* 4096 */
	FILTER_RESULT             = (1U << 13),   /* 8192 */
	QUICKCOVER                = (1U << 12),   /* 4096 */
	LPWG                      = (1U << 14),   /* 16384 */
	NOISE                     = (1U << 15),   /* 32768 */
	LPWG_COORDINATES          = (1U << 16),   /* 65536 */
};

#define TOUCH_I(fmt, args...)					\
	pr_info("[Touch] "					\
			fmt, ##args)

#define TOUCH_E(fmt, args...)					\
	pr_err("[Touch E] [%s %d] "				\
			fmt, __func__, __LINE__, ##args)

extern u32 touch_debug_mask;
#define TOUCH_D(condition, fmt, args...)			\
	do {							\
		if (unlikely(touch_debug_mask & (condition)))	\
			pr_info("[Touch] " fmt, ##args);	\
	} while (0)

#define TOUCH_DEBUG_SHOW_FILE
#ifdef TOUCH_DEBUG_SHOW_FILE
#define __SHORT_FILE__ (strrchr(__FILE__, '/') + 1)
#define TOUCH_TRACE()	TOUCH_D(TRACE, "- %s(%s) %d\n",		\
		__func__, __SHORT_FILE__, __LINE__)
#else
#define TOUCH_TRACE()	TOUCH_D(TRACE, "- %s %d", __func__, __LINE__)
#endif

#define TOUCH_IRQ_NONE			0
#define TOUCH_IRQ_FINGER		(1 << 0)
#define TOUCH_IRQ_KNOCK			(1 << 1)
#define TOUCH_IRQ_PASSWD		(1 << 2)
#define TOUCH_IRQ_SWIPE_DOWN		(1 << 3)
#define TOUCH_IRQ_SWIPE_UP		(1 << 4)
#define TOUCH_IRQ_SWIPE_RIGHT		(1 << 5)
#define TOUCH_IRQ_SWIPE_LEFT		(1 << 6)
#define TOUCH_IRQ_AI_BUTTON		(1 << 9)
#define TOUCH_IRQ_AI_PICK		(1 << 10)
#define TOUCH_IRQ_SWIPE_LEFT2		(1 << 12)
#define TOUCH_IRQ_SWIPE_RIGHT2		(1 << 13)
#define TOUCH_IRQ_ERROR			(1 << 15)
#define TOUCH_IRQ_LPWG_LONGPRESS_DOWN		(1 << 16)
#define TOUCH_IRQ_LPWG_LONGPRESS_UP		(1 << 17)
#define TOUCH_IRQ_LPWG_SINGLE_WAKEUP		(1 << 18)
#define TOUCH_IRQ_PEN_WAKEUP		(1 << 19)
#define TOUCH_IRQ_PEN_WAKEUP_BTN	(1 << 20)
#define TOUCH_IRQ_PEN_DETECTION		(1 << 21)


enum {
	POWER_OFF = 0,
	POWER_SLEEP,
	POWER_WAKE,
	POWER_ON,
	POWER_SLEEP_STATUS,
	POWER_HW_RESET,
	POWER_SW_RESET,
};

enum {
	UEVENT_IDLE = 0,
	UEVENT_BUSY,
};

enum {
	PROX_NEAR = 0,
	PROX_FAR,
};

enum {
	HALL_FAR = 0,
	HALL_NEAR,
};

enum {
	DEV_PM_RESUME = 0,
	DEV_PM_SUSPEND,
	DEV_PM_SUSPEND_IRQ,
};

enum {
	FB_RESUME = 0,
	FB_SUSPEND,
};

enum {
	SP_DISCONNECT = 0,
	SP_CONNECT,
};

/* Deep Sleep or not */
enum {
	IC_NORMAL = 0,
	IC_DEEP_SLEEP,
};

/* TCI */
enum {
	ENABLE_CTRL = 0,
	TAP_COUNT_CTRL,
	MIN_INTERTAP_CTRL,
	MAX_INTERTAP_CTRL,
	TOUCH_SLOP_CTRL,
	TAP_DISTANCE_CTRL,
	INTERRUPT_DELAY_CTRL,
	ACTIVE_AREA_CTRL,
	ACTIVE_AREA_RESET_CTRL,
};

enum {
	LPWG_NONE = 0,
	LPWG_DOUBLE_TAP,
	LPWG_PASSWORD,
	LPWG_PASSWORD_ONLY,
};

enum {
	LPWG_SLEEP = 0,
	LPWG_WAKE,
	LPWG_KNOCK,
	LPWG_PASSWD,
	LPWG_PARTIAL,
	LPWG_NORMAL,
};

enum {
	LPWG_CMD_MODE = 0,
	LPWG_CMD_AREA,
};

enum {
	TCI_1 = 0,
	TCI_2,
	SWIPE,
};

enum {
	LPWG_ENABLE = 1,
	LPWG_LCD,
	LPWG_ACTIVE_AREA,
	LPWG_TAP_COUNT,
	LPWG_LENGTH_BETWEEN_TAP,
	LPWG_EARLY_SUSPEND,
	LPWG_SENSOR_STATUS,
	LPWG_DOUBLE_TAP_CHECK,
	LPWG_UPDATE_ALL,
	LPWG_READ,
	LPWG_REPLY,
};

enum {
	LOCKSCREEN_UNLOCK = 0,
	LOCKSCREEN_LOCK,
};

enum {
	IME_OFF = 0,
	IME_ON,
	IME_SWYPE,
};

enum {
	QUICKCOVER_OPEN = 0,
	QUICKCOVER_CLOSE,
};

enum {
	INCOMING_CALL_IDLE,
	INCOMING_CALL_RINGING,
	INCOMING_CALL_OFFHOOK,
	INCOMING_CALL_CDMA_RINGING,
	INCOMING_CALL_CDMA_OFFHOOK,
	INCOMING_CALL_LTE_RINGING,
	INCOMING_CALL_LTE_OFFHOOK,
};

enum {
	MFTS_NONE = 0,
	MFTS_FOLDER,
	MFTS_FLAT,
	MFTS_CURVED,
};

enum { /* Command lists */
	CMD_VERSION,
	CMD_ATCMD_VERSION,
};

enum {
	INTERRUPT_DISABLE = 0,
	INTERRUPT_ENABLE,
};

enum {
	CORE_NONE = 0,
	CORE_PROBE,
	CORE_CHARGER_LOGO,
	CORE_MFTS,
	CORE_UPGRADE,
	CORE_NORMAL,
};

enum {
	CONNECT_INVALID = 0,
	CONNECT_SDP,
	CONNECT_DCP,
	CONNECT_CDP,
	CONNECT_PROPRIETARY,
	CONNECT_FLOATED,
	CONNECT_HUB, /* SHOULD NOT change the value */
};

enum {
	EARJACK_NONE = 0,
	EARJACK_NORMAL,
	EARJACK_DEBUG,
};

enum {
	DEBUG_TOOL_DISABLE = 0,
	DEBUG_TOOL_ENABLE,
};

enum {
	DEBUG_OPTION_DISABLE = 0,
	DEBUG_OPTION_0 = 1,
	DEBUG_OPTION_1 = 2,
	DEBUG_OPTION_2 = 4,
	DEBUG_OPTION_3 = 8,
	DEBUG_OPTION_4 = 16,
	DEBUG_OPTION_5 = 32,
	DEBUG_OPTION_6 = 64,
	DEBUG_OPTION_7 = 128,
	DEBUG_OPTION_8 = 256,
	DEBUG_OPTION_9 = 512,
	DEBUG_OPTION_ALL = 1023,
};

enum {
	NORMAL_BOOT = 0,
	MINIOS_AAT,
	MINIOS_MFTS_FOLDER,
	MINIOS_MFTS_FLAT,
	MINIOS_MFTS_CURVED,
};

enum {
	TOUCH_UEVENT_KNOCK = 0,
	TOUCH_UEVENT_PASSWD,
	TOUCH_UEVENT_SWIPE_DOWN,
	TOUCH_UEVENT_SWIPE_UP,
	TOUCH_UEVENT_SWIPE_RIGHT,
	TOUCH_UEVENT_SWIPE_LEFT,
	TOUCH_UEVENT_SWIPE_RIGHT2,
	TOUCH_UEVENT_SWIPE_LEFT2,
	TOUCH_UEVENT_AI_BUTTON,
	TOUCH_UEVENT_AI_PICK,
	TOUCH_UEVENT_SIZE,
};

enum {
	KEY_GESTURE_SWIPE_DOWN = 247,
	KEY_GESTURE_SWIPE_UP = 248,
	KEY_GESTURE_SWIPE_LEFT = 249,
	KEY_GESTURE_SWIPE_RIGHT = 250,
	KEY_GESTURE_SWIPE_LEFT2 = 251,
	KEY_GESTURE_SWIPE_RIGHT2 = 252,
};

enum {
	APP_HOME = 0,
	APP_CONTACTS,
	APP_MENU,
	LPWG_SET_COORDS = 3,
	LPWG_SET_TAPS = 4,
	LPWG_ENABLE_TAP_LISTENER = 8,
	LPWG_MASTER = 9,
};

struct state_info {
	atomic_t core;
	atomic_t pm;
	atomic_t fb;
	atomic_t sleep;
	atomic_t uevent;
	atomic_t irq_enable;
	atomic_t connect; /* connection using USB port */
	atomic_t wireless; /* connection using wirelees_charger */
	atomic_t earjack; /* connection using earjack */
	atomic_t lockscreen;
	atomic_t ime;
	atomic_t quick_cover;
	atomic_t film;
	atomic_t incoming_call;
	atomic_t mfts;
	atomic_t sp_link;
	atomic_t debug_tool;
	atomic_t debug_option_mask;
	atomic_t onhand;
};

struct touch_driver {
	int (*probe)(struct device *dev);
	int (*remove)(struct device *dev);
	int (*shutdown)(struct device *dev);
	int (*suspend)(struct device *dev);
	int (*resume)(struct device *dev);
	int (*init)(struct device *dev);
	int (*irq_handler)(struct device *dev);
	int (*power)(struct device *dev, int power_mode);
	int (*upgrade)(struct device *dev);
	int (*lpwg)(struct device *dev,	u32 code, void *param);
	int (*notify)(struct device *dev, ulong event, void *data);
	int (*register_sysfs)(struct device *dev);
	int (*set)(struct device *dev, u32 cmd, void *input, void *output);
	int (*get)(struct device *dev, u32 cmd, void *input, void *output);
};

struct touch_device_caps {
	u32 max_x;
	u32 max_y;
	u32 max_pressure;
	u32 max_width_major;
	u32 max_width_minor;
	u32 max_orientation;
	u32 max_id;
	u32 hw_reset_delay;
	u32 sw_reset_delay;
};

struct touch_operation_role {
	bool use_lpwg;
	u32 use_lpwg_test;
	bool hide_coordinate;
	u32 mfts_lpwg;
	bool use_firmware;
	bool use_fw_upgrade;
	bool use_fw_recovery;
};

struct touch_quick_cover {
	u32 x1;
	u32 y1;
	u32 x2;
	u32 y2;
};

struct tci_info {
	u16 tap_count;
	u16 min_intertap;
	u16 max_intertap;
	u16 touch_slop;
	u16 tap_distance;
	u16 intr_delay;
};

struct active_area {
	u16 x1;
	u16 y1;
	u16 x2;
	u16 y2;
};

struct tci_ctrl {
	u32 mode;
	struct active_area area;
	u8 double_tap_check;
	struct tci_info info[2];
};

struct touch_pinctrl {
	struct pinctrl *ctrl;
	struct pinctrl_state *active;
	struct pinctrl_state *suspend;
};

struct touch_data {
	u16 id;
	u16 x;
	u16 y;
	u16 width_major;
	u16 width_minor;
	s16 orientation;
	u16 pressure;
	/* finger, palm, pen, glove, hover */
	u16 type;
};

struct point {
	int x;
	int y;
};

struct lpwg_info {
	u8 mode;
	u8 screen;
	u8 sensor;
	u8 qcover;
	u8 code_num;
	struct point area[2];
	struct point code[MAX_LPWG_CODE];
};

struct app_info {
	int app;
	char version[10];
	int icon_size;
	int touch_slop;
};

struct perf_test_info {
	bool enable;
	u32 delay;
	u16 pressure;
	u16 width;
	u16 click_x;
	u16 click_y;
	u16 v_drag_x;
	u16 v_drag_start_y;
	u16 v_drag_end_y;
	u16 h_drag_start_x;
	u16 h_drag_end_x;
	u16 h_drag_y;
};

struct touch_core_data {
	struct platform_device *pdev;

	u8 bus_type;
	int irq;
	unsigned long irqflags;

	struct device *dev;			/* client device : i2c or spi */
	struct input_dev *input;
	struct touch_driver *driver;
	struct kobject kobj;

	int reset_pin;
	int int_pin;
	int maker_id_pin;
	int vdd_pin;
	int vio_pin;

	void *vdd;
	void *vio;

	struct touch_device_caps caps;
	struct touch_operation_role role;
	struct state_info state;
	struct touch_quick_cover qcover;
	struct touch_pinctrl pinctrl;

	u32 intr_status;
	u16 new_mask;
	u16 old_mask;
	int tcount;
	struct touch_data tdata[MAX_FINGER];
	int is_cancel;
	struct lpwg_info lpwg;
	struct tci_ctrl tci;

	u8 def_fwcnt;
	const char *def_fwpath[4];
	char test_fwpath[256];
	const char *panel_spec;
	const char *panel_spec_mfts;
	const char *panel_spec_mfts_flat;
	const char *panel_spec_mfts_curved;
	u8 force_fwup;

	u8 *tx_buf;
	u8 *rx_buf;
	int buf_size;
	struct touch_xfer_msg *xfer;

	struct mutex lock;
	struct workqueue_struct *wq;
	struct delayed_work init_work;
	struct delayed_work upgrade_work;
	struct delayed_work notify_work;
	struct delayed_work fb_work;
	struct delayed_work panel_reset_work;

	struct notifier_block blocking_notif;
	struct notifier_block atomic_notif;
	struct notifier_block display_notif;
	struct notifier_block notif;
	unsigned long notify_event;
	int notify_data;
	struct atomic_notify_event notify_event_arr[ATOMIC_NOTIFY_EVENT_SIZE];
#if defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#elif defined(CONFIG_DRM) & defined(CONFIG_FB)
        struct notifier_block drm_notif;
#elif defined(CONFIG_FB)
	struct notifier_block fb_notif;
#endif
	void *touch_device_data;

#if defined(CONFIG_SECURE_TOUCH)
	atomic_t st_enabled;
	atomic_t st_pending_irqs;
	struct completion st_powerdown;
	struct completion st_irq_processed;
	bool st_initialized;
	struct clk *core_clk;
	struct clk *iface_clk;
	struct mutex touch_io_ctrl_mutex;
#endif

	/* for MTK */
	int vdd_id;
	int vdd_vol;
	int vio_id;
	int vio_vol;

	u32 tx_pa;
	u32 rx_pa;

	struct app_info app_data[3];
	struct perf_test_info perf_test;
};

#define PROPERTY_GPIO(np, string, target)				\
	(target = of_get_named_gpio_flags(np, string, 0, NULL))

#define PROPERTY_BOOL(np, string, target)				\
	do {								\
		u32 tmp_val = 0;					\
		if (of_property_read_u32(np, string, &tmp_val) < 0)	\
			target = 0;					\
		else							\
			target = (u8)tmp_val;				\
	} while (0)

#define PROPERTY_U32(np, string, target)				\
	do {								\
		u32 tmp_val = 0;					\
		if (of_property_read_u32(np, string, &tmp_val) < 0)	\
			target = -1;					\
		else							\
			target = tmp_val;				\
	} while (0)

#define PROPERTY_STRING_ARRAY(np, string, target, cnt)			\
	do {								\
		int i;						\
		cnt = of_property_count_strings(np, string);		\
		for (i = 0; i < cnt; i++) {				\
			of_property_read_string_index(np, string, i,	\
						      &target[i]);	\
		}							\
	} while (0)

#define PROPERTY_STRING(np, string, target)				\
	do {								\
		of_property_read_string(np, string, &target);		\
	} while (0)

struct touch_attribute {
	struct attribute attr;
	ssize_t (*show)(struct device *dev, char *buf);
	ssize_t (*store)(struct device *idev, const char *buf, size_t count);
};

#define TOUCH_ATTR(_name, _show, _store)		\
			struct touch_attribute touch_attr_##_name	\
			= __ATTR(_name, S_IRUGO | S_IWUSR, _show, _store)

static inline void touch_set_device(struct touch_core_data *ts, void *data)
{
	TOUCH_I("%s, data:%p\n", __func__, data);
	ts->touch_device_data = data;
}

static inline void *touch_get_device(struct touch_core_data *ts)
{
	return ts->touch_device_data;
}

static inline struct touch_core_data *to_touch_core(struct device *dev)
{
	return dev ? (struct touch_core_data *)dev_get_drvdata(dev) : NULL;
}

int touch_lpwg(struct touch_core_data *ts, u32 code, int *value);

extern irqreturn_t touch_irq_handler(int irq, void *dev_id);
extern irqreturn_t touch_irq_thread(int irq, void *dev_id);
extern int touch_get_dts(struct touch_core_data *ts);
extern int touch_get_platform_data(struct touch_core_data *ts);
extern int touch_init_sysfs(struct touch_core_data *ts);
extern void touch_report_all_event(struct touch_core_data *ts);
extern void touch_suspend(struct device *dev);
extern void touch_resume(struct device *dev);

#if defined(CONFIG_SECURE_TOUCH)
extern void secure_touch_notify(struct touch_core_data *ts);
extern void secure_touch_init(struct touch_core_data *ts);
extern void secure_touch_stop(struct touch_core_data *ts, bool blocking);
#endif

enum touch_device_type {
	TYPE_DUMMY,
	TYPE_SW49408,
	TYPE_SW49409,
	TYPE_SW49410,
	TYPE_SW43402,
	TYPE_SW43406,
	TYPE_SW43408,
	TYPE_FTM4,
	TYPE_S3707,
	TYPE_MAX,
};

extern enum touch_device_type touch_get_device_type(void);
#endif /* LGE_TOUCH_CORE_H */

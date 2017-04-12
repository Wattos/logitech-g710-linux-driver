/*
 *  Logitech G710+ Keyboard Input Driver
 *
 *  Driver generates additional key events for the keys M1-MR, G1-G6
 *  and supports setting the backlight levels of the keyboard
 *
 *  Copyright (c) 2013 Filip Wieladke <Wattos@gmail.com>
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/hid.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/version.h>

#include "hid-ids.h"
#include "usbhid/usbhid.h"

#define USB_DEVICE_ID_LOGITECH_KEYBOARD_G710_PLUS 0xc24d

// 20 seeconds timeout
#define WAIT_TIME_OUT 20000

#define LOGITECH_KEY_MAP_SIZE 16

static const u8 g710_plus_key_map[LOGITECH_KEY_MAP_SIZE] = {
    0, /* unused */
    0, /* unused */
    0, /* unused */
    0, /* unused */
    KEY_PROG1, /* M1 */
    KEY_PROG2, /* M2 */
    KEY_PROG3, /* M3 */
    KEY_PROG4, /* MR */
    KEY_F14, /* G1 */
    KEY_F15, /* G2 */
    KEY_F16, /* G3 */
    KEY_F17, /* G4 */
    KEY_F18, /* G5 */
    KEY_F19, /* G6 */
    0, /* unused */
    0, /* unused */
};

/* Convenience macros */
#define lg_g710_plus_get_data(hdev) \
        ((struct lg_g710_plus_data *)(hid_get_drvdata(hdev)))

#define BIT_AT(var,pos) ((var) & (1<<(pos)))

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0)
enum lg_g710_plus_leds{
	G710_LED_M1 = 0,
	G710_LED_M2 = 1,
	G710_LED_M3 = 2,
	G710_LED_MR = 3,
	G710_LED_KEYS = 4,
	G710_LED_WASD = 5,
	G710_LED_MAX = 6
};
#endif

struct lg_g710_plus_data {
    struct hid_report *g_mr_buttons_support_report; /* Needs to be written to enable G1-G6 and M1-MR keys */
    struct hid_report *mr_buttons_led_report; /* Controls the backlight of M1-MR buttons */
    struct hid_report *other_buttons_led_report; /* Controls the backlight of other buttons */
//    struct hid_report *gamemode_report; /*  */

    u16 macro_button_state; /* Holds the last state of the G1-G6, M1-MR buttons. Required to know which buttons were pressed and which were released */
    struct hid_device *hdev;
    struct input_dev *input_dev;
    struct attribute_group attr_group;

    spinlock_t lock; /* lock for communication with user space */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0)
    struct g710_led_s {
		spinlock_t lock; /* lock */
        struct led_classdev cd;
        struct work_struct work;
    } *leds[G710_LED_MAX];
#endif
};


static ssize_t lg_g710_plus_show_led_mr_m1(struct device *device, struct device_attribute *attr, char *buf);
static ssize_t lg_g710_plus_store_led_mr_m1(struct device *device, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t lg_g710_plus_show_led_mr_m2(struct device *device, struct device_attribute *attr, char *buf);
static ssize_t lg_g710_plus_store_led_mr_m2(struct device *device, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t lg_g710_plus_show_led_mr_m3(struct device *device, struct device_attribute *attr, char *buf);
static ssize_t lg_g710_plus_store_led_mr_m3(struct device *device, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t lg_g710_plus_show_led_mr_mr(struct device *device, struct device_attribute *attr, char *buf);
static ssize_t lg_g710_plus_store_led_mr_mr(struct device *device, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t lg_g710_plus_show_led_wasd(struct device *device, struct device_attribute *attr, char *buf);
static ssize_t lg_g710_plus_store_led_wasd(struct device *device, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t lg_g710_plus_show_led_keys(struct device *device, struct device_attribute *attr, char *buf);
static ssize_t lg_g710_plus_store_led_keys(struct device *device, struct device_attribute *attr, const char *buf, size_t count);

//static ssize_t lg_g710_plus_get_game_mode(struct device *device, struct device_attribute *attr, char *buf);
//static ssize_t lg_g710_plus_set_game_mode(struct device *device, struct device_attribute *attr, const char *buf, size_t count);

static DEVICE_ATTR(led_mr_m1, 0660, lg_g710_plus_show_led_mr_m1, lg_g710_plus_store_led_mr_m1);
static DEVICE_ATTR(led_mr_m2, 0660, lg_g710_plus_show_led_mr_m2, lg_g710_plus_store_led_mr_m2);
static DEVICE_ATTR(led_mr_m3, 0660, lg_g710_plus_show_led_mr_m3, lg_g710_plus_store_led_mr_m3);
static DEVICE_ATTR(led_mr_mr, 0660, lg_g710_plus_show_led_mr_mr, lg_g710_plus_store_led_mr_mr);
static DEVICE_ATTR(led_wasd,  0660, lg_g710_plus_show_led_wasd,  lg_g710_plus_store_led_wasd);
static DEVICE_ATTR(led_keys,  0660, lg_g710_plus_show_led_keys,  lg_g710_plus_store_led_keys);
//static DEVICE_ATTR(game_mode, 0660, lg_g710_plus_get_game_mode,  lg_g710_plus_set_game_mode);

static struct attribute *lg_g710_plus_attrs[] = {
        &dev_attr_led_mr_m1.attr,
        &dev_attr_led_mr_m2.attr,
        &dev_attr_led_mr_m3.attr,
        &dev_attr_led_mr_mr.attr,
        &dev_attr_led_wasd.attr,
        &dev_attr_led_keys.attr,
//        &dev_attr_game_mode.attr,
        NULL,
};

//---------------------------------------------------------------------
//
//---------------------------------------------------------------------

static int lg_g710_plus_extra_key_event(struct hid_device *hdev, struct hid_report *report, u8 *data, int size) {
    u8 i;
    u16 keys_pressed;
    struct lg_g710_plus_data* g710_data = lg_g710_plus_get_data(hdev);
    if (g710_data == NULL || size < 3 || data[0] != 3) {
        return 1; /* cannot handle the event */
    }

    keys_pressed= data[1] << 8 | data[2];
    for (i = 0; i < LOGITECH_KEY_MAP_SIZE; i++) {
        if (g710_plus_key_map[i] != 0 && (BIT_AT(keys_pressed, i) != BIT_AT(g710_data->macro_button_state, i))) {
            input_report_key(g710_data->input_dev, g710_plus_key_map[i], BIT_AT(keys_pressed, i) != 0);
        }
    }
    input_sync(g710_data->input_dev);
    g710_data->macro_button_state= keys_pressed;
    return 1;
}

//---------------------------------------------------------------------
//
//---------------------------------------------------------------------
static int lg_g710_plus_control_key_event(struct hid_device *hdev, struct hid_report *report, u8 *data, int size) {

    struct lg_g710_plus_data* g710_data = lg_g710_plus_get_data(hdev);
    if (g710_data == NULL || size < 4 || data[0] != 4) {
        return 1; /* cannot handle the event */
    }
    //data->mr_buttons_led_report->field[0]->value[0]= (data[2] & 0xF) << 4;
    g710_data->other_buttons_led_report->field[0]->value[0]= data[2];
    g710_data->other_buttons_led_report->field[0]->value[1]= data[3];

#if LINUX_VERSION_CODE > KERNEL_VERSION(4,3,0)
	if(g710_data->leds[G710_LED_WASD]){
		spin_lock(&g710_data->leds[G710_LED_WASD]->lock);
		g710_data->leds[G710_LED_WASD]->cd.brightness = 4 - data[2];
		spin_unlock(&g710_data->leds[G710_LED_WASD]->lock);
	}
	if(g710_data->leds[G710_LED_KEYS]){
		spin_lock(&g710_data->leds[G710_LED_KEYS]->lock);
		g710_data->leds[G710_LED_KEYS]->cd.brightness = 4 - data[3];
		spin_unlock(&g710_data->leds[G710_LED_KEYS]->lock);
	}
#endif
	return 1;
}

//---------------------------------------------------------------------
//
//---------------------------------------------------------------------
static int lg_g710_plus_raw_event(struct hid_device *hdev, struct hid_report *report, u8 *data, int size)
{
    switch(report->id) {
        case 3: return lg_g710_plus_extra_key_event(hdev, report, data, size);
        case 4: return lg_g710_plus_control_key_event(hdev, report, data, size);
        default: return 0;
    }
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(4,3,0)
#define CONFIGURED_SUCCESS 0
static int lg_g710_plus_input_configured(struct hid_device *hdev,
                                        struct hid_input *hi) {
#else
#define CONFIGURED_SUCCESS
static void lg_g710_plus_input_configured(struct hid_device *hdev,
                                        struct hid_input *hi) {
#endif
    struct lg_g710_plus_data* data = lg_g710_plus_get_data(hdev);
    u8 i;
    struct list_head *feature_report_list = &hdev->report_enum[HID_FEATURE_REPORT].report_list;

    if (list_empty(feature_report_list)) {
        //bail on the keyboard device, we only want the aux key device.
        return CONFIGURED_SUCCESS;
    }

    if (data != NULL && data->input_dev == NULL) {
        data->input_dev= hi->input;
    }

    set_bit(EV_KEY, data->input_dev->evbit);
    memset(data->input_dev->keybit, 0, sizeof(data->input_dev->keybit));
    //add the synthetic keys
    for (i = 0; i < LOGITECH_KEY_MAP_SIZE; i++) {
        if (g710_plus_key_map[i] != 0) {
            set_bit(g710_plus_key_map[i], data->input_dev->keybit);
        }
    }
    //also, add the media keys back
    set_bit(KEY_PLAYPAUSE, data->input_dev->keybit);
    set_bit(KEY_STOPCD, data->input_dev->keybit);
    set_bit(KEY_PREVIOUSSONG, data->input_dev->keybit);
    set_bit(KEY_NEXTSONG, data->input_dev->keybit);
    set_bit(KEY_VOLUMEUP, data->input_dev->keybit);
    set_bit(KEY_VOLUMEDOWN, data->input_dev->keybit);
    set_bit(KEY_MUTE, data->input_dev->keybit);

    return CONFIGURED_SUCCESS;
}

//---------------------------------------------------------------------
//
//---------------------------------------------------------------------
enum req_type {
    REQTYPE_READ,
    REQTYPE_WRITE
};

static void hidhw_request(struct hid_device *hdev, struct hid_report *report, enum req_type reqtype) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    hid_hw_request(hdev, report, reqtype == REQTYPE_READ ? HID_REQ_GET_REPORT : HID_REQ_SET_REPORT);
#else
    usbhid_submit_report(hdev, report, reqtype == REQTYPE_READ ? USB_DIR_IN : USB_DIR_OUT);
#endif
}

//---------------------------------------------------------------------
//
//---------------------------------------------------------------------
static int lg_g710_plus_led_set(struct lg_g710_plus_data* data, enum lg_g710_plus_leds lednro, enum led_brightness brightness) {

	s32 *mask = NULL;
	u8 shift = 0;
	
    spin_lock(&data->lock);
	if( lednro >= G710_LED_M1 &&  lednro <= G710_LED_MR){
		mask = &data->mr_buttons_led_report->field[0]->value[0];
		shift = 4 + lednro;				
		if (brightness == LED_OFF) {
			*mask = (*mask) & ~(1 << shift);
		} else {
			*mask = (*mask) |  (1 << shift);
		}
		hidhw_request(data->hdev, data->mr_buttons_led_report, REQTYPE_WRITE);				
	} else if ( lednro == G710_LED_WASD) {
		mask = &data->other_buttons_led_report->field[0]->value[0];
		*mask = 4 - brightness;
		hidhw_request(data->hdev, data->other_buttons_led_report, REQTYPE_WRITE);
	} else if ( lednro == G710_LED_KEYS) {
		mask = &data->other_buttons_led_report->field[0]->value[1];
		*mask = 4 - brightness;
		hidhw_request(data->hdev, data->other_buttons_led_report, REQTYPE_WRITE);
	}
    spin_unlock(&data->lock);
	return 0;
}

static u8 lg_g710_plus_led_get(struct lg_g710_plus_data* data, enum lg_g710_plus_leds lednro) {
	u8 ret = 0;
    spin_lock(&data->lock);
	if( lednro >= G710_LED_M1 &&  lednro <= G710_LED_MR){
		ret = (data->mr_buttons_led_report->field[0]->value[0] >> lednro) & 0x1;
	} else if ( lednro == G710_LED_WASD) {
		ret = 4 - data->other_buttons_led_report->field[0]->value[0];
	} else if ( lednro == G710_LED_KEYS) {
		ret = 4 - data->other_buttons_led_report->field[0]->value[1];
	}
    spin_unlock(&data->lock);
	return ret;
}

//---------------------------------------------------------------------
//
//---------------------------------------------------------------------

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0)
static int brightness_set_sync(struct led_classdev *led_cdev, enum led_brightness brightness) {
	struct device *dev = led_cdev->dev->parent;
	struct hid_device *hdev = container_of(dev, struct hid_device, dev);
    struct lg_g710_plus_data* data = hid_get_drvdata(hdev);
	u32 ind;

	spin_lock(&data->lock);
	for(ind=0; ind < G710_LED_MAX; ++ind) {
		if (data->leds[ind] != NULL && led_cdev == &data->leds[ind]->cd) {
			break;
		}
	}
    spin_unlock(&data->lock);
	return lg_g710_plus_led_set(data, ind, brightness);
}

static void led_work(struct work_struct *work) {
    struct g710_led_s *led = container_of(work, struct g710_led_s, work);
    brightness_set_sync(&led->cd, led->cd.brightness);
}

static void brightness_set(struct led_classdev *led_cdev, enum led_brightness brightness) {
    struct g710_led_s *led = container_of(led_cdev, struct g710_led_s, cd);
    if ((led_cdev->flags & LED_UNREGISTERING) != 0)
        return; //do nothing if we're shutting down.
    spin_lock(&led->lock);
    led_cdev->brightness = brightness;
    spin_unlock(&led->lock);
    schedule_work(&led->work);
}
static enum led_brightness brightness_get(struct led_classdev *led_cdev){
	enum led_brightness ret = LED_OFF;
    struct g710_led_s *led = container_of(led_cdev, struct g710_led_s, cd);
    if ((led_cdev->flags & LED_UNREGISTERING) != 0)
        return ret;
    spin_lock(&led->lock);
    ret = led_cdev->brightness;
    spin_unlock(&led->lock);
	return ret;
}

static int lg_g710_plus_initialize_led(struct hid_device *hdev, struct lg_g710_plus_data *data, enum lg_g710_plus_leds lednro, const char* ledname, u32 max_bright , const char* colorname) {
	int ret = 0;
	int name_sz = strlen(dev_name(&data->input_dev->dev)) + strlen(colorname) + strlen(ledname) + 3;
	char *name = devm_kzalloc(&hdev->dev,name_sz, GFP_KERNEL);
	if (name != NULL) {
		data->leds[lednro] = devm_kzalloc(&hdev->dev,sizeof(*data->leds[lednro]), GFP_KERNEL);
		if (data->leds[lednro] != NULL) {
			snprintf(name, name_sz, "%s:%s:%s", dev_name(&data->input_dev->dev), colorname, ledname);

			spin_lock_init(&data->leds[lednro]->lock);

			INIT_WORK(&data->leds[lednro]->work, led_work);
			data->leds[lednro]->cd.name = name;
			data->leds[lednro]->cd.brightness_get = brightness_get;
			data->leds[lednro]->cd.brightness_set = brightness_set;
			data->leds[lednro]->cd.brightness_set_blocking = brightness_set_sync;
			data->leds[lednro]->cd.brightness = 0;
			data->leds[lednro]->cd.max_brightness = max_bright;
			if (0 != led_classdev_register(&hdev->dev, &data->leds[lednro]->cd)) {
				devm_kfree(&hdev->dev, name);
				devm_kfree(&hdev->dev, data->leds[lednro]);
				data->leds[lednro] = NULL;
				ret = -1;
			}
		}
		else {
			devm_kfree(&hdev->dev, name);
			ret = -1;
		}
	} else {
		ret = -1;
	}
    return -1;
}

#endif

//---------------------------------------------------------------------
//
//---------------------------------------------------------------------

static int lg_g710_plus_initialize(struct hid_device *hdev) {
    int ret = 0;
    struct lg_g710_plus_data *data;
    struct list_head *feature_report_list = &hdev->report_enum[HID_FEATURE_REPORT].report_list;
    struct hid_report *report;

    if (list_empty(feature_report_list)) {
        return 0; /* Currently, the keyboard registers as two different devices */
    }

    data = lg_g710_plus_get_data(hdev);
    list_for_each_entry(report, feature_report_list, list) {

    switch(report->id) {
//            case ?: data->gamemode_report= report; break;
            case 6: 
				data->mr_buttons_led_report = report;
				hidhw_request(hdev, report, REQTYPE_WRITE);
				break;
            case 8: 
				data->other_buttons_led_report = report; 
				hidhw_request(hdev, report, REQTYPE_WRITE);
				break;
            case 9:
                data->g_mr_buttons_support_report = report;
                hidhw_request(hdev, report, REQTYPE_WRITE);
                break;
        }
    }

    ret= sysfs_create_group(&hdev->dev.kobj, &data->attr_group);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0)
	lg_g710_plus_initialize_led(hdev, data, G710_LED_M1, "m1",  1, "yellow");
	lg_g710_plus_initialize_led(hdev, data, G710_LED_M2, "m2",  1, "yellow");
	lg_g710_plus_initialize_led(hdev, data, G710_LED_M3, "m3",  1, "yellow");
	lg_g710_plus_initialize_led(hdev, data, G710_LED_MR, "mr",  1, "red");
	lg_g710_plus_initialize_led(hdev, data, G710_LED_KEYS, "keys",  4, "white");
	lg_g710_plus_initialize_led(hdev, data, G710_LED_WASD, "wasd",  4, "white");
#endif

    return ret;
}

static struct lg_g710_plus_data* lg_g710_plus_create(struct hid_device *hdev)
{
    struct lg_g710_plus_data* data;
    data= kzalloc(sizeof(struct lg_g710_plus_data), GFP_KERNEL);
    if (data == NULL) {
        return NULL;
    }

    data->attr_group.name= "logitech-g710";
    data->attr_group.attrs= lg_g710_plus_attrs;
    data->hdev= hdev;

    spin_lock_init(&data->lock);
 
    return data;
}

static int lg_g710_plus_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
    int ret;
    struct lg_g710_plus_data *data;

    data = lg_g710_plus_create(hdev);
    if (data == NULL) {
        dev_err(&hdev->dev, "can't allocate space for Logitech G710+ device attributes\n");
        ret= -ENOMEM;
        goto err_free;
    }
    hid_set_drvdata(hdev, data);

    /*
     * Without this, the device would send a first report with a key down event for
     * certain buttons, but never the key up event
     */
    hdev->quirks |= HID_QUIRK_NOGET;

    ret = hid_parse(hdev);
    if (ret) {
        hid_err(hdev, "parse failed\n");
        goto err_free;
    }

    ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
    if (ret) {
        hid_err(hdev, "hw start failed\n");
        goto err_free;
    }

    ret= lg_g710_plus_initialize(hdev);
    if (ret) {
        ret = -ret;
        hid_hw_stop(hdev);
        goto err_free;
    }



    return 0;

err_free:
    if (data != NULL) {
        kfree(data);
    }
    return ret;
}

static void lg_g710_plus_remove(struct hid_device *hdev)
{
    struct lg_g710_plus_data* data = lg_g710_plus_get_data(hdev);
    struct list_head *feature_report_list = &hdev->report_enum[HID_FEATURE_REPORT].report_list;
	u32 ind;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0)
    if (data != NULL) {
		for(ind=0; ind < G710_LED_MAX; ++ind) {
			if (data->leds[ind]) {
				led_classdev_unregister(&data->leds[ind]->cd);
			}
		}
    }
#endif

    if (data != NULL && !list_empty(feature_report_list))
        sysfs_remove_group(&hdev->dev.kobj, &data->attr_group);

    hid_hw_stop(hdev);
    if (data != NULL) {
        kfree(data);
    }
}

//---------------------------------------------------------------------
//
//---------------------------------------------------------------------
static ssize_t lg_g710_plus_show_led(struct device *device, struct device_attribute *attr, char *buf, enum lg_g710_plus_leds lednro)
{
    struct lg_g710_plus_data* data = hid_get_drvdata(dev_get_drvdata(device->parent));
    if (data != NULL) {
        return sprintf(buf, "%d\n", lg_g710_plus_led_get(data, lednro));
    }
    return 0;
}

static ssize_t lg_g710_plus_store_led(struct device *device, struct device_attribute *attr, const char *buf, size_t count, enum lg_g710_plus_leds lednro)
{
    unsigned long key_mask;
    int retval;
    struct lg_g710_plus_data* data = hid_get_drvdata(dev_get_drvdata(device->parent));
    retval = kstrtoul(buf, 10, &key_mask);
    if (retval) {
        return retval;
	}
	key_mask = (key_mask <= 4? key_mask : 0);
	
    lg_g710_plus_led_set(data, lednro, key_mask);

#if LINUX_VERSION_CODE > KERNEL_VERSION(4,3,0)
	if(data->leds[lednro]){
		spin_lock(&data->leds[lednro]->lock);
		data->leds[lednro]->cd.brightness = key_mask;
		spin_unlock(&data->leds[lednro]->lock);
	}
#endif
    return count;
}


//---------------------------------------------------------------------
//
//---------------------------------------------------------------------
static ssize_t lg_g710_plus_show_led_mr_m1(struct device *device, struct device_attribute *attr, char *buf) {
	return lg_g710_plus_show_led(device, attr, buf, G710_LED_M1);
}
static ssize_t lg_g710_plus_store_led_mr_m1(struct device *device, struct device_attribute *attr, const char *buf, size_t count) {
	return lg_g710_plus_store_led(device, attr, buf, count, G710_LED_M1);
}
//---------------------------------------------------------------------
//
//---------------------------------------------------------------------
static ssize_t lg_g710_plus_show_led_mr_m2(struct device *device, struct device_attribute *attr, char *buf) {
	return lg_g710_plus_show_led(device, attr, buf, G710_LED_M2);
}
static ssize_t lg_g710_plus_store_led_mr_m2(struct device *device, struct device_attribute *attr, const char *buf, size_t count) {
	return lg_g710_plus_store_led(device, attr, buf, count, G710_LED_M2);
}
//---------------------------------------------------------------------
//
//---------------------------------------------------------------------
static ssize_t lg_g710_plus_show_led_mr_m3(struct device *device, struct device_attribute *attr, char *buf) {
	return lg_g710_plus_show_led(device, attr, buf, G710_LED_M3);
}
static ssize_t lg_g710_plus_store_led_mr_m3(struct device *device, struct device_attribute *attr, const char *buf, size_t count) {
	return lg_g710_plus_store_led(device, attr, buf, count, G710_LED_M3);
}
//---------------------------------------------------------------------
//
//---------------------------------------------------------------------
static ssize_t lg_g710_plus_show_led_mr_mr(struct device *device, struct device_attribute *attr, char *buf) {
	return lg_g710_plus_show_led(device, attr, buf, G710_LED_MR);
}
static ssize_t lg_g710_plus_store_led_mr_mr(struct device *device, struct device_attribute *attr, const char *buf, size_t count) {
	return lg_g710_plus_store_led(device, attr, buf, count, G710_LED_MR);
}
//---------------------------------------------------------------------
//
//---------------------------------------------------------------------
static ssize_t lg_g710_plus_show_led_wasd(struct device *device, struct device_attribute *attr, char *buf) {
	return lg_g710_plus_show_led(device, attr, buf, G710_LED_WASD);
}
static ssize_t lg_g710_plus_store_led_wasd(struct device *device, struct device_attribute *attr, const char *buf, size_t count) {
	return lg_g710_plus_store_led(device, attr, buf, count, G710_LED_WASD);
}
//---------------------------------------------------------------------
//
//---------------------------------------------------------------------
static ssize_t lg_g710_plus_show_led_keys(struct device *device, struct device_attribute *attr, char *buf) {
	return lg_g710_plus_show_led(device, attr, buf, G710_LED_KEYS);
}
static ssize_t lg_g710_plus_store_led_keys(struct device *device, struct device_attribute *attr, const char *buf, size_t count) {
	return lg_g710_plus_store_led(device, attr, buf, count, G710_LED_KEYS);
}

//---------------------------------------------------------------------
// To remember
//---------------------------------------------------------------------

//    struct completion ready; /* ready indicator */
//
//    init_completion(&data->ready);    
//
//   g710_data->led_macro= (data[1] >> 4) & 0xF;
//   complete_all(&g710_data->ready);
//
//  !!! next will not work as wait_for_completion_timeout is inside spin_lock !!!
//	spin_lock(&data->lock);
//	init_completion(&data->ready);
//	hidhw_request(data->hdev, data->mr_buttons_led_report, REQTYPE_READ);
//	wait_for_completion_timeout(&data->ready, WAIT_TIME_OUT);
//	spin_unlock(&data->lock);
//	return sprintf(buf, "%d\n", data->led_macro);

//---------------------------------------------------------------------
//
//---------------------------------------------------------------------

static const struct hid_device_id lg_g710_plus_devices[] = {
    { HID_USB_DEVICE(USB_VENDOR_ID_LOGITECH, USB_DEVICE_ID_LOGITECH_KEYBOARD_G710_PLUS) },
    { }
};

MODULE_DEVICE_TABLE(hid, lg_g710_plus_devices);
static struct hid_driver lg_g710_plus_driver = {
    .name = "hid-lg-g710-plus",
    .id_table = lg_g710_plus_devices,
    .raw_event = lg_g710_plus_raw_event,
    .input_configured = lg_g710_plus_input_configured,
    .probe= lg_g710_plus_probe,
    .remove= lg_g710_plus_remove,
};

static int __init lg_g710_plus_init(void)
{
    return hid_register_driver(&lg_g710_plus_driver);
}

static void __exit lg_g710_plus_exit(void)
{
    hid_unregister_driver(&lg_g710_plus_driver);
}

module_init(lg_g710_plus_init);
module_exit(lg_g710_plus_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Filip Wieladek <Wattos@gmail.com>");
MODULE_DESCRIPTION("Logitech G710+ driver");


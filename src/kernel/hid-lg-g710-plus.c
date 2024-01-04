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
    KEY_SCALE, /* G6 */
    0, /* unused */
    0, /* unused */
};

/* Convenience macros */
#define lg_g710_plus_get_data(hdev) \
        ((struct lg_g710_plus_data *)(hid_get_drvdata(hdev)))

#define BIT_AT(var,pos) ((var) & (1<<(pos)))

struct lg_g710_plus_data {
    struct hid_report *g_mr_buttons_support_report; /* Needs to be written to enable G1-G6 and M1-MR keys */
    struct hid_report *mr_buttons_led_report; /* Controls the backlight of M1-MR buttons */
    struct hid_report *other_buttons_led_report; /* Controls the backlight of other buttons */
    struct hid_report *gamemode_report; /* Controls the backlight of other buttons */

    u16 macro_button_state; /* Holds the last state of the G1-G6, M1-MR buttons. Required to know which buttons were pressed and which were released */
    struct hid_device *hdev;
    struct input_dev *input_dev;
    struct attribute_group attr_group;

    u8 led_macro; /* state of the M1-MR macro leds as returned by the keyboard ==> binary coded 0 -> 0xF*/
    u8 led_keys; /* state of the WASD key leds as returned by the keyboard  ==> 0 -> 4 */

    spinlock_t lock; /* lock for communication with user space */
    struct completion ready; /* ready indicator */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0)
    struct g710_led_s {
        struct led_classdev cd;
        struct work_struct work;
    } *m1, *m2, *m3, *mr, *keys, *wasd;
#endif
};

static ssize_t lg_g710_plus_show_led_macro(struct device *device, struct device_attribute *attr, char *buf);
static ssize_t lg_g710_plus_store_led_macro(struct device *device, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t lg_g710_plus_show_led_keys(struct device *device, struct device_attribute *attr, char *buf);
static ssize_t lg_g710_plus_store_led_keys(struct device *device, struct device_attribute *attr, const char *buf, size_t count);

static DEVICE_ATTR(led_macro, 0660, lg_g710_plus_show_led_macro, lg_g710_plus_store_led_macro);
static DEVICE_ATTR(led_keys,  0660, lg_g710_plus_show_led_keys,  lg_g710_plus_store_led_keys);

static struct attribute *lg_g710_plus_attrs[] = {
        &dev_attr_led_macro.attr,
        &dev_attr_led_keys.attr,
        NULL,
};

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

static int lg_g710_plus_extra_led_mr_event(struct hid_device *hdev, struct hid_report *report, u8 *data, int size) {
    struct lg_g710_plus_data* g710_data = lg_g710_plus_get_data(hdev);
    g710_data->led_macro= (data[1] >> 4) & 0xF;
    complete_all(&g710_data->ready);
    return 1;
}

static int lg_g710_plus_extra_led_keys_event(struct hid_device *hdev, struct hid_report *report, u8 *data, int size) {
    struct lg_g710_plus_data* g710_data = lg_g710_plus_get_data(hdev);
    g710_data->led_keys= data[1] << 4 | data[2];
    complete_all(&g710_data->ready);
    return 1;
}

static int lg_g710_plus_raw_event(struct hid_device *hdev, struct hid_report *report, u8 *data, int size)
{
    switch(report->id) {
        case 3: return lg_g710_plus_extra_key_event(hdev, report, data, size);
        case 6: return lg_g710_plus_extra_led_mr_event(hdev, report, data, size);
        case 8: return lg_g710_plus_extra_led_keys_event(hdev, report, data, size);
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


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0)
static int brightness_set_sync(struct led_classdev *led_cdev,
                            enum led_brightness brightness);

static void led_work(struct work_struct *work) {
    struct g710_led_s *led = container_of(work, struct g710_led_s, work);
    brightness_set_sync(&led->cd, led->cd.brightness);
}

static void brightness_set(struct led_classdev *led_cdev,
                        enum led_brightness brightness) {
    struct g710_led_s *led = container_of(led_cdev, struct g710_led_s, cd);
    if ((led_cdev->flags & LED_UNREGISTERING) != 0)
        return; //do nothing if we're shutting down.
    led_cdev->brightness = brightness;
    schedule_work(&led->work);
}
#endif


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
            case 6: data->mr_buttons_led_report= report; break;
            case 8: data->other_buttons_led_report= report; break;
            case 9:
                data->g_mr_buttons_support_report= report;
                hidhw_request(hdev, report, REQTYPE_WRITE);
                break;
        }
    }

    ret= sysfs_create_group(&hdev->dev.kobj, &data->attr_group);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0)
#define SETUP_LED(x, color, max) {                                      \
        int name_sz = strlen(dev_name(&data->input_dev->dev))           \
            + strlen(#color) + strlen(#x) + 3;                          \
        char *name = devm_kzalloc(&hdev->dev,                           \
                                name_sz, GFP_KERNEL);                   \
        if (name != NULL) {                                             \
            data->x = devm_kzalloc(&hdev->dev,                          \
                                sizeof(*data->x), GFP_KERNEL);          \
            if (data->x != NULL) {                                      \
                snprintf(name, name_sz, "%s:%s:%s",                     \
                        dev_name(&data->input_dev->dev), #color, #x);   \
                                                                        \
                INIT_WORK(&data->x->work, led_work);                    \
                data->x->cd.name = name;                                \
                data->x->cd.brightness_set = brightness_set;            \
                data->x->cd.brightness_set_blocking = brightness_set_sync; \
                data->x->cd.brightness = 0;                             \
                data->x->cd.max_brightness = max;                       \
                if (0 != led_classdev_register(&hdev->dev, &data->x->cd)) { \
                    devm_kfree(&hdev->dev, name);                       \
                    devm_kfree(&hdev->dev, data->x);                    \
                    data->x = NULL;                                     \
                }                                                       \
            }                                                           \
            else {                                                      \
                devm_kfree(&hdev->dev, name);                           \
            }                                                           \
        }                                                               \
    }
    
    SETUP_LED(m1, yellow, 1);
    SETUP_LED(m2, yellow, 1);
    SETUP_LED(m3, yellow, 1);
    SETUP_LED(mr, red, 1);
    SETUP_LED(keys, white, 4);
    SETUP_LED(wasd, white, 4);
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
    init_completion(&data->ready);
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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0)
    if (data != NULL) {
        if (data->m1) {
            led_classdev_unregister(&data->m1->cd);
        }
        if (data->m2) {
            led_classdev_unregister(&data->m2->cd);
        }
        if (data->m3) {
            led_classdev_unregister(&data->m3->cd);
        }
        if (data->mr) {
            led_classdev_unregister(&data->mr->cd);
        }
        if (data->keys) {
            led_classdev_unregister(&data->keys->cd);
        }
        if (data->wasd) {
            led_classdev_unregister(&data->wasd->cd);
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

static ssize_t lg_g710_plus_show_led_macro(struct device *device, struct device_attribute *attr, char *buf)
{
    struct lg_g710_plus_data* data = hid_get_drvdata(dev_get_drvdata(device->parent));
    if (data != NULL) {
        spin_lock(&data->lock);
        init_completion(&data->ready);
        hidhw_request(data->hdev, data->mr_buttons_led_report, REQTYPE_READ);
        wait_for_completion_timeout(&data->ready, WAIT_TIME_OUT);
        spin_unlock(&data->lock);
        return sprintf(buf, "%d\n", data->led_macro);
    }
    return 0;
}

static ssize_t lg_g710_plus_show_led_keys(struct device *device, struct device_attribute *attr, char *buf)
{
    struct lg_g710_plus_data* data = hid_get_drvdata(dev_get_drvdata(device->parent));
    if (data != NULL) {
        spin_lock(&data->lock);
        init_completion(&data->ready);
        hidhw_request(data->hdev, data->other_buttons_led_report, REQTYPE_READ);
        wait_for_completion_timeout(&data->ready, WAIT_TIME_OUT);
        spin_unlock(&data->lock);
        return sprintf(buf, "%d\n", data->led_keys);
    }
    return 0;
}

static ssize_t lg_g710_plus_store_led_macro(struct device *device, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned long key_mask;
    int retval;
    struct lg_g710_plus_data* data = hid_get_drvdata(dev_get_drvdata(device->parent));
    retval = kstrtoul(buf, 10, &key_mask);
    if (retval)
        return retval;

    spin_lock(&data->lock);
    data->mr_buttons_led_report->field[0]->value[0]= (key_mask & 0xF) << 4;
    hidhw_request(data->hdev, data->mr_buttons_led_report, REQTYPE_WRITE);
    spin_unlock(&data->lock);
    return count;
}

static ssize_t lg_g710_plus_store_led_keys(struct device *device, struct device_attribute *attr, const char *buf, size_t count)
{
    int retval;
    unsigned long key_mask;
    u8 wasd_mask, keys_mask;
    struct lg_g710_plus_data* data = hid_get_drvdata(dev_get_drvdata(device->parent));
    retval = kstrtoul(buf, 10, &key_mask);
    if (retval)
        return retval;

    wasd_mask= (key_mask >> 4) & 0xF;
    keys_mask= (key_mask) & 0xF;

    wasd_mask= wasd_mask > 4 ? 4 : wasd_mask;
    keys_mask= keys_mask > 4 ? 4 : keys_mask;

    spin_lock(&data->lock);
    data->other_buttons_led_report->field[0]->value[0]= wasd_mask;
    data->other_buttons_led_report->field[0]->value[1]= keys_mask;
    hidhw_request(data->hdev, data->other_buttons_led_report, REQTYPE_WRITE);
    spin_unlock(&data->lock);
    return count;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,0)
static int brightness_set_sync(struct led_classdev *led_cdev,
                        enum led_brightness brightness) {
	struct device *dev = led_cdev->dev->parent;
	struct hid_device *hdev = container_of(dev, struct hid_device, dev);
    struct lg_g710_plus_data* data = hid_get_drvdata(hdev);
    bool mk = false;

    spin_lock(&data->lock);
    {
        s32 *mask = NULL;
        u8 shift = 0;
        if (data->m1 != NULL && led_cdev == &data->m1->cd) {
            mask = &data->mr_buttons_led_report->field[0]->value[0];
            shift = 4;
            mk = true;
        }
        else if (data->m2 != NULL && led_cdev == &data->m2->cd) {
            mask = &data->mr_buttons_led_report->field[0]->value[0];
            shift = 5;
            mk = true;
        }
        else if (data->m3 != NULL && led_cdev == &data->m3->cd) {
            mask = &data->mr_buttons_led_report->field[0]->value[0];
            shift = 6;
            mk = true;
        }
        else if (data->mr != NULL && led_cdev == &data->mr->cd) {
            mask = &data->mr_buttons_led_report->field[0]->value[0];
            shift = 7;
            mk = true;
        }
        else if (data->wasd != NULL && led_cdev == &data->wasd->cd) {
            mask = &data->other_buttons_led_report->field[0]->value[0];
            mk = false;
        }
        else if (data->keys != NULL && led_cdev == &data->keys->cd) {
            mask = &data->other_buttons_led_report->field[0]->value[1];
            mk = false;
        }
        
        if (mask != NULL) {
        
            if (mk) {
                if (brightness == LED_OFF) {
                    *mask = (*mask) & ~(1 << shift);
                }
                else {
                    *mask = (*mask) |  (1 << shift);
                }
            }
            else {
                *mask = 4 - brightness;
            }
        }
        
        
    }

    if (mk) {
        hidhw_request(data->hdev, data->mr_buttons_led_report, REQTYPE_WRITE);
    }
    else {
        hidhw_request(data->hdev, data->other_buttons_led_report, REQTYPE_WRITE);
    }
    spin_unlock(&data->lock);

    return 0;
}
#endif

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

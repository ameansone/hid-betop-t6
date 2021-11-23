// SPDX-License-Identifier: GPL-2.0+
/*
 * HID driver for Betop T6 Controller（北通宙斯）
 * Hou Lei <ameansone@outlook.com>
 * almost copied from
 *      hid-nintendo https://github.com/nicman23/dkms-hid-nintendo
 * and references serveral gamepad drivers in linux source tree like
 *      hid-sony driver
 *      hid-playstation driver
 *      xpad driver
 */

#include "hid-ids.h"

#include <linux/module.h>
#include <linux/hid.h>
#include <linux/input.h>
#include <linux/input-event-codes.h>
#include <asm-generic/errno-base.h>
#include <linux/jiffies.h>

/*
 * constants for input parameter,
 * some are filled by zero, don't know what's the proper number,
 * works well for now, may be filled later(or never).
 */
static const u16 T6_STICK_MAX           = 127;
static const u16 T6_STICK_MAG           = 32767;
static const u16 T6_STICK_CENTER        = 0x80;
static const u16 T6_STICK_FUZZ          = 0;
static const u16 T6_STICK_FLAT          = 0;

static const u16 T6_TRIGGER_MAX         = 255;
static const u16 T6_TRIGGER_FUZZ        = 0;
static const u16 T6_TRIGGER_FLAT        = 0;

/*
 * according hid-nintendo, RES stands for digits per G,
 * which on my observation, is also about 4k.
 */
static const s32 T6_IMU_ACCEL_MAX       = 32767;
static const u16 T6_IMU_ACCEL_FUZZ      = 10;
static const u16 T6_IMU_ACCEL_FLAT      = 0;
static const u16 T6_IMU_ACCEL_RES       = 4096;

/*
 * copied from hid-nintendo, works fine for now.
 * i suddenly realized that this controller is NS compatible,
 * that's why i can directly copy parameter from hid-nintendo
 * and it works.
 */
static const u32 T6_IMU_GYRO_MAX        = 32767 * 1000;
static const u16 T6_IMU_GYRO_FUZZ       = 10;
static const u16 T6_IMU_GYRO_FLAT       = 0;
static const u16 T6_IMU_GYRO_RES        = 16383;

/*
 * button bits in report.
 * seems skipped two bits, don't why, may missed something.
 * according to https://github.com/ValveSoftware/steamos_kernel/commit/76e4b04b93b40db186d0c4bbbd1824f5d98c76a9
 * M1 - M4 are mapped to BTN_BASE - BTN_BASE4
 * will keep an eye on steamos3
 */
static const u32 T6_BTN_UP              = BIT(0);
static const u32 T6_BTN_DOWN            = BIT(1);
static const u32 T6_BTN_LEFT            = BIT(2);
static const u32 T6_BTN_RIGHT           = BIT(3);
static const u32 T6_BTN_START           = BIT(4);
static const u32 T6_BTN_SELECT          = BIT(5);
static const u32 T6_BTN_LSTICK          = BIT(6);
static const u32 T6_BTN_RSTICK          = BIT(7);
static const u32 T6_BTN_LB              = BIT(8);
static const u32 T6_BTN_RB              = BIT(9);
//static const u32 T6_BTN_                = BIT(10);
//static const u32 T6_BTN_                = BIT(11);
static const u32 T6_BTN_A               = BIT(12);
static const u32 T6_BTN_B               = BIT(13);
static const u32 T6_BTN_X               = BIT(14);
static const u32 T6_BTN_Y               = BIT(15);
static const u32 T6_BTN_M1              = BIT(16);
static const u32 T6_BTN_M2              = BIT(17);
static const u32 T6_BTN_M3              = BIT(18);
static const u32 T6_BTN_M4              = BIT(19);

static const unsigned int btp_t6_buttons[] = {
    BTN_BASE, BTN_BASE2, BTN_BASE3, BTN_BASE4,
    BTN_SOUTH, BTN_EAST, BTN_NORTH, BTN_WEST,
    BTN_TL, BTN_TR, //BTN_TL2, BTN_TR2,
    BTN_SELECT, BTN_START, BTN_THUMBL, BTN_THUMBR,
    BTN_DPAD_UP, BTN_DPAD_DOWN, BTN_DPAD_LEFT, BTN_DPAD_RIGHT,
};

static const unsigned int btp_t6_sticks[] = {
    ABS_X, ABS_Y, ABS_RX, ABS_RY,
};

static const unsigned int btp_t6_triggers[] = {
    ABS_Z, ABS_RZ,
};

static const unsigned int btp_t6_imu_accel[] = {
    ABS_X, ABS_Y, ABS_Z,
};

static const unsigned int btp_t6_imu_gyro[] = {
    ABS_RX, ABS_RY, ABS_RZ,
};

/*
 * with this order it's ns-style axises.
 * which means
 * X: positive is pointing ahead
 * Y: positive is pointing left
 * Z: positive is pointing up
 */
struct btp_t6_imu_data {
    s16 accel_x;
    s16 accel_y;
    s16 accel_z;
    s16 gyro_x;
    s16 gyro_y;
    s16 gyro_z;
};

struct btp_t6_controller_data {
    u8 left_stick_x;
    u8 left_stick_y;
    u8 right_stick_x;
    u8 right_stick_y;
    u8 left_trigger;
    u8 right_trigger;
    u8 button_status[3];
};

// input_data only contain u8 or u8 array to avoid auto padding
// report id 4
struct btp_t6_input_data4 {
    u8 padding1;
    u8 raw_imu[sizeof(struct btp_t6_imu_data)];
    u8 padding2[18];
};
// report id 5
struct btp_t6_input_data5 {
    u8 padding1;
    u8 raw_ctlr[sizeof(struct btp_t6_controller_data)];
    u8 padding2[12];
    u8 raw_imu[sizeof(struct btp_t6_imu_data)];
    u8 padding3[13+16];
};

struct btp_t6_input_report {
    u8 id;
    union {
        struct btp_t6_input_data4 data4;
        struct btp_t6_input_data5 data5;
    };
};

enum btp_t6_ctlr_state {
    T6_CTLR_STATE_INIT,
    T6_CTLR_STATE_READ,
    T6_CTLR_STATE_REMOVED,
};

struct btp_t6_ctlr {
    enum btp_t6_ctlr_state state;
    struct hid_device *hdev;
    struct input_dev *input;
    struct input_dev *imu_input;
    unsigned int timestamp_us;
    unsigned int last_pkt_ms;
};

/*
 * these axises are nintendo layout.
 * got some shift, don't know how to calibrate.
 */
static void btp_t6_parse_imu(struct btp_t6_ctlr *ctlr,
                struct btp_t6_imu_data *imu_data)
{
    struct input_dev *imu_input = ctlr->imu_input;
    unsigned int msecs = jiffies_to_msecs(jiffies);
    ctlr->timestamp_us += (msecs - ctlr->last_pkt_ms) * 1000;

    input_event(imu_input, EV_MSC, MSC_TIMESTAMP, ctlr->timestamp_us);
    input_report_abs(imu_input, ABS_X, imu_data->accel_x);
    input_report_abs(imu_input, ABS_Y, imu_data->accel_y);
    input_report_abs(imu_input, ABS_Z, imu_data->accel_z);
    input_report_abs(imu_input, ABS_RX, imu_data->gyro_x * 1000);
    input_report_abs(imu_input, ABS_RY, imu_data->gyro_y * 1000);
    input_report_abs(imu_input, ABS_RZ, imu_data->gyro_z * 1000);

    ctlr->last_pkt_ms = msecs;
}

static void btp_t6_parse_controller(struct btp_t6_ctlr *ctlr,
                struct btp_t6_controller_data *ctlr_data)
{
    long lx, ly, rx, ry;
    struct input_dev *input = ctlr->input;
    u32 btns = hid_field_extract(ctlr->hdev,
                ctlr_data->button_status, 0, 24);
    
    input_report_key(input, BTN_DPAD_UP, btns & T6_BTN_UP);
    input_report_key(input, BTN_DPAD_DOWN, btns & T6_BTN_DOWN);
    input_report_key(input, BTN_DPAD_LEFT, btns & T6_BTN_LEFT);
    input_report_key(input, BTN_DPAD_RIGHT, btns & T6_BTN_RIGHT);
    input_report_key(input, BTN_START, btns & T6_BTN_START);
    input_report_key(input, BTN_SELECT, btns & T6_BTN_SELECT);
    input_report_key(input, BTN_THUMBL, btns & T6_BTN_LSTICK);
    input_report_key(input, BTN_THUMBR, btns & T6_BTN_RSTICK);
    input_report_key(input, BTN_TL, btns & T6_BTN_LB);
    input_report_key(input, BTN_TR, btns & T6_BTN_RB);
    input_report_key(input, BTN_A, btns & T6_BTN_A);
    input_report_key(input, BTN_B, btns & T6_BTN_B);
    input_report_key(input, BTN_X, btns & T6_BTN_X);
    input_report_key(input, BTN_Y, btns & T6_BTN_Y);
    input_report_key(input, BTN_BASE, btns & T6_BTN_M1);
    input_report_key(input, BTN_BASE2, btns & T6_BTN_M2);
    input_report_key(input, BTN_BASE3, btns & T6_BTN_M3);
    input_report_key(input, BTN_BASE4, btns & T6_BTN_M4);
    
    lx = ctlr_data->left_stick_x - T6_STICK_CENTER;
    lx = lx * T6_STICK_MAG / T6_STICK_MAX;
    ly = T6_STICK_CENTER - ctlr_data->left_stick_y;
    ly = ly * T6_STICK_MAG / T6_STICK_MAX;
    
    rx = ctlr_data->right_stick_x - T6_STICK_CENTER;
    rx = rx * T6_STICK_MAG / T6_STICK_MAX;
    ry = T6_STICK_CENTER - ctlr_data->right_stick_y;
    ry = ry * T6_STICK_MAG / T6_STICK_MAX;

    input_report_abs(input, ABS_X, lx);
    input_report_abs(input, ABS_Y, ly);
    input_report_abs(input, ABS_RX, rx);
    input_report_abs(input, ABS_RY, ry);
    input_report_abs(input, ABS_Z, ctlr_data->left_trigger);
    input_report_abs(input, ABS_RZ, ctlr_data->right_trigger);
}

static void btp_t6_parse_input4(struct btp_t6_ctlr *ctlr,
                struct btp_t6_input_report *report)
{
    btp_t6_parse_imu(ctlr, 
        (struct btp_t6_imu_data*)report->data4.raw_imu);
    input_sync(ctlr->imu_input);
}

static void btp_t6_parse_input5(struct btp_t6_ctlr *ctlr,
                struct btp_t6_input_report *report)
{
    btp_t6_parse_controller(ctlr, 
        (struct btp_t6_controller_data*)report->data5.raw_ctlr);
    btp_t6_parse_imu(ctlr, 
        (struct btp_t6_imu_data*)report->data5.raw_imu);
    
    input_sync(ctlr->input);
    input_sync(ctlr->imu_input);
}

static int btp_t6_ctlr_read_handler(struct btp_t6_ctlr *ctlr,
                u8 *data, int size)
{
    int ret = 0;
    if (data[0] == 4 && size >= 32) {
        btp_t6_parse_input4(ctlr, 
            (struct btp_t6_input_report*)data);
    } else if (data[0] == 5 && size >= 64) {
        btp_t6_parse_input5(ctlr, 
            (struct btp_t6_input_report*)data);
    }
    return ret;
}

static int btp_t6_ctlr_handle_event(struct btp_t6_ctlr *ctlr,
                u8 *data, int size)
{
    int ret;
    ret = btp_t6_ctlr_read_handler(ctlr, data, size);
    return ret;
}

static struct input_dev *btp_t6_init_input(struct btp_t6_ctlr *ctlr,
                char *name)
{
    struct input_dev *input;
    struct hid_device *hdev;
    
    hdev = ctlr->hdev;

    input = devm_input_allocate_device(&hdev->dev);
    if (!input)
        return 0;
    
    input->name = name;
    input->uniq = hdev->uniq;
    input->phys = hdev->phys;
    input->id.bustype = hdev->bus;
    input->id.vendor = hdev->vendor;
    input->id.product = hdev->product;
    input->id.version = hdev->version;
    input->dev.parent = &hdev->dev;
    input_set_drvdata(input, ctlr);
    return input;
}

static int btp_t6_register_controller(struct btp_t6_ctlr *ctlr,
                char *name)
{
    int i;
    
    ctlr->input = btp_t6_init_input(ctlr, name);
    if (!ctlr->input)
        return -ENOMEM;
    
    for (i = 0; i < ARRAY_SIZE(btp_t6_buttons); ++i) {
        input_set_capability(ctlr->input, EV_KEY, btp_t6_buttons[i]);
    }
    for (i = 0; i < ARRAY_SIZE(btp_t6_sticks); ++i) {
        input_set_abs_params(ctlr->input, btp_t6_sticks[i], 
            -T6_STICK_MAG, T6_STICK_MAG, T6_STICK_FUZZ, T6_STICK_FLAT);
    }
    for (i = 0; i < ARRAY_SIZE(btp_t6_triggers); ++i) {
        input_set_abs_params(ctlr->input, btp_t6_triggers[i], 
            0, T6_TRIGGER_MAX, T6_TRIGGER_FUZZ, T6_TRIGGER_FLAT);
    }
    return input_register_device(ctlr->input);
}

static int btp_t6_register_imu(struct btp_t6_ctlr *ctlr,
                char *name)
{
    int i;

    ctlr->imu_input = btp_t6_init_input(ctlr, name);
    if (!ctlr->imu_input)
        return -ENOMEM;

    for (i = 0; i < ARRAY_SIZE(btp_t6_imu_accel); ++i) {
        input_set_abs_params(ctlr->imu_input, btp_t6_imu_accel[i],
            -T6_IMU_ACCEL_MAX, T6_IMU_ACCEL_MAX, 
            T6_IMU_ACCEL_FUZZ, T6_IMU_ACCEL_FLAT);
        input_abs_set_res(ctlr->imu_input, btp_t6_imu_accel[i], 
            T6_IMU_ACCEL_RES);
    }
    for (i = 0; i < ARRAY_SIZE(btp_t6_imu_gyro); ++i) {
        input_set_abs_params(ctlr->imu_input, btp_t6_imu_gyro[i],
            -T6_IMU_GYRO_MAX, T6_IMU_GYRO_MAX, 
            T6_IMU_GYRO_FUZZ, T6_IMU_GYRO_FLAT);
        input_abs_set_res(ctlr->imu_input, btp_t6_imu_gyro[i], 
            T6_IMU_GYRO_RES);
    }
    input_set_capability(ctlr->imu_input, EV_MSC, MSC_TIMESTAMP);
    __set_bit(INPUT_PROP_ACCELEROMETER, ctlr->imu_input->propbit);

    return input_register_device(ctlr->imu_input);
}

static int btp_t6_input_create(struct btp_t6_ctlr *ctlr)
{
    int ret;
    struct hid_device *hdev;
    char *name, *imu_name;

    hdev = ctlr->hdev;

    switch(hdev->product) {
    case USB_DEVICE_ID_BETOP_T6_USB:
        name = "Betop T6 For USB";
        imu_name = "Betop T6 For USB IMU";
        break;
    case USB_DEVICE_ID_BETOP_T6_ADAPTER:
        name = "Betop T6 For Adapter";
        imu_name = "Betop T6 For Adapter IMU";
        break;
    case USB_DEVICE_ID_BETOP_T6_USB_WITH_AUDIO:
        name = "Betop T6 For USB With Audio";
        imu_name = "Betop T6 For USB With Audio IMU";
        break;
    case USB_DEVICE_ID_BETOP_T6_ADAPTER_WITH_AUDIO:
        name = "Betop T6 For Adapter With Audio";
        imu_name = "Betop T6 For Adapter With Audio IMU";
        break;
    }
    
    // only get report id 5 when wired, sad
    if (hdev->product == USB_DEVICE_ID_BETOP_T6_USB ||
        hdev->product == USB_DEVICE_ID_BETOP_T6_USB_WITH_AUDIO) {
        ret = btp_t6_register_controller(ctlr, name);
        if (ret) return ret;
    }

    ret = btp_t6_register_imu(ctlr, imu_name);
    if (ret) return ret;

    return 0;
}

/*
 * there're two hid interface with this device
 * we just need one of them
 * maybe there's better way to do this
 */
static bool btp_t6_verify_device(struct hid_device *hdev)
{
    return hdev->dev_rsize == 211;
}

static bool btp_t6_hid_match(struct hid_device *hdev,
                bool ignore_special_driver)
{
    if (ignore_special_driver) return true;
    return btp_t6_verify_device(hdev);
}

static int btp_t6_hid_probe(struct hid_device *hdev,
                const struct hid_device_id *id)
{
    int ret;
    struct btp_t6_ctlr *ctlr;

    hid_dbg(hdev, "probe - start\n");
    
    ctlr = devm_kzalloc(&hdev->dev, sizeof(*ctlr), GFP_KERNEL);
    if (!ctlr) {
        ret = -ENOMEM;
        goto err;
    }
    
    ctlr->hdev = hdev;
    ctlr->state = T6_CTLR_STATE_INIT;
    hid_set_drvdata(hdev, ctlr);

    ret = hid_parse(hdev);
    if (ret) {
        hid_err(hdev, "HID parse failed\n");
        goto err;
    }
    
    ret = hid_hw_start(hdev, HID_CONNECT_HIDRAW);
    if (ret) {
        hid_err(hdev, "HW start failed\n");
        goto err;
    }
    ret = hid_hw_open(hdev);
    if (ret) {
        hid_err(hdev, "cannot start hardware I/O\n");
        goto err_stop;
    }
    hid_device_io_start(hdev);

	ret = btp_t6_input_create(ctlr);
	if (ret) {
		hid_err(hdev, "Failed to create input device; ret=%d\n", ret);
		goto err_close;
	}
    
    ctlr->state = T6_CTLR_STATE_READ;
    
    hid_dbg(hdev, "probe - success\n");
    return 0;

err_close:
    hid_hw_close(hdev);
err_stop:
    hid_hw_stop(hdev);
err:
    hid_err(hdev, "probe - fail = %d\n", ret);
    return ret;
}

static int btp_t6_hid_event(struct hid_device *hdev, 
                struct hid_report *report, u8 *raw_data, int size)
{
    int ret = 0;
    struct btp_t6_ctlr *ctlr = hid_get_drvdata(hdev);
    
	if (!ctlr || size < 1)
		return -EINVAL;

    if (ctlr->state == T6_CTLR_STATE_READ)
	    ret = btp_t6_ctlr_handle_event(ctlr, raw_data, size);
    return ret;
}

static void btp_t6_hid_remove(struct hid_device *hdev)
{
    struct btp_t6_ctlr *ctlr = hid_get_drvdata(hdev);

    hid_dbg(hdev, "remove\n");

    ctlr->state = T6_CTLR_STATE_REMOVED;

    hid_hw_close(hdev);
    hid_hw_stop(hdev);
}

static const struct hid_device_id btp_t6_hid_devices[] = {
    { HID_USB_DEVICE(USB_VENDOR_ID_BETOP,
            USB_DEVICE_ID_BETOP_T6_USB) },
    { HID_USB_DEVICE(USB_VENDOR_ID_BETOP, 
            USB_DEVICE_ID_BETOP_T6_ADAPTER) },
    { HID_USB_DEVICE(USB_VENDOR_ID_BETOP,
            USB_DEVICE_ID_BETOP_T6_USB_WITH_AUDIO) },
    { HID_USB_DEVICE(USB_VENDOR_ID_BETOP,
            USB_DEVICE_ID_BETOP_T6_ADAPTER_WITH_AUDIO) },
    { }
};
MODULE_DEVICE_TABLE(hid, btp_t6_hid_devices);

static struct hid_driver btp_t6_hid_driver = {
    .name           = "btp_t6",
    .id_table       = btp_t6_hid_devices,
    .match          = btp_t6_hid_match,
    .probe          = btp_t6_hid_probe,
    .remove         = btp_t6_hid_remove,
    .raw_event      = btp_t6_hid_event,
};

module_hid_driver(btp_t6_hid_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hou Lei <ameansone@outlook.com>");
MODULE_DESCRIPTION("Driver for Betop T6 Controller");
#ifndef GAMEPAD_H
#define GAMEPAD_H

#include <stddef.h>
#include <stdint.h>

/* Button bits in gp_state.buttons */
enum {
    GP_A      = 1 << 0,
    GP_B      = 1 << 1,
    GP_X      = 1 << 2,
    GP_Y      = 1 << 3,
    GP_LB     = 1 << 4,
    GP_RB     = 1 << 5,
    GP_VIEW   = 1 << 6,   /* Back / Select */
    GP_MENU   = 1 << 7,   /* Start */
    GP_L3     = 1 << 8,
    GP_R3     = 1 << 9,
    GP_UP     = 1 << 10,
    GP_DOWN   = 1 << 11,
    GP_LEFT   = 1 << 12,
    GP_RIGHT  = 1 << 13,
    GP_GUIDE  = 1 << 14,  /* Xbox / home button */
};

/* Device-independent gamepad state. Sticks are signed 16-bit with +Y = up,
 * triggers are 10-bit (0..1023). */
struct gp_state {
    uint16_t buttons;
    uint16_t lt, rt;
    int16_t  lx, ly, rx, ry;
};

/* Xbox One GIP protocol (wired USB). */
#define GIP_CMD_GUIDE  0x07
#define GIP_CMD_INPUT  0x20

/* Parse one GIP packet into *st. Returns 1 if the packet carried input
 * (and *st was updated), 0 if it was something else (ignored). */
int gip_parse(const uint8_t *buf, size_t len, struct gp_state *st);

/* Virtual HID device: Xbox One S Bluetooth-style input report. */
#define HID_REPORT1_LEN 16  /* report id + 15 bytes */
#define HID_REPORT2_LEN 2   /* report id + 1 byte (Xbox button) */
extern const uint8_t hid_report_descriptor[];
extern const size_t  hid_report_descriptor_len;

void hid_build_report1(const struct gp_state *st, uint8_t out[HID_REPORT1_LEN]);
void hid_build_report2(const struct gp_state *st, uint8_t out[HID_REPORT2_LEN]);

#endif

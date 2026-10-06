#include "gamepad.h"

/* Mirrors the Xbox One S (model 1708) Bluetooth HID layout, VID 045E / PID 02FD,
 * which macOS' GameController framework and SDL already recognise.
 *
 * Report 1 (15 bytes + id): X, Y, Z, Rz (u16 each, centre 0x8000), Brake (LT, 10 bit
 * in 16), Accelerator (RT, 10 bit in 16), Hat (4 bit, 1..8, 0 = neutral + 4 pad),
 * 15 buttons + 1 pad.
 * Report 2 (1 byte): Consumer AC Home (Xbox button).
 */
const uint8_t hid_report_descriptor[] = {
    0x05, 0x01, 0x09, 0x05, 0xA1, 0x01,             /* GenDesktop, GamePad, Collection(App) */
    0x85, 0x01,                                     /*   Report ID 1 */
    0x09, 0x01, 0xA1, 0x00,                         /*   Pointer, Collection(Physical) */
    0x09, 0x30, 0x09, 0x31,                         /*     X, Y */
    0x15, 0x00, 0x27, 0xFF, 0xFF, 0x00, 0x00,       /*     Logical 0..65535 */
    0x95, 0x02, 0x75, 0x10, 0x81, 0x02,             /*     2 x 16 bit Input */
    0xC0,
    0x09, 0x01, 0xA1, 0x00,
    0x09, 0x32, 0x09, 0x35,                         /*     Z, Rz */
    0x15, 0x00, 0x27, 0xFF, 0xFF, 0x00, 0x00,
    0x95, 0x02, 0x75, 0x10, 0x81, 0x02,
    0xC0,
    0x05, 0x02, 0x09, 0xC5,                         /*   Simulation: Brake */
    0x15, 0x00, 0x26, 0xFF, 0x03,                   /*     0..1023 */
    0x95, 0x01, 0x75, 0x0A, 0x81, 0x02,
    0x15, 0x00, 0x25, 0x00, 0x75, 0x06, 0x95, 0x01, 0x81, 0x03,   /* 6 bit pad */
    0x05, 0x02, 0x09, 0xC4,                         /*   Accelerator */
    0x15, 0x00, 0x26, 0xFF, 0x03,
    0x95, 0x01, 0x75, 0x0A, 0x81, 0x02,
    0x15, 0x00, 0x25, 0x00, 0x75, 0x06, 0x95, 0x01, 0x81, 0x03,
    0x05, 0x01, 0x09, 0x39,                         /*   Hat switch */
    0x15, 0x01, 0x25, 0x08, 0x35, 0x00, 0x46, 0x3B, 0x01, 0x66, 0x14, 0x00,
    0x75, 0x04, 0x95, 0x01, 0x81, 0x42,
    0x75, 0x04, 0x95, 0x01, 0x15, 0x00, 0x25, 0x00, 0x35, 0x00, 0x45, 0x00,
    0x65, 0x00, 0x81, 0x03,                         /*   4 bit pad */
    0x05, 0x09, 0x19, 0x01, 0x29, 0x0F,             /*   15 buttons */
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x0F, 0x81, 0x02,
    0x15, 0x00, 0x25, 0x00, 0x75, 0x01, 0x95, 0x01, 0x81, 0x03,   /* 1 bit pad */
    0x05, 0x0C, 0x0A, 0x23, 0x02,                   /*   Report 2: Consumer AC Home */
    0x85, 0x02,
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x01, 0x81, 0x02,
    0x15, 0x00, 0x25, 0x00, 0x75, 0x07, 0x95, 0x01, 0x81, 0x03,
    0xC0,
};
const size_t hid_report_descriptor_len = sizeof hid_report_descriptor;

static void put16(uint8_t *p, uint16_t v) { p[0] = v & 0xFF; p[1] = v >> 8; }

/* Button bit positions within the 15-bit field (0-based). */
enum { B_A = 0, B_B = 1, B_X = 3, B_Y = 4, B_LB = 6, B_RB = 7,
       B_VIEW = 10, B_MENU = 11, B_L3 = 13, B_R3 = 14 };

static uint8_t hat_value(uint16_t b)
{
    int up = !!(b & GP_UP), dn = !!(b & GP_DOWN), lf = !!(b & GP_LEFT), rt = !!(b & GP_RIGHT);
    if (up && dn) up = dn = 0;
    if (lf && rt) lf = rt = 0;
    if (up) return rt ? 2 : lf ? 8 : 1;
    if (dn) return rt ? 4 : lf ? 6 : 5;
    if (rt) return 3;
    if (lf) return 7;
    return 0;
}

static uint16_t axis(int16_t v, int invert)
{
    int32_t x = invert ? -(int32_t)v : v;
    if (x > 32767) x = 32767;
    return (uint16_t)(x + 32768);
}

void hid_build_report1(const struct gp_state *s, uint8_t o[HID_REPORT1_LEN])
{
    uint16_t b = s->buttons, f = 0;
    o[0] = 1;
    put16(o + 1,  axis(s->lx, 0));
    put16(o + 3,  axis(s->ly, 1));   /* HID Y grows downward */
    put16(o + 5,  axis(s->rx, 0));
    put16(o + 7,  axis(s->ry, 1));
    put16(o + 9,  s->lt & 0x3FF);
    put16(o + 11, s->rt & 0x3FF);
    o[13] = hat_value(b);
    if (b & GP_A)    f |= 1 << B_A;
    if (b & GP_B)    f |= 1 << B_B;
    if (b & GP_X)    f |= 1 << B_X;
    if (b & GP_Y)    f |= 1 << B_Y;
    if (b & GP_LB)   f |= 1 << B_LB;
    if (b & GP_RB)   f |= 1 << B_RB;
    if (b & GP_VIEW) f |= 1 << B_VIEW;
    if (b & GP_MENU) f |= 1 << B_MENU;
    if (b & GP_L3)   f |= 1 << B_L3;
    if (b & GP_R3)   f |= 1 << B_R3;
    put16(o + 14, f);
}

void hid_build_report2(const struct gp_state *s, uint8_t o[HID_REPORT2_LEN])
{
    o[0] = 2;
    o[1] = (s->buttons & GP_GUIDE) ? 1 : 0;
}

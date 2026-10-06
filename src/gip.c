#include "gamepad.h"

/* Layout (see Linux drivers/input/joystick/xpad.c, xpadone_process_packet):
 *   [0] cmd  [1] flags  [2] seq  [3] payload length
 *   0x20 input: [4] btn lo  [5] btn hi  [6..7] LT  [8..9] RT
 *               [10..17] LX LY RX RY, int16 little endian
 *   0x07 guide: [4] bit0 = pressed
 */
static int16_t s16(const uint8_t *p) { return (int16_t)(p[0] | (p[1] << 8)); }
static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

int gip_parse(const uint8_t *b, size_t len, struct gp_state *st)
{
    if (len < 5)
        return 0;

    if (b[0] == GIP_CMD_GUIDE) {
        if (b[4] & 1) st->buttons |= GP_GUIDE;
        else          st->buttons &= (uint16_t)~GP_GUIDE;
        return 1;
    }

    if (b[0] != GIP_CMD_INPUT || len < 18)
        return 0;

    uint16_t keep = st->buttons & GP_GUIDE;  /* guide arrives in its own packet */
    uint16_t m = keep;
    if (b[4] & 0x04) m |= GP_MENU;
    if (b[4] & 0x08) m |= GP_VIEW;
    if (b[4] & 0x10) m |= GP_A;
    if (b[4] & 0x20) m |= GP_B;
    if (b[4] & 0x40) m |= GP_X;
    if (b[4] & 0x80) m |= GP_Y;
    if (b[5] & 0x01) m |= GP_UP;
    if (b[5] & 0x02) m |= GP_DOWN;
    if (b[5] & 0x04) m |= GP_LEFT;
    if (b[5] & 0x08) m |= GP_RIGHT;
    if (b[5] & 0x10) m |= GP_LB;
    if (b[5] & 0x20) m |= GP_RB;
    if (b[5] & 0x40) m |= GP_L3;
    if (b[5] & 0x80) m |= GP_R3;
    st->buttons = m;

    st->lt = u16(b + 6) & 0x3FF;
    st->rt = u16(b + 8) & 0x3FF;
    st->lx = s16(b + 10);
    st->ly = s16(b + 12);
    st->rx = s16(b + 14);
    st->ry = s16(b + 16);
    return 1;
}

#include "../src/gamepad.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    struct gp_state st = {0};
    /* A + RB + dpad up, LT full, LX full right, LY full up, RX -32768 */
    uint8_t in[18] = { 0x20, 0x00, 0x01, 0x0E, 0x10, 0x21, 0xFF, 0x03, 0x00, 0x00,
                       0xFF, 0x7F, 0xFF, 0x7F, 0x00, 0x80, 0x00, 0x00 };
    assert(gip_parse(in, sizeof in, &st) == 1);
    assert(st.buttons == (GP_A | GP_RB | GP_UP));
    assert(st.lt == 1023 && st.rt == 0);
    assert(st.lx == 32767 && st.ly == 32767 && st.rx == -32768 && st.ry == 0);

    uint8_t g[5] = { 0x07, 0x20, 0x02, 0x01, 0x01 };
    assert(gip_parse(g, sizeof g, &st) == 1 && (st.buttons & GP_GUIDE));
    assert(gip_parse(in, sizeof in, &st) == 1 && (st.buttons & GP_GUIDE)); /* guide survives */
    g[4] = 0;
    gip_parse(g, sizeof g, &st);
    assert(!(st.buttons & GP_GUIDE));

    uint8_t junk[5] = { 0x03, 0, 0, 0, 0 };
    assert(gip_parse(junk, sizeof junk, &st) == 0);

    uint8_t r[HID_REPORT1_LEN];
    struct gp_state s = { .buttons = GP_A | GP_UP | GP_RIGHT, .lt = 1023, .ly = 32767 };
    hid_build_report1(&s, r);
    assert(r[0] == 1);
    assert(r[1] == 0x00 && r[2] == 0x80);   /* LX centre */
    assert(r[3] == 0x01 && r[4] == 0x00);   /* LY up -> HID 1 (inverted: 0x0001) */
    assert(r[9] == 0xFF && r[10] == 0x03);  /* LT */
    assert(r[13] == 2);                     /* up+right = NE */
    assert(r[14] == 0x01 && r[15] == 0x00); /* A */
    s.buttons = 0; hid_build_report1(&s, r);
    assert(r[13] == 0);
    s.buttons = GP_VIEW | GP_MENU | GP_L3 | GP_R3; hid_build_report1(&s, r);
    assert((r[14] | r[15] << 8) == ((1<<10)|(1<<11)|(1<<13)|(1<<14)));
    puts("ok");
    return 0;
}

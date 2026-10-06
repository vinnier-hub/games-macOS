#include "../src/hidmap.h"
#include <assert.h>
#include <stdio.h>

#define K(p, u) (((uint32_t)(p) << 16) | (u))

int main(void)
{
    struct hidmap m; struct gp_state s = {0};
    assert(hidmap_preset(&m, "xbox") == 0);
    assert(hidmap_preset(&m, "nope") == -1);
    hidmap_preset(&m, "xbox");

    assert(hidmap_apply(&m, K(9, 1), 1, 0, 1, &s) == 1 && (s.buttons & GP_A));
    hidmap_apply(&m, K(9, 1), 0, 0, 1, &s);
    assert(!(s.buttons & GP_A));
    assert(hidmap_apply(&m, K(9, 99), 1, 0, 1, &s) == 0);

    /* stick: full right, full down (HID) -> +32767 / -32768 (up positive) */
    hidmap_apply(&m, K(1, 0x30), 65535, 0, 65535, &s);
    hidmap_apply(&m, K(1, 0x31), 65535, 0, 65535, &s);
    assert(s.lx == 32767 && s.ly == -32768);
    hidmap_apply(&m, K(1, 0x31), 0, 0, 65535, &s);
    assert(s.ly == 32767);

    hidmap_apply(&m, K(2, 0xC5), 1023, 0, 1023, &s);
    assert(s.lt == 1023);

    /* Xbox hat: 1..8, 0 = neutral */
    hidmap_apply(&m, K(1, 0x39), 2, 1, 8, &s);
    assert((s.buttons & (GP_UP | GP_RIGHT)) == (GP_UP | GP_RIGHT) && !(s.buttons & GP_DOWN));
    hidmap_apply(&m, K(1, 0x39), 0, 1, 8, &s);
    assert(!(s.buttons & (GP_UP | GP_DOWN | GP_LEFT | GP_RIGHT)));

    /* overrides */
    assert(hidmap_override(&m, "b3=Y") == 0);
    hidmap_apply(&m, K(9, 3), 1, 0, 1, &s);
    assert(s.buttons & GP_Y);
    assert(hidmap_override(&m, "a0x33=lt") == 0);
    assert(hidmap_override(&m, "0x0C:0x224=GUIDE") == 0);
    assert(hidmap_override(&m, "b1=BOGUS") == -1);
    assert(hidmap_override(&m, "junk") == -1);

    hidmap_preset(&m, "generic");
    hidmap_apply(&m, K(9, 3), 1, 0, 1, &s);
    assert(s.buttons & GP_X);
    puts("ok");
    return 0;
}

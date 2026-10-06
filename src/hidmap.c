#include "hidmap.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define K(page, usage) (((uint32_t)(page) << 16) | (usage))
#define PG_DESKTOP 0x01
#define PG_SIM     0x02
#define PG_BUTTON  0x09
#define PG_CONS    0x0C

static const struct { const char *n; uint16_t v; } btn_names[] = {
    {"A", GP_A}, {"B", GP_B}, {"X", GP_X}, {"Y", GP_Y}, {"LB", GP_LB}, {"RB", GP_RB},
    {"VIEW", GP_VIEW}, {"MENU", GP_MENU}, {"L3", GP_L3}, {"R3", GP_R3}, {"GUIDE", GP_GUIDE},
};
static const struct { const char *n; uint16_t v; } axis_names[] = {
    {"LX", AX_LX}, {"LY", AX_LY}, {"RX", AX_RX}, {"RY", AX_RY},
    {"LT", AX_LT}, {"RT", AX_RT}, {"HAT", AX_HAT},
};

static int put(struct hidmap *m, uint32_t key, int kind, uint16_t value)
{
    for (int i = 0; i < m->n; i++)
        if (m->e[i].key == key) { m->e[i].kind = kind; m->e[i].value = value; return 0; }
    if (m->n >= HM_MAX) return -1;
    m->e[m->n++] = (struct hm_entry){ key, (uint8_t)kind, value };
    return 0;
}

static void put_common_axes(struct hidmap *m)
{
    put(m, K(PG_DESKTOP, 0x30), HM_AXIS, AX_LX);
    put(m, K(PG_DESKTOP, 0x31), HM_AXIS, AX_LY);
    put(m, K(PG_DESKTOP, 0x39), HM_AXIS, AX_HAT);
    put(m, K(PG_CONS, 0x223), HM_BUTTON, GP_GUIDE);
}

int hidmap_preset(struct hidmap *m, const char *name)
{
    m->n = 0;
    put_common_axes(m);
    if (!strcasecmp(name, "xbox")) {
        put(m, K(PG_DESKTOP, 0x32), HM_AXIS, AX_RX);
        put(m, K(PG_DESKTOP, 0x35), HM_AXIS, AX_RY);
        put(m, K(PG_SIM, 0xC5), HM_AXIS, AX_LT);
        put(m, K(PG_SIM, 0xC4), HM_AXIS, AX_RT);
        static const uint16_t b[] = { [1]=GP_A, [2]=GP_B, [4]=GP_X, [5]=GP_Y, [7]=GP_LB,
            [8]=GP_RB, [11]=GP_VIEW, [12]=GP_MENU, [14]=GP_L3, [15]=GP_R3 };
        for (unsigned i = 1; i < sizeof b / sizeof *b; i++)
            if (b[i]) put(m, K(PG_BUTTON, i), HM_BUTTON, b[i]);
        /* NB: the virtual device uses usages 11/12/14/15 for View/Menu/L3/R3 */
        return 0;
    }
    if (!strcasecmp(name, "generic")) {
        put(m, K(PG_DESKTOP, 0x32), HM_AXIS, AX_RX);
        put(m, K(PG_DESKTOP, 0x35), HM_AXIS, AX_RY);
        put(m, K(PG_DESKTOP, 0x33), HM_AXIS, AX_LT);
        put(m, K(PG_DESKTOP, 0x34), HM_AXIS, AX_RT);
        static const uint16_t b[] = { [1]=GP_A, [2]=GP_B, [3]=GP_X, [4]=GP_Y, [5]=GP_LB,
            [6]=GP_RB, [7]=GP_VIEW, [8]=GP_MENU, [9]=GP_L3, [10]=GP_R3 };
        for (unsigned i = 1; i < sizeof b / sizeof *b; i++)
            put(m, K(PG_BUTTON, i), HM_BUTTON, b[i]);
        return 0;
    }
    return -1;
}

int hidmap_override(struct hidmap *m, const char *spec)
{
    const char *eq = strchr(spec, '=');
    if (!eq || eq == spec) return -1;
    char key[32];
    size_t kl = (size_t)(eq - spec);
    if (kl >= sizeof key) return -1;
    memcpy(key, spec, kl); key[kl] = 0;
    const char *name = eq + 1;

    uint32_t k;
    char *end;
    if (key[0] == 'b' || key[0] == 'B') {
        unsigned long u = strtoul(key + 1, &end, 0);
        if (*end || end == key + 1) return -1;
        k = K(PG_BUTTON, u);
    } else if (key[0] == 'a' || key[0] == 'A') {
        unsigned long u = strtoul(key + 1, &end, 0);
        if (*end || end == key + 1) return -1;
        k = K(PG_DESKTOP, u);
    } else {
        unsigned long p = strtoul(key, &end, 0);
        if (*end != ':') return -1;
        unsigned long u = strtoul(end + 1, &end, 0);
        if (*end) return -1;
        k = K(p, u);
    }
    for (unsigned i = 0; i < sizeof btn_names / sizeof *btn_names; i++)
        if (!strcasecmp(name, btn_names[i].n)) return put(m, k, HM_BUTTON, btn_names[i].v);
    for (unsigned i = 0; i < sizeof axis_names / sizeof *axis_names; i++)
        if (!strcasecmp(name, axis_names[i].n)) return put(m, k, HM_AXIS, axis_names[i].v);
    return -1;
}

static int16_t norm_stick(long v, long min, long max, int invert)
{
    if (max <= min) return 0;
    if (v < min) v = min;
    if (v > max) v = max;
    long x = (v - min) * 65535L / (max - min) - 32768L;
    if (invert) x = -x - 1;           /* HID Y grows downward; map so max -> -32768 */
    if (x > 32767) x = 32767;
    if (x < -32768) x = -32768;
    return (int16_t)x;
}

static uint16_t norm_trigger(long v, long min, long max)
{
    if (max <= min) return 0;
    if (v < min) v = min;
    if (v > max) v = max;
    return (uint16_t)((v - min) * 1023L / (max - min));
}

int hidmap_apply(const struct hidmap *m, uint32_t key, long v, long min, long max,
                 struct gp_state *st)
{
    for (int i = 0; i < m->n; i++) {
        const struct hm_entry *e = &m->e[i];
        if (e->key != key) continue;
        if (e->kind == HM_BUTTON) {
            if (v) st->buttons |= e->value; else st->buttons &= (uint16_t)~e->value;
            return 1;
        }
        switch (e->value) {
        case AX_LX: st->lx = norm_stick(v, min, max, 0); break;
        case AX_LY: st->ly = norm_stick(v, min, max, 1); break;
        case AX_RX: st->rx = norm_stick(v, min, max, 0); break;
        case AX_RY: st->ry = norm_stick(v, min, max, 1); break;
        case AX_LT: st->lt = norm_trigger(v, min, max); break;
        case AX_RT: st->rt = norm_trigger(v, min, max); break;
        case AX_HAT: {
            static const uint16_t dir[8] = {
                GP_UP, GP_UP | GP_RIGHT, GP_RIGHT, GP_DOWN | GP_RIGHT,
                GP_DOWN, GP_DOWN | GP_LEFT, GP_LEFT, GP_UP | GP_LEFT };
            long idx = v - min;
            st->buttons &= (uint16_t)~(GP_UP | GP_DOWN | GP_LEFT | GP_RIGHT);
            if (idx >= 0 && idx < 8) st->buttons |= dir[idx];
            break;
        }
        }
        return 1;
    }
    return 0;
}

#ifndef HIDMAP_H
#define HIDMAP_H
#include "gamepad.h"

/* Maps raw HID usages (from a Bluetooth pad) onto struct gp_state. */
enum hm_kind { HM_BUTTON, HM_AXIS };
enum hm_axis { AX_LX, AX_LY, AX_RX, AX_RY, AX_LT, AX_RT, AX_HAT };

struct hm_entry {
    uint32_t key;      /* (usage page << 16) | usage */
    uint8_t  kind;     /* enum hm_kind */
    uint16_t value;    /* GP_* mask for buttons, enum hm_axis for axes */
};

#define HM_MAX 64
struct hidmap {
    struct hm_entry e[HM_MAX];
    int n;
};

/* Presets: "xbox" (Xbox One S Bluetooth layout, the default) or "generic"
 * (DirectInput-style: 10 buttons in A B X Y LB RB View Menu L3 R3 order). */
int hidmap_preset(struct hidmap *m, const char *name);

/* Override one entry. spec: "<key>=<NAME>"; key is bN (button page, usage N),
 * aU (Generic Desktop usage U) or P:U (page:usage). Numbers accept 0x. */
int hidmap_override(struct hidmap *m, const char *spec);

/* Apply one HID value. Returns 1 if the usage was mapped. */
int hidmap_apply(const struct hidmap *m, uint32_t key, long value, long min, long max,
                 struct gp_state *st);

#endif

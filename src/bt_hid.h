#ifndef BT_HID_H
#define BT_HID_H
#include "hidmap.h"
#include <signal.h>

typedef void (*bt_emit_fn)(const struct gp_state *st, void *ctx);

/* Run until *stop is set. Reads every HID gamepad (optionally filtered by
 * vid/pid) except our own virtual device, applies `map`, and calls emit().
 * dump: print each usage/value instead of mapping. Returns 0 on clean exit. */
int bt_run(uint16_t vid, uint16_t pid, const struct hidmap *map, int seize, int dump,
           volatile sig_atomic_t *stop, bt_emit_fn emit, void *ctx);
#endif

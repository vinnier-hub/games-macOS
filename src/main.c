#include "bt_hid.h"
#include "gamepad.h"
#include "hidmap.h"
#include "usb_gip.h"
#include "vhid.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static volatile sig_atomic_t stop;
static void on_sig(int s) { (void)s; stop = 1; }

static void usage(const char *a)
{
    fprintf(stderr,
        "usage: %s [options]\n"
        "  --source usb|bt|auto  input transport (default auto: USB first, else Bluetooth)\n"
        "  --list                list attached USB GIP (Xbox One protocol) devices and exit\n"
        "  --list-hid            list every HID device macOS sees (use to debug Bluetooth)\n"
        "  --dump                print raw input instead of creating the virtual pad\n"
        "  --vid 0xVVVV --pid 0xPPPP   pick a specific device\n"
        "Bluetooth options:\n"
        "  --preset xbox|generic  button/axis layout of the physical pad (default xbox)\n"
        "  --map KEY=NAME         override one mapping, e.g. b3=Y  a0x33=LT  0x0C:0x223=GUIDE\n"
        "  --no-seize             do not take exclusive access to the physical pad\n", a);
}

static struct vhid *vh;

/* Send the full state; report 2 (Xbox button) only when it changes. */
static void emit(const struct gp_state *st, void *unused)
{
    static uint16_t last_guide;
    uint8_t r1[HID_REPORT1_LEN], r2[HID_REPORT2_LEN];
    (void)unused;
    hid_build_report1(st, r1);
    vhid_send(vh, r1, sizeof r1);
    if ((st->buttons & GP_GUIDE) != last_guide) {
        last_guide = st->buttons & GP_GUIDE;
        hid_build_report2(st, r2);
        vhid_send(vh, r2, sizeof r2);
    }
}

static int run_usb(struct gip_dev *dev, int dump)
{
    struct gp_state st = {0};
    uint8_t buf[64];
    while (!stop) {
        int n = gip_read(dev, buf, sizeof buf, 500);
        if (n < 0) return 1;            /* unplugged: exit, launchd restarts us */
        if (n == 0) continue;
        if (dump) {
            for (int i = 0; i < n; i++) printf("%02x ", buf[i]);
            printf("\n");
        }
        if (!gip_parse(buf, (size_t)n, &st)) continue;
        if (dump)
            printf("  btn=%04x lt=%u rt=%u L=(%d,%d) R=(%d,%d)\n",
                   st.buttons, st.lt, st.rt, st.lx, st.ly, st.rx, st.ry);
        else
            emit(&st, NULL);
    }
    return 0;
}

int main(int argc, char **argv)
{
    uint16_t vid = 0, pid = 0;
    int dump = 0, seize = 1;
    const char *source = "auto", *preset = "xbox";
    const char *maps[32]; int nmaps = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--list")) return gip_list() > 0 ? 0 : 1;
        else if (!strcmp(argv[i], "--list-hid")) return bt_list();
        else if (!strcmp(argv[i], "--dump")) dump = 1;
        else if (!strcmp(argv[i], "--no-seize")) seize = 0;
        else if (!strcmp(argv[i], "--source") && i + 1 < argc) source = argv[++i];
        else if (!strcmp(argv[i], "--preset") && i + 1 < argc) preset = argv[++i];
        else if (!strcmp(argv[i], "--map") && i + 1 < argc && nmaps < 32) maps[nmaps++] = argv[++i];
        else if (!strcmp(argv[i], "--vid") && i + 1 < argc) vid = (uint16_t)strtoul(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "--pid") && i + 1 < argc) pid = (uint16_t)strtoul(argv[++i], NULL, 0);
        else { usage(argv[0]); return 2; }
    }
    int want_usb = strcmp(source, "bt") != 0;
    int want_bt = strcmp(source, "usb") != 0;
    if (!want_usb && !want_bt) { usage(argv[0]); return 2; }

    struct hidmap map;
    if (hidmap_preset(&map, preset) != 0) { fprintf(stderr, "unknown preset %s\n", preset); return 2; }
    for (int i = 0; i < nmaps; i++)
        if (hidmap_override(&map, maps[i]) != 0) { fprintf(stderr, "bad --map %s\n", maps[i]); return 2; }

    signal(SIGINT, on_sig);
    signal(SIGTERM, on_sig);

    struct gip_dev *dev = want_usb ? gip_open(vid, pid) : NULL;
    if (!dev && !want_bt) {
        fprintf(stderr, "no GIP controller found. Run with --list. Is it in wired/USB mode?\n");
        return 1;
    }
    if (!dump && !(vh = vhid_create())) {
        fprintf(stderr, "could not create virtual HID device (needs root; see README)\n");
        gip_close(dev);
        return 1;
    }

    int rc;
    if (dev) {
        rc = run_usb(dev, dump);
        gip_close(dev);
    } else {
        rc = bt_run(vid, pid, &map, seize && !dump, dump, &stop, emit, NULL);
    }
    vhid_destroy(vh);
    return rc;
}

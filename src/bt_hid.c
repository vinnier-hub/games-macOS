#include "bt_hid.h"
#include <stdio.h>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>

#define VIRTUAL_SERIAL "G7PRO-VIRTUAL"   /* must match vhid_mac.c */

struct bt_ctx {
    const struct hidmap *map;
    struct gp_state st;
    bt_emit_fn emit;
    void *user;
    int dump, seize;
    volatile sig_atomic_t *stop;
    int matched, ticks;
};

static int is_virtual(IOHIDDeviceRef d)
{
    CFTypeRef s = IOHIDDeviceGetProperty(d, CFSTR(kIOHIDSerialNumberKey));
    if (!s || CFGetTypeID(s) != CFStringGetTypeID()) return 0;
    return CFStringCompare(s, CFSTR(VIRTUAL_SERIAL), 0) == kCFCompareEqualTo;
}

static void on_value(void *c, IOReturn r, void *sender, IOHIDValueRef v)
{
    (void)r; (void)sender;
    struct bt_ctx *ctx = c;
    IOHIDElementRef el = IOHIDValueGetElement(v);
    uint32_t key = (IOHIDElementGetUsagePage(el) << 16) | IOHIDElementGetUsage(el);
    long val = IOHIDValueGetIntegerValue(v);
    long min = IOHIDElementGetLogicalMin(el), max = IOHIDElementGetLogicalMax(el);
    if (ctx->dump) {
        printf("page 0x%02x usage 0x%03x  value %ld  range [%ld, %ld]\n",
               key >> 16, key & 0xFFFF, val, min, max);
        return;
    }
    if (hidmap_apply(ctx->map, key, val, min, max, &ctx->st))
        ctx->emit(&ctx->st, ctx->user);
}

static void on_match(void *c, IOReturn r, void *sender, IOHIDDeviceRef d)
{
    (void)r; (void)sender;
    struct bt_ctx *ctx = c;
    if (is_virtual(d)) return;
    CFStringRef name = IOHIDDeviceGetProperty(d, CFSTR(kIOHIDProductKey));
    char buf[128] = "?";
    if (name && CFGetTypeID(name) == CFStringGetTypeID())
        CFStringGetCString(name, buf, sizeof buf, kCFStringEncodingUTF8);
    if (ctx->seize) {
        IOHIDDeviceClose(d, kIOHIDOptionsTypeNone);
        if (IOHIDDeviceOpen(d, kIOHIDOptionsTypeSeizeDevice) != kIOReturnSuccess) {
            fprintf(stderr, "could not seize \"%s\" (grant Input Monitoring? run as root?)\n", buf);
            IOHIDDeviceOpen(d, kIOHIDOptionsTypeNone);
        }
    }
    ctx->matched++;
    fprintf(stderr, "using HID device \"%s\"\n", buf);
    IOHIDDeviceRegisterInputValueCallback(d, on_value, ctx);
}

static void on_remove(void *c, IOReturn r, void *sender, IOHIDDeviceRef d)
{
    (void)r; (void)sender; (void)d;
    struct bt_ctx *ctx = c;
    if (is_virtual(d)) return;
    /* Release everything so nothing stays "held" in games. */
    ctx->st = (struct gp_state){0};
    if (!ctx->dump) ctx->emit(&ctx->st, ctx->user);
}

static void on_tick(CFRunLoopTimerRef t, void *info)
{
    (void)t;
    struct bt_ctx *ctx = info;
    if (*ctx->stop) CFRunLoopStop(CFRunLoopGetCurrent());
    if (++ctx->ticks == 25 && !ctx->matched)
        fprintf(stderr, "no gamepad-type HID device found after 5s. Is the pad paired and connected?\n"
                        "Run --list-hid to see everything macOS can see.\n");
}

static CFMutableDictionaryRef match_dict(int page, int usage, uint16_t vid, uint16_t pid)
{
    CFMutableDictionaryRef d = CFDictionaryCreateMutable(NULL, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    int vals[4] = { page, usage, vid, pid };
    const void *keys[4] = { CFSTR(kIOHIDDeviceUsagePageKey), CFSTR(kIOHIDDeviceUsageKey),
                            CFSTR(kIOHIDVendorIDKey), CFSTR(kIOHIDProductIDKey) };
    for (int i = 0; i < 4; i++) {
        if (i == 2 && !vid) continue;
        if (i == 3 && !pid) continue;
        CFNumberRef n = CFNumberCreate(NULL, kCFNumberIntType, &vals[i]);
        CFDictionarySetValue(d, keys[i], n);
        CFRelease(n);
    }
    return d;
}

static void prop_str(IOHIDDeviceRef d, CFStringRef key, char *out, size_t n)
{
    CFTypeRef v = IOHIDDeviceGetProperty(d, key);
    snprintf(out, n, "?");
    if (v && CFGetTypeID(v) == CFStringGetTypeID())
        CFStringGetCString(v, out, (CFIndex)n, kCFStringEncodingUTF8);
}

static int prop_int(IOHIDDeviceRef d, CFStringRef key)
{
    int x = -1;
    CFTypeRef v = IOHIDDeviceGetProperty(d, key);
    if (v && CFGetTypeID(v) == CFNumberGetTypeID()) CFNumberGetValue(v, kCFNumberIntType, &x);
    return x;
}

static void print_dev(const void *v, void *unused)
{
    (void)unused;
    IOHIDDeviceRef d = (IOHIDDeviceRef)v;
    char name[128], transport[64];
    prop_str(d, CFSTR(kIOHIDProductKey), name, sizeof name);
    prop_str(d, CFSTR(kIOHIDTransportKey), transport, sizeof transport);
    printf("%-34s VID %04x PID %04x  %-10s usage page 0x%02x usage 0x%02x%s\n", name,
           prop_int(d, CFSTR(kIOHIDVendorIDKey)) & 0xFFFF,
           prop_int(d, CFSTR(kIOHIDProductIDKey)) & 0xFFFF, transport,
           prop_int(d, CFSTR(kIOHIDPrimaryUsagePageKey)) & 0xFFFF,
           prop_int(d, CFSTR(kIOHIDPrimaryUsageKey)) & 0xFFFF,
           is_virtual(d) ? "  (our virtual device)" : "");
}

int bt_list(void)
{
    IOHIDManagerRef mgr = IOHIDManagerCreate(NULL, kIOHIDOptionsTypeNone);
    IOHIDManagerSetDeviceMatching(mgr, NULL);
    CFSetRef devs = IOHIDManagerCopyDevices(mgr);
    if (!devs) { printf("no HID devices found\n"); CFRelease(mgr); return 1; }
    CFSetApplyFunction(devs, print_dev, NULL);
    CFRelease(devs);
    CFRelease(mgr);
    return 0;
}

int bt_run(uint16_t vid, uint16_t pid, const struct hidmap *map, int seize, int dump,
           volatile sig_atomic_t *stop, bt_emit_fn emit, void *user)
{
    struct bt_ctx ctx = { .map = map, .emit = emit, .user = user,
                          .dump = dump, .seize = seize, .stop = stop };
    IOHIDManagerRef mgr = IOHIDManagerCreate(NULL, kIOHIDOptionsTypeNone);
    CFMutableDictionaryRef gp = match_dict(0x01, 0x05, vid, pid);   /* Game Pad */
    CFMutableDictionaryRef js = match_dict(0x01, 0x04, vid, pid);   /* Joystick */
    const void *arr[2] = { gp, js };
    CFArrayRef list = CFArrayCreate(NULL, arr, 2, &kCFTypeArrayCallBacks);
    IOHIDManagerSetDeviceMatchingMultiple(mgr, list);
    CFRelease(list); CFRelease(gp); CFRelease(js);

    IOHIDManagerRegisterDeviceMatchingCallback(mgr, on_match, &ctx);
    IOHIDManagerRegisterDeviceRemovalCallback(mgr, on_remove, &ctx);
    IOHIDManagerScheduleWithRunLoop(mgr, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    IOReturn rc = IOHIDManagerOpen(mgr, kIOHIDOptionsTypeNone);
    if (rc != kIOReturnSuccess) {
        fprintf(stderr, "IOHIDManagerOpen failed (0x%x). Grant Input Monitoring to your terminal.\n", rc);
        CFRelease(mgr);
        return 1;
    }
    CFRunLoopTimerContext tc = { 0, &ctx, NULL, NULL, NULL };
    CFRunLoopTimerRef t = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent() + 0.2, 0.2,
                                               0, 0, on_tick, &tc);
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), t, kCFRunLoopDefaultMode);
    fprintf(stderr, "waiting for a Bluetooth/HID gamepad...\n");
    CFRunLoopRun();
    CFRunLoopTimerInvalidate(t); CFRelease(t);
    IOHIDManagerClose(mgr, kIOHIDOptionsTypeNone);
    CFRelease(mgr);
    return 0;
}
#else
int bt_list(void) { fprintf(stderr, "macOS only\n"); return 1; }
int bt_run(uint16_t vid, uint16_t pid, const struct hidmap *map, int seize, int dump,
           volatile sig_atomic_t *stop, bt_emit_fn emit, void *ctx)
{
    (void)vid; (void)pid; (void)map; (void)seize; (void)dump; (void)stop; (void)emit; (void)ctx;
    fprintf(stderr, "Bluetooth HID source is only supported on macOS\n");
    return 1;
}
#endif

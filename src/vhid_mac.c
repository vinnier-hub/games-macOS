#include "vhid.h"
#include "gamepad.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDUserDevice.h>

struct vhid { IOHIDUserDeviceRef dev; };

static void set_num(CFMutableDictionaryRef d, CFStringRef key, int v)
{
    CFNumberRef n = CFNumberCreate(NULL, kCFNumberIntType, &v);
    CFDictionarySetValue(d, key, n);
    CFRelease(n);
}

struct vhid *vhid_create(void)
{
    CFMutableDictionaryRef p = CFDictionaryCreateMutable(NULL, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDataRef desc = CFDataCreate(NULL, hid_report_descriptor, (CFIndex)hid_report_descriptor_len);
    CFDictionarySetValue(p, CFSTR(kIOHIDReportDescriptorKey), desc);
    CFRelease(desc);
    set_num(p, CFSTR(kIOHIDVendorIDKey), 0x045E);    /* Microsoft */
    set_num(p, CFSTR(kIOHIDProductIDKey), 0x02FD);   /* Xbox One S controller */
    set_num(p, CFSTR(kIOHIDVersionNumberKey), 0x0903);
    set_num(p, CFSTR(kIOHIDCountryCodeKey), 0);
    CFDictionarySetValue(p, CFSTR(kIOHIDProductKey), CFSTR("Xbox Wireless Controller"));
    CFDictionarySetValue(p, CFSTR(kIOHIDManufacturerKey), CFSTR("Microsoft"));
    CFDictionarySetValue(p, CFSTR(kIOHIDTransportKey), CFSTR("Bluetooth"));
    CFDictionarySetValue(p, CFSTR(kIOHIDSerialNumberKey), CFSTR("G7PRO-VIRTUAL"));

    IOHIDUserDeviceRef dev = IOHIDUserDeviceCreateWithProperties(NULL, p, 0);
    CFRelease(p);
    if (!dev) return NULL;
    struct vhid *v = calloc(1, sizeof *v);
    v->dev = dev;
    return v;
}

int vhid_send(struct vhid *v, const uint8_t *r, size_t len)
{
    return IOHIDUserDeviceHandleReport(v->dev, r, (CFIndex)len) == kIOReturnSuccess ? 0 : -1;
}

void vhid_destroy(struct vhid *v)
{
    if (!v) return;
    CFRelease(v->dev);
    free(v);
}
#else
struct vhid *vhid_create(void) { fprintf(stderr, "virtual HID is only supported on macOS\n"); return NULL; }
int  vhid_send(struct vhid *v, const uint8_t *r, size_t n) { (void)v; (void)r; (void)n; return -1; }
void vhid_destroy(struct vhid *v) { (void)v; }
#endif

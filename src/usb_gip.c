#include "usb_gip.h"
#include <libusb.h>
#include <stdio.h>
#include <stdlib.h>

#define GIP_CLASS 0xFF
#define GIP_SUBCLASS 0x47
#define GIP_PROTOCOL 0xD0

struct gip_dev {
    libusb_context *ctx;
    libusb_device_handle *h;
    int iface;
    uint8_t ep_in, ep_out;
};

static int find_gip_iface(libusb_device *dev, int *iface, uint8_t *in, uint8_t *out)
{
    struct libusb_config_descriptor *cfg;
    if (libusb_get_active_config_descriptor(dev, &cfg) != 0)
        return 0;
    int found = 0;
    for (int i = 0; i < cfg->bNumInterfaces && !found; i++) {
        const struct libusb_interface_descriptor *a = &cfg->interface[i].altsetting[0];
        if (a->bInterfaceClass != GIP_CLASS || a->bInterfaceSubClass != GIP_SUBCLASS ||
            a->bInterfaceProtocol != GIP_PROTOCOL)
            continue;
        *iface = a->bInterfaceNumber;
        *in = *out = 0;
        for (int e = 0; e < a->bNumEndpoints; e++) {
            uint8_t ea = a->endpoint[e].bEndpointAddress;
            if ((a->endpoint[e].bmAttributes & 3) != LIBUSB_TRANSFER_TYPE_INTERRUPT) continue;
            if (ea & 0x80) { if (!*in) *in = ea; } else if (!*out) *out = ea;
        }
        found = *in != 0;
    }
    libusb_free_config_descriptor(cfg);
    return found;
}

int gip_list(void)
{
    libusb_context *ctx;
    libusb_device **list;
    if (libusb_init(&ctx) != 0) return -1;
    ssize_t n = libusb_get_device_list(ctx, &list);
    int count = 0;
    for (ssize_t i = 0; i < n; i++) {
        struct libusb_device_descriptor dd;
        int ifn; uint8_t in, out;
        if (libusb_get_device_descriptor(list[i], &dd) != 0) continue;
        if (find_gip_iface(list[i], &ifn, &in, &out)) {
            printf("GIP device  VID %04x  PID %04x  (interface %d, ep in 0x%02x out 0x%02x)\n",
                   dd.idVendor, dd.idProduct, ifn, in, out);
            count++;
        }
    }
    libusb_free_device_list(list, 1);
    libusb_exit(ctx);
    return count;
}

struct gip_dev *gip_open(uint16_t vid, uint16_t pid)
{
    struct gip_dev *d = calloc(1, sizeof *d);
    libusb_device **list;
    if (!d || libusb_init(&d->ctx) != 0) { free(d); return NULL; }
    ssize_t n = libusb_get_device_list(d->ctx, &list);
    for (ssize_t i = 0; i < n && !d->h; i++) {
        struct libusb_device_descriptor dd;
        if (libusb_get_device_descriptor(list[i], &dd) != 0) continue;
        if ((vid && dd.idVendor != vid) || (pid && dd.idProduct != pid)) continue;
        if (!find_gip_iface(list[i], &d->iface, &d->ep_in, &d->ep_out)) continue;
        int rc = libusb_open(list[i], &d->h);
        if (rc != 0) {
            fprintf(stderr, "libusb_open %04x:%04x failed: %s (try sudo)\n",
                    dd.idVendor, dd.idProduct, libusb_error_name(rc));
            d->h = NULL;
            continue;
        }
        libusb_set_auto_detach_kernel_driver(d->h, 1);  /* no-op on macOS */
        rc = libusb_claim_interface(d->h, d->iface);
        if (rc != 0) {
            fprintf(stderr, "claim interface failed: %s\n", libusb_error_name(rc));
            libusb_close(d->h); d->h = NULL;
            continue;
        }
        fprintf(stderr, "opened %04x:%04x\n", dd.idVendor, dd.idProduct);
    }
    libusb_free_device_list(list, 1);
    if (!d->h) { libusb_exit(d->ctx); free(d); return NULL; }

    /* Power-on / "start input" packet. The pad stays silent until it sees this. */
    static uint8_t start[] = { 0x05, 0x20, 0x00, 0x01, 0x00 };
    int sent;
    if (d->ep_out)
        libusb_interrupt_transfer(d->h, d->ep_out, start, sizeof start, &sent, 1000);
    return d;
}

int gip_read(struct gip_dev *d, uint8_t *buf, size_t len, unsigned timeout_ms)
{
    int got = 0;
    int rc = libusb_interrupt_transfer(d->h, d->ep_in, buf, (int)len, &got, timeout_ms);
    if (rc == 0) return got;
    if (rc == LIBUSB_ERROR_TIMEOUT) return 0;
    fprintf(stderr, "usb read: %s\n", libusb_error_name(rc));
    return -1;
}

void gip_close(struct gip_dev *d)
{
    if (!d) return;
    libusb_release_interface(d->h, d->iface);
    libusb_close(d->h);
    libusb_exit(d->ctx);
    free(d);
}

#ifndef USB_GIP_H
#define USB_GIP_H
#include <stdint.h>
#include <stddef.h>

struct gip_dev;

/* Print every USB device that exposes a GIP (Xbox One) interface. */
int gip_list(void);
/* Open the first GIP device (optionally matching vid/pid, 0 = any). */
struct gip_dev *gip_open(uint16_t vid, uint16_t pid);
/* Blocking read. >0 bytes read, 0 timeout, <0 fatal (unplugged/error). */
int gip_read(struct gip_dev *d, uint8_t *buf, size_t len, unsigned timeout_ms);
void gip_close(struct gip_dev *d);
#endif

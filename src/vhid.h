#ifndef VHID_H
#define VHID_H
#include <stddef.h>
#include <stdint.h>
struct vhid;
struct vhid *vhid_create(void);   /* NULL on failure (macOS only) */
int  vhid_send(struct vhid *v, const uint8_t *report, size_t len);
void vhid_destroy(struct vhid *v);
#endif

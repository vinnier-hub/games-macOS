CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra
CORE = src/gip.c src/hid_report.c src/hidmap.c
LIBUSB_CFLAGS := $(shell pkg-config --cflags libusb-1.0 2>/dev/null)
LIBUSB_LIBS   := $(shell pkg-config --libs libusb-1.0 2>/dev/null || echo -lusb-1.0)
UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
FRAMEWORKS = -framework IOKit -framework CoreFoundation
endif

g7pro-driver: src/main.c src/usb_gip.c src/vhid_mac.c src/bt_hid.c $(CORE)
	$(CC) $(CFLAGS) $(LIBUSB_CFLAGS) -o $@ $^ $(LIBUSB_LIBS) $(FRAMEWORKS)

test: tests/test_parse.c tests/test_hidmap.c $(CORE)
	$(CC) $(CFLAGS) -o tests/test_parse tests/test_parse.c $(CORE) && ./tests/test_parse
	$(CC) $(CFLAGS) -o tests/test_hidmap tests/test_hidmap.c $(CORE) && ./tests/test_hidmap

clean:
	rm -f g7pro-driver tests/test_parse tests/test_hidmap
.PHONY: test clean

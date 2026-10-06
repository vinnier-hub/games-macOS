# g7pro-driver — GameSir G7 Pro as an Xbox controller on macOS

A small userspace daemon that reads a GameSir G7 Pro and presents it to macOS as a
virtual **Xbox One S controller** (VID 045E / PID 02FD, Bluetooth HID layout), which
macOS' GameController framework, SDL and most games already understand.

```
 wired USB ── GIP (Xbox One protocol) over libusb ─┐
                                                   ├─> gp_state ─> virtual HID Xbox pad
 Bluetooth ── IOHIDManager + usage remapping ──────┘
```

> **Status: untested on real hardware.** It was written without access to a Mac or a
> G7 Pro. The protocol parsing, report building and button mapping are unit tested
> (`make test`); the libusb, IOKit and virtual-device code compiles/behaves as designed on
> paper only. Expect to use `--dump` to tune things. Please report what you see.

## Build

```sh
brew install libusb pkg-config
make            # builds ./g7pro-driver
make test       # unit tests (runs anywhere)
```

## Run

```sh
sudo ./g7pro-driver                 # auto: USB first, otherwise Bluetooth
sudo ./g7pro-driver --source usb
sudo ./g7pro-driver --source bt
```

`sudo` is used because creating a virtual HID device (`IOHIDUserDevice`) is restricted:
non-root processes need Apple's `com.apple.developer.hid.virtual.device` entitlement.
Bluetooth also needs **Input Monitoring** permission for your terminal
(System Settings → Privacy & Security).

### Wired (USB)

Plug the pad in with the USB mode your G7 Pro uses for Xbox/PC (hold the mode button
per GameSir's manual). Check it is visible:

```sh
./g7pro-driver --list      # should print a "GIP device" line with its VID/PID
./g7pro-driver --dump      # raw packets + parsed state while you press buttons
```

If `--list` shows nothing, the pad is not in an Xbox (GIP) mode.

### Bluetooth

Pair the pad in macOS first. The driver then reads it as a plain HID device, remaps it,
and (by default) **seizes** the physical device so games only see the virtual one
(`--no-seize` to disable).

I don't know the G7 Pro's Bluetooth HID layout, so find it empirically:

```sh
sudo ./g7pro-driver --source bt --dump     # prints page/usage/value/range per input
```

Then pick the closest preset and fix up differences:

```sh
--preset xbox       # Xbox One S layout (default)
--preset generic    # DirectInput-style: buttons 1-10 = A B X Y LB RB View Menu L3 R3
--map b3=Y          # button usage 3 -> Y
--map a0x33=LT      # Generic Desktop usage 0x33 -> left trigger
--map 0x0C:0x223=GUIDE   # page:usage form
```

Names: buttons `A B X Y LB RB VIEW MENU L3 R3 GUIDE`; axes `LX LY RX RY LT RT HAT`.
Note: if the pad already pairs as a native Xbox controller, macOS may support it with no
driver at all; try it before installing anything.

## Run at login

```sh
sudo cp g7pro-driver /usr/local/bin/
sudo cp launchd/com.games-macos.g7pro.plist /Library/LaunchDaemons/
sudo launchctl load /Library/LaunchDaemons/com.games-macos.g7pro.plist
```

## Known limitations

- No rumble/force feedback and no LEDs yet (input only).
- No gyro, back paddles or extra G7 Pro buttons; only standard Xbox controls.
- Hot-plug on USB: the daemon exits when the pad is unplugged and launchd restarts it.
- Games that read the pad via the GameController framework should see an Xbox pad; I have
  not verified that macOS accepts this virtual device for GCController on every release.

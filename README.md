# teatime-os

A small bare-metal operating system for the ESP8266, written from scratch in
C and Xtensa assembly with no vendor SDK, RTOS or binary blobs linked in. Its
purpose in life is to serve tea over HTCPCP-TEA (RFC 7168) from a real teapot.

As far as I can tell, this is the first ever fully open-source hardware bringup
for the ESP8266. Hopefully you find this as intriguing as I do, and I hope this
opens up the ESP8266 development environment for more interesting projects in
the future. Obviously, this is incomplete, but there should be enough to figure
out what everything does, and what registers need poking to get things going. 

Have fun!

What it does:

- Preemptive fixed-priority scheduler, semaphores, condition variables,
  timed waits, a first-fit allocator and a device table with
  open/read/write/control.
- Its own WiFi driver: RF, baseband and MAC bring-up by direct register
  access, DMA rings, 802.11 station association, WPA2 4-way handshake and
  hardware CCMP.
- Its own network stack: ARP, IPv4, ICMP, UDP, DNS, DHCP and a single-connection
  TCP.
- An HTCPCP-TEA server that drives a pump and a solenoid over GPIO.
- Fully hardware verified on my home network

## Build

Requires the `xtensa-lx106-elf` GCC toolchain on `PATH`, GNU make 4 or
later, `esptool` and `picocom`. On macOS use Homebrew's `gmake`; the system
`make` is too old.

```sh
cp src/include/ap_secrets.h.example src/include/ap_secrets.h   # fill in your AP
cd compile
make            # prog.bin + prog.irom.bin
make flash      # write both images over the serial port
make monitor    # serial console at 76800 baud
make test       # host-compiled crypto self-test against RFC/FIPS vectors
```

The serial port is found automatically on macOS and Linux; override it with
`USB=/dev/...`. `ESPTOOL`, `PICOCOM`, `HOSTCC` and `BAUD` can be set the same
way. `make test` needs only a host C compiler.

On Linux, add yourself to the `dialout` group to flash without `sudo`, and use
a `picocom` with custom baud rate support (the Debian package has it), since
76800 baud is not a standard rate.

`ap_secrets.h` holds the SSID, passphrase and BSSID of the access point.
The build refuses to run without it by default. The board takes a DHCP
lease at boot; the static addresses in the file are a fallback. If you 
want to only use a static IP, you can comment out the call to spawn the
DHCP process.

## Layout

```
compile/        Makefile and linker scripts
src/system/     kernel: boot, scheduler, interrupts, memory, sync, timers
src/device/     device table, UART, GPIO, WiFi driver, network stack
src/lib/        freestanding string and integer helpers
src/app/        teapot state machine, HTCPCP-TEA server, demo web server
src/test/       on-target test processes
tests/          host-side tests
```

## Credits

The kernel design is heavily inspired by [Xinu](https://github.com/xinu-os/xinu).
Many of the kernel internals, including the process table, scheduler queues, and
allocators are reimplementations of the Xinu algorithms. No Xinu source code is
included.

The ESP8266 hardware knowledge comes from Espressif's
[ESP8266_RTOS_SDK](https://github.com/espressif/ESP8266_RTOS_SDK)
(Apache-2.0), which was used as a reference for register addresses, the memory
map and the linker layout, and from observing the behavior of the vendor
WiFi firmware on the hardware. The SDK and its libraries are not linked into
this project. The only SDK material included is the linker script, which is
derived from the SDK's bootloader linker script and carries its Apache-2.0
notice.

## License

MIT. See [LICENSE](LICENSE).

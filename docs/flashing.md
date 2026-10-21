# Building and flashing

Two ways to get the firmware onto the ESP32. Both use the same YAML, e.g.
[examples/altherma-ech2o.yaml](../examples/altherma-ech2o.yaml). The first flash goes over USB; after that
updates go over WiFi (OTA).

| | A: Home Assistant | B: own computer |
| --- | --- | --- |
| Needs | ESPHome add-on (Device Builder) | [uv](https://docs.astral.sh/uv/), this repository |
| Builds on | the Home Assistant machine | your computer (first build downloads about 1 GB) |
| First flash | USB in the browser (Chrome or Edge) | USB from the terminal |
| Updates | "Install" → wirelessly | `uv run esphome run …` → over WiFi |

## WiFi credentials

The example reads them from `secrets.yaml` next to the YAML:

```yaml
wifi_ssid: "MyNetwork"
wifi_password: "secret"
```

`secrets.yaml` is ignored by git. Without a reachable network the ESP opens its own access point
(`ap:` in the YAML) after a minute.

## A: ESPHome add-on in Home Assistant

1. Install the add-on: Settings → Add-ons → Add-on store → ESPHome Device Builder, start it, open the web UI.
2. Copy `components/` to `/config/esphome/components/` (Samba or File editor add-on), so that
   `/config/esphome/components/daikin_x10a/` and `/config/esphome/components/altherma_control/` exist.
   Once the repository is public, `external_components` can point at GitHub instead.
3. New device → skip the wizard → edit its YAML and paste the example. Change the `external_components` path to
   `components` (relative to `/config/esphome/`) and add the WiFi keys to the add-on's secrets (top right,
   "Secrets").
4. First flash: plug the ESP into the computer with the browser, then Install → "Plug into this computer"
   (Chrome or Edge, Safari has no WebSerial). Alternatively Install → "Manual download" → "Factory format" and
   flash that file as in C.
5. Later: Install → "Wirelessly".
6. Home Assistant finds the device ("Altherma") under Settings → Devices → ESPHome.

## B: Own computer with uv

```sh
brew install uv                       # macOS; Linux/Windows: see the uv docs
git clone <repository> && cd esphome-altherma
printf 'wifi_ssid: "MyNetwork"\nwifi_password: "secret"\n' > examples/secrets.yaml
uv run esphome run examples/altherma-ech2o.yaml
```

`uv run` installs the ESPHome version pinned in `uv.lock`. The first build downloads the ESP-IDF toolchain and
takes several minutes; later builds take about a minute. ESPHome then asks for the port: the USB port on the
first flash (`/dev/cu.usbserial-…` on macOS, `/dev/ttyUSB0` on Linux, `COM3` on Windows), the device on the network
afterwards.

Other commands:

```sh
uv run esphome compile examples/altherma-ech2o.yaml    # build only
uv run esphome logs examples/altherma-ech2o.yaml       # log over WiFi or USB
```

The image for C is then at `examples/.esphome/build/altherma/build/firmware.factory.bin`.

## C: Flash a ready image

For an image built elsewhere (`firmware.factory.bin`, or the "Factory format" download of A). Nothing to build.

* **Browser:** [web.esphome.io](https://web.esphome.io) in Chrome or Edge → Connect → port → Install → the file.
* **Terminal:**

  ```sh
  uvx --from "esptool>=5" esptool --chip esp32 erase-flash                       # optional: also clears stored settings
  uvx --from "esptool>=5" esptool --chip esp32 write-flash 0x0 firmware.factory.bin
  ```

`erase-flash` also deletes what the ESP keeps in flash: the learned UA, the deficit and the controller settings.
Leave it out for an update.

## Trying it without the heat pump

The ESP runs without X10A: the log says "no heat pump answered" once a minute, the values stay empty and the
controller reports "no values from the heat pump". The relay pins switch, which is harmless with nothing wired.

Without a WiFi network (on the road, a phone hotspot), give it only its own access point:

```yaml
wifi:
  ap:
    ssid: "Altherma-Test"
    password: "altherma123"
```

Join that network and open `http://192.168.4.1/altherma/`.

## Troubleshooting

* **Stuck at "Connecting…" while flashing:** hold the BOOT button on the ESP until writing starts.
* **No port:** a charge-only USB cable, or the USB-serial driver is missing (CP210x or CH340, depending on the board).
* **Page does not load although the computer has an address (192.168.4.x or in the home network):** macOS asks
  each browser for access to the local network. Allow it under System Settings → Privacy & Security →
  Local Network, or try Safari.
* **The ESP restarts over and over / no access point:** read the serial log. On macOS:

  ```sh
  (stty 115200; cat) < /dev/cu.usbserial-XXXX > boot.log   # press EN/RST on the ESP, stop with Ctrl-C
  ```

  Interactive: `screen /dev/cu.usbserial-XXXX 115200`, quit with Ctrl-A, K, y. After a crash ESPHome logs the
  backtrace on the next boot; decode it with `addr2line -pfiaC -e <build>/altherma.elf <addresses>` (the
  toolchain's `xtensa-esp32-elf-addr2line`).

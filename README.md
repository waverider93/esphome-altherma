# ESPHome Altherma

ESPHome external component that reads a **Daikin Altherma** heat pump over its **X10A** serial port.
It is a port of [ESPAltherma](https://github.com/raomin/ESPAltherma) by Raomin (MIT License): the X10A
protocol, the value conversions and the model definition files come from there.

Why ESPHome: native Home Assistant API, the local web dashboard of ESPHome (usable without Home Assistant),
any ESP32 board, and own logic on the board (relays, fallback, control).

> Status: work in progress. The code compiles for ESP32 (ESP-IDF). It has **not yet run on a heat pump**.

## Usage

```yaml
external_components:
  - source: {type: local, path: components}   # later: github://<user>/esphome-altherma

uart:
  id: x10a_uart
  rx_pin: GPIO16       # <- X10A pin 2 (TX of the unit)
  tx_pin: GPIO17       # -> X10A pin 3 (RX of the unit)
  baud_rate: 9600
  parity: EVEN

daikin_x10a:
  id: altherma
  model: "ERGA E EHSH-X P30-50 E_EF series 04-08kW-ECH2O"   # file name in components/daikin_x10a/definitions
  update_interval: 30s
  protocol: I          # S for older units
  refrigerant: R32     # for pressure -> temperature conversions
  log_raw: false       # true logs every reply as hex

sensor:
  - platform: daikin_x10a
    label: "Indoor ambient temp. (R1T)"     # label exactly as in the definition file
    name: Room temperature
  - platform: daikin_x10a                   # or without a model: raw position in the reply
    register: 0x62
    offset: 9
    conversion: 105
    data_size: 2
    name: Flow

binary_sensor:
  - platform: daikin_x10a
    label: "Water pump operation"

text_sensor:
  - platform: daikin_x10a
    label: "I/U operation mode"
```

* `label` must match the definition file. When a label exists in several registers, add `register:`.
* Numbers go to `sensor`, on/off bits to `binary_sensor`, everything else to `text_sensor`.
  Temperatures, flow, pressure and current get a unit automatically.
* `id(altherma).is_connected()` is true while the last poll cycle succeeded within three update intervals.

List the models, or the values of one model:

```sh
python3 components/daikin_x10a/definitions.py
python3 components/daikin_x10a/definitions.py "ERGA E EHSH-X P30-50 E_EF series 04-08kW-ECH2O"
```

Full example with web dashboard and (commented) relays: [examples/altherma-ech2o.yaml](examples/altherma-ech2o.yaml).

## Wiring

X10A (5 pins, JST EH 2.5 mm), as documented by ESPAltherma:

| X10A | ESP32 |
| --- | --- |
| 1 5 V | 5 V / VIN (or power the ESP from USB and keep GND) |
| 2 TX | RX (GPIO16 on an ESP32 DevKit) |
| 3 RX | TX (GPIO17) |
| 4 | not connected |
| 5 GND | GND |

ROTEX units have an 8-pin X10A with a different layout, see the ESPAltherma README.

## Differences to ESPAltherma

Not ported (yet): model detection, automatic layout correction of definitions, writing hot water and
leaving water targets, MQTT, the web installer.

## Tests

```sh
g++ -std=c++17 -Icomponents/daikin_x10a tests/test_converters.cpp && ./a.out
esphome compile tests/compile-test.yaml
```

## License

MIT, see [LICENSE](LICENSE). Protocol, conversions and definition files: Copyright (c) 2020 Raomin (ESPAltherma).

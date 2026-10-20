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
  model: auto          # or a definition name, e.g. "ERGA E EHSH-X P30-50 E_EF series 04-08kW-ECH2O"
  language: de         # en, de, fr, it, es: entity names and the diagram page
  diagram: true        # drawing of the installation on /altherma/ (needs web_server)
  update_interval: 30s

sensor:
  - platform: daikin_x10a
    label: "Indoor ambient temp. (R1T)"     # no name: the label in the chosen language
  - platform: daikin_x10a
    label: "Wasserdruck"                    # translated labels work too
  - platform: daikin_x10a                   # raw position in the reply, read as is
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
  - platform: daikin_x10a
    info: detection    # also: model, layout
```

Options of `daikin_x10a`:

| Option | Default | Meaning |
| --- | --- | --- |
| `model` | `auto` | Definition the values are read with. `auto` detects it after boot. |
| `detect` | `true` | Reads every known register once after boot (survey), detects the model, checks a configured one. |
| `layout_check` | `true` | Compares the replies with the layouts of other models and corrects registers that clearly do not match (stored in flash). |
| `language` | `en` | Default entity names and the diagram page: `en`, `de`, `fr`, `it`, `es`. |
| `diagram` | `false` | Serves the drawing of the installation on `/altherma/` and its values on `/altherma/values.json`. Needs `web_server`. |
| `refrigerant` | `auto` | For pressure to temperature conversions; `auto` takes it from the model. |
| `protocol` | `I` | Only for raw values without a known model. |
| `log_raw` | `false` | Logs every reply as hex. |

* `label` is a label of the definition files, in English or translated. When a label exists in several registers of
  the model, add `register:`. The labels are placed on the ESP: after a detection or a layout correction they follow
  the model.
* With `model: auto` and a detection that is not certain, only values that all close models read the same way are
  published; the log says which are held back. Set `model:` to confirm.
* Numbers go to `sensor`, on/off bits to `binary_sensor`, everything else to `text_sensor`.
  Temperatures, flow, pressure and current get a unit automatically.
* `id(altherma).is_connected()` is true while the last poll cycle succeeded within three update intervals.

List the models, or the values of a model (`*` = recommended, shown on the diagram):

```sh
python3 components/daikin_x10a/catalog.py
python3 components/daikin_x10a/catalog.py "ERGA E EHSH-X P30-50 E_EF series 04-08kW-ECH2O" de
```

Full example with diagram, relays and fallback: [examples/altherma-ech2o.yaml](examples/altherma-ech2o.yaml).
Control on the ESP (`altherma_control`: deficit controller with self-calibration, operating modes, fallbacks):
[docs/control-concept.md](docs/control-concept.md).

## Diagram

`diagram: true` adds a page with the drawing of the installation from ESPAltherma's web interface: outdoor unit,
water circuit coloured by temperature, 3-way valve, tank, room, pressure gauge, the state panel and all values. It
works without Home Assistant, light and dark, on a phone too. Open `http://<device>/altherma/`. The page is
`web/altherma.html`; after changing it, run `python3 tools/embed_diagram.py`.

## Relays

The example drives four relays (plus an optional fifth) and falls back to the heat pump's own control when Home
Assistant is gone for 15 minutes. A dead ESP does the same, by the wiring:

| Relay | Terminal | Contact | Off (ESP dead) | On |
| --- | --- | --- | --- | --- |
| Heizanforderung | X12M 15 (H) + 22 (COM) | normally closed | request on | request off |
| Kühlanforderung | X12M 16 (C) + 22 | normally open | no request | cool request |
| Smart Grid 1 / 2 | X15M 3-4 / 5-6 | normally open | normal | see select |
| Sicherheitsabschaltung | in the loop X15M 7-8 | normally closed | runs | stops (error) |

X12M carries 230 V: wiring by an electrician. The heat pump uses the contacts only with the field settings
`[C-07]=1` (external room thermostat), `[C-05]=1` (one contact; `2` with separate heat/cool contacts) and
`[D-01]=3` (Smart Grid). Check the relay pins of your board first.

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

Ported from the 2.0 beta: model detection (survey.h, detect.h), layout correction (layoutfix.h), the label catalog
with its translations (catalog.h, generated catalog_data.h), and the diagram of the web interface. These headers
are vendored unchanged except for a guard (see their first lines).

Not ported: writing hot water and leaving water targets, MQTT, the web installer, the configuration pages (ESPHome
has its own).

## Tests

```sh
g++ -std=c++17 -Icomponents/daikin_x10a tests/test_converters.cpp && ./a.out
g++ -std=c++17 -Icomponents/altherma_control tests/test_deficit_model.cpp && ./a.out
for t in test_survey test_detect test_layoutfix; do   # ESPAltherma's own tests, on the vendored headers
  g++ -std=c++17 -w -DDAIKIN_X10A_ESPALTHERMA_IMPL -Itests/espaltherma -Icomponents/daikin_x10a \
    tests/espaltherma/$t.cpp -o /tmp/$t && /tmp/$t
done
esphome compile tests/compile-test.yaml      # no network
python3 tools/make_compile_test.py           # the example without WiFi, for builds without downloads
esphome compile tests/compile-diagram.yaml   # Ethernet, API, diagram, relays, control
```

## License

MIT, see [LICENSE](LICENSE). Protocol, conversions, catalog, detection, layout correction, tests and the diagram
page: Copyright (c) 2020 Raomin (ESPAltherma).

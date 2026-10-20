# Setting up the heat pump for the ESP controller

How to set a Daikin Altherma 3 (tested on paper with EHSX04P50EF + ERGA04EV, Madoka BRC1HHDA) so that
`altherma_control` decides **when** it heats and the heat pump decides **how**. Field settings are made on the
Madoka (installer level). Write down the current values before changing anything: going back is the same list in
reverse.

## Order

1. **Read only.** Flash the ESP, connect X10A, nothing on the relays. Compare the values with what you know
   (Madoka, P1P2, Daikin app) for a few days. Turn on `all_values` and look for odd ones.
2. **Shadow.** Select `Regelung: Home Assistant` (or leave the heat relay unwired): the ESP calculates and shows
   when it would switch, the heat pump still runs as before. One or two weeks, then compare starts and run times.
3. **Wire** the relays (X12M carries 230 V: electrician). Relay unpowered = heat request on, so a dead ESP heats.
4. **Field settings** below, then `Regelung: ESP`.
5. Watch the first days: starts per day, `Regler Eigenstopps WP`, room temperature. Adjust start/stop and the
   minimum run and pause on the ESP page or in Home Assistant.

## Field settings

| Setting | Meaning | For the ESP controller | Why |
| --- | --- | --- | --- |
| `[C-07]` | Unit control | `1` external room thermostat | The heat pump heats while H is closed. The Madoka stops switching on its own. |
| `[C-05]` | Thermostat contacts main zone | `1` one contact (H) | Heat or cool is chosen on the Madoka / P1P2. `2` = H and C decide the mode (then wire C and interlock the relays). |
| `[D-01]` | Contact on X15M 3-4 | `3` Smart Grid | Only with the SG relays wired. If X15M 3-4 already carries a utility (EVU) lock-out contact, keep it and leave SG out. |
| `[D-00]` | Heaters allowed during lock-out | `0` | No backup heater while the utility locks the heat pump. |
| Heating curve | Leaving water temperature | keep | The ESP only says when; the curve still sets the temperature. A low curve gives long, quiet runs. |
| `[4-02]` | Heating off above outside temperature | about `18` °C | Otherwise the heat pump refuses requests in mild weather, when the controller wants to heat in a few long runs. |
| `[9-04]` | Leaving water overshoot | keep (max `4` K) | Above setpoint + overshoot the compressor stops by itself. The controller recognises that (`Eigenstopps`), ends the request and starts its pause. |
| `[8-05]` | Leaving water modulation (room) | no effect | Only used with Madoka room control (`[C-07]=2`). |
| `[2-0A]` | Madoka room sensor offset | keep | The controller reads the Madoka room temperature; check after the switch that the ESP shows the corrected value. |
| `[F-0D]` | Pump during thermo off | keep | The pump stops in the pauses, which is what saves electricity. |
| Hot water | Tank schedule and setpoints | keep | Not touched by the controller; hot water runs do not count as heating. |

To check after switching to `[C-07]=1`:

* The Madoka still shows and sends its room temperature (the ESP needs it). Without it the ESP reports
  "no values from the heat pump" and keeps the heat request on: the heat pump then runs on its curve.
* `Thermostateingang Heizen` follows the relay.
* The room setpoint on the Madoka is still the setpoint the controller uses (`Raum Soll`).

## Going back

Set `Regelung` to Home Assistant and the heat relay to on (request on), or simply power off the ESP, then set
`[C-07]` back (here it was `2`, Madoka room control). The heat pump then runs exactly as before.

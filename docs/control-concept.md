# Control concept

The heat pump is driven through its room thermostat input (X12M 15/16). The controller runs on the ESP
(`components/altherma_control`), uses only values of the heat pump read over X10A, calibrates itself and needs no
network. Home Assistant is optional: it watches, keeps the history, and can change the settings, the Madoka
setpoint and Smart Grid.

## Layers

| Layer | Task | Needs |
| --- | --- | --- |
| 1. Heat pump | heating curve when the ESP is dead (heat relay normally closed on X12M 15) | nothing |
| 2. ESP | deficit controller, self-calibration, settings and state kept in flash | X10A only |
| 3. Home Assistant (optional) | display, history, settings, setpoint, Smart Grid | network, never required |

## Operating modes ("Regelung" on the ESP)

Exactly one side switches the heat request; the other is ignored (logged as a warning).

| Mode | Switches the heat request | Home Assistant gone for 15 min |
| --- | --- | --- |
| ESP (default) | the controller on the ESP | Smart Grid back to Normal, nothing else changes |
| Home Assistant (tests) | Home Assistant through "Heizanforderung"; the controller keeps calculating without switching | Smart Grid Normal, Regelung back to ESP |

Cooling ("Kühlanforderung") is switched by hand in both modes; the controller does not heat while it is on. The
safety stop can be switched in every mode.

| Situation | Heat request | Smart Grid |
| --- | --- | --- |
| Everything running | controller | from Home Assistant |
| Home Assistant gone | controller | Normal after 15 min |
| X10A gone (no room, setpoint or outside temperature) | on: heating curve | unchanged |
| ESP dead or unpowered | on (normally closed contact): heating curve | Normal (contacts open) |

## Deficit

Every minute:

```
deficit += (UA x (room - outside) - heat delivered) x 1 min
deficit += (k x (setpoint - room) - deficit) x 1 min / 12 h      room anchor
deficit += k x setpoint change                                   at once, when the setpoint changes
```

| Value | Source (X10A) |
| --- | --- |
| room, setpoint | Madoka: Indoor ambient temp. (R1T), RT setpoint |
| outside | R1T-Outdoor air temp. |
| heat delivered | flow x 4.18 x (leaving R2T - inlet R4T) / 60 kW; 0 while the 3-way valve is on hot water |
| UA | learned (start value 90 W/K) |
| k (room gain) | setting, 5 kWh/K |

The room anchor keeps model errors from adding up: if the room stays too warm, the deficit shrinks bit by bit, and
the other way round. Sun, appliances and the fireplace are not measured; on average they are inside the learned UA,
and the anchor handles a sunny day.

Example: 5 °C outside, 21.5 °C inside, UA 90 W/K gives 1.5 kW loss, so +1.5 kWh deficit per hour without heating:
the start threshold of 3 kWh after 2 h. With 2.5 kW from the heat pump the deficit falls by 1 kWh per hour: back at
0 after about 3 h of running.

## Self-calibration

Over a window of 72 h: UA measured = heat delivered / sum of (room - outside) x hours. It counts only when the room
ends the window within 0.3 K of where it started (the floor stored nothing on balance), the values were there for
90 % of the time, and it was on average at least 5 K colder outside than inside. The new UA is 70 % old + 30 %
measured, between 20 and 400 W/K.

UA, the deficit and the running window are saved in flash every 10 minutes. After a power cut the controller goes on
at once with the last UA: there is no gap in heating, the calibration only refines. Setting UA by hand starts a new
window from that value.

## Switching rules (settings, from Home Assistant or the ESP web page)

| Rule | Default |
| --- | --- |
| Start | deficit >= 3 kWh, after the minimum pause |
| Stop | deficit <= 0 kWh, after the minimum run time |
| Minimum run time | 2 h |
| Minimum pause | 1 h |
| Room gain k | 5 kWh/K |
| Room too cold | room < setpoint - 0.5 K: on at once (`room_low` in the YAML); the deficit is raised to k x (setpoint - room) |
| Room too warm | room > setpoint + 1 K: off at once, even within the minimum run time (`room_high`); an open deficit is dropped to k x (setpoint - room) |
| Heat pump stopped itself | compressor off for 5 min during the request and deficit < start: request off, the pause starts |
| Hot water | left to the heat pump |

Start threshold, minimum run time and minimum pause prevent the cycling. With `compressor_frequency` set (in the
example), run and pause count real compressor time: a run starts when the compressor does, not the relay, and a
pause when it stops.

At the room limits the room is right and the model is not. Without the correction a warm room would end a run with
deficit left, and the next start would follow right after the minimum pause.

Daikin's own stops remain: the overshoot `[9-04]` ends a run when the leaving water reaches its target + 4 K. The
controller counts them (`self_stops`) and ends its request too, so the heat pump does not restart on its own a few
minutes later. Fewer of them need a fitting heating curve.

## Field settings

See [heat-pump-settings.md](heat-pump-settings.md): order of the switch-over, every setting with its reason, and how
to go back.

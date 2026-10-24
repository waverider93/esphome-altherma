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
| UA | learned (start value 60 W/K) |
| k (room gain) | setting, 14 kWh/K: the heat the floor stores per kelvin of room |

The room anchor keeps model errors from adding up: if the room stays too warm, the deficit shrinks bit by bit, and
the other way round. Sun, appliances and the fireplace are not measured; on average they are inside the learned UA,
and the anchor handles a sunny day.

Example: 5 °C outside, 21.5 °C inside, UA 90 W/K gives 1.5 kW loss, so +1.5 kWh deficit per hour without heating:
the start threshold of 3 kWh after 2 h. With 2.5 kW from the heat pump the deficit falls by 1 kWh per hour: back at
0 after about 3 h of running.

## Self-calibration

Over a window of 72 h, moved on every 24 h (each day looks back over the last three): UA measured = (heat delivered - k x room change) / sum of (room - outside) x hours. The
k x room change is the heat the floor stored (room warmer at the end) or gave back, so every window counts, not only
those that end as warm as they started. It counts when the room moved less than 2 K, the values were there for 90 % of
the time, and it was on average at least 5 K colder outside than inside. Windows a day apart share two thirds of their
data, so the new UA is 2/3 old + 1/3 measured, between 20 and 400 W/K: after a change it is half way there in about
two days and nearly there in a week. The first calibration comes after 72 h.

Why the storage term matters: when the room moves 1.75 K over a window (as it did on 23-24 October), leaving it out
puts UA about 30 W/K off; with k off by 3 kWh/K it is about 6 W/K.

The learned UA is the effective one: the heat the heat pump has to deliver per kelvin. Appliances, people and sun
heat the house too, and the room temperature shows it, so on average they are inside. In the house this was built
for (20-24 October 2026) the windows gave 54-58 W/K with k = 14 kWh/K, while the building itself loses about 93 W/K:
about a third came from the household electricity. When that changes (holidays, winter), UA follows within a few
days.

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
| Room gain k | 14 kWh/K |
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

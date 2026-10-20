# Control concept (stage 3, planned)

How the ESP is meant to drive the heat pump through the room thermostat input. **Not implemented yet**: today the
example only has the relays and a fallback; the controller below is the plan. The shadow controllers in Home
Assistant (`waermebilanz_schattenregler*.yaml` in the Home Assistant config) run the same model without switching.

## Operating modes

Select "Regelung" on the ESP. Exactly one side switches the relays; the other side is ignored (logged as a warning).

| Mode | Decides heat and cool | Smart Grid | Home Assistant needed | Home Assistant gone for 15 min |
| --- | --- | --- | --- | --- |
| ESP | controller on the ESP, UA/C fixed in the YAML, no gains | always Normal | no, `api:` can be removed | nothing changes |
| Home Assistant | Home Assistant through Heizanforderung/Kühlanforderung; the ESP only executes | from Home Assistant | yes | heat request on (heating curve), Smart Grid Normal |
| ESP + HA (recommended) | controller on the ESP; Home Assistant supplies UA/C, gains and Smart Grid | from Home Assistant | for the inputs only | the ESP goes on with the last values, Smart Grid Normal |

The safety stop can be switched in every mode. Until the controller runs on the ESP, its modes keep the heat
request on (heating curve).

## Who decides what

| Situation | Heat request (X12M 15) | Cool request (X12M 16) | Smart Grid 1/2 | Safety contact |
| --- | --- | --- | --- | --- |
| Everything running (ESP + HA) | deficit controller on the ESP, gains and UA/C from Home Assistant | mode from Home Assistant | from Home Assistant (PV, price) | closed |
| Home Assistant gone | ESP keeps controlling with the last UA/C, without gains | unchanged | Normal after 15 min | closed |
| X10A gone (no room temperature) | on, the heat pump follows its heating curve | off | Normal | closed |
| ESP dead or unpowered | on (normally closed contact), heating curve | off | Normal | closed |
| Manual (ESP web page) | as set | as set | as set | as set |

Until the controller runs on the ESP, the example's fallback after 15 minutes without Home Assistant is the heat
request on (heating curve), Smart Grid Normal, safety contact closed.

## Deficit

Every minute:

```
deficit += (heat loss - heat delivered - gains) x 1 min
```

| Term | Formula | Source |
| --- | --- | --- |
| Heat loss | UA x (room - outside) | UA from the Home Assistant calibration (about 70-105 W/K so far); room = Madoka (Indoor ambient temp. R1T), outside = R1T-Outdoor air temp., both over X10A |
| Heat delivered | flow x 4.18 x (leaving - inlet) / 60 [kW, flow in l/min] | X10A: Flow sensor, Leaving water temp. after BUH (R2T), Inlet water temp. (R4T); only while the 3-way valve is on space heating, so hot water runs do not count |
| Gains | PV, waste heat of the household power, fireplace | Home Assistant only; 0 without it, so the ESP rather heats a little too much |
| Room anchor | the deficit is pulled towards C x (setpoint - room) with a time constant of about 12 h | C (thermal capacity) from Home Assistant, setpoint = Madoka (RT setpoint) |
| Setpoint change | deficit += C x change of setpoint, at once | as in Home Assistant |

The room anchor keeps model errors (wrong UA, missing gains) from adding up: if the room stays too warm, the deficit
shrinks bit by bit, and the other way round.

Example: 5 °C outside, 21.5 °C inside, UA 90 W/K gives 1.5 kW loss, so the deficit grows by 1.5 kWh per hour
without heating and reaches the start threshold of 3 kWh after 2 h. With the heat pump delivering 2.5 kW it falls by
1 kWh per hour: back at 0 after about 3 h of running.

## Switching rules

| Rule | Proposal |
| --- | --- |
| Start | deficit >= 3 kWh |
| Stop | deficit <= 0 |
| Minimum run time | 2 h |
| Minimum pause | 1 h |
| Room too cold | room < setpoint - 0.5 K: on at once |
| Room too warm | room > setpoint + 1 K: off, even within the minimum run time |
| Hot water | left to the heat pump |

Start threshold, minimum run time and minimum pause prevent the cycling. Daikin's own stops remain: the overshoot
`[9-04]` ends a run when the leaving water reaches its target + 4 K. Only a fitting heating curve helps there, not
the relay.

## Field settings

`[C-07]=1` (external room thermostat; the Madoka stays control panel and sensor), `[C-05]=1` (one contact: H asks,
heating or cooling is chosen in software; `2` with separate heat and cool contacts), `[D-01]=3` (Smart Grid on
X15M 3-4 / 5-6). Check the heating curve and `[4-02]` before switching over.

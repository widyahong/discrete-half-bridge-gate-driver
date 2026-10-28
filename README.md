# Discrete Half-Bridge Gate Driver

An N-channel half-bridge gate driver built from general-purpose BJTs (one NPN type, one PNP type) and a handful of resistors, with no gate driver IC. It is controlled directly by two 3.3 V MCU pins (`HIN` and `LIN`) and targets DC buses up to roughly 24 V (28 V maximum).

> **Scope:** low-voltage DC buses only (up to about 28 V). Not suitable for mains or high-voltage buses. See [Bus voltage limits](#bus-voltage-limits) for alternatives.

## Architecture

The BJT driver is current-controlled: every drive current is set by a plain series resistor, nothing is ratioed against anything else, and there is no gate-oxide voltage to protect. The same circuit therefore works from a 12 V rail up to VB without extra parts.

Each side (low and high) is the same circuit, two blocks in series:

```
MCU pin -> Rb -> [Level shifter: Q1 NPN -> Rc -> Q2 PNP] -> N -> [Output stage: Q3 NPN / Q4 PNP push-pull] -> Rg -> Power MOSFET gate
```

| Block | Contents | Role |
|---|---|---|
| Level shifter | Q1 (NPN), Q2 (PNP) | Q1 sinks a small current referenced to the MCU ground; that current turns on Q2 at the top rail. Carries little current; its job is voltage translation only. |
| Output stage | Q3 (NPN), Q4 (PNP) | Complementary emitter followers. Q3 pulls the gate up, Q4 pulls it down, both actively. Its job is gate current only. |

Low-side and high-side differ only in the rails. The high side additionally has the bootstrap supply.

| | Vtop | Vbot | Q1 emitter | Power MOSFET source |
|---|---|---|---|---|
| Low-side | +12 V | GND | GND | GND |
| High-side | VB (bootstrap) | VS (half-bridge midpoint) | GND | VS |

The Q1 emitter is always the MCU ground, also on the high side. Pin high = power MOSFET ON (non-inverting). With the MCU pin floating, Rpd holds Q1 off and the power MOSFET stays OFF.

## Pin legend

BC337 and BC327 share the same TO-92 pinout:

```
      _____
     /     \      flat face toward you,
    |  BC   |     legs pointing down
    |_______|
      | | |
      C B E
```

Every drawing below marks the pins of each transistor as `C`, `B`, `E`.

| Part | E | B | C |
|---|---|---|---|
| Q1 BC337 (NPN) | GND | Rb / Rpd node | Rc |
| Q2 BC327 (PNP) | Vtop | Rbe / Rc node | N |
| Q3 BC337 (NPN) | OUT | N | Vtop |
| Q4 BC327 (PNP) | OUT | N | Vbot |

## 1. Level shifter (current mode), one channel

```
 Vtop  (+12 V low side | VB high side)
      |
      +----------------+
      |                |
      |                E
    [Rbe]         +---------+
     4.7k         |   Q2    |
      |           |  PNP    |
      +------B----|  BC327  |
      |           +---------+
     [Rc]              C
     22k               |
      |                +-----> N  (to output stage)
      C
 +---------+
 |   Q1    |
 |  NPN    |-----B-----[Rb 4.7k]-----+----- MCU pin (HIN or LIN)
 |  BC337  |                         |
 +---------+                       [Rpd]
      E                             10k
      |                              |
     GND                            GND
```

- MCU high drives about 0.5 mA into Q1 through Rb, which is enough to saturate it.
- Q1 pulls the base current of Q2 through Rc: about (Vtop - 0.9 V) / Rc, i.e. roughly 0.5 mA with a 12 V rail and 1.6 mA with VB at 36 V. Q2 then connects Vtop to node N.
- **Rbe** (Q2 base to emitter) keeps Q2 off when Q1 is off and removes Q2's stored charge at turn-off.
- **Rpd** (Q1 base to GND) keeps Q1 off when the MCU pin is floating or high-impedance at boot.
- Q1 is the only device that sees the full VB-to-GND voltage. Q2, Q3 and Q4 only see Vtop - Vbot (about 12 V).

## 2. Output stage (push-pull emitter follower), per power MOSFET

```
 Vtop ----------------+
                      |
                      C
                 +---------+
 N ---+----B-----|   Q3    |
      |          |  NPN    |
      |          |  BC337  |
      |          +---------+
      |               E
      |               |
      |               +----------- OUT ---[Rg 47R]---- G   power MOSFET
      |               |                                |
      |               E                             [Rgs 47k]
      |          +---------+                           |
      +----B-----|   Q4    |                           |
      |          |  PNP    |                           |
      |          |  BC327  |                           |
      |          +---------+                           |
      |               C                                |
  [Rn 4.7k]           |                                |
      |               |                                |
 Vbot +---------------+--------------------------------+--- S   power MOSFET
```

- Low-side: Vtop = +12 V, Vbot = GND.
- High-side: Vtop = VB, Vbot = VS (half-bridge midpoint).
- Q3 and Q4 share the base node N and the emitter node OUT, so only one of them can conduct: the output stage has no shoot-through of its own.
- Gate swing: about Vtop - 0.9 V when on (roughly 11 V on the low side, 10.5 V on the high side) and Vbot + 0.7 V when off. Rgs pulls the gate the rest of the way down.
- **Rn** (N to Vbot) pulls N low when Q2 is off, and carries the base current of Q4 during turn-off. With Rg = 47 ohm the peak turn-on current is about 200 mA and the peak turn-off current about 130 mA. Lower Rg to 33 ohm for faster edges; peaks stay well below the 800 mA rating of both parts.
- Place 100 nF between Vtop and Vbot close to Q3 and Q4 to supply the gate-current pulses.

## 3. Bootstrap supply

```
 +12 V --|>|-- [10R] --+-- VB
         SB160         |
                    [1 uF X7R 25 V]
                       |
                      VS (midpoint)
```

- The capacitor charges while VS is low, i.e. while the low-side MOSFET is on. **Turn the low side on briefly at start-up** before using the high side.
- While the high side is on, the capacitor supplies about 4 mA: Rn about 2.3 mA, Rc about 1.6 mA (the level-shift current returns to GND through Q1), Rgs about 0.2 mA. With a 1 V droop budget, continuous high-side ON time is limited to about 0.2 ms with 1 uF, or about 2 ms with 10 uF. 100% duty is not possible without a charge pump.

## Bill of materials (through-hole)

| Position | Part | Notes |
|---|---|---|
| Power MOSFET (x2) | IRFZ44N (55 V, thin margin at 24 V), IRF540N (100 V) or IRFB4310 (100 V), TO-220 | 10 V Vgs matches the 12 V rail |
| Q1, Q3 (x4) | BC337-40 (NPN, TO-92, 45 V, 800 mA) | 2 per channel; any hFE group works, -40 preferred |
| Q2, Q4 (x4) | BC327-40 (PNP, TO-92, -45 V, 800 mA) | 2 per channel |
| Bootstrap diode | SB160 (Schottky 60 V, DO-41) or UF4002 | |
| Bootstrap capacitor | 1 uF X7R 25 V, radial ceramic | Qg 60 nC gives about 0.1 V droop |
| 4.7 k x6 | 1/4 W axial | Rb, Rbe, Rn (3 per channel) |
| 10 k x2 | 1/4 W axial | Rpd (Q1 base pull-down) |
| 22 k x2 | 1/4 W axial | Rc (level-shift current) |
| 47 ohm x2 | 1/4 W axial | Rg |
| 47 k x2 | 1/4 W axial | Rgs, gate-to-source on the power MOSFETs |
| 10 ohm | 1/4 W axial | Bootstrap series resistor |
| 100 nF x3 | Radial ceramic | Decoupling |
| 10 uF | Electrolytic, 25 V | Supply decoupling |

Small-signal transistor count for two channels: 8 in total (4 BC337, 4 BC327). Only two transistor types are used in the whole design.

## Usage

- **Control:** two MCU pins, `HIN` and `LIN`. Dead time comes from firmware or a hardware timer; start at 5 us and reduce it while measuring.
- **Supply sequencing:** the 12 V gate rail must come up before or together with the bus. Add 10 uF of decoupling close to the circuit.
- **MCU pins:** choose pins that are high-impedance or low at boot (avoid strapping pins, e.g. on the ESP32).
- **Bring-up:** low side first with an LED as load, then the high side with no power load, then the full half-bridge with a small load.
- **Limits:** 24 V bus (28 V maximum). VB reaches about 36 V (40 V at 28 V), which Q1 must hold off against its 45 V rating, so keep VS ringing small.

## Test firmware

[`firmware.ino`](firmware.ino) is a single-file ESP32 sketch (Arduino-ESP32 core, no external libraries) for bring-up. Pins, test mode, frequency, duty, dead time and bootstrap charge time start from the defaults at the top of the file and can be overridden at run time over Serial (115200 baud, newline-terminated), for example `mode=half`, `freq=10000`, `duty=30`, `dead=3`. Type `help` for the full list; `show`, `start`, `stop`, `charge` and `defaults` are also available. Three modes are provided: low-side blink, high-side PWM, and complementary half-bridge PWM. The default dead time is 5 us. Note that the Arduino IDE requires the sketch to sit in a folder with the same name as the `.ino` file.

## Bus voltage limits

| Bus | Option |
|---|---|
| up to about 24 V (28 V max) | This circuit as drawn. Lower buses need no changes; nothing in the level shifter is ratioed. |
| about 28-50 V | Only Q1 (it sees VB to GND) and the bootstrap diode see the full voltage. Replace both with 100 V parts, use a 1/2 W Rc, and choose the power MOSFET accordingly. Q2, Q3 and Q4 only see the 12 V rail. Not verified on hardware. |
| about 50-100 V | Dedicated half-bridge gate driver IC (e.g. IR2184, IR2110 class) |
| hundreds of volts | Use a gate driver IC: a high-voltage half-bridge driver (e.g. IR2110, IR2113) or an isolated driver (e.g. UCC21520, Si827x class). Pulse-transformer or set/reset latch schemes are the discrete alternative. This level shifter is not viable here. |

## Known limitations

- Bootstrap limits the high-side ON time (about 0.2 ms with 1 uF) and the maximum duty cycle. For BLDC drives, PWM the low side or add a charge pump.
- Q2 saturates lightly, so its storage time dominates the turn-off delay. Start with 5 us of dead time and reduce it while measuring. Pulses shorter than about 2 us may not come through cleanly.
- Fast VS edges can couple through the collector capacitance of Q1 into its base and falsely turn on the high side. Keep Rb and Rpd as specified (low impedance), slow the edges with a larger Rg if needed, and check the high-side gate on a scope at full bus voltage.
- On a 24 V bus Q1 sees about 36 V against a 45 V rating; ringing on VS eats into that margin.
- Gate swing is about 0.9 V below Vtop, so a lower gate rail directly lowers the power MOSFET Vgs.
- No UVLO, interlock or overcurrent protection. Add current sensing and protection for motor loads.
- No galvanic isolation from the MCU. Do not use directly on mains.
- Currents and timings in this document are hand calculations for BC337-40 / BC327-40, not measurements. Verify on the bench.

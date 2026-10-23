# Discrete Half-Bridge Gate Driver

An N-channel half-bridge gate driver built from small-signal MOSFETs and a handful of passives, with no gate driver IC. It is controlled directly by two 3.3 V MCU pins (`HIN` and `LIN`) and targets DC buses up to roughly 28 V.

> **Scope:** low-voltage DC buses only (up to about 30 V). Not suitable for mains or high-voltage buses. See [Bus voltage limits](#bus-voltage-limits) for alternatives.

## Architecture

Each side (low and high) consists of two blocks in series:

```
MCU (A, B) -> [Cross-coupled level shifter] -> X -> [Output stage: N/P inverter] -> Rg -> Power MOSFET gate
```

| Block | Contents | Role |
|---|---|---|
| Level shifter | P1, P2, N1, N2 (cross-coupled latch) | Translates the 3.3 V MCU signal into a full-swing signal at node X. Carries little current; its job is voltage only. |
| Output stage | P3, N3 (complementary inverter) | Sources and sinks the large gate current. Its job is current only. |

The output stage is identical in every case. Low-side and high-side differ only in the supply rails and in the level shifter protection.

| | Vtop | Vbot | P-gate protection |
|---|---|---|---|
| Low-side | +12 V | GND | Not required |
| High-side | VB (bootstrap) | VS (half-bridge midpoint) | Required for bus > 6-7 V |

## 1. Level shifter (cross-coupled), one channel

```
        Vtop (+12 V for low-side, VB for high-side)
          |                       |
        S | P1 (BS250)          S | P2 (BS250)
   +---- G                  +---- G
   |      D                 |      D
   |      |                 |      |
   |    [470R]              |    [470R]
   |      |                 |      |
   |      +---- X           |      +---- Y
   |      |      \         /|      |
   |      |       \_______/ |      |
   |      |        cross    |      |
   |    D | N1 (2N7000)   D | N2 (2N7000)
   | A -- G                G -- B
   |    S |                S |
   |     GND               GND
```

- Cross-coupling: gate of P1 <- Y, gate of P2 <- X.
- `A` is the MCU signal, `B` is its complement.
- **X** is the output. A high on A pulls X low; the output stage then inverts it, so the power MOSFET gate turns ON (non-inverting from the firmware's point of view).
- The **470 ohm resistor in each P drain** weakens the P devices so the N devices win during transitions (a 2N7000 at 3.3 V is not much stronger than a BS250).
- **Complement generator B** (one per channel): one 2N7000 with its gate on A, source on GND, and drain pulled up to 3.3 V through 100 k. That drain is B.
- MCU pin: 100 ohm in series, with a 100 k pull-down on the gate of N1 (pin A).
- Default state with the MCU pin floating: A low, B high, power MOSFET OFF.

## 2. High-side gate protection (P devices)

```
 VB ---+--------------+
       |              | Zener 15 V
       |              | cathode to VB, anode to GP
       |       X --[10k]--+-- GP2 --> gate of P2 and inverter input
       |                  |
       +---- Zener -------+

 (Identical for Y --[10k]-- GP1 --> gate of P1, with its own Zener)
```

- Without this, the P gate is pulled to 0 V while its source sits at VB (about 35 V). The resulting Vgs of -35 V exceeds the BS250 rating (±20 V).
- The Zener clamps the P Vgs at about -15 V; the 10 k resistor limits the current N1 can push through the Zener.
- Trade-offs: static current of about (VB - 15 V) / 10 k = 2 mA while VS is high, and edge times of around 1 us (adequate for 20-50 kHz PWM).
- The inverter input is taken from **GP2**, not directly from X.

## 3. Output stage (N/P inverter), per power MOSFET

```
 Vtop ---- S  P3 (BS250)
              G --+ (from X, or GP2 on the high side)
 input -----------+
              G --+
 Vbot ---- S  N3 (2N7000)
                      D(P3) --+-- D(N3)
                              |
                           [Rg 47R]
                              |
                          Power MOSFET gate --[Rgs 10k]-- Power MOSFET source
```

- Low-side: Vtop = +12 V, Vbot = GND.
- High-side: Vtop = VB, Vbot = VS (half-bridge midpoint).
- Place 100 nF between Vtop and Vbot close to each output stage to absorb the brief shoot-through current spike.

## 4. Bootstrap supply

```
 +12 V --|>|-- [10R] --+-- VB
         SB160         |
                    [1 uF X7R 25 V]
                       |
                      VS (midpoint)
```

- The capacitor charges while VS is low, i.e. while the low-side MOSFET is on. **Turn the low side on briefly at start-up** before using the high side.
- Continuous high-side ON time is limited to about 0.3 ms (1 uF, discharged at about 2 mA through the Zener). Use 10 uF for roughly 3 ms. 100% duty is not possible without a charge pump.

## Bill of materials (through-hole)

| Position | Part | Notes |
|---|---|---|
| Power MOSFET (x2) | IRFZ44N (55 V, thin margin at 24 V), IRF540N (100 V) or IRFB4310 (100 V), TO-220 | 10 V Vgs matches the 12 V rail |
| N1, N2, N3, B generator | 2N7000 or BS170 (TO-92, 60 V, ±20 V) | 3 per channel + 1 B generator; check threshold spread at 3.3 V |
| P1, P2, P3 | BS250 (TO-92, -45 V, ±20 V) | 3 per channel |
| High-side Zener (x2) | 1N5245B or BZX55C15 (15 V, 0.5 W, DO-35) | |
| Bootstrap diode | SB160 (Schottky 60 V, DO-41) or UF4002 | |
| Bootstrap capacitor | 1 uF X7R 25 V, radial ceramic | Qg 60 nC gives about 0.1 V droop |
| 470 ohm x4 | 1/4 W axial | P-side weakening |
| 10 k x2 | 1/4 W axial | High-side only |
| 47 ohm x2 | 1/4 W axial | Rg |
| 10 k x2 | 1/4 W axial | Gate-to-source on power MOSFETs (Rgs) |
| 100 k x2 | 1/4 W axial | Pin A pull-down, B generator pull-up |
| 100 ohm | 1/4 W axial | MCU pin series resistor |
| 10 ohm | 1/4 W axial | Bootstrap series resistor |
| 100 nF x3 | Radial ceramic | Decoupling |
| 10 uF | Electrolytic, 25 V | Supply decoupling |

Small-signal transistor count for two channels: 6 P-channel (BS250) and 8 N-channel (2N7000), including the two complement generators.

## Usage

- **Control:** two MCU pins, `HIN` and `LIN`. Dead time comes from firmware or a hardware timer; start at 2 us and reduce it while measuring.
- **Supply sequencing:** the 12 V gate rail must come up before or together with the bus. Add 10 uF of decoupling close to the circuit.
- **MCU pins:** choose pins that are high-impedance or low at boot (avoid strapping pins, e.g. on the ESP32).
- **Bring-up:** low side first with an LED as load, then the high side with no power load, then the full half-bridge with a small load.
- **Limits:** 24 V bus (28 V maximum). VB can reach about 36 V, which leaves margin for the 2N7000/BS250 voltage ratings.

## Test firmware

[`firmware.ino`](firmware.ino) is a single-file ESP32 sketch (Arduino-ESP32 core, no external libraries) for bring-up. Pins, test mode, frequency, duty, dead time and bootstrap charge time start from the defaults at the top of the file and can be overridden at run time over Serial (115200 baud, newline-terminated), for example `mode=half`, `freq=10000`, `duty=30`, `dead=3`. Type `help` for the full list; `show`, `start`, `stop`, `charge` and `defaults` are also available. Three modes are provided: low-side blink, high-side PWM, and complementary half-bridge PWM. Note that the Arduino IDE requires the sketch to sit in a folder with the same name as the `.ino` file.

## Bus voltage limits

| Bus | Option |
|---|---|
| <= 6-7 V | Plain cross-coupled shifter, no protection |
| about 8-30 V | Zener + resistor (section 2), or a series P-MOS clamp |
| about 30-100 V | Dedicated half-bridge gate driver IC (e.g. IR2184, IR2110 class), or HV discrete parts with a cascode stage |
| hundreds of volts | Use a gate driver IC: a high-voltage half-bridge driver (e.g. IR2110, IR2113) or an isolated driver (e.g. UCC21520, Si827x class). Pulse-transformer or set/reset latch schemes are the discrete alternative. A cross-coupled shifter is not viable here. |

## Known limitations

- High-side edges are slow (about 1 us) because of the P-gate protection resistors.
- There is brief contention between N and P in the level shifter and a brief shoot-through spike in the output stage. Both are mitigated by the 470 ohm resistors and the 100 nF decoupling.
- No UVLO, interlock or overcurrent protection. Add current sensing and protection for motor loads.
- Bootstrap limits maximum duty cycle. For BLDC drives, PWM the low side or add a charge pump.
- The N-vs-P strength ratio in the cross-coupled stage is sensitive to component tolerance. Verify across parts and temperature.
- No galvanic isolation from the MCU. Do not use directly on mains.

# Driver for Delta ASDA-Ax-E Series Servos (A2 and A3-E)

The [`lcec_deasda`](../src/devices/lcec_deasda.c) driver supports Delta Automation ASDA-Ax-E series drives. 

It currently supports 2 basic families of devices:

- ASDA-A2-E series drives (type "DeASDA")
- ASDA-A3-E series of drives (type "DeASDA3")
- ASDA-B3-E Series of Drives (type "DeASDB3")

Note that to support the legacy driver, DeASDA at CSV mode is the default setting of the driver.

See the [source code](../src/devices/lcec_deasda.c) or [device
documentation](devices/) for precise details on which hardware is
currently supported. We aim to provide support for both CSV (cyclic synchronous velocity) and CSP (cyclic synchronous position) modes. 

## General Setup 
The servo setup is fairly basic.  To configure them, add a line
like this to your `ethercat.xml` file:

```xml
    <slave idx="1" type="DeASDA" name="x-axis"/>
```
type: specifies which device this is: A2 (type="DeASDA"), A3 (type="DeASDA3"), B3 (type="DeASDB3")

name: whatever you want to call this ethercat slave, e.g. "x-axis"

Example for A3 series:

```xml
    <slave idx="1" type="DeASDA3" name="A3"/>
```

To ensure that a change in operational mode does not occur during the linuxcnc runtime, mode setting is done using modParam in the configuration file. This setting is optional. The default mode is CSV (Cyclic Synchronous Velocity).

```xml
    <slave idx="1" type="DeASDA" name="x-axis">
      <modParam name="opmode" value="CSV"/>
    </slave>
```

There is 1 `<modParam>` setting that applies to all drives (the B3 has more for
homing, see [Homing](#homing-asda-b3-only-csp-mode)):

- `opmode`: controls the operational mode based on Object 6060h: Modes of operation.
  Supported modes are:
  - `CSV`: Cyclic Synchronous Velocity mode (Value for 0x6060: 9) DEFAULT when modParam not used.
  - `CSP`: Cyclic Synchronous Position mode (Value for 0x6060: 8)
    
All of these settings are case insensitive.

## Encoder count and Axis Movement Scale

### Motor Encoder resolution - Different between ASDA-A2 and ASDA-A3/B3 series - 

The standard motors that come with either series of drives differ slightly. A2 drives will have a default encoder resolution of 1,280,000, whereas A3 and B3 series (henceforth X3) motors have a resolution of 16,777,216. Note that X3 drives support both x2 and x3 series motors. The default value in this deasda driver is that of ASDA A2 series motors.

To set the encoder resolution use hal pin: %s.%s.%s.srv-pulses-per-rev (also know as pprev).
Example for A3:
```
lcec.0.A3.srv-pulses-per-rev 16777216
```

### Setting Pulses per Revolution with Scale

To set the correct scaling to account for ball screw pitch or gear reduction use hal pin: %s.%s.%s.pos-scale

For example: Using an A3 servo with 24 bit (16,777,216 count) encoder, directly coupled to a 5mm pitch ball screw:
```
lcec.0.A3.srv-pulses-per-rev 16777216
lcec.0.A3.pos-scale 5
```

### External encoder / full-closed loop - A2 and A3

A2 and A3 drives can use a second external encoder. Usually a linear encoder is used. The number of pulses of the external encoder per motor revolution can be set by hal pin %s.%s.%s.extenc-scale

The units are pulses per revolution and depend on:
1. The resolution of the scale (e.g. 5um, 1um, 0.1um)
2. the Linear distance moved by the axis per revolution of the motor

Example:
5mm (=5000um) pitch ball screw. 0.5um linear scale:
5000/0.5 = 10000
```
lcec.0.A3.extenc-scale 10000
```
Another example:
16mm (=16000um) pitch ball screw, 5um linear scale:
16,000/5 = 3200
```
lcec.0.A3.extenc-scale 3200
```

### Setting Pulses per Rev with E-Gear

ASDA drives use an electronic gear (E-Gear) to translate between the
position unit used on the bus, the PUU ("pulse per user unit"), and the
drive's internal 24-bit resolution of 16,777,216 pulses per motor revolution,
whatever the physical encoder resolution is.  The ratio is

```
internal pulses = PUU * N / M          (N = numerator, M = denominator)
PUU per motor revolution = 16,777,216 * M / N
```

On the drive, N and M are parameters `P1.044` (N) and `P1.045` (M).  Over
EtherCAT they are also available as object `0x6093` (position factor):
sub-index 1 is N, sub-index 2 is M.

**What the driver assumes.**  The driver sends and reads positions in PUU
and converts them with `srv-pulses-per-rev` (pprev) and `pos-scale`:

```
target PUU      = srv-pos-cmd * srv-pulses-per-rev / pos-scale
enc-pos [units] = PUU read from the drive / srv-pulses-per-rev * pos-scale
```

`pos-scale` is the distance (in your machine units) travelled per motor
revolution.  So `srv-pulses-per-rev` must always equal the number of PUU per
motor revolution that the drive is really using.  With the E-Gear at 1:1
(N = M), one PUU is one internal pulse, so the number of PUU per revolution is
16,777,216, which is the driver's default for the A3 and B3.  The
B3's EtherCAT defaults for `0x6093` are N = 1 and M = 1, so the defaults
work without any E-Gear setting.

**When you need the E-Gear.**  Leave it at 1:1 if you are happy to work with
`pos-scale` alone.  Set it if you want a round PUU, for example 1 PUU =
1 um, so that the numbers seen on the bus and in the drive's own monitor are in
real units, or if you must keep the PUU count per revolution small.

**Example: 5 mm pitch ball screw, direct drive (gear ratio 1:1), 1 PUU = 1 um.**

One motor revolution moves 5 mm = 5000 um, so we want 5000 PUU per revolution:

```
16,777,216 * M / N = 5000   ->   N = 16,777,216,  M = 5000
```

Set `P1.044 = 16777216` and `P1.045 = 5000` (or `0x6093:01 = 16777216`,
`0x6093:02 = 5000`).  Then tell the driver:

```
lcec.0.B3.srv-pulses-per-rev 5000
lcec.0.B3.pos-scale 5
```

Check: a command of 1 mm gives `1 * 5000 / 5 = 1000` PUU, which is 1000 um.

Without the E-Gear (1:1) the same axis uses `srv-pulses-per-rev 16777216` and
`pos-scale 5`; 1 mm is then 3,355,443 PUU (about 0.3 um per PUU).  Both give the
same motion; only the PUU resolution differs.

**Belts, gearboxes.**  Treat any transmission as part of `pos-scale`.  Divide
the screw pitch (or the distance per revolution of the driven part) by the
reduction between motor and load.  Example: 5 mm pitch with a 2:1 belt
reduction (the motor turns twice per screw revolution) moves 2.5 mm per motor
revolution, so `pos-scale = 2.5`.  If you also want 1 PUU = 1 um you then
need 2500 PUU per revolution (`N = 16,777,216`, `M = 2500`) and
`srv-pulses-per-rev 2500`.

**E-Gear modParams (B3).**  The B3 driver can set the E-Gear for you at every
start:

| modParam | OD / parameter | Description |
|---|---|---|
| `eGearNumerator` | `0x6093:01` / `P1.044` | E-Gear numerator N. |
| `eGearDenominator` | `0x6093:02` / `P1.045` | E-Gear denominator M. |

```xml
    <slave idx="1" type="DeASDB3" name="B3">
      <modParam name="eGearNumerator" value="16777216"/>
      <modParam name="eGearDenominator" value="5000"/>
    </slave>
```

- Both must be given together and be non-zero, otherwise the driver refuses to start.
- The driver also sets the default of `srv-pulses-per-rev` to
  16,777,216 * M / N (rounded; 5000 in the example), so
  you only need `pos-scale` in HAL.  You can still override
  `srv-pulses-per-rev` with `setp`.
- Without these modParams the driver does not touch `0x6093`, and the drive
  uses its own value (1:1 after power-up in EtherCAT mode).
- Over EtherCAT the drive resets the E-Gear to the protocol default (1:1)
  after a power cycle, unless `P3.012.Z` is 1; since the modParams write it at every start,
  that reset does not matter when you use them.  Changing the E-Gear still
  loses the absolute position, so home again after changing it.
- Alternatively, set `P1.044` / `P1.045` on the drive (keypad or ASDA-Soft), or
  write `0x6093` yourself with `<sdoConfig>`.  In that case set
  `srv-pulses-per-rev` in HAL as well.

**Related modParams.**  `pos-scale` is set with a HAL parameter, and `srv-pulses-per-rev` follows the
E-Gear modParams above unless you override it.  These modParams are affected by the
E-Gear or needed for it to matter:

| modParam | Relation to the E-Gear |
|---|---|
| `opmode` | `pos-scale` and `srv-pulses-per-rev` are used for the position target and feedback in `CSP`; `CSV` uses RPM and ignores the PUU scaling for the command. Use `CSP` for the example above. |
| `homeOffset` (B3) | Written to `0x607C` in **PUU**, so it depends on the E-Gear. With 1 PUU = 1 um, `homeOffset="2000"` is 2 mm; at 1:1 E-Gear the same 2 mm of this screw is about 6,710,886 PUU. |
| `homeSpeedFast`, `homeSpeedSlow`, `homeAccel` (B3) | Not affected: they are in 0.1 rpm and ms, independent of PUU. |
| `enableHoming`, `homeMethod`, `homeFakeFeedback` (B3) | Not affected by the E-Gear. |

Complete example for the 5 mm screw above (E-Gear N = 16,777,216, M = 5000,
1 PUU = 1 um), using only modParams:

```xml
    <slave idx="1" type="DeASDB3" name="B3">
      <modParam name="opmode" value="CSP"/>
      <modParam name="enableHoming" value="true"/>
      <modParam name="homeMethod" value="33"/>
      <modParam name="homeOffset" value="0"/>
      <modParam name="homeSpeedFast" value="1000"/>
      <modParam name="homeSpeedSlow" value="200"/>
      <modParam name="eGearNumerator" value="16777216"/>
      <modParam name="eGearDenominator" value="5000"/>
    </slave>
```

and in your HAL file (`srv-pulses-per-rev` is already 5000 from the E-Gear):

```
setp lcec.0.B3.pos-scale 5
```

Do not change the E-Gear while the servo is enabled.  The values above come
from the ASDA-B3 manual (section 6.2.5, parameters `P1.044` / `P1.045`, object
`0x6093`); the same idea applies to the A2/A3, but check their manuals for the defaults.


## Exposed Pins Common

The following pins are exposed in both modes (CSV and CSP) and for all drives. The pin names below contain "0.A3" - this will depend on the naming in your xml file.

Encoder values (servo motor and external encoder at CN5)

_Encoder Pins_
```
u32   I/O lcec.0.A3.enc-ext-hi
u32   I/O lcec.0.A3.enc-ext-lo
bit   I/O lcec.0.A3.enc-index-ena
bit   OUT lcec.0.A3.enc-on-home-neg
bit   OUT lcec.0.A3.enc-on-home-pos
float OUT lcec.0.A3.enc-pos
float OUT lcec.0.A3.enc-pos-abs 
float OUT lcec.0.A3.enc-pos-enc
bit   IN  lcec.0.A3.enc-pos-reset
s32   OUT lcec.0.A3.enc-raw
u32   OUT lcec.0.A3.enc-ref-hi
u32   OUT lcec.0.A3.enc-ref-lo
u32   I/O lcec.0.A3.extenc-ext-hi
u32   I/O lcec.0.A3.extenc-ext-lo
bit   I/O lcec.0.A3.extenc-index-ena
bit   OUT lcec.0.A3.extenc-on-home-neg
bit   OUT lcec.0.A3.extenc-on-home-pos
float OUT lcec.0.A3.extenc-pos
float OUT lcec.0.A3.extenc-pos-abs
float OUT lcec.0.A3.extenc-pos-enc
bit   IN  lcec.0.A3.extenc-pos-reset
s32   OUT lcec.0.A3.extenc-raw
u32   OUT lcec.0.A3.extenc-ref-hi
u32   OUT lcec.0.A3.extenc-ref-lo
```
_Slave Status_
```
bit   OUT lcec.0.A3.slave-online
bit   OUT lcec.0.A3.slave-oper
bit   OUT lcec.0.A3.slave-state-init
bit   OUT lcec.0.A3.slave-state-op
bit   OUT lcec.0.A3.slave-state-preop
bit   OUT lcec.0.A3.slave-state-safeop
```
_Drive Status and Control_
```
bit   OUT lcec.0.A3.srv-at-speed
bit   IN  lcec.0.A3.srv-enable 
bit   IN  lcec.0.A3.srv-enable-volt
bit   OUT lcec.0.A3.srv-fault
bit   IN  lcec.0.A3.srv-fault-reset 
bit   IN  lcec.0.A3.srv-halt
bit   OUT lcec.0.A3.srv-on-disabled
bit   OUT lcec.0.A3.srv-limit-active
bit   OUT lcec.0.A3.srv-oper-enabled
u32   OUT lcec.0.A3.srv-operation-mode
bit   IN  lcec.0.A3.srv-quick-stop
bit   OUT lcec.0.A3.srv-quick-stoped
bit   OUT lcec.0.A3.srv-ready
bit   OUT lcec.0.A3.srv-remote
bit   IN  lcec.0.A3.srv-switch-on
bit   OUT lcec.0.A3.srv-switched-on
float OUT lcec.0.A3.srv-torque-rel
float IN  lcec.0.A3.srv-vel-cmd 
float OUT lcec.0.A3.srv-vel-fb
float OUT lcec.0.A3.srv-vel-fb-rpm
float OUT lcec.0.A3.srv-vel-fb-rpm-abs
float OUT lcec.0.A3.srv-vel-rpm
bit   OUT lcec.0.A3.srv-volt-enabled
bit   OUT lcec.0.A3.srv-warning
bit   OUT lcec.0.A3.srv-zero-speed
```
_Digital Inputs and Outputs at CN1_
```
bit   OUT lcec.0.A3.din-1
bit   OUT lcec.0.A3.din-2
bit   OUT lcec.0.A3.din-3
bit   OUT lcec.0.A3.din-4
bit   OUT lcec.0.A3.din-5
bit   OUT lcec.0.A3.din-6
bit   OUT lcec.0.A3.din-7
bit   OUT lcec.0.A3.din-home
bit   OUT lcec.0.A3.din-neg-lim
bit   OUT lcec.0.A3.din-pos-lim
bit   IN lcec.0.A3.dout-d01
bit   IN lcec.0.A3.dout-d02
bit   IN lcec.0.A3.dout-d03
bit   IN lcec.0.A3.dout-d04
```


## CSV Mode (Default)

When the CSV mode is chosen, the driver exposes the following additional pin:

```
float IN     lcec.0.A3.srv-vel-cmd| Connect to velocity command from either joint or PID
```

Note: 
- To support easy migration from legacy linuxcnc-ethercat, CSV mode has been made the default setting.
- x.x.x.srv-operation-mode is set to 9 by the driver 


## CSP Mode

When the CSP mode is chosen, the driver exposes the following additional pin:

```
float IN     lcec.0.A3.srv-pos-cmd| Connect to position command from either joint or PID
```

Note: 
- x.x.x.srv-operation-mode is set to 8 by the driver 

## Homing (ASDA-B3 only, CSP mode)

The B3 can home itself using the CiA 402 homing mode (mode 6, see chapter
12.3.4 of the ASDA-B3 EtherCAT manual).  This is **opt-in** and only works
together with `opmode="CSP"`.  When enabled, the driver additionally maps
objects 0x6060 (modes of operation) and 0x6061 (modes of operation display)
into the PDOs so it can switch between CSP and homing mode at runtime.

```xml
    <slave idx="1" type="DeASDB3" name="x-axis">
      <modParam name="opmode" value="CSP"/>
      <modParam name="enableHoming" value="true"/>
      <modParam name="homeMethod" value="33"/>
      <modParam name="homeSpeedFast" value="1000"/>
      <modParam name="homeSpeedSlow" value="200"/>
    </slave>
```

| modParam | OD | Default | Description |
|---|---|---|---|
| `enableHoming` | - | `false` | Enable homing (B3, CSP only). |
| `homeMethod` | 6098h | `35` | `1`-`14`, `17`-`35`, or `-1`..`-4` (hard stop). `15`/`16` are reserved. `35` = current position becomes the origin (no motion). |
| `homeOffset` | 607Ch | `0` | Home offset in PUU. |
| `homeSpeedFast` | 6099h:01 | `100` | Speed while searching for the switch, in 0.1 rpm (1-20000). |
| `homeSpeedSlow` | 6099h:02 | `20` | Speed while searching for the Z pulse, in 0.1 rpm (1-5000). |
| `homeAccel` | 609Ah | `100` | Time for 0 to 3000 rpm in ms. |
| `homeFakeFeedback` | - | `true` | While `srv-home` is set, `enc-pos` reports `srv-pos-cmd` instead of the real position (see below). |

The values are written to the drive in its own units, exactly as in the manual.
The home switch and limit switches are wired to, and assigned in, the drive's own digital
inputs (see the drive's `P2.010`-`P2.017` DI settings); the driver does not
configure them.  The drive must be enabled (operation enabled) for homing.

Additional pins:

```
bit   IO  lcec.0.B3.srv-home         Set to start homing; cleared by the driver when homing is complete.
bit   OUT lcec.0.B3.srv-homed        Homing attained (statusword bits 10 and 12 in homing mode).
bit   OUT lcec.0.B3.srv-homing       Homing is running.
bit   OUT lcec.0.B3.srv-home-error   Homing error (statusword bit 13 in homing mode).
```

`srv-operation-mode` shows the mode the drive reports in 0x6061 (8 = CSP, 6 = homing).

Sequence: when `srv-home` is set, the driver requests mode 6.  Once the
drive confirms mode 6 and is operation enabled, it asserts controlword bit 4 (the
drive starts on the rising edge).  When the drive reports homing attained, the
driver clears `srv-home` and returns the drive to CSP.  While the drive is not in
CSP, the position target follows the actual position so the axis does not jump
when CSP resumes.  `srv-home` is also cleared on a drive fault or loss of the bus.
On a homing error `srv-home-error` is set and `srv-home` stays set; clear it (or
abort in LinuxCNC) to leave homing mode.

**Following error during homing:** the drive moves by itself, so LinuxCNC would
see feedback moving without a command.  With `homeFakeFeedback` (default), the
position feedback pin `enc-pos` is set equal to `srv-pos-cmd` for as long as
`srv-home` is set, so no following error appears.  The driver's raw encoder
tracking (`enc-raw`, `enc-pos-enc`, `enc-pos-abs`) and the velocity pins still show
the real values.  Real feedback returns in the cycle after the driver clears
`srv-home`; that is the cycle in which LinuxCNC latches its home position
from `joint.N.index-enable`, so the jump from the old command to the drive's new
position is absorbed by the homing offset.  If you abort homing yourself (clearing
`srv-home` before it completes), the real feedback returns at once and the
joint is normally disabled by LinuxCNC anyway.  Set `homeFakeFeedback` to `false`
to see the real position at all times.

Connect `srv-home` to `joint.N.index-enable` and use `HOME_USE_INDEX = YES`
with `HOME_SEQUENCE`, as with the generic CiA 402 driver; see the
[CiA 402 documentation](cia402.md).

Note: this has not been tested on real hardware yet.

## How to file a bug

Please file an
[issue](http://github.com/linuxcnc-ethercat/linuxcnc-ethercat/issues)
if you have any problems with this driver, including:

- hardware that should work but doesn't.
- unexpected limitations.
- features that the hardware supports but the driver does not.

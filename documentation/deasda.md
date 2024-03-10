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

There is 1 `<modParam>` setting:

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

### Setting Pulses per Rev with Egear 

TODO:
```
ASDA series drive follow the concept of using egear to translate between encoder pulses and external input. This setting relates the encoder's pprev with the pitch of the ballscrew and additional transmission factors of belt drives if these are in place.

For a ball screw pitch of p = 5mm, a gearing ratio of 1:1 the calculcation would be as follows:
```
TODO/


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

## How to file a bug

Please file an
[issue](http://github.com/linuxcnc-ethercat/linuxcnc-ethercat/issues)
if you have any problems with this driver, including:

- hardware that should work but doesn't.
- unexpected limitations.
- features that the hardware supports but the driver does not.

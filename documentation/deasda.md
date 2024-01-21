# Driver for Delta ASDA-Ax-E Series Servos (A2 and A3-E)

The [`lcec_deasda`](../src/devices/lcec_deasda.c) driver supports Delta Automation ASDA-Ax-E series drives. 

It currently supports 2 basic families of devices:

- ASDA-A2-E series drives (type "DeASDA")
- ASDA-A3-E series of drives (type "DeASDA3")

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

To ensure that a change in operational mode does not occure during the linuxcnc runtime, mode setting is done using modParam in the configuration file. This setting is optional as it defaults to mode CSV.

```xml
    <slave idx="1" type="DeASDA" name="x-axis"/>
      <modParam name="opmode" value="CSV"/>
    </slave>
```

There is 1 `<modParam>` setting:

- `mode`: controls the operational mode based on Object 6060h: Modes of operation.
  Supported modes are:
  - `CSV`: Cyclic Synchronous Velocity mode (Value for 0x6060: 9) DEFAULT when modParam not used.
  - `CSP`: Cyclic Synchronous Position mode (Value for 0x6060: 8)
    
All of these settings are case insensitive.

## Differences of ASDA-A2 and ASDA-A3

The stnadard motors that come with either series of drives differ slightly. A2 drives will have a default encoder resolution of 1,280,000 whereas A3 series motors have a resolution of 16,777,216. Note that A3 drives support both A2 and A3 series motors. The default value in the driver is that of ASDA A2 series motors.

To set the resolution use hal pin: %s.%s.%s.srv-pulses-per-rev  (also know as pprev). The following remark for further help on the topic.

### Setting Pulses per Rev with Egear 

ASDA series drive follow the concept of using egear to translate between encoder pulses and external input. This setting relates the encoder's pprev with the pitch of the ballscrew and additional transmission factors of belt drives if these are in place.

For a ball screw pitch of p = 5mm, a gearing ratio of 1:1 the calculcation would be as follows:

TODO()


## CSV Mode (Default)

When the CSV mode is chosen, the driver exposes the following set of pins:

TODO()

Note: To support easy migration from legacy linuxcnc-ethercat, CSV mode has been made the default setting.

## CSP Mode

When the CSP mode is chosen, the driver exposes the following set of pins:

Some as above, except:

Removed:
x.x.x.srv-vel-cmd 

Repalced by: x.x.x.srv-pos-cmd

## How to file a bug

Please file an
[issue](http://github.com/linuxcnc-ethercat/linuxcnc-ethercat/issues)
if you have any problems with this driver, including:

- hardware that should work but doesn't.
- unexpected limitations.
- features that the hardware supports but the driver does not.


## Relevant open issues
- Expose further parameters
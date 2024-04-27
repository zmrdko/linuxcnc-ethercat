# Leadshine EtherCAT Stepper Drivers

The `lcec_leadshine_stepper` driver supports a number of
[Leadshine](http://www.leadshine.com) EtherCAT stepper
drives.

## Setup

In your XML file, you should have an entry somewhat like this:

```xml
<masters>
  <master idx="0" appTimePeriod="2000000" refClockSyncCycles="1000">
    ...
    <slave idx="1" type="2CS3E-D507" name="x-axis"/>
	...
  </master>
</masters>
```

See the [CiA 402](cia402.md) documentation for additional details
about how to configure CiA 402 devices in LinuxCNC.  At a minimum, you
will need to include the `cia402` HAL component.

## Devices

This driver includes settings for most known Leadshine open- and
closed-loop EtherCAT stepper drives as of early 2024.  Unfortunately,
Leadshine's websites and EtherCAT ESI config files differ on some
details (including product names!), so not every device that *could*
be supported is currently implemented.

- [CS3E-D503](https://www.leadshine.com/product-detail/CS3E-D503.html) 1-axis, closed-loop, 3A, 7 din, 7 dout
- [CS3E-D507](https://www.leadshine.com/product-detail/CS3E-D507.html) 1-axis, closed-loop, 7A, 7 din, 7 dout
- [CS3E-D1008](https://www.leadshine.com/product-detail/CS3E-D1008.html) 1-axis, closed-loop, 8A, 7 din, 7 dout
- [CS3E-D503E](https://www.leadshine.com/product-detail/CS3E-D503E.html) 1-axis, closed-loop, 3A, 6 din, 2 dout
- [CS3E-D507E](https://www.leadshine.com/product-detail/CS3E-D507E.html) 1-axis, closed-loop, 7A, 6 din, 2 dout
- [EM3E-522E](https://www.leadshine.com/product-detail/EM3E-522E.html) 1-axis, open-loop, 2.2A, 6 din, 2 dout
- [EM3E-556E](https://www.leadshine.com/product-detail/EM3E-556E.html) 1-axis, open-loop, 5.6A, 6 din, 2 dout
- [EM3E-870E](https://www.leadshine.com/product-detail/EM3E-870E.html) 1-axis, open-loop, 7A, 6 din, 2 dout
- [2CS3E-D503](https://www.leadshine.com/product/2CS3E-D503-14-15-993-70.html) 2-axis, closed-loop, 2.2A, 2x4 din, 2x2 dout
- [2CS3E-D507](https://www.leadshine.com/product/2CS3E-D507-14-15-994-70.html) 2-axis, closed-loop, 7A, 2x4 din, 2x2 dout
- [2EM3E-D522](https://www.leadshine.com/product-detail/2EM3E-522.html) 2-axis, open-loop, 2.2A, 2x4 din, 2x2 dout
- [2EM3E-D556](https://www.leadshine.com/product-detail/2EM3E-556.html) 2-axis, open-loop, 5.6A, 2x4 din, 2x2 dout

Leadshine has a number of very similar models on their website, and
sometimes also lists the same device in different locations with
slightly different names; the 2CS3E-D507 is sometimes referred to as
the 2CL3-EC507, for instance.

There are a number of devices on their website that would probably
work if we had their EtherCAT ID numbers, including the CS3E-D503B,
CS3E-D507B, EM3E-522B, EM3E-556B, EM3E-870B, and DM3C-EC882AC.

Only the 2CS3E-D507 has been tested at this point.

### Caveats

- Only the 2CS3E-D507 has been tested.
- It appears that some Leadshine devices measure peak current in units
  of 100mA and other in units of 1mA.  Right now, it looks like
  closed-loop devices use 100mA and open-loop devices use 1mA, but
  this may not be universally correct.  See object 0x2000:0 in the
  device manual and file a bug if your device doesn't match this
  pattern.
- A number of useful device-specific `<modParams>` aren't directly
  supported yet.
- Digital input controls aren't complete yet.  The 2CS3E series, at
  least, doesn't *quite* seem to follow the CiA 402 spec.  It puts
  GPIO pins at 0x60fd:0 bits 4-7, while the spec says that bits 4-15
  are reserved and implies that GPIO pins should start at bit 16.
  Also, the controls for mapping hardware input pins to logical pins
  seem to require a save followed by a restart of the device in order
  to take effect, which is awkward in a `<modParam>`. See Leadshine's
  documentation for details on how to change pin function for now.

## Configuration

This driver takes a number of `<modParam>` options that control its
operation.  There are a few Leadshine-specific parameters, plus a
number of additional [`cia402` modParams](cia402.md).

The parameters are listed in their single-axis form; dual-axis devices
have two sets of these parameters, one starting with `ch1` and one
starting with `ch2`.  For example, `ch1peakCurrent_amps` and
`ch2peakCurrent_amps`.

<dl>
<dt>&lt;modParam name="feedRatio" value="10000"/&gt;</dt>
<dd>The number of microsteps per rotation.  This can be provided as a floating-point number or an integer ratio (`10000/6`).</dd>

<dt>&lt;modParam name="encoderRatio" value="4000"/&gt;</dt>
<dd>The number of encoder steps per rotation.  Only applicable to closed-loop steppers.  Can be a floating point number or an integer ratio.</dd>

<dt>&lt;modParam name="peakCurrent_amps" value="3.0"/&gt</dt>
<dd>Set the maximum current for the motor, in amps.</dd>

<dt>&lt;modParam name="controlMode"  value="openloop|closedloop"/&gt</dt>
<dd>Set the control mode for the motor.  </dd>

</dl>

## Inputs

To use digital inputs, you will most likely need to manually make
configuration changes to your stepper drive using `ethercat download`
on the command line.  Unfortunately, Leadshine only re-reads digital
input configuration on reboot, so it's not really useful to configure
this via `<modParam>`, like other drivers currently do.

Also, the exact commands needed depend on the Leadshine model that
you're using.  Various models have between 4 and 7 inputs with
somewhat different capabilities.

To start, please find your device's manual.  Leadshine has PDF
available at
https://www.leadshine.com/networks/fieldbus/EtherCAT.html.

I'm going to use the 2CS3E-D507, because that's what I have, and it's
slightly more complex than most devices.  In the manual, in the "I/O
Configuration Object" section, there are 2 tables that we'll need:

- Input Ports Function Value
- 0x60FD Corresponding Function Table

The first table shows the defaults for each port, as well as which
SDOs control each.  Generally, input 1 on a single-axis device is at
0x2152:01, input 2 is at :02, and so on.  For dual axis devices, use
0x2952 instead of 0x2152 for the second axis.

For the 2CS3E-D507, the input port function value table says that
input 1 defaults to "touch probe 1", which is 0x17.  Looking in the
other table, we can see that 0x17 is, indeed "Probe 1".

For the 2CS3E, there are 7 valid functions listed:

- probe1 (0x17)
- probe2 (0x18)
- home (0x16)
- positive limit (0x01)
- negative limit (0x02)
- quick stop (0x14)
- GPIO (0x19).

All of these *except* GPIO have special meanings in specific contexts.
For instance, home and the two limit switches can be used in CiA
402-native homing.

You *may* be able to still read from these inputs if they're in
their default config, but the hardware reports their status
differently, so we've given them different HAL pin names.

- probe1 -> `srv-din-probe1`
- probe2 -> `srv-din-probe2`
- home -> `srv-din-home`
- positive limit -> `srv-din-positive-limit`
- negative limit -> `srv-din-negative-limit`
- quick stop -> `srv-din-quick-stop`

When pins are configured as GPIO, they get simple numeric names:

- Input 1 -> `srv-din-1`
- Input 2 -> `srv-din-2`

and so on.

To, to change input 1 from its default (probe1) to GPIO, you'll need
to run these commands on the command line.  Replace XX and YY with the
master and slave address for your Leadshine device; see `ethercat
slaves` for a list.

```
$ ethercat download -m XX -p YY 0x2152 0x01 0x19
```

If you want to change more than one port, do those next.  Then run the
following command to save these settings as the new startup defaults:

```
$ ethercat download -m XX -p YY 0x1010 0x01 0x65766173
```

Once this is complete, you'll need to power-cycle the stepper drive for the
new configs to take effect.  You can verify the new config after
reboot by running:

```
$ ethercat upload -m XX -p YY 0x2152 0x01
```

This should return 0x19, which is what we set above.  If the save
failed, then we'll probably get 0x17 back instead.

## Outputs

Digital outputs work similarly to digital inputs, in that they have
default settings and may need to be overridden before they can be
used.  And, like digital inputs, you need to power-cycle the device
before the change takes effect.

Digital output functions are controlled via `0x2156:n`, where `n` is
the output port number (1, 2, etc).  For the second axis on 2-axis
drives, use `0x2956` instead.

There are 5 options for configure outputs:

- alarm: `0x2156:n` = 1
- servo on: `0x2156:n` = 2
- brake: `0x2156:n` = 3
- in-position: `0x2156:n` = 4
- GPIO: `0x2156:n` = 5

By default on 2CS3E-D507, output port 1 is set to "alarm" and port 2
is set to "brake".  See Leadshine's documentation for the default
values for other models.

To change these, you'll need to use thed `ethercat` command-line tool.
For example, to switch port 2 to GPIO (5), you'll need to run:

```
$ ethercat download -m XX -p YY 0x2156 2 5
```

After you've finished setting output (and input) ports, you'll need to save the settings via:

```
$ ethercat download -m XX -p YY 0x1010 0x01 0x65766173
```

Then power-cycle the drive.

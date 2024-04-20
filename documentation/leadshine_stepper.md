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
- A number of device-specific settings (both `<modParams>` and HAL
  pins) aren't directly supported yet.

## Configuration

This driver takes a number of `<modParam>` options that control its
operation.  There are a number of `rtec`-specific parameters, plus a
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

</dl>

## Inputs

TBD

## Outputs

TBD

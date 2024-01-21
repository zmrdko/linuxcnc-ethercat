# linuxcnc-ethercat

This is a set of [LinuxCNC](https://linuxcnc.org/) drivers for
EtherCAT devices, intended to be used to help build a [CNC
machine](https://en.wikipedia.org/wiki/Numerical_control).

[EtherCAT](https://en.wikipedia.org/wiki/EtherCAT) is a standard for
connecting industrial control equipment to PCs using Ethernet.
EtherCAT uses dedicated Ethernet networks and achieves consistently
low latency without requiring special hardware.  A number of
manufacturers produce EtherCAT equipment for driving servos, steppers,
digital I/O, and reading sensors.

This tree was forked from
[sittner/linuxcnc-ethercat](https://github.com/sittner/linuxcnc-ethercat),
and is intended to be the new default version of LinuxCNC EtherCAT.


## Installing

The recommended way to install this driver is to use the `.deb` apt
repository managed by the Etherlab folks.  It should contain
everything that you need to install Ethercat support for LinuxCNC with
minimal manual work.

### Initial setup

First, you need to tell `apt` how to find the Etherlab repository,
hosted at https://build.opensuse.org/project/show/science:EtherLab.  This
is the preferred mechanism from the [LinuxCNC
forum](https://forum.linuxcnc.org/ethercat/45336-ethercat-installation-from-repositories-how-to-step-by-step):


```
sudo mkdir -p /usr/local/share/keyrings/
wget -O- https://build.opensuse.org/projects/science:EtherLab/signing_keys/download?kind=gpg | gpg --dearmor | sudo dd of=/etc/apt/trusted.gpg.d/science_EtherLab.gpg
sudo tee -a /etc/apt/sources.list.d/ighvh.sources > /dev/null <<EOT
Types: deb
Signed-By: /etc/apt/trusted.gpg.d/science_EtherLab.gpg
Suites: ./
URIs: http://download.opensuse.org/repositories/science:/EtherLab/Debian_12/
EOT
sudo apt update
sudo apt install -y linux-headers-$(uname -r) ethercat-master linuxcnc-ethercat
```

(These directions are for Debian 12.  Debian 11 should be very similar,
just change `Debian_12` to `Debian_11`.)

You will then need to do a bit of setup for Ethercat; at a minimum
you'll need to edit `/etc/ethercat.conf` to tell it which interface it
should use.  See the forum link (above) for additional details.

You can verify that Ethercat is working when `ethercat slaves` shows
the devices attached to your system.  See the forum link above for
additional helpful steps.

### Updates

Ongoing updates should be easy and *mostly* handled automatically by
`apt`.  Just run `sudo apt update` followed by `sudo apt upgrade` and
things will mostly work, with one possible exception.  If the kernel
is upgraded, then you *may* need to re-run this command in order to
get Ethercat working again:

```
sudo apt install -y linux-headers-$(uname -r)
```

This is because the real-time kernel that LinuxCNC prefers doesn't get
its headers intalled by default, and this breaks compiling the
Ethercat modules for the new kernel.  Just run this `apt` command and
then either reboot or run `sudo systemctl start ethercat`.

### Manual Installation

If you decide that you want to install this manually and not use a
package manager, then first you'll need to make sure that you have the
[Ethercat Master](https://gitlab.com/etherlab.org/ethercat) and
LinuxCNC (with its development tools) installed.  Then download
linuxcnc-ethercat and run `make install`.

## Contributing

See [the contributing documentation](CONTRIBUTING.md) for details.  If
you have any issues with the contributing process, *please* file an
issue here.  Everything is new, and it may be broken.

[API Documentation](https://linuxcnc-ethercat.github.io/linuxcnc-ethercat/doxygen/) via Doxygen is available, but incomplete.


## Configuring

At a minimum, you will need two files.  First, you'll need an XML file
(commonly called `ethercat.xml`) that describes your hardware.  Then
you'll need a LinuxCNC HAL file that loads the LinuxCNC-Ethercat
driver and tells LinuxCNC about your CNC.

Examples TBD.

## Devices Supported

See [the device documentation](documentation/DEVICES.md) for a partial
list of Ethercat devices supported by this project.  Not all devices
are equally supported.  If you have any problems, please [file a
bug](https://github.com/linuxcnc-ethercat/linuxcnc-ethercat/issues/new/choose).

There are two ways to use EtherCAT hardware with this driver.  First,
many devices have dedicated drivers which know about all of the
details of devices.  For instance, you can tell it to use a [Beckhoff
EL7041 Stepper controller](http://beckhoff.com/EL7041) as `x-axis` by
saying

```XML
   <slave idx="3" type="EL7041" name="x-axis"/>
```

This will create a number of LinuxCNC
[`pins`](https://linuxcnc.org/docs/html/hal/intro.html) that talk
directly to the EL7041 and control the stepper connected to it.  You
will still need to tell LinuxCNC what to do with the new hardware, but
the low-level details will be handled automatically.

The second way to use EtherCAT hardware is with the "generic" driver.
You can tell LinuxCNC-Ethercat about your hardware entirely in XML,
and it will let you send EtherCAT messages to any hardware, even if
we've never seen it before.  This is easier than writing a new driver,
but more difficult than using a pre-written driver.

TODO(): write generic documentation.

## Differences from `sittner/linuxcnc-ethercat`

This tree includes drivers for many devices that the original did not
support.  Common classes of device, like digital/analog input/output
boards should have much wider device support, and a number of
commnuity-created drivers have been merged into the tree.  See the
[device list](documentation/DEVICES.md) for the full list of known
hardware.

There are several differences between this version of
LinuxCNC-Ethercat and [the
original](https://github.com/sittner/linuxcnc-ethercat) that should
make life easier, but may make transitioning from older versions of
linuxcnc-ethercat more complicated.  If you're just *using* EtherCAT
with LinuxCNC, then you can safely ignore all of this.

Developers, though, should be aware:

1. In this version, all device-specific code
([`lcec_el1xxx.c`](src/devices/lcec_el1xxx.c), for example) lives in
`src/devices`, while it used to be in `src/`.
2. The mapping between Ethercat VID/PID and device drivers now lives
   in the device source files themselves, *not* in `lcec_main.c` and
   `lcec_conf.c`.  See example below.
3. There is no need to edit `Kbuild` when adding new devices.

In short, to add a new device, you should just be able to drop source
files into `src/devices` and everything should build and work, as long
as you make one minor addition to the source.  Near the top of the
`.c` file for your driver, add a block like this to replace the code
that was in `lcnc_main.c`:

```c
static lcec_typelist_t types[]={
  { "EL1002", LCEC_EL1xxx_VID, LCEC_EL1002_PID, LCEC_EL1002_PDOS, 0, NULL, lcec_el1xxx_init},
  { "EL1004", LCEC_EL1xxx_VID, LCEC_EL1004_PID, LCEC_EL1004_PDOS, 0, NULL, lcec_el1xxx_init},
  ...
  { NULL },
};

ADD_TYPES(types);
```

This is from `lcec_el1xxx.c`, your names will vary, of course.  The
first field is the string that identifies the device in
`ethercat.xml`, and the other fields match up with the fields that
used to be in `lcec_main.c`.

If your driver needs `<modParam>`s in `ethercat.xml` (like the AX* and
assorted TwinSAFE devices), then you'll need to define the module
parameters in your `.c` file as well, see
[`lcec_el6900.c`](src/devices/lcec_el6900.c) for an example.

Be aware that a number of drivers have been merged together,
particularly drivers for Beckhoff EL3xxx-series analog input devices
and Beckhoff EL185x/EK18xx/EP23xx digital combo devices.  Existing
configurations should keep working just fine, as all
externally-visible names should have been kept the same.

There are also a family of libraries for supporting basic digital and
analog input and output channels with a common interface, which should
make supporting new devices less complicated.  See
[`lcec_class_din.c`](src/devices/lcec_class_din.c),
[`lcec_class_dout.c`](src/devices/lcec_class_dout.c), and
[`lcec_class_ain.c`](src/devices/lcec_class_ain.c).

# Driver for Beckhoff EL4xxx Analog Output Devices

The [`lcec_el4xxx`](../src/devices/lcec_el4xxx.c) driver supports a
wide range of analog output devices and replaces a number of older
single-purpose drivers.

It currently supports 3 basic families of devices:

- EL40xx 12-bit analog output modules
- EL41xx 16-bit analog output modules
- EP4174 16-bit multi-function output module

The EP4174 is somewhat unique in that it can be a -10..10V, 0..10V,
0..20mA, or 4..20mA device, and needs configuration settings to
control this.  At the moment this needs to be done by hand with SDO
settings, but support will be added to this driver eventually.


## Pins

This driver creates a large number of pins for each analog out device.
Here's an example from a EL4032, a 2-port device:

```
     8  bit   IN          FALSE  lcec.0.D19.aout-0-absmode
     8  float OUT             0  lcec.0.D19.aout-0-curr-dc
     8  bit   IN          FALSE  lcec.0.D19.aout-0-enable
     8  float I/O             1  lcec.0.D19.aout-0-max-dc
     8  float I/O            -1  lcec.0.D19.aout-0-min-dc
     8  bit   OUT         FALSE  lcec.0.D19.aout-0-neg
     8  float I/O             0  lcec.0.D19.aout-0-offset
     8  bit   OUT         FALSE  lcec.0.D19.aout-0-pos
     8  s32   OUT             0  lcec.0.D19.aout-0-raw
     8  float I/O             1  lcec.0.D19.aout-0-scale
     8  float IN              0  lcec.0.D19.aout-0-value
     8  bit   IN          FALSE  lcec.0.D19.aout-1-absmode
     8  float OUT             0  lcec.0.D19.aout-1-curr-dc
     8  bit   IN          FALSE  lcec.0.D19.aout-1-enable
     8  float I/O             1  lcec.0.D19.aout-1-max-dc
     8  float I/O            -1  lcec.0.D19.aout-1-min-dc
     8  bit   OUT         FALSE  lcec.0.D19.aout-1-neg
     8  float I/O             0  lcec.0.D19.aout-1-offset
     8  bit   OUT         FALSE  lcec.0.D19.aout-1-pos
     8  s32   OUT             0  lcec.0.D19.aout-1-raw
     8  float I/O             1  lcec.0.D19.aout-1-scale
     8  float IN              0  lcec.0.D19.aout-1-value
     8  bit   OUT          TRUE  lcec.0.D19.slave-online
     8  bit   OUT          TRUE  lcec.0.D19.slave-oper
     8  bit   OUT         FALSE  lcec.0.D19.slave-state-init
     8  bit   OUT          TRUE  lcec.0.D19.slave-state-op
     8  bit   OUT         FALSE  lcec.0.D19.slave-state-preop
     8  bit   OUT         FALSE  lcec.0.D19.slave-state-safeop
```

For each channel, the following pins are created:

### Write or read/write pins:

- `absmode`: If set, then the device's output won't be allowed to go
  negative.  Any attempt to output a negative value will be
  interpreted as a positive value instead.  That is, requesting `-0.3`
  will result in `+0.3` instead.
- `enable`: If false (default), then output on this port is disabled.
  You must set `enable` to true before outputting anything.
- `max-dc`: The maximum duty cycle allowed for this port.  Must be
  between `1.0` and `min-dc`.
- `min-dc`: The minimum duty cycle allowed for this port.  Must be
  between `-1.0` and `max-dc`.
- `offset`: Used to control the scaling between `value` and `curr-dc` (and hence `raw`).  See below.
- `scale`:  Used to control the scaling between `value` and `curr-dc` (and hence `raw`).  See below.
- `value`: The requested output value.  Should be between `max-dc` and
  `min-dc`, unless `offset` or `scale` are set.  See below.

### Read-only pins:

- `curr-dc`: The current duty cycle for this device.
- `neg`: True if the current output value is negative.
- `pos`: True if the current output value is positive.
- `raw`: The raw value sent to the hardware; should be between `-0x7fff` and `0x7fff`.

### Mapping between `value` and `raw`.

The device's actual output is controlled by `raw` and `curr_dc`, which
are calculated as follows:

`curr_dc = (value / scale) + offset`

The value of `curr_dc` should range between `-1` and `+1`, but can be restricted by `min-dc` and `max-dc`.

`raw = curr_dc * max_value`.  For Beckhoff hardware, `max_value` is generally `0x7fff`, or 32,767.

If `scale` is 1 (the default) and `offset` is 0, then setting `value`
to 0 leads to `raw = (0/1 + 0) * 32767`, which is `0`.  If `value` is
`1`, then the same math would give `curr_dc = 1/1 + 0`, or `1`, and
then `raw = 1 * 32767`, which is `32767`.

### Example

Suppose you have a spindle that takes a 0..10V input to control the
spindle RPM.  The maximum RPM is 25,000 RPM, but it loses torque as
the speed slows and doesn't support anything below 5,000 RPM, which
happens at 2V.  To disable the spindle entirely, the voltage needs to
be turned off (set to 0V).

To accomplish this set `scale=25000` and `min_dc=0.2`.  This should
result in being able to set `value=RPM` directly:

If `RPM=25000`, then `value=25000`.  Then `curr_dc=25000/25000=1`, so
`raw=32767`, which is the largest value allowed, which will result in
the hardware sending 10V.  Any attempt to set RPM higher will just be
capped at 25000.

If `RPM=3000` (which is below the 5,000 RPM limit mentioned above), then `value=3000`.  Then `curr_dc=3000/25000=0.120`.
This is less than `min_dc`, so it will be bumped up to `0.2`.  That
will result in a `raw` value of `0.2*32767=6554`, which should result
in around 2V, which will give 5,000 RPM.

To disable the spindle entirely, instead of setting RPM=0, just set
`enable` to `false`.  That will drop the output to 0V.

# Omron MX2 VFD Driver

The `lcec_ommx2` driver supports the Omron MX2 VFD with Omron's 3G3AX-MX2-ECT EtherCAT module.

## Setup

In your XML file, you should have an entry somewhat like this:

```xml
<masters>
  <master idx="0" appTimePeriod="2000000" refClockSyncCycles="1000">
    ...
    <slave idx="1" type="OMMX2" name="spindle"/>
	...
  </master>
</masters>
```

This is the first CiA 402 VFD supported by LinuxCNC Ethercat, and
higher-level support is still lacking.  Instructions TBD.

See the [CiA 402](cia402.md) documentation for additional details
about how to configure CiA 402 devices in LinuxCNC. 

## Devices

This driver was developed with a 2.2 kW first-generation ("v1") Omron
MX2, with hardware revision `V1.00` and software `V1.10`.  It's
currently unknown if later MX2s or newer firmware will change anything
critical.

### Caveats

- The MX2 has a very weird PDO layout; it only allows 2 PDO entries
  per PDO (vs 8-12 for normal devices), but it allows up to 512 (!) RX
  and TX PDOs, vs 2-4 on many cheaper devices.
- On my hardware, I get gripes in `dmesg` about reads from 0x6046
  using the wrong size.  I'm not sure what's up; I'm using 32 bit
  reads, CiA 402 says that should be 32 bits, `ethercat sdos` for the
  device shows that it should be 32 bits, and `etherat upload -t
  uint32` works fine.

## Configuration

TBD

## Inputs

TBD

## Outputs

TBD

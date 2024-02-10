# RTelligent ECR and ECT Stepper Drivers

The `lcec_rtec` driver supports a number of
[RTelligent](http://www.rtelligent.net/index.htm) EtherCAT stepper
drives, including the ECT60 and ECR60.

## Setup

In your XML file, you should have an entry somewhat like this:

```xml
<masters>
  <master idx="0" appTimePeriod="2000000" refClockSyncCycles="1000">
    ...
    <slave idx="1" type="ECT60" name="x-axis">
      <dcConf assignActivate="300" sync0Cycle="*1" sync0Shift="0"/>
      <modParam name="peakCurrent_amps" value="1.0"/>
      <modParam name="controlMode" value="closedloop"/>
      <modParam name="homeOffset" value="1"/>
    </slave>
	...
  </master>
</masters>
```

See the [CiA 402](cia402.md) documentation for additional details
about how to configure CiA 402 devices in LinuxCNC.  At a minimum, you
will need to include the `cia402` HAL component.

## Configuration

This driver takes a number of `<modParam>` options that control its
operation.  There are a number of `rtec`-specific parameters, plus a
number of additional [`cia402` modParams](cia402.md).

#### `<modParam name="peakCurrent_amps" value="3.0"/>`

Set the maximum current for the motor, in amps.

#### `<modParam name="motorResolution_pulses" value="10000"/>`

Set the number of pulses for a full motor rotation.

#### `<modParam name="standbyTime_ms" value="500"/>`

Set the standby time, before the drive drops down to standby current.  See below.

#### `<modParam name="standbyCurrent_pct" value="50"/>`

Set the percentage of current used when in standby mode.

#### `<modParam name="output1Func" value="general|alarm|brake|in-place"/>`

Set the function for output port 1.  See below for options.

#### `<modParam name="output2Func" value="general|alarm|brake|in-place"/>`

Set the function for output port 2.  See below for options.

#### `<modParam name="output1Polarity" value="nc|no"/>`

Set the polarity for output port 1, either `nc` or `no`.

#### `<modParam name="output2Polarity" value="nc|no"/>`

Set the polarity for output port 2, either `nc` or `no`.

#### `<modParam name="input3Func" value="general|..."/>`

Set the function for input port 3.  See below for options.

#### `<modParam name="input4Func" value="general|..."/>`

Set the function for input port 4.  See below for options.

#### `<modParam name="input5Func" value="general|..."/>`

Set the function for input port 5.  See below for options.

#### `<modParam name="input6Func" value="general|..."/>`

Set the function for input port 6.  See below for options.

#### `<modParam name="input3Polarity" value="nc|no"/>`

Set the polarity for input port 3, either `nc` or `no`.

#### `<modParam name="input4Polarity" value="nc|no/>`

Set the polarity for input port 4, either `nc` or `no`.

#### `<modParam name="input5Polarity" value="nc|no/>`

Set the polarity for input port 5, either `nc` or `no`.

#### `<modParam name="input6Polarity" value="nc|no/>`

Set the polarity for input port 6, either `nc` or `no`.

#### `<modParam name="filterTime_us"  value="6400"/>`

Set the filter time in microseconds.

#### `<modParam name="shaftLockTime_us" value="1000"/>`

Set the shaft lock time in microseconds.

#### `<modParam name="motorAutoTune" value="true|false"/>`

Enable or disable motor autotuning.

#### `<modParam name="stepperPhases" value="2"/>`

Set the number of stepper phases for the motor.

#### `<modParam name="controlMode"  value="openloop|closedloop|foc"/>`

Set the control mode for the motor.

#### `<modParam name="encoderResolution" value="4000"/>`

The number of encoder pulses per revolution.

#### `<modParam name="positionErrorLimit" value="4000"/>`

## Inputs

The single-axis ECR/ECT devices have 6 inputs.  Inputs 1 and 2 are
used for the encoder (and don't appear to be available on the
open-loop ECR models), while inputs 3, 4, 5, and 6 are available for
use.

The `inputXFunc` (where X is 3, 4, 5, or 6) modParam controls the function of each input.  The available options are:

- `general`
- `cw-limit`
- `ccw-limit`
- `home`
- `clear-fault`
- `emergency-stop`
- `motor-offline`
- `probe1`
- `probe2`

On my ECT60, the pins come pre-assigned with various functions:

- input 3 is `cw-limit`
- input 4 is `ccw-limit`
- input 5 is `home`
- input 6 is `motor-offline`

If you want to use these as general-purpose inputs, then you'll want
to add `<modParam name="input3Func" value="general"/>` to your XML
config.

On my system, all of the inputs default to "normally-closed"
operation.  You may also want to change this via `<modParam
name="input3Polarity" value="nc"/>`.

## Outputs

The single-axis ECR/ECT devices have 2 outputs.  The `output1Func` and `output2Func` modParams control the function of each output.  The available options are:

- `general`
- `alarm`
- `brake`
- `in-place`

On my ECT60, the pins come pre-assigned as:

- output 1 is `alarm`
- output 2 is `brake`

To use these as general-purpose outputs, you'll probably want to add `<modParam name="output1Func" value="general">`.

Like the input pins, you can also change the polarity between
normally-open and normally-closed using `<modParam
name="output1Polarity" value="no"/>`.  My ECT60 defaults to `nc` on
both ports.

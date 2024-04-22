# RTelligent DRV-E EtherCAT DC Servo Drives

The `rtdrv` driver supports 3 of RTelligent's DRV series of DC servo drives.

## Setup

In your XML file, you should have an entry somewhat like this:

```xml
<masters>
  <master idx="0" appTimePeriod="2000000" refClockSyncCycles="1000">
    ...
    <slave idx="1" type="DRV400E" name="x-axis"/>
	...
  </master>
</masters>
```

See the [CiA 402](cia402.md) documentation for additional details
about how to configure CiA 402 devices in LinuxCNC.  At a minimum, you
will need to include the `cia402` HAL component.

## Devices

This driver includes settings for all 3 known DRV-series EtherCAT DC
servos:

- [DRV400E](http://www.rtelligent.net/Low-Voltage-Brushless-Servo-Drive.html)
  1-axis, 400W DC servo.
- [DRV750E](http://www.rtelligent.net/Low-Voltage-Brushless-Servo-Drive.html)
  1-axis, 750W DC servo.
- [DRV1500E](http://www.rtelligent.net/Low-Voltage-Brushless-Servo-Drive.html)
  1-axis, 1500W DC servo.

Only the DRV400E has been tested at this point.

### Caveats

- Only the DRV400E has been tested.
- The EtherCAT PID numbers for the 750 and 1500W models isn't actually
  documented anywhere that I can find, so I guessed at them.
  RTelligent is fairly predictable (they tend to increment by 1 in
  each series), so they're *probably* right.
- There's currently no support for specifying motor parameters.  For
  my motor, it looks like everything was autodetected correctly, so
  this hopefully won't be needed.

## Configuration

This driver takes a number of `<modParam>` options that control its
operation.  There are a few Leadshine-specific parameters, plus a
number of additional [`cia402` modParams](cia402.md).

The parameters are listed in their single-axis form; dual-axis devices
have two sets of these parameters, one starting with `ch1` and one
starting with `ch2`.  For example, `ch1peakCurrent_amps` and
`ch2peakCurrent_amps`.

<dl>
<dt>&lt;modParam name="input1Func" value="..."/&gt</dt>
<dd>Set the function of input IN1.</dd>

<dt>&lt;modParam name="input2Func" value="..."/&gt</dt>
<dd>Set the function of input IN2.</dd>

<dt>&lt;modParam name="input3Func" value="..."/&gt</dt>
<dd>Set the function of input IN3.</dd>

<dt>&lt;modParam name="input4Func" value="..."/&gt</dt>
<dd>Set the function of input IN4.</dd>

<dt>&lt;modParam name="input5Func" value="..."/&gt</dt>
<dd>Set the function of input IN5.</dd>

<dt>&lt;modParam name="input6Func" value="..."/&gt</dt>
<dd>Set the function of input IN6.</dd>

<dt>&lt;modParam name="input7Func" value="..."/&gt</dt>
<dd>Set the function of input IN7.</dd>

<dt>&lt;modParam name="input8Func" value="..."/&gt</dt>
<dd>Set the function of input IN8.</dd>

<dt>&lt;modParam name="output1Func" value="..."/&gt</dt>
<dd>Set the function of output OUT1.</dd>

<dt>&lt;modParam name="output2Func" value="..."/&gt</dt>
<dd>Set the function of output OUT2.</dd>

<dt>&lt;modParam name="output3Func" value="..."/&gt</dt>
<dd>Set the function of output OUT3.</dd>

<dt>&lt;modParam name="output4Func" value="..."/&gt</dt>
<dd>Set the function of output OUT4.</dd>

## Inputs

The DRV series has 8 digital inputs available.  By default the first 4
of these have special functions assigned to them and the other 4 are
available for use.  The assigned functions can be controlled via the
`inputXFunc` series of `modParams`.  Available values are:

- `general`: the input is available as a general-purpose input port
  for LinuxCNC to use.
- `servo_enable`: enable (disable?) the servo.
- `alarm_clear`: clean any servo alarms.
- `pulse_command_prohibition`: unknown
- `clear_position_deviation`: unknown
- `positive_limit`: The positive limit switch for homing.  Note that
  this is for CiA 402-native homing, which is not yet supported.
- `negative_limit`: The positive limit switch for homing.  Note that
  this is for CiA 402-native homing, which is not yet supported.
- `gain_switching`: unknown
- `egear_switching`: unknown
- `zero-speed_clamp`: unknown
- `control_mode_selection_1`: unknown
- `estop`: Trigger an emergency stop.
- `position_command_prohibition`: unknown
- `step_position_trigger`: unknown
- `multi_segment_run_command_switching_1`: unknown
- `multi_segment_run_command_switching_2`: unknown
- `multi_segment_run_command_switching_3`: unknown
- `multi_segment_run_command_switching_4`: unknown
- `torque_command_direction_setting`: unknown
- `speed_command_direction_setting`: unknown
- `position_command_direction_setting`: unknown
- `multi_segment_position_command_enable`: unknown
- `back_to_home_input`: unknown
- `home`: Home
- `user1`: unknown
- `user2`: unknown
- `user3`: unknown
- `user4`: unknown
- `user5`: unknown
- `control_mode_selection_2`: unknown
- `probe1`: Used for CiA 402 probing.
- `probe2`: Used for CiA 402 probing.

Many of these are not generally useful with LinuxCNC, but are included
for completeness.

## Outputs

The DRV series has 4 digital outputs available.  By default the first 3
of these have special functions assigned to them and the last one is
available for use.  The assigned functions can be controlled via the
`outputXFunc` series of `modParams`.  Available values are:

- `brake`: motor brake trigger.
- `alarm`: drive alarm triggered.
- `position_reached`: true if the servo is in position.
- `speed_reached`: true if the servo is at the requested speed.
- `servo_ready`: true if the servo is ready.
- `internal_position_command_stop`: unknown
- `return_to_origin_completed`: unknown
- `user1`: unknown
- `user2`: unknown
- `user3`: unknown
- `user4`: unknown
- `user5`: unknown
- `torque_reached`: true if target torque reached.
- `out_of_tolerance_output`: unknown
- `general`: the output is available as a general-purpose output port
  for LinuxCNC to use.


# Fault injection using Curious Bolt as a voltage glitcher
![](images/scope.jpg)

Curious Bolt has a _crowbar_ voltage glitcher, a type of glitcher that can pull the voltage rails of a target chip to ground at very high resolution (as short as a few nanoseconds).

## Connecting to your target

- Connect at least one wire from ground on the Bolt to ground on the target (see [Pinout](pinout.md)).
- Connect the glitcher _sig_ pin to the voltage rails you want to glitch on your target.

General tip: voltage glitching is finicky and works in very narrow bands. Improve your luck by getting as good a physical connection as you can. Shorter leads, better grounding, and better contact give better results.

## Configuring the glitch

Next you need to configure the duration of your glitch. The python library exposes `Scope.glitch.repeat`, where you can configure how many 8.3ns clock cycles you want the glitch to last:

```python
from scope import Scope
s = Scope()
s.glitch.repeat = 60  # ~0.5 microseconds
```

## Triggering the glitch

Now that you've connected and set up your voltage glitcher, put your target in the state where you want to glitch, and trigger the glitcher once:

```python
s.trigger()
```

This will make the voltage drop very briefly only once. You can also trigger repeatedly, using e.g.:

```python
for i in range(5000):
    s.trigger()
```

Additionally, you can arm the Bolt to be triggered by an incoming signal change on one of the 8 input channels:

```python
# Trigger when input 0 goes from high to low
s.arm(0, Scope.FALLING_EDGE)
```

When triggering from an input pin, you might want to start the glitch some specific time after the trigger occurs. You can set the time offset in 8.3ns cycles before the glitch is executed, using:

```python
s.glitch.ext_offset=1000  # ~8.3 microseconds
```

ℹ️ Note: both manual and external triggers are shared between the glitcher, logic analyzer (via PulseView), and the scope.
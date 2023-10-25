# Programmable I/O

You might want to reset your target before glitching, or send a specific sequence of button presses.

The Bolt has 4 output channels that can be programmed to go high or low at certain times after a trigger has occurred (see [Voltage Glitching](voltage_glitching.md#triggering-the-glitch) for info about triggers).

## I/O API

```python
from scope import Scope
s = Scope()

# Set output channel 0 to high level after 500*8.3ns
s.io.add(0, 1, delay=500)

# Set output channel 0 to low level after 1000*8.3ns
s.io.add(0, 0, delay=500)


# Set output channel 3 to high level after 1500*8.3ns
s.io.add(3, 1, delay=1500)
```
import logging
import serial
from serial.tools.list_ports import comports
from typing import Union, List

class ADCSettings():
    def __init__(self, dev:serial) -> None:
        self._dev = dev
        self._clk_freq = 25000000
        self._delay = 0

    @property
    def clk_freq(self) -> int:
        """
        Get ADC CLK Frequency
        """
        return self._clk_freq
    
    @clk_freq.setter
    def clk_freq(self, freq:int) -> None:
        """
        Set ADC CLK Frequency, valid between 0.5kHz and 31.25MHz
        """
        BASE_CLK = 250000000
        pio_freq = freq*4
        divider = BASE_CLK/pio_freq
        integer = int(divider)
        frac = int((divider-integer)*256)
        if frac == 256:
            frac = 0
            integer += 1
        self._dev.write(f":ADC:PLL {integer},{frac}\n".encode("ascii"))
        self._clk_freq = BASE_CLK/(integer+frac/256)/4
    
    @property
    def delay(self) -> int:
        """
        Get delay between trigger and start of sampling in cycles (8.3ns)
        """
        return self._delay
    
    @delay.setter
    def delay(self, delay) -> None:
        """
        Set delay between trigger and start of sampling in cycles (8.3ns)
        """
        self._delay = delay
        self._dev.write(f":ADC:DELAY {int(delay)}\n".encode("ascii"))

class GlitchSettings():
    def __init__(self, dev:serial) -> None:
        self._dev = dev
        self._offset = 10
        self._repeat = 10

    @property
    def ext_offset(self) -> int:
        """
        Delay between trigger and start of glitch in cycles (8.3ns)
        """
        return self._offset
    
    @ext_offset.setter
    def ext_offset(self, offset:int) -> None:
        """
        Set delay between trigger and start of glitch in cycles (8.3ns)
        """
        self._dev.write(f":GLITCH:DELAY {int(offset)}\n".encode("ascii"))
        self._offset = offset
    
    @property
    def repeat(self) -> int:
        """Width of glitch in cycles (approx = 8.3 ns * width)"""
        return self._repeat

    @repeat.setter
    def repeat(self, width:int) -> None:
        """
        Set width of glitch in cycles (8.3ns)
        """
        self._dev.write(f":GLITCH:LEN {int(width)}\n".encode("ascii"))
        self._repeat = width

class GPIOSettings():
    def __init__(self, dev:serial) -> None:
        self.gpio = []
        for i in range(0, 4):
            self.gpio.append(list())
        self.dev = dev
        self.MAX_CHANGES = 255
        self.MAX_DELAY = 2147483647
        
    def add(self, pin:int, state:bool, delay:int=None, seconds:float=None) -> None:
        """
        Add state change to gpio

        Arguments
        ---------
        pin : int
            Which pin to add state change to, [0,3]
        state : bool
            What the state of the pin should be
        delay : int
            Number of cycles delay after state change, each cycle is 8.3ns
        seconds : float
            Seconds of delay after state change if delay is not provided

        Returns
        -------
        None        
        """
        if pin < 0 or pin > 3:
            raise ValueError("Pin must be between 0 and 3")
        
        if len(self.gpio[pin]) >= self.MAX_CHANGES:
            raise ValueError("Pin reached max state changes")

        if delay is None:
            if seconds is None:
                raise ValueError("delay or seconds must be provided")
            delay = int(seconds*100000000)

        if delay > self.MAX_DELAY:
            raise ValueError("delay exceeds maximum")
        
        self.gpio[pin].append((delay << 1) | state)
    
    def reset(self) -> None:
        """
        Reset all GPIO state changes

        Arguments
        ---------
        None

        Returns
        -------
        None
        """
        self.dev.write(b":GPIO:RESET\n")
        for i in range(0, 4):
            self.gpio[i].clear()

    def upload(self) -> None:
        """
        Upload GPIO changes to device

        Arguments
        ---------
        None

        Returns
        -------
        None
        """
        self.dev.write(b":GPIO:RESET\n")
        for i in range(0, 4):
            for item in self.gpio[i]:
                self.dev.write(f":GPIO:ADD {i},{item}\n".encode("ascii"))
    

class Scope():
    RISING_EDGE = 0
    FALLING_EDGE = 1

    def __init__(self, port=None) -> None:
        if port is None:
            ports = comports()
            matches = [p.device for p in ports if p.interface == "Curious Bolt API"]
            if len(matches) != 1:
                matches = [p.device for p in ports if p.product == "Curious Bolt"]
                matches.sort()
                matches.reverse()
                if len(matches) != 2:
                    raise IOError('Curious Bolt device not found. Please check if it\'s connected, and pass its port explicitly if it is.')
            port = matches[0]

        self._port = port
        self._dev = serial.Serial(port, 115200*10, timeout=1.0)
        self._dev.reset_input_buffer()
        self._dev.write(b":VERSION?\n")
        data = self._dev.readline().strip()
        if data is None or data == b"":
            raise ValueError("Unable to connect")
        print(f"Connected to version: {data.decode('ascii')}")
        self.adc = ADCSettings(self._dev)
        self.glitch = GlitchSettings(self._dev)
        self.io = GPIOSettings(self._dev)

    def arm(self, pin:int=0, edge:int=RISING_EDGE) -> None:
        """
        Arms the glitch/gpio/adc based on trigger pin

        Arguments
        ---------
        pin : int
            Which pin to use for trigger [0:7]
        edge : int
            On what edge to trigger can be RISING_EDGE or FALLING_EDGE

        Returns
        -------
        None
        """
        if pin < 0 or pin > 7:
            raise ValueError("Pin invalid")
        
        if edge != self.RISING_EDGE and edge != self.FALLING_EDGE:
            raise ValueError("Edge invalid")

        self._dev.write(f":TRIGGER:PIN {pin},{edge}\n".encode("ascii"))

    def trigger(self) -> None:
        """
        Immediately trigger the glitch/gpio/adc

        Arguments
        ---------
        None

        Returns
        -------
        None
        """
        self._dev.write(b":TRIGGER:NOW\n")
    
    def default_setup(self) -> None:
        """
        Load some safe defaults into settings
        """
        self.glitch.repeat = 10
        self.glitch.ext_offset = 0
        self.adc.delay = 0
        self.adc.clk_freq = 10000000
        self.io.reset()

    def con(self) -> None:
        """
        Connect to device if serial port is not open
        """
        if not self._dev.is_open:
            self._dev.open()

    def dis(self) -> None:
        """
        Disconnect from serial port
        """
        self._dev.close()

    def get_last_trace(self, as_int:bool=False) -> Union[List[int], List[float]]:
        """
        Returns the latest captured data from ADC

        Arguments
        ---------
        as_int : bool
            Returns the data as raw 10bit value from the adc
        
        Returns
        -------
        data : list<int>
        
        """
        self._dev.reset_input_buffer() #Clear any data
        self._dev.write(b":ADC:DATA?\n")

        data = self._dev.readline()
        if data is None:
            return []
        data = data.decode("ascii").strip()
        if "ERR" in data:
            logging.warning(f"Received: {data}")
            return []
        data = data.split(",")
        data = [x for x in data if x != '']
        if as_int:
            return [int(x) for x in data]
        volt_per_step = 2 / 10 / 1024  # 2V pk-pk, 10x amplified from source, in 10-bit ADC
        return [(float(x)-512)*volt_per_step for x in data]

    def plot_last_trace(self, continuous=False):
        try:
            import matplotlib.pyplot as plt 
        except ImportError:
            print("Dependencies missing, please install python package matplotlib")
            return
        plt.ion()
        fig = plt.figure()
        ax = fig.add_subplot(111)
        ax.set_xlabel("Time since trigger (us)")
        ax.set_ylabel("Voltage difference (mV)")
        us_per_measurement = 1e6 / self.adc.clk_freq
        line, = ax.plot([float(x) * us_per_measurement for x in range(50000)], [0] * 50000, 'b-')

        while True:
            try:
                res = self.get_last_trace()
                if len(res) != 50000:
                    print(f"Got {len(res)} entries, skipping")
                    if continuous:
                        continue
                    else:
                        break
                trace = [x*1000 for x in res]
                line.set_ydata(trace)
                ax.relim()
                ax.autoscale_view()
                fig.canvas.draw()
                fig.canvas.flush_events()
                if continuous:
                    self.trigger()
                    continue
                else:
                    plt.show()
                    break
            except KeyboardInterrupt:
                break

    def update(self):
        self._dev.write(b":BOOTLOADER\n")
    


if __name__ == "__main__":
    s = Scope()
    s.default_setup()
    s.trigger()
    s.plot_last_trace(continuous=True)

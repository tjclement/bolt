# File permissions for USB devices in Linux

Some distros do not allow users access to USB devices by default. If you think this might be the case for you, run the following:

```bash
# Allow access to serial devices
adduser <your username> dialout

# Allow access to other USB devices
adduser <your username> plugdev

# Set SWD debugger to access by plugdev group
echo 'SUBSYSTEM=="usb", ATTRS{idVendor}=="0483", ATTRS{idProduct}=="3748", MODE="0660", GROUP="plugdev"' > /etc/udev/rules.d/49-ecsc.rules

# Now log out and log in again, or run `newgrp -` to load groups only in the current shell
```
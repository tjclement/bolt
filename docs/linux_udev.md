# File permissions for USB devices in Linux

Some distros do not allow access to USB serial devices to users by default. If you think this might be the case for you, run the following:

```bash
adduser <your username> plugdev

# Set SWD debugger to access by plugdev group
echo 'SUBSYSTEM=="usb", ATTRS{idVendor}=="0483", ATTRS{idProduct}=="3748", MODE="0660", GROUP="plugdev"' > /etc/udev/rules.d/49-ecsc.rules

# Set target board to access by plugdev group
echo 'SUBSYSTEM=="usb", ATTRS{idVendor}=="1a86", ATTRS{idProduct}=="7523", MODE="0660", GROUP="plugdev"' >> /etc/udev/rules.d/49-ecsc.rules

# Set Curious Bolt to access by plugdev group
echo 'SUBSYSTEM=="usb", ATTRS{idVendor}=="cafe", ATTRS{idProduct}=="4002", MODE="0660", GROUP="plugdev"' >> /etc/udev/rules.d/49-ecsc.rules

# Now log out and log in again (or restart)
```
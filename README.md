# usbcomm

requires the appropriate ftdi d2xx driver to be installed. See the drivers/ folder for compatible with a few platforms. For other platform drivers must be downloaded from ftdichip.com

Be sure to understand compatibility issues between D2XX drivers and other FTDI serial usb drivers.

For the D2XX driver to work, other drivers may need to unloaded (rmmod, modprobe -r or kextunload). You can also unbind specific usb devices in linux by writing the bound device id to 'unbind' in /sys/... 


build with:

$ mkdir build
$ cd build
$ cmake ../
$ make

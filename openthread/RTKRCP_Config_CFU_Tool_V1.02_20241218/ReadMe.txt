This tool currently only works on little-endian systems.

Compile the tool:
	make

Recompile the tool:
	make clean
	make

Usage:

1)try to update the firmware of the RCP

	./rtkcfu update ./config.txt [folder path to offer bins] [folder path to payload bins] <forceIgnoreVersion> 
	
	the config.txt describes the RCP device node and the path to the RCP configuration file. For example:
		device = /dev/ttyUSB0
		ConfigBin = ./8771HTV_Config.bin
	the ConfigBin represents the path to the RCP configuration file.

	forceIgnoreVersion is optional. You can use this option when you need to ignore the firmware version check and force a firmware upgrade.

Note：
	if RCP connects with host dircetly, such as:
			____________        __________
			| 8771GUV   |  usb   | host    |
			|___________|<------>|_________|
	RCP's device node may be changed each time RCP reboots.
	there is a way in Linux to fix the /dev/ttyACMx or /dev/ttyUSBx assignment for the RCP.
	for example, fix the /dev/ttyACMx assignment for 8771GUV RCP as follows.
	1)Make sure 8771GUV is plugged in.
	2)Find out the ttyACMx assignment for 8771GUV RCP.
	3)input the following command and found out the idVendor and idProduct.
		udevadm info -a -p /sys/class/tty/ttyACMx
	4)create the file /etc/udev/rules.d/10-local.rules. Then add the following line：
		KERNEL=="ttyACM*", ATTRS{idVendor}=="0bda", ATTRS{idProduct}=="8777", MODE:="0777", SYMLINK+="ttyRTKRCP"
	  the idVendor and idProduct are got in step 3. Then unplug the 8771GUV RCP and re-plug it.
	  Finally, Each time 8771GUV RCP is plugged in, the device name ttyRTKRCP is assigned for 8771GUV RCP.
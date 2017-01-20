Logitech G710+ Keyboard Driver
==========================

The Logitech G710+ mechanical keyboard is a great piece of hardware. Unfortunately, there is no support in the kernel for the additional features of the keyboard.

This kernel driver allows the keys M1-MR and G1-G6 to be used.


Instalation Instructions
-------------------------

First you have to compile the driver:
<pre>
make
</pre>

if the compilation was successfull, you will now have a new kernel module in the directory.

The next step is to install the kernel module:

<pre>
sudo make install
sudo depmod -a
</pre>

At this point the generic driver will still take control. The simple fix for that issue is to copy the 90-logitech-g710-plus.rules file from the misc folder to /etc/udev/rules.d/:

<pre>
sudo cp misc/90-logitech-g710-plus.rules /etc/udev/rules.d/
</pre>

Finally, if you do not receive any events in your environment, it might be necessary to use the modmap provided in the misc folder:

<pre>
xmodmap misc/.Xmodmap
</pre>


Usage
--------------------------
Use the key shortcut utilities provided by your DE to make use of the additional buttons.

API
--------------------------
The driver also exposes a way to set the keyboard backlight bightness. That is done by writing either:

<pre>
/sys/bus/hid/devices/0003:046D:C24D.XXXX/logitech-g710/led_m1
/sys/bus/hid/devices/0003:046D:C24D.XXXX/logitech-g710/led_m2
/sys/bus/hid/devices/0003:046D:C24D.XXXX/logitech-g710/led_m3
/sys/bus/hid/devices/0003:046D:C24D.XXXX/logitech-g710/led_mr
/sys/bus/hid/devices/0003:046D:C24D.XXXX/logitech-g710/led_keys
/sys/bus/hid/devices/0003:046D:C24D.XXXX/logitech-g710/led_wasd
</pre>

where XXXX is varies (e.g. 0008)

The led_* file expects a single number which is a brightness value 0-4 (if other value is given it defaults to 0):

<pre>
echo -n "0" > /sys/bus/hid/devices/0003:046D:C24D.XXXX/logitech-g710/led_m*  off
echo -n "1" > /sys/bus/hid/devices/0003:046D:C24D.XXXX/logitech-g710/led_m*  on
</pre>

<pre>
echo -n "0" > /sys/bus/hid/devices/0003:046D:C24D.XXXX/logitech-g710/led_(keys|wasd)  off
echo -n "1" > /sys/bus/hid/devices/0003:046D:C24D.XXXX/logitech-g710/led_(keys|wasd)  brightness 1
echo -n "2" > /sys/bus/hid/devices/0003:046D:C24D.XXXX/logitech-g710/led_(keys|wasd)  brightness 2
echo -n "3" > /sys/bus/hid/devices/0003:046D:C24D.XXXX/logitech-g710/led_(keys|wasd)  brightness 3
echo -n "4" > /sys/bus/hid/devices/0003:046D:C24D.XXXX/logitech-g710/led_(keys|wasd)  brightness 4 (FULL)
</pre>


The driver also used led device if kernel version is above 4.3.0. Above attibutes are aligned with below.

<pre>
/sys/bus/hid/devices/0003:046D:C24D.XXXX/leds/input*:red:mr/brightness
/sys/bus/hid/devices/0003:046D:C24D.XXXX/leds/input*:yellow:m1/brightness
/sys/bus/hid/devices/0003:046D:C24D.XXXX/leds/input*:yellow:m2/brightness
/sys/bus/hid/devices/0003:046D:C24D.XXXX/leds/input*:yellow:m3/brightness
/sys/bus/hid/devices/0003:046D:C24D.XXXX/leds/input*:while:keys/brightness
/sys/bus/hid/devices/0003:046D:C24D.XXXX/leds/input*:while:wasd/brightness
</pre>

Note led devices can be found also from "/sys/class/leds/"

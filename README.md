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
The driver also exposes a way to set the keyboard backlight intensity. That is done by writing either:

<pre>
/sys/bus/hid/devices/0003:046D:C24D.XXXX/logitech-g710/led_macro
/sys/bus/hid/devices/0003:046D:C24D.XXXX/logitech-g710/led_keys
</pre>

where XXXX is varies (e.g. 0008)

The led_macro file expects a single number which is a bitmask of the first 4 bits. Each bit corresponds to a button. E.g. if you want to light up M1 and M3, you must write the bit pattern 0101, which corresponds to 5 in decimal:

<pre>
echo -n "5" > /sys/bus/hid/devices/0003:046D:C24D.XXXX/logitech-g710/led_macro
</pre>

Writing the led_keys is a bit more involved. The file expects a single digit which is constructed in the following way:
value= wasd << 4 | key

where: 
   wasd - WASD light intensity
   key - Other key light intensity

Only values from 0-4 accepted
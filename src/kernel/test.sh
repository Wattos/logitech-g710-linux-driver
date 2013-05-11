sudo modprobe -r hid_lg_g710_plus && sudo modprobe -r hid_generic && make && sudo make install && sudo depmod -a && sudo modprobe hid-lg-g710-plus && sudo modprobe hid-generic

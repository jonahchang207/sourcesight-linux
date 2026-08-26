# Makefile — build the person_mouse kernel module.
#
# Prerequisites: linux-headers-$(uname -r), build-essential
#
# Usage:
#   make           build the module
#   sudo make install   install and load
#   sudo make uninstall unload

obj-m := person_mouse.o

KDIR ?= /lib/modules/$(shell uname -r)/build
SRC  := $(CURDIR)

all:
	$(MAKE) -C $(KDIR) M=$(SRC) modules

clean:
	$(MAKE) -C $(KDIR) M=$(SRC) clean

install:
	sudo ./install.sh
	@echo "Loaded.  Device at /dev/person-mouse"
	@echo "Test:  echo -n '960 540' > /dev/person-mouse"

uninstall:
	-sudo modprobe -r person_mouse
	sudo rm -f /etc/modules-load.d/person-mouse.conf
	sudo rm -f /etc/udev/rules.d/99-person-mouse.rules
	sudo rm -f /lib/modules/$(shell uname -r)/extra/person_mouse.ko
	sudo depmod -a
	sudo udevadm control --reload-rules

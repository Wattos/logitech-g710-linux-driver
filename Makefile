SUBDIRS=src/kernel src/userspace

default:
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir $@; done

install:
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir $@; done

clean:
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir $@; done
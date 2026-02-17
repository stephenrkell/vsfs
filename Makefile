.PHONY: default run-qemu clean
default: vsfs
run-qemu: qemu-disk-image

sources += vsfs.c cmdline.c

CFLAGS += -g -Wall -MMD
deps := $(patsubst %.c,%.d,$(sources))
-include $(deps)

vsfs: $(patsubst %.c,%.o,$(sources))

clean:
	rm -f vsfs *.o *.d *.i *.s

.PHONY: default run-qemu clean
default: vsfs
run-qemu: qemu-disk-image

CFLAGS += -g -Wall

vsfs: vsfs.o cmdline.o

clean:
	rm -f vsfs *.o

obj-m := sfs.o

sfs-objs := super.o 

all: ko mkfs-sfs

ko:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

mkfs-sfs:
	gcc -Wall mkfs.sfs.c -o mkfs.sfs

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm mkfs.sfs -f 
#all:
#	make -C /home/walkerlala/project/os/linux-2.6 M=$(PWD) modules
#clean:
#	make -C /home/walkerlala/project/os/linux-2.6 M=$(PWD) clean

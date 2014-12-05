

CC = gcc
CFLAGS = -Wall -O2 -D__KERNEL__ -DMODULE
#CFLAGS = -Wall -O2 -D__KERNEL__ -DMODULE -DUZIX_DEBUG=1

all: uzix.o

uzix.o: uzix.c uzix.h
	$(CC) $(CFLAGS) -c uzix.c -o uzix.o

clean:
	rm -f *.o *~

install:
	@echo
	@echo "just copy uzix.o to /lib/modules/XXX/yyy"
	@echo "where XXX is your kernel version and yyy is something"
	@echo "configured in your /etc/modules.conf (misc is a good"
	@echo "one, if you have it)."
	@echo "Then run depmod -a , and you should live happily"
	@echo "ever after."
	@echo


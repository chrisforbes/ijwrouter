# make -r -R --warn-undefined-variables

################################################################################

CFLAGS := 
INPUTS := billing dhcp ethernet nbns sntp stats str table user \
			gcc-arm/hal_ethernet \
			ip/arp ip/arptab ip/conf ip/icmp ip/stack ip/tcp ip/udp

################################################################################

OBJECTS := $(INPUTS:%=%.o)

all: $(OBJECTS)
clean:
	-rm -f $(OBJECTS)

.SECONDEXPANSION:
$(OBJECTS): %.o: $$(@:%.o=../src/%.c)
	arm-elf-gcc $(CFLAGS) -o $@ -c $<


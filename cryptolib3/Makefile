export CRLIB3 := $(dir $(firstword $(MAKEFILE_LIST)))
include makeconf
BINDIR := _$(OS)-amd64

all:
	$(MAKE) -f $(CRLIB3)/aes/Makefile

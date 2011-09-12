include Makefile.defs

BRCM	:= $(shell test -d /opt/brcm && echo -n yes)
all: client/test_client process_spy/process_enforcer os_spy/os_enforcer 

.PHONY:

process_spy/process_enforcer: enforcer/fake_enforcer
	cd process_spy; make -j3

os_spy/os_enforcer: enforcer/fake_enforcer
	cd os_spy; make -j3

vmm_spy/vmm_enforcer: enforcer/fake_enforcer
ifdef BRCM
	cd vmm_spy; make -j3
else
	touch vmm_spy/vmm_enforcer
endif

client/test_client:
	cd client; make

enforcer/fake_enforcer:
	cd enforcer; make -j3

clean:
	cd enforcer; make clean
	cd process_spy; make clean
	cd os_spy; make clean
	cd vmm_spy; make clean
	cd client; make clean

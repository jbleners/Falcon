include ../Makefile.defs
CXX		:= /opt/brcm/hndtools-mipsel-uclibc/bin/mipsel-linux-g++
CXXFLAGS	:= -Wall -g -I/usr/include/sfslite -I.. -I../binary_libs -I${PROJECT_INCLUDES}
LDFLAGS		:= -L../binary_libs -lasync -lbridge -lyajl -static
HEADERS		:= ../enforcer/enforcer.h ../enforcer/client_prot.h util.h
OBJS		:= vmm_enforcer.o util.o spy_prot.o obs_prot.o
all: vmm_enforcer test_fdb

.PHONY:
vmm_enforcer: $(OBJS) enforcer.o client_prot.o config.o
	$(CXX) $(LDFLAGS) $^ -o $@

test_fdb: $(OBJS) test_fdb.cc
	$(CXX) $(CXXFLAGS) -L../binary_libs -lbridge -static util.o test_fdb.cc -o $@

enforcer.o: $(HEADERS) ../enforcer/enforcer.cc
	$(CXX) $(CXXFLAGS) -c ../enforcer/enforcer.cc

client_prot.o: $(HEADERS)
	$(CXX) $(CXXFLAGS) -c ../enforcer/client_prot.cc

config.o: ../common/config.cc
	$(CXX) $(CXXFLAGS) -c $^

$(OBJS): spy_prot.h spy_prot.cc obs_prot.h obs_prot.cc $(HEADERS)

spy_prot.h: ../enforcer/spy_prot.x
	${SFSLIB}/rpcc -h $^ -o spy_prot.h

spy_prot.cc: ../enforcer/spy_prot.x
	${SFSLIB}/rpcc -c $^ -o spy_prot.C
	mv spy_prot.C spy_prot.cc

obs_prot.h: obs_prot.x
	${SFSLIB}/rpcc -h $^ -o obs_prot.h

obs_prot.cc: obs_prot.x
	${SFSLIB}/rpcc -c $^ -o obs_prot.C
	mv obs_prot.C obs_prot.cc
clean:
	rm -fr *.o spy_prot.cc spy_prot.h obs_prot.cc obs_prot.h vmm_enforcer test_fdb

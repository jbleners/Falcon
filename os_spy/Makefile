include ../Makefile.defs
CXXFLAGS	:= -Wall -g -I.. -I${SFSINCLUDE} -I${PROJECT_INCLUDES}
LDFLAGS		:= ${SFSLINK} -lasync -larpc -lyajl -lvirt
HEADERS		:= ../enforcer/enforcer.h ../enforcer/client_prot.h
OBJS		:= os_enforcer.o spy_prot.o obs_prot.o
GENERATED	:= obs_prot.cc obs_prot.h spy_prot.cc spy_prot.h
all: os_enforcer os_worker vmm_observer

.PHONY:
os_enforcer: $(OBJS) enforcer.o client_prot.o config.o
	$(CXX) $(LDFLAGS) $^ -o $@

vmm_observer: vmm_observer.o config.o obs_prot.o spy_prot.o
	$(CXX) $(LDFLAGS) $^ -o $@

os_worker: os_worker.o
	$(CXX) $(LDFLAGS) $^ -o $@

enforcer.o: $(HEADERS) ../enforcer/enforcer.cc
	$(CXX) $(CXXFLAGS) -c ../enforcer/enforcer.cc

client_prot.o: $(HEADERS)
	$(CXX) $(CXXFLAGS) -c ../enforcer/client_prot.cc

config.o: ../common/config.cc
	$(CXX) $(CXXFLAGS) -c $^

$(OBJS): $(GENERATED) $(HEADERS)


spy_prot.h: ../enforcer/spy_prot.x
	${SFSLIB}/rpcc -h $^ -o spy_prot.h

spy_prot.cc: ../enforcer/spy_prot.x
	${SFSLIB}/rpcc -c $^ -o spy_prot.C
	mv spy_prot.C spy_prot.cc

obs_prot.h: ../vmm_spy/obs_prot.x
	${SFSLIB}/rpcc -h $^ -o obs_prot.h

obs_prot.cc: ../vmm_spy/obs_prot.x
	${SFSLIB}/rpcc -c $^ -o obs_prot.C
	mv obs_prot.C obs_prot.cc

clean:
	rm -fr *.o $(GENERATED) os_enforcer os_worker vmm_observer

include ../Makefile.defs
CXXFLAGS	:= -Wall -g -I.. -I${SFSINCLUDE} -I${PROJECT_INCLUDES}
LDFLAGS		:= ${SFSLINK} -lasync -larpc -lyajl
HEADERS		:= ../enforcer/enforcer.h ../enforcer/client_prot.h process_observer.h obs_prot.h
OBJS		:= process_enforcer.o spy_prot.o parse_proc.o
LIBOBJ		:= spy.o
all: incrementer process_enforcer $(LIBOBJ)

.PHONY:
incrementer: incrementer.cc
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@

process_enforcer: $(OBJS) enforcer.o client_prot.o config.o
	$(CXX) $(LDFLAGS) $^ -o $@

$(LIBOBJ): $(HEADERS) process_observer.cc process_observer.h
	$(CXX) -Wall -g -I.. -I${PROJECT_INCLUDES} -fPIC process_observer.cc -c -o $@

# I wish there was a better way to build the following 3 object files. However,
# because we are building the enforcer for 3 distinct platforms, we just
# compile the base files for each part.
enforcer.o: $(HEADERS) ../enforcer/enforcer.cc
	$(CXX) $(CXXFLAGS) -c ../enforcer/enforcer.cc

client_prot.o: $(HEADERS)
	$(CXX) $(CXXFLAGS) -c ../enforcer/client_prot.cc

config.o: ../common/config.cc
	$(CXX) $(CXXFLAGS) -c $^

$(OBJS): spy_prot.h spy_prot.cc $(HEADERS)

spy_prot.h: ../enforcer/spy_prot.x
	${SFSLIB}/rpcc -h $^ -o spy_prot.h

spy_prot.cc: ../enforcer/spy_prot.x
	${SFSLIB}/rpcc -c $^ -o spy_prot.C
	mv spy_prot.C spy_prot.cc

clean:
	rm -fr *.o spy_prot.cc spy_prot.h process_enforcer incrementer

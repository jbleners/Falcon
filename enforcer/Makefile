include ../Makefile.defs
CXXFLAGS	:= -Wall -g -I.. -I${SFSINCLUDE} -I${PROJECT_INCLUDES} -I.
LDFLAGS		:= ${SFSLINK} -lasync -larpc -lyajl
HEADERS 	:= enforcer.h
OBJS		:= enforcer.o client_prot.o spy_prot.o

.PHONY:
fake_enforcer: enforcer.o spy_prot.o client_prot.o fake_enforcer.cc
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -lresolv $^ -o $@

$(OBJS): $(HEADERS) spy_prot.h client_prot.h spy_prot.cc client_prot.cc

spy_prot.h: spy_prot.x
	${SFSLIB}/rpcc -h $^ -o spy_prot.h

spy_prot.cc: spy_prot.x
	${SFSLIB}/rpcc -c $^ -o spy_prot.C
	mv spy_prot.C spy_prot.cc

client_prot.cc: ../client/client_prot.x
	${SFSLIB}/rpcc -c $^ -o client_prot.C
	mv client_prot.C $@
	
client_prot.h: ../client/client_prot.x
	${SFSLIB}/rpcc -h $^ -o $@

clean:
	rm -fr *.o spy_prot.cc spy_prot.h fake_enforcer client_prot.cc client_prot.h


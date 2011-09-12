include ../Makefile.defs
CXXFLAGS	:= -Wall -fPIC -g -I.. -I${PROJECT_INCLUDES}
LDFLAGS		:= -lpthread -lrt
TESTFLAGS	:= -Wall -g $(LDFLAGS)
HEADERS		:= FalconCallback.h CallbackList.h Generation.h
GENERATED	:= client_prot_xdr.cc spy_prot_xdr.cc spy_prot_clnt.cc client_prot_svc.cc spy_prot.h client_prot.h status.h
OBJS		:= FalconCallback.o CallbackList.o Generation.o Watchdog.o FalconLayer.o spy_prot_xdr.o client_prot_xdr.o spy_prot_clnt.o client_prot_svc.o client_ops.o FalconClient.o client.o
TESTS		:= FalconCallback.test CallbackList.test Generation.test

all: libfalcon.a test_client


.PHONY:

test_client: libfalcon.a fake_client.cc
	g++ -I${PROJECT_INCLUDES} $(LDFLAGS) libfalcon.a fake_client.cc -o test_client

libfalcon.a: $(OBJS)
	rm -fr $@
	$(LD) -r -o $@ $(OBJS)

#test_client: libfalcon.a
#$(CXX) $(CXXFLAGS) $(LDFLAGS) -L. -lfalcon test.cc -o $@

$(OBJS): $(GENERATED) $(HEADERS)

#$(OBJS): $(HEADERS)

test: $(TESTS)

$(TESTS): libfalcon.a

%.test: %.cc
	g++ $(TESTFLAGS) -DFALCON_TEST=1 $^ -o $@
	./$@

$(GENERATED): client_prot.x ../enforcer/spy_prot.x
	rpcgen -M client_prot.x
	cp ../enforcer/spy_prot.x .
	cp ../enforcer/status.h .
	rpcgen -M spy_prot.x
	rm spy_prot.x
	rm spy_prot_svc.c
	rm client_prot_clnt.c
	sed -i 's/static//' client_prot_svc.c
	sed -i 's/main/__unused_main_/' client_prot_svc.c
	sed -i '/buf/d' *_xdr.c
	rename .c .cc *.c

clean:
	rm -fr $(GENERATED) *.o test_client libfalcon.a *.test

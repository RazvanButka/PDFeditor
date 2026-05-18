INC=$(shell pkg-config --cflags gsoap)
LIB=$(shell pkg-config --libs gsoap)
OBJECTS=soapds.o #unixds.o inetds.o soapds.o proto.o

help:
	@echo "Use one of these"
	@echo "	make serverds -- to build the server"
	@echo "	make test_client -- to build the test client"
	@echo "	make clean -- to remove unwanted files"
	@echo ""
	@echo "Notice that you need some libraries to compile:"
	@echo "	sudo apt install gsoap libgsoap-dev"
	#@echo "	sudo apt install libncurses-dev libncursesw-dev"
	#@echo "	-- NCurses support was removed, however"
	#@echo ""
	@echo "If something went wrong, you can try..."
	@echo "	sudo apt update"
	@echo "	sudo apt upgrade"

serverds: threeds.c ${OBJECTS} operationsSOAP.nsmap
	gcc -g ${INC} threeds.c soapC.c soapServer.c ${OBJECTS} ${LIB} -l pthread -l gsoap -Wall -o serverds

test_client: test_client.c
	gcc -g test_client.c -Wall -o test_client

soapds.o: soapds.c operationsSOAP.nsmap
	gcc -g ${INC} soapds.c -c -o soapds.o -Wall  -l pthread -l gsoap

operationsSOAP.nsmap: operations.h
	soapcpp2 -S -c -x operations.h

clean:
	rm -f ${OBJECTS} operationsSOAP* soapC.c soapH.h soapServer.c soapServerLib.c soapStub.h soapClient.c soapClient.cpp soapClientLib.c soapClientLib.cpp *.xsd *.wsdl

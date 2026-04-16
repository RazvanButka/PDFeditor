INC=-I/opt/homebrew/include
LIB=-L/opt/homebrew/lib
OBJECTS=soapds.o #unixds.o inetds.o soapds.o proto.o

help:
	@echo "Use one of these"
	@echo "	make serverds -- to build the server"
	#@echo "	make inetclient -- to build the inet test client"
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

serverds: threeds.c ${OBJECTS} SampleServices.nsmap
	gcc -g ${INC} threeds.c soapC.c soapServer.c ${OBJECTS} ${LIB} -l pthread -l gsoap -Wall -o serverds


soapds.o: soapds.c SampleServices.nsmap
	gcc -g ${INC} soapds.c -c -o soapds.o -Wall  -l pthread -l gsoap

SampleServices.nsmap: sclient.h
	soapcpp2 -S -c -x sclient.h

clean:
	rm -f ${OBJECTS} *.nsmap soapC.c soapH.h soapServer.c soapServerLib.c soapStub.h *.xsd *.wsdl

INC=-I/opt/homebrew/include
LIB=-L/opt/homebrew/lib
OBJECTS=unixds.o inetds.o soapds.o proto.o

help:
	@echo "Use one of these"
	@echo "	make serverds -- to build the server"
	@echo "	make inetclient -- to build the inet test client"
	@echo "	make clean -- to remove unwanted files"
	@echo ""
	@echo "Notice that you need some libraries to compile:"
	@echo "	sudo apt install gsoap libgsoap-dev"
	@echo "	sudo apt install libncurses-dev libncursesw-dev"
	@echo "	-- NCurses support was removed, however"
	@echo ""
	@echo "If something went wrong, you can try..."
	@echo "	sudo apt update"
	@echo "	sudo apt upgrade"

serverds: threeds.c ${OBJECTS} SampleServices.nsmap
	gcc -g ${INC} threeds.c soapC.c soapServer.c ${OBJECTS} ${LIB} -l pthread -l gsoap -Wall -o serverds

unixds.o: unixds.c
	gcc -g ${INC} unixds.c -c -o unixds.o -Wall -l pthread

inetds.o: inetds2.c proto.o
	gcc -g ${INC} inetds2.c -c -o inetds.o -Wall  -l pthread

proto.o: proto.c proto.h
	gcc -g proto.c -c -o proto.o -Wall  -l pthread

soapds.o: soapds.c SampleServices.nsmap
	gcc -g ${INC} soapds.c -c -o soapds.o -Wall  -l pthread -l gsoap

SampleServices.nsmap: sclient.h
	soapcpp2 -S -c -x sclient.h

inetclient: inetsample2.c proto.o
	gcc -g inetsample2.c proto.o -Wall -o inetclient

clean:
	rm -f ${OBJECTS} *.nsmap soapC.c soapH.h soapServer.c soapServerLib.c soapStub.h *.xsd *.wsdl

# NOTICE: use sudo apt-get install gsoap libgsoap-dev
# NOTICE: use sudo apt-get install libncurses5-dev libncursesw5-dev
# NOTICE: use the sample INET client (gcc inetsample.c -o inets; ./inets)

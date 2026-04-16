MUPDF_LIB = -lmupdf -lmujs -lfreetype -lz -lm \
			-ljpeg  -lopenjp2  -ljbig2dec

OBJECTS=unixds.o inetds.o soapds.o proto.o pdfds.o 

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
	gcc -g threeds.c soapC.c soapServer.c ${OBJECTS} -lpthread -l gsoap -l config ${MUPDF_LIB} -Wall -o serverds

unixds.o: unixds.c 
	gcc -g unixds.c -c -o unixds.o -Wall -lpthread

inetds.o: inetds2.c proto.o
	gcc -g inetds2.c -c -o inetds.o -Wall  -lpthread

proto.o: proto.c proto.h
	gcc -g proto.c -c -o proto.o -Wall  -lpthread

soapds.o: soapds.c SampleServices.nsmap
	gcc -g soapds.c -c -o soapds.o -Wall  -lpthread -l gsoap

pdfds.o: pdfds.c proto.h 
	gcc -g pdfds.c -c -o pdfds.o -Wall -lpthread ${MUPDF_LIB}

SampleServices.nsmap: sclient.h
	soapcpp2 -S -c -x sclient.h

inetclient: inetsample2.c proto.o
	gcc -g inetsample2.c proto.o -Wall -o inetclient

pdfclient: pdfclient.c proto.o 
	gcc -g pdfclient.c proto.o -Wall -o pdfclient

clean:
	rm -f ${OBJECTS} *.nsmap soapC.c soapH.h soapServer.c soapServerLib.c soapStub.h *.xsd *.wsdl


# NOTICE: use sudo apt-get install gsoap libgsoap-dev
# NOTICE: use sudo apt-get install libncurses5-dev libncursesw5-dev
# NOTICE: use the sample INET client (gcc inetsample.c -o inets; ./inets)
# NOTICE: use sudo apt-get install -y gsoap libgsoap-dev libconfig-dev libmupdf-dev
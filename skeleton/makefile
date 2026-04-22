# Makefile — Server PCD (Milestone 1 + T26 rezolvator sisteme liniare)
#
# Folosim aceste optiuni:
#   -Wall: activeaza warning-urile de baza recomandate
#   -Wextra: activeaza warning-uri suplimentare utile
#   -Wpedantic: semnaleaza abateri fata de standardul C
#   -Wshadow: avertizeaza cand o variabila o ascunde pe alta
#   -Wconversion: avertizeaza la conversii implicite riscante
#   -Wformat=2: verificari mai stricte pentru printf / fprintf etc.
#   -Wnull-dereference: avertizeaza pentru pointeri potential nuli
#
#
# Instaleaza dependintele mai intai:
#   make deps
#
# Tinte de build:
#   make all           — server + clientii de test
#   make serverds      — server multi-thread (UNIX + INET + SOAP)
#   make inetclient    — client general de test INET
#   make tidy          — analiza statica pentru fisierele scrise manual
#   make clean         — sterge artefactele de build

CC      = gcc
CFLAGS  = -g -std=c11 -D_POSIX_C_SOURCE=200809L           -Wall -Wextra -Wpedantic -Wshadow -Wconversion           -Wformat=2 -Wnull-dereference
LDFLAGS = -lpthread -lgsoap -lconfig -llapacke -lopenblas -lm

SERVER_OBJS = config.o unixds.o inetds2.o soapds.o proto.o solver.o

.PHONY: all clean deps help tidy

all: serverds inetclient equation_type1_client

help:
	@echo "Targets:"
	@echo "  make deps                  -- install build dependencies"
	@echo "  make all                   -- build server + clients"
	@echo "  make serverds              -- build the server only"
	@echo "  make inetclient            -- build the general INET test client"
	@echo "  make equation_type1_client -- build the equation test client"
	@echo "  make tidy                  -- run clang-tidy on manual source files"
	@echo "  make clean                 -- remove build artifacts"

deps:
	sudo apt-get update
	sudo apt-get install -y 		gsoap libgsoap-dev 		libconfig-dev 		liblapacke-dev 		libopenblas-dev 		clang-tidy

serverds: threeds.c $(SERVER_OBJS) SampleServices.nsmap
	$(CC) $(CFLAGS) threeds.c soapC.c soapServer.c 		$(SERVER_OBJS) $(LDFLAGS) -o serverds

config.o: config.c config.h
	$(CC) $(CFLAGS) -c config.c -o config.o

unixds.o: unixds.c config.h
	$(CC) $(CFLAGS) -c unixds.c -o unixds.o

inetds2.o: inetds2.c proto.h config.h solver.h
	$(CC) $(CFLAGS) -c inetds2.c -o inetds2.o

proto.o: proto.c proto.h
	$(CC) $(CFLAGS) -c proto.c -o proto.o

solver.o: solver.c solver.h
	$(CC) $(CFLAGS) -c solver.c -o solver.o

soapds.o: soapds.c SampleServices.nsmap config.h
	$(CC) $(CFLAGS) -c soapds.c -o soapds.o -I. -lgsoap

SampleServices.nsmap: sclient.h
	soapcpp2 -S -c -x sclient.h

inetclient: inetsample2.c proto.o
	$(CC) $(CFLAGS) inetsample2.c proto.o -o inetclient

equation_type1_client: equation_type1_client.c proto.o
	$(CC) $(CFLAGS) equation_type1_client.c proto.o -o equation_type1_client

tidy:
	clang-tidy config.c -- $(CFLAGS)
	clang-tidy equation_type1_client.c -- $(CFLAGS)
	clang-tidy inetds2.c -- $(CFLAGS)
	clang-tidy inetsample2.c -- $(CFLAGS)
	clang-tidy proto.c -- $(CFLAGS)
	clang-tidy solver.c -- $(CFLAGS)
	clang-tidy threeds.c -- $(CFLAGS)
	clang-tidy unixds.c -- $(CFLAGS)
	clang-tidy soapds.c -- $(CFLAGS)

clean:
	rm -f $(SERVER_OBJS) serverds inetclient equation_type1_client
	rm -f soapC.c soapH.h soapServer.c soapServerLib.c soapStub.h
	rm -f *.nsmap *.xsd *.wsdl

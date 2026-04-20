# Makefile — PCD Server (Milestone 1 + T26 Linear System Solver)
#
# Install dependencies first:
#   make deps
#
# Build targets:
#   make all           — server + both test clients
#   make serverds      — multi-threaded server (UNIX + INET + SOAP)
#   make inetclient    — general INET test client
#   make solve_client  — T26 linear system test client
#   make clean         — remove build artifacts

CC      = gcc
CFLAGS  = -g -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lpthread -lgsoap -lconfig -llapacke -lopenblas -lm

# -----------------------------------------------------------------------
# Objects shared between server modules
# -----------------------------------------------------------------------

SERVER_OBJS = config.o unixds.o inetds2.o soapds.o proto.o solver.o

# -----------------------------------------------------------------------
# Top-level targets
# -----------------------------------------------------------------------

.PHONY: all clean deps help

all: serverds inetclient solve_client

help:
	@echo "Targets:"
	@echo "  make deps          -- install build dependencies"
	@echo "  make all           -- build server + both clients"
	@echo "  make serverds      -- build the server only"
	@echo "  make inetclient    -- build the general INET test client"
	@echo "  make solve_client  -- build the T26 linear system client"
	@echo "  make clean         -- remove build artifacts"

deps:
	sudo apt-get update
	sudo apt-get install -y \
		gsoap libgsoap-dev \
		libconfig-dev \
		liblapacke-dev \
		libopenblas-dev

# -----------------------------------------------------------------------
# Server
# -----------------------------------------------------------------------

serverds: threeds.c $(SERVER_OBJS) SampleServices.nsmap
	$(CC) $(CFLAGS) threeds.c soapC.c soapServer.c \
		$(SERVER_OBJS) $(LDFLAGS) -o serverds

# -----------------------------------------------------------------------
# Individual object files
# -----------------------------------------------------------------------

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

# -----------------------------------------------------------------------
# gsoap code generation (from sclient.h service description)
# -----------------------------------------------------------------------

SampleServices.nsmap: sclient.h
	soapcpp2 -S -c -x sclient.h

# -----------------------------------------------------------------------
# Test clients
# -----------------------------------------------------------------------

inetclient: inetsample2.c proto.o
	$(CC) $(CFLAGS) inetsample2.c proto.o -o inetclient

solve_client: solve_client.c proto.o
	$(CC) $(CFLAGS) solve_client.c proto.o -lm -o solve_client

equation_client: equation_type1_client.c proto.o
	$(CC) $(CFLAGS) equation_client.c proto.o -o equation_type1_client

# -----------------------------------------------------------------------
# Clean
# -----------------------------------------------------------------------

clean:
	rm -f $(SERVER_OBJS) serverds inetclient solve_client
	rm -f soapC.c soapH.h soapServer.c soapServerLib.c soapStub.h
	rm -f *.nsmap *.xsd *.wsdl

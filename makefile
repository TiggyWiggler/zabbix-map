CC=gcc		#compiler
TARGET=zabbix-map		# name of output file is same as one of the internal .c libraries. Not a good approach but I wanted this output name.
JSONCDIR=/usr/local/include/json-c/		# Location of JSON C Include files
JSONLDIR=/usr/local/lib			# Location of JSON C Library files

CFLAGS=-I$(JSONCDIR) -I.
LDFLAGS=-L$(JSONLDIR)
LIBS=-ljson-c -lcurl -lm

all:	main.o strcommon.o zconn.o zmap.o Forests.o ip.o 
	$(CC) main.c strcommon.c zconn.c zmap.c Forests.c ip.c -o $(TARGET) $(CFLAGS) $(LDFLAGS) $(LIBS)

clean:	
	rm *.o $(TARGET)

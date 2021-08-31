CC=gcc		#compiler
TARGET=zabbix-map		# name of output file is same as one of the internal .c libraries. Not a good approach but I wanted this output name.
JSONCDIR=/usr/local/include/json-c/		# Location of JSON C

CFLAGS=-I$(JSONCDIR) -I.
LIBS=-ljson-c -lcurl -lm 

all:	main.o zconn.o zmap.o Forests.o ip.o 
	$(CC) main.c zconn.c zmap.c Forests.c ip.c -o $(TARGET) $(CFLAGS) $(LIBS)

clean:	
	rm *.o $(TARGET)

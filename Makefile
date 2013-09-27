SOURCES=novena-eeprom.c
OBJECTS=$(SOURCES:.c=.o)
EXEC=novena-eeprom
MY_CFLAGS += -Wall -O0 -g
MY_LIBS +=

all: $(OBJECTS)
	$(CC) $(LIBS) $(LDFLAGS) $(OBJECTS) $(MY_LIBS) -o $(EXEC)

clean:
	rm -f $(EXEC) $(OBJECTS)

.c.o:
	$(CC) -c $(CFLAGS) $(MY_CFLAGS) $< -o $@

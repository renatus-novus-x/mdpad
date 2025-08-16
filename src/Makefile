TARGET = mdpad.x

CROSS = m68k-xelf-
CC = $(CROSS)gcc
AS = $(CROSS)as
LD = $(CROSS)gcc

CFLAGS = -m68000 -O2 -g -DX68K
LDFLAGS =

all: $(TARGET)

$(TARGET): mdpad.o
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	-rm -f *.o *.elf* $(TARGET)

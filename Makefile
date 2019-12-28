CC=gcc
CFLAGS=-g `pkg-config gtk+-3.0 cairo libpulse-mainloop-glib --cflags`
LD=gcc
LIBS=-g `pkg-config gtk+-3.0 cairo libpulse-mainloop-glib --libs` -lm

all: mainwindow

clean:
	rm -f *.o mainwindow

mainwindow.o: \
	mainwindow.c \
	pulse.h \
	aylet/driver.h \
	aylet/sound.h
	$(CC) $(CFLAGS) $< -o $@ -c

pulse.o: \
	pulse.c \
	pulse.h
	$(CC) $(CFLAGS) $< -o $@ -c

sound.o: \
	aylet/sound.c \
	aylet/driver.h \
	aylet/sound.h
	$(CC) $(CFLAGS) $< -o $@ -c

mainwindow: \
	mainwindow.o \
	pulse.o \
	sound.o
	$(LD) $^ $(LIBS) -o $@

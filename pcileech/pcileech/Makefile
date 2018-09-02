CC=gcc
CPP=g++
CFLAGS=-I. -Ijson/include -D LINUX -pthread `pkg-config libusb-1.0 --libs --cflags` -g
DEPS = pcileech.h
OBJ = pcileech oscompatibility.o pcileech.o device.o device3380.o devicefile.o devicefpga.o device605_tcp.o devicetmd.o executor.o extra.o help.o kmd.o memdump.o mempatch.o statistics.o tlp.o util.o vfs.o vmm.o vmmproc.o
OBJCPP = deviceqemu.o

%.o: %.cpp $(DEPS)
	$(CPP) -c -o $@ $< $(CFLAGS)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

pcileech: $(OBJ) $(OBJCPP)
	$(CPP) -g -o $@ $^ $(CFLAGS) -lstdc++

clean:
	rm *.o

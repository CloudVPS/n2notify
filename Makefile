include makeinclude

OBJ	= main.o

all: n2notifyd n2event

n2notifyd: $(OBJ)
	$(LD) $(LDFLAGS) -o n2notifyd $(OBJ) $(LIBS)

n2event: n2event.o
	$(LD) $(LDFLAGS) -o n2event n2event.o $(LIBS)

clean:
	rm -f *.o
	rm -f n2notifyd
	rm -f n2event

allclean: clean
	rm -f makeinclude configure.paths platform.h
	
install: all
	./makeinstall

makeinclude:
	@echo please run ./configure
	@false

SUFFIXES: .cpp .o
.cpp.o:
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $<

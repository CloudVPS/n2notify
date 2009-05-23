include makeinclude

OBJ	= main.o schema.o

all: mkschema n2notifyd n2event

mkschema: mkschema.o
	$(LD) $(LDFLAGS) -o mkschema mkschema.o $(LIBS)

n2notifyd: $(OBJ)
	$(LD) $(LDFLAGS) -o n2notifyd $(OBJ) $(LIBS)

n2event: n2event.o
	$(LD) $(LDFLAGS) -o n2event n2event.o $(LIBS)

clean:
	rm -f *.o
	rm -f n2notifyd
	rm -f n2event
	rm -f schema.cpp
	rm -f mkschema

allclean: clean
	rm -f makeinclude configure.paths platform.h
	
install: all
	./makeinstall

makeinclude:
	@echo please run ./configure
	@false

schema.cpp: mkschema n2host.schema.xml
	./mkschema

SUFFIXES: .cpp .o
.cpp.o:
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $<

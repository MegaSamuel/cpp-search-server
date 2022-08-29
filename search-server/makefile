CC  = g++
CFLAGS  = -c -O2 -std=c++17 -Wall -Wextra -pedantic
LDFLAGS = 
SOURCES = $(sort $(patsubst %.cpp,%.o,$(wildcard *.cpp)))
OBJECTS = $(SOURCES:.cpp=.o)
PRJNAME = search_server

ifeq ($(OS),Windows_NT)
CMD_DELETE	=	del /F
EXESUFFIX	=	.exe
SYSLIBFILES	=
else
CMD_DELETE	=	rm -f
EXESUFFIX	=
SYSLIBFILES	=	tbb pthread
endif

STRIP		=	strip
LIBFILES	=	$(addprefix -l, $(SYSLIBFILES))

all: $(SOURCES) $(PRJNAME)$(EXESUFFIX)

# make executable file
$(PRJNAME)$(EXESUFFIX): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@ $(LIBFILES)
	$(STRIP) $@

# make one object file for each *.cpp file
.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	$(CMD_DELETE) $(OBJECTS)
	$(CMD_DELETE) $(PRJNAME)$(EXESUFFIX)

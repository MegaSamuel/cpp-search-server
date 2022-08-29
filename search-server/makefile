CC  = g++
CFLAGS  = -c -O -std=c++17 -Wall -Wextra -pedantic
LDFLAGS = 
SOURCES = $(sort $(patsubst %.cpp,%.o,$(wildcard *.cpp)))
OBJECTS = $(SOURCES:.cpp=.o)
PRJNAME = search_server

ifeq ($(OS),Windows_NT)
CMD_DELETE	=	del /F
EXESUFFIX	=	.exe
else
CMD_DELETE	=	rm -f
EXESUFFIX	=
endif

all: $(SOURCES) $(PRJNAME)
	
$(PRJNAME): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	$(CMD_DELETE) $(OBJECTS)
	$(CMD_DELETE) $(PRJNAME)$(EXESUFFIX)

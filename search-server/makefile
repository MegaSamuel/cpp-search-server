CC  = g++
CFLAGS  = -c -std=c++20 -Wall -Wextra -pedantic
LDFLAGS = 
SOURCES = $(sort $(patsubst %.cpp,%.o,$(wildcard *.cpp)))
OBJECTS = $(SOURCES:.cpp=.o)
PRJNAME = search_server

CMD_DELETE	=	del /F
EXESUFFIX	=	.exe

all: $(SOURCES) $(PRJNAME)
	
$(PRJNAME): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) -O3 $< -o $@

clean:
	$(CMD_DELETE) $(OBJECTS)
	$(CMD_DELETE) $(PRJNAME)$(EXESUFFIX)

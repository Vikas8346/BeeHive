EXECUTABLE = beehive

# The following line looks for a project's main() in files named project*.cpp,
# executable.cpp (substituted from EXECUTABLE above), or main.cpp
PROJECTFILE = $(or $(wildcard project*.cpp $(EXECUTABLE).cpp), main.cpp)
# If main() is in another file delete line above, edit and uncomment below
#PROJECTFILE = mymainfile.cpp

# designate which compiler to use
CXX         = g++

# rule for creating objects
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $*.cpp

# list of sources used in project
SOURCES = beehive.cpp
# list of objects used in project
OBJECTS     = $(SOURCES:%.cpp=%.o)

# Default Flags
CXXFLAGS = -std=c++17 -Wconversion -Wall -Wextra -pedantic
POSTFIXFLAGS =
SANITIZEFLAGS = -fsanitize=address -fsanitize=undefined

# Operating system specific stuff
ifeq ($(OS),Windows_NT)
	POSTFIXFLAGS = -lgdi32
	SANITIZEFLAGS =
else
	UNAME_S = $(shell uname -s)
	ifeq ($(UNAME_S),Darwin)
		OSXFLAGS = -framework CoreFoundation -framework CoreGraphics -framework ImageIO -framework CoreServices -framework ApplicationServices
		CXXFLAGS += $(OSXFLAGS)
	endif
endif

# Build all executables
all: debug release

debug:
	make beehive_debug

release:
	make beehive_release

# make debug - will compile sources with $(CXXFLAGS) -g3 and -fsanitize
#              flags also defines DEBUG and _GLIBCXX_DEBUG
beehive_debug: beehive.cpp screencapture.cpp
	$(CXX) $(CXXFLAGS) -g3 -DDEBUG -D_GLIBCXX_DEBUG $(SANITIZEFLAGS) $(SOURCES) -o $(EXECUTABLE)_debug $(POSTFIXFLAGS)
beehive_release: beehive.cpp screencapture.cpp
	$(CXX) $(CXXFLAGS) -O3 $(SOURCES) -o $(EXECUTABLE)_release $(POSTFIXFLAGS)

clean:
	make clean_files
	make clean_output
.PHONY: clean

# make clean_files - remove .o files, executables, tarball
clean_files:
	rm -Rf *.dSYM
	rm -f $(OBJECTS) $(EXECUTABLE) $(EXECUTABLE)_debug
	rm -f $(EXECUTABLE)_release
	rm -f $(EXECUTABLE)_valgrind
.PHONY: clean_files

clean_output:
	rm -f output/*
.PHONY: clean_output

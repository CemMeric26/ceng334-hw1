# Define compiler commands
CC=gcc
CXX=g++

# Define any compile-time flags
CFLAGS=
CXXFLAGS=

# Define the source files
C_SOURCES=parser.c
CPP_SOURCES=main.cpp

# Define the object files based on source files
C_OBJECTS=$(C_SOURCES:.c=.o)
CPP_OBJECTS=$(CPP_SOURCES:.cpp=.o)

# Define the executable file
EXECUTABLE=myprogram

# Default rule
all: $(EXECUTABLE)

# Rule for linking the final executable
$(EXECUTABLE): $(C_OBJECTS) $(CPP_OBJECTS)
	$(CXX) $(LDFLAGS) $^ -o $@

# Rule for compiling C source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Rule for compiling C++ source files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean rule
clean:
	rm -f $(C_OBJECTS) $(CPP_OBJECTS) $(EXECUTABLE)

# Phony targets
.PHONY: all clean

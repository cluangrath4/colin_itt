# Malefile for itt_mini_collector.cpp

CXX = icpx
TARGET = libitt_tracer.so
SRC = colintrace.cpp

# Add the path to your ittnotify.h
ITT_INCLUDE = /home/cluangrath/colin_itt/

# C++ flags
CXXFLAGS = -std=c++17 -g -O2 -fPIC

# Linker flags
LDFLAGS = -shared -lpthread

# === BUILD RULES ===
all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -I$(ITT_INCLUDE) -o $@ $< $(LDFLAGS)

.PHONY: all clean

clean:
	rm -f $(TARGET)
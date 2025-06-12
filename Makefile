ITT_INCLUDE = /home/cluangrath/pti-gpu/tools/unitrace/build/ittheaders

# Compiler (same as Unitrace used)
CXX = icpx

# Output shared object
TARGET = libcolintrace.so

# Source file
SRC = colintrace.cpp

# C++ flags
CXXFLAGS = -std=c++17 -O2 -fPIC -fvisibility=default -DPTI_VERSION=0.49.28

# Include paths
INCLUDES = -I$(ITT_INCLUDE)

# Linker flags
LDFLAGS = -shared -lpthread -ldl

# === BUILD RULES ===
all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET)
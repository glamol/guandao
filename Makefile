# compiler and flags
CXX = clang++
CXXFLAGS = -std=c++17 -O2

# paths
RAYLIB_PATH = lib/raylib-5.5
RAYLIB_BUILD = $(RAYLIB_PATH)/build
RAYLIB_LIB = $(RAYLIB_BUILD)/raylib/libraylib.a
RAYLIB_INCLUDE = -I$(RAYLIB_PATH)/src

# project
SRC = src/main.cpp
OUT = build/guandao

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S), Darwin)  # macOS
    LDFLAGS = -lm -ldl -lpthread -framework OpenGL -framework Cocoa -framework IOKit -framework CoreFoundation
else ifeq ($(UNAME_S), Linux)  # Linux
    LDFLAGS = -lm -ldl -lpthread -lGL
else
    $(error Unsupported platform: $(UNAME_S))
endif


all: $(RAYLIB_LIB) $(OUT)

# build guandao
$(OUT): $(SRC) $(RAYLIB_LIB)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $(OUT) $(SRC) $(RAYLIB_LIB) $(RAYLIB_INCLUDE) $(LDFLAGS)

# build Raylib
$(RAYLIB_LIB):
	@echo "Building Raylib..."
	@mkdir -p $(RAYLIB_BUILD)
	cd $(RAYLIB_BUILD) && cmake .. -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DBUILD_SHARED_LIBS=OFF -DPLATFORM=Desktop
	cd $(RAYLIB_BUILD) && make

# clean
clean:
	@echo "Cleaning project..."
	rm -rf build
	@echo "Cleaning Raylib..."
	rm -rf $(RAYLIB_BUILD)

# compiler and flags
CC = clang
CXX = clang++
CXXFLAGS = -std=c++17 -O2

# paths
RAYLIB_PATH = lib/raylib-5.5
RAYLIB_BUILD = $(RAYLIB_PATH)/build
RAYLIB_LIB = $(RAYLIB_BUILD)/raylib/libraylib.a
RAYLIB_INCLUDE = -I$(RAYLIB_PATH)/src
TINYFD_PATH = lib/tinyfiledialogs
TINYFD_SRC = $(TINYFD_PATH)/tinyfiledialogs.c
TINYFD_INCLUDE = -I$(TINYFD_PATH)
TINYFD_BUILD = $(TINYFD_PATH)/build
TINYFD_OBJ = $(TINYFD_BUILD)/tinyfiledialogs.o

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
$(OUT): $(SRC) $(RAYLIB_LIB) $(TINYFD_OBJ)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $(OUT) $(SRC) $(RAYLIB_LIB) $(TINYFD_OBJ) $(RAYLIB_INCLUDE) $(TINYFD_INCLUDE) $(LDFLAGS)

# build Raylib
$(RAYLIB_LIB):
	@echo "Building Raylib..."
	@mkdir -p $(RAYLIB_BUILD)
	cd $(RAYLIB_BUILD) && cmake .. -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DBUILD_SHARED_LIBS=OFF -DPLATFORM=Desktop
	cd $(RAYLIB_BUILD) && make

# build tinyfiledialogs
$(TINYFD_OBJ): $(TINYFD_SRC)
	@echo "Building tinyfiledialogs..."
	@mkdir -p $(TINYFD_BUILD)
	$(CC) -c $(TINYFD_SRC) -o $(TINYFD_OBJ)

# clean
.PHONY: clean
clean:
	@echo "Cleaning project..."
	rm -rf build
	@echo "Cleaning Raylib..."
	rm -rf $(RAYLIB_BUILD)
	@echo "Cleaning tinyfiledialogs..."
	rm -rf $(TINYFD_BUILD)


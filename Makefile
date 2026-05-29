# compiler and flags
CC = clang
CFLAGS = -std=c11 -O2
CFLAGS += -Wall -Wextra -Wpedantic

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

SQLITE_PATH = lib/sqlite-amalgamation-3480000
SQLITE_SRC = $(SQLITE_PATH)/sqlite3.c
SQLITE_INCLUDE = -I$(SQLITE_PATH)
SQLITE_BUILD = $(SQLITE_PATH)/build
SQLITE_OBJ = $(SQLITE_BUILD)/sqlite3.o
SQLITE_LIB = $(SQLITE_BUILD)/libsqlite3.a


# project
SRC = src/main.c book_src/book_manager.c db_src/db.c
DB_INCLUDE = -Idb_src/

OUT = build/guandao

#temp
SRC_TEST = db_src/db.c
TEST_SRC = tests/db_test.c
TEST_OUT = build/db_test
TEST_INCLUDE = -Idb_src/ $(SQLITE_INCLUDE)
TEST_RES = tests/res

#book
# BOOK_SRC = book_src/
BOOK_INCLUDE = -Ibook_src/


UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S), Darwin)  # macOS
    LDFLAGS = -lm -ldl -lpthread -framework OpenGL -framework Cocoa -framework IOKit -framework CoreFoundation
else ifeq ($(UNAME_S), Linux)  # Linux
    LDFLAGS = -lm -ldl -lpthread -lGL
else
    $(error Unsupported platform: $(UNAME_S))
endif


FONT_DIR = assets/fonts
FONT = $(FONT_DIR)/GoNotoKurrent-Regular.ttf
FONT_URL = https://github.com/satbyy/go-noto-universal/releases/latest/download/GoNotoKurrent-Regular.ttf

all: $(RAYLIB_LIB) $(TINYFD_OBJ) $(SQLITE_LIB) $(FONT) $(OUT)

$(FONT):
	@mkdir -p $(FONT_DIR)
	@echo "Downloading GoNotoKurrent-Regular.ttf..."
	curl -L -o $(FONT) $(FONT_URL)

.PHONY: fonts
fonts: $(FONT)


# build guandao
$(OUT): $(SRC) $(RAYLIB_LIB) $(TINYFD_OBJ) $(SQLITE_LIB)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $(OUT) $(SRC) $(RAYLIB_LIB) $(TINYFD_OBJ) $(SQLITE_LIB) $(RAYLIB_INCLUDE) $(TINYFD_INCLUDE) $(SQLITE_INCLUDE) $(LDFLAGS) $(BOOK_INCLUDE) $(DB_INCLUDE)

# build db_test
$(TEST_OUT): $(TEST_SRC) $(SRC_TEST) $(SQLITE_LIB)
	@mkdir -p build
	@mkdir -p tests/res
	$(CC) $(CFLAGS) -o $(TEST_OUT) $(TEST_SRC) $(SRC_TEST) $(TEST_INCLUDE) $(SQLITE_LIB) $(LDFLAGS)


# clang db_src/tests/manga_db_test.c db_src/manga_d
# b.c lib/sqlite-amalgamation-3480000/build/libsqlite3.a -Idb_src/ -Ilib/sqlite-
# amalgamation-3480000/  

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

# build SQLite
$(SQLITE_LIB): $(SQLITE_SRC)
	@echo "Building SQLite..."
	@mkdir -p $(SQLITE_BUILD)
	$(CC) -c $(SQLITE_SRC) -o $(SQLITE_OBJ)
	ar rcs $(SQLITE_LIB) $(SQLITE_OBJ)



.PHONY: full 
full: $(RAYLIB_LIB) $(TINYFD_OBJ) $(SQLITE_LIB) $(OUT) $(TEST_OUT)
	$(MAKE) test



# clean
.PHONY: clean
clean:
	@echo "Cleaning project..."
	rm -rf build
	@echo "Cleaning Raylib..."
	rm -rf $(RAYLIB_BUILD)
	@echo "Cleaning tinyfiledialogs..."
	rm -rf $(TINYFD_BUILD)
	@echo "Cleaning SQLite..."
	rm -rf $(SQLITE_BUILD)
	@echo "Cleaning tests..."
	rm -rf $(TEST_RES)

# test
.PHONY: test
test: $(TEST_OUT)
	@echo "Running db_test..."
	./$(TEST_OUT)

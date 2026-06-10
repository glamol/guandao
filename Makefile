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

MINIZ_PATH = lib/miniz
MINIZ_SRC = $(MINIZ_PATH)/miniz.c
MINIZ_INCLUDE = -I$(MINIZ_PATH)
MINIZ_BUILD = $(MINIZ_PATH)/build
MINIZ_OBJ = $(MINIZ_BUILD)/miniz.o


# project
SRC = src/main.c book_src/book_manager.c db_src/db.c book_src/cbz.c book_src/cbt.c book_src/archive_util.c
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

all: $(RAYLIB_LIB) $(TINYFD_OBJ) $(SQLITE_LIB) $(MINIZ_OBJ) $(FONT) $(OUT)

$(FONT):
	@mkdir -p $(FONT_DIR)
	@echo "Downloading GoNotoKurrent-Regular.ttf..."
	curl -L -o $(FONT) $(FONT_URL)

.PHONY: fonts
fonts: $(FONT)


# build guandao
$(OUT): $(SRC) $(RAYLIB_LIB) $(TINYFD_OBJ) $(SQLITE_LIB) $(MINIZ_OBJ)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $(OUT) $(SRC) $(RAYLIB_LIB) $(TINYFD_OBJ) $(SQLITE_LIB) $(MINIZ_OBJ) $(RAYLIB_INCLUDE) $(TINYFD_INCLUDE) $(SQLITE_INCLUDE) $(MINIZ_INCLUDE) $(LDFLAGS) $(BOOK_INCLUDE) $(DB_INCLUDE)

# build db_test
$(TEST_OUT): $(TEST_SRC) $(SRC_TEST) $(SQLITE_LIB)
	@mkdir -p build
	@mkdir -p tests/res
	$(CC) $(CFLAGS) -o $(TEST_OUT) $(TEST_SRC) $(SRC_TEST) $(TEST_INCLUDE) $(SQLITE_LIB) $(LDFLAGS)

# build cbz_test
CBZ_TEST_OUT = build/cbz_test
$(CBZ_TEST_OUT): tests/cbz_test.c book_src/cbz.c book_src/archive_util.c $(MINIZ_OBJ)
	@mkdir -p build tests/res
	$(CC) $(CFLAGS) -o $(CBZ_TEST_OUT) tests/cbz_test.c book_src/cbz.c book_src/archive_util.c $(MINIZ_OBJ) $(BOOK_INCLUDE) $(MINIZ_INCLUDE) $(LDFLAGS)

# build cbt_test
CBT_TEST_OUT = build/cbt_test
$(CBT_TEST_OUT): tests/cbt_test.c book_src/cbt.c book_src/archive_util.c
	@mkdir -p build tests/res
	$(CC) $(CFLAGS) -o $(CBT_TEST_OUT) tests/cbt_test.c book_src/cbt.c book_src/archive_util.c $(BOOK_INCLUDE) $(LDFLAGS)

# build book_test
BOOK_TEST_OUT = build/book_test
$(BOOK_TEST_OUT): tests/book_test.c book_src/book_manager.c
	@mkdir -p build tests/res
	$(CC) $(CFLAGS) -o $(BOOK_TEST_OUT) tests/book_test.c book_src/book_manager.c $(BOOK_INCLUDE) $(LDFLAGS)

# build archive_util_test
AU_TEST_OUT = build/archive_util_test
$(AU_TEST_OUT): tests/archive_util_test.c book_src/archive_util.c
	@mkdir -p build tests/res
	$(CC) $(CFLAGS) -o $(AU_TEST_OUT) tests/archive_util_test.c book_src/archive_util.c $(BOOK_INCLUDE) $(LDFLAGS)


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

# build miniz
$(MINIZ_OBJ): $(MINIZ_SRC)
	@echo "Building miniz..."
	@mkdir -p $(MINIZ_BUILD)
	$(CC) -c $(MINIZ_SRC) -o $(MINIZ_OBJ)



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
	@echo "Cleaning miniz..."
	rm -rf $(MINIZ_BUILD)
	@echo "Cleaning tests..."
	rm -rf $(TEST_RES)

# test
.PHONY: test
test: $(TEST_OUT) $(CBZ_TEST_OUT) $(CBT_TEST_OUT) $(BOOK_TEST_OUT) $(AU_TEST_OUT)
	@echo "Running db_test..."
	./$(TEST_OUT)
	@echo "Running cbz_test..."
	./$(CBZ_TEST_OUT)
	@echo "Running cbt_test..."
	./$(CBT_TEST_OUT)
	@echo "Running book_test..."
	./$(BOOK_TEST_OUT)
	@echo "Running archive_util_test..."
	./$(AU_TEST_OUT)

# sample PD comics (clach04/sample_reading_media, MIT, gitignored)
SAMPLES_DIR = build/samples
SAMPLE_BUNDLE_URL = https://github.com/clach04/sample_reading_media/releases/download/v0.2/sample_reading_media.zip
.PHONY: samples
samples: $(SAMPLES_DIR)/sample.cbz $(SAMPLES_DIR)/sample.cbt
$(SAMPLES_DIR)/sample.cbz:
	@mkdir -p $(SAMPLES_DIR)
	@echo "Downloading sample bundle..."
	curl -L -o $(SAMPLES_DIR)/bundle.zip "$(SAMPLE_BUNDLE_URL)"
	cd $(SAMPLES_DIR) && unzip -o bundle.zip "*.cbz"
	mv $(SAMPLES_DIR)/bobby_make_believe_sample.cbz $@
	rm -f $(SAMPLES_DIR)/bundle.zip

# repackage cbz pages as cbt (tar) for CBT testing
$(SAMPLES_DIR)/sample.cbt: $(SAMPLES_DIR)/sample.cbz
	@echo "Building sample.cbt from sample.cbz..."
	rm -rf $(SAMPLES_DIR)/_unpack
	mkdir -p $(SAMPLES_DIR)/_unpack
	cd $(SAMPLES_DIR)/_unpack && unzip -o ../sample.cbz
	cd $(SAMPLES_DIR)/_unpack && tar -cf ../sample.cbt *
	rm -rf $(SAMPLES_DIR)/_unpack

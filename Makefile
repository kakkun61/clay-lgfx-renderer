# Build entry point. Run `nix develop` first: it provides the toolchain and
# sets CLAY_DIR and LOVYANGFX_DIR to the sources pinned in flake.lock.
#
#   make test          build and run the unit tests on the host
#   make sim           build the SDL simulator of the demo
#   make run-sim       build and start the simulator
#   make esp32-setup   install the ESP32 Arduino core (once, needs network)
#   make all           build everything: tests, simulator, doc and every sketch for every board
#   make esp32         build a sketch for a board (FQBN=..., SKETCH=example/minimal)
#   make esp32-all     build every sketch for every supported board
#   make upload        build and flash the demo (PORT=/dev/ttyUSB0)
#   make monitor       open the serial monitor
#   make doc           generate the API documentation with Doxygen

CLAY_DIR ?=
LOVYANGFX_DIR ?=

BUILD_DIR := build
CC ?= cc
CXX ?= c++
PKG_CONFIG ?= pkg-config

WARNINGS := -Wall -Wextra
CFLAGS ?= -O2
CXXFLAGS ?= -O2
# Third party headers are system includes so that their warnings stay quiet.
CPPFLAGS += -Isrc -isystem $(CLAY_DIR)

LIB_SOURCES := $(wildcard src/clay_lgfx/*.cpp)

.PHONY: all test sim run-sim esp32-setup esp32-core-version esp32 esp32-all upload monitor doc clean check-dep

# Builds everything: unit tests, the simulator, the documentation and the
# sketches for every supported board. The ESP32 part needs the core installed
# by `make esp32-setup`.
all: test sim doc esp32-all

# ---------------------------------------------------------------------------
# SDL simulator. LovyanGFX is compiled from source, following the file list
# of its own PC examples.

SIM_DIR := $(BUILD_DIR)/sim
SIM_BIN := $(SIM_DIR)/clay-lgfx-demo
DEMO_DIR := example/demo

LGFX_SRC := $(LOVYANGFX_DIR)/src
LGFX_SOURCES := $(wildcard \
	$(LGFX_SRC)/lgfx/Fonts/efont/*.c \
	$(LGFX_SRC)/lgfx/Fonts/IPA/*.c \
	$(LGFX_SRC)/lgfx/Fonts/lvgl/*.c \
	$(LGFX_SRC)/lgfx/utility/*.c \
	$(LGFX_SRC)/lgfx/v1/*.cpp \
	$(LGFX_SRC)/lgfx/v1/panel/*.cpp \
	$(LGFX_SRC)/lgfx/v1/touch/*.cpp \
	$(LGFX_SRC)/lgfx/v1/lv_font/*.c)

SIM_CPPFLAGS := $(CPPFLAGS) -isystem $(LGFX_SRC) -I$(DEMO_DIR) -D LGFX_SDL -D CLAY_LGFX_SIMULATOR \
	$(shell $(PKG_CONFIG) --cflags sdl2 2>/dev/null)
SIM_LIBS := $(shell $(PKG_CONFIG) --libs sdl2 2>/dev/null) -lpthread -lm

# Objects are named after their source path, so files from the LovyanGFX tree
# and from this repository can never collide.
sim_object = $(SIM_DIR)/$(patsubst $(LOVYANGFX_DIR)/%,lovyangfx/%,$(basename $(1))).o
SIM_SOURCES := $(LIB_SOURCES) $(wildcard $(DEMO_DIR)/*.cpp $(DEMO_DIR)/*.c)
SIM_OBJECTS := $(foreach s,$(SIM_SOURCES) $(LGFX_SOURCES),$(call sim_object,$(s)))

# Unit tests. Text is measured and drawn by LovyanGFX's fonts, so the tests
# link LovyanGFX the way the simulator does and share its objects. They draw
# on off-screen sprites only and never open a window.
TEST_BIN := $(BUILD_DIR)/test/run_tests
TEST_OBJECTS := $(foreach s,test/test_main.cpp $(LIB_SOURCES) $(LGFX_SOURCES),$(call sim_object,$(s)))

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_OBJECTS)
	@mkdir -p $(@D)
	$(CXX) $(LDFLAGS) -o $@ $^ $(SIM_LIBS)

sim: $(SIM_BIN)

run-sim: $(SIM_BIN)
	./$(SIM_BIN)

$(SIM_BIN): $(SIM_OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(SIM_LIBS)

# LovyanGFX is third party code; its warnings are not ours to fix.
$(SIM_DIR)/lovyangfx/%.o: $(LOVYANGFX_DIR)/%.cpp | check-dep
	@mkdir -p $(@D)
	$(CXX) -std=c++17 $(CXXFLAGS) $(SIM_CPPFLAGS) -w -MMD -MP -c -o $@ $<

$(SIM_DIR)/lovyangfx/%.o: $(LOVYANGFX_DIR)/%.c | check-dep
	@mkdir -p $(@D)
	$(CC) -std=c11 $(CFLAGS) $(SIM_CPPFLAGS) -w -MMD -MP -c -o $@ $<

$(SIM_DIR)/%.o: %.cpp | check-dep
	@mkdir -p $(@D)
	$(CXX) -std=c++20 $(WARNINGS) $(CXXFLAGS) $(SIM_CPPFLAGS) -MMD -MP -c -o $@ $<

$(SIM_DIR)/%.o: %.c | check-dep
	@mkdir -p $(@D)
	$(CC) -std=c11 $(WARNINGS) $(CFLAGS) $(SIM_CPPFLAGS) -MMD -MP -c -o $@ $<

# ---------------------------------------------------------------------------
# Real hardware, through arduino-cli. The repository root is itself an Arduino
# library (see library.properties), so it is passed with --library together
# with Clay and LovyanGFX.

# The ESP32 Arduino core this repository is tested with. arduino-cli installs
# cores outside Nix, so the version is pinned here instead of in flake.lock.
ESP32_CORE_VERSION := 3.3.12
FQBN ?= esp32:esp32:m5stack_core
PORT ?= /dev/ttyUSB0
ARDUINO_CLI ?= arduino-cli
# The sketch to build: example/demo by default, or example/minimal.
SKETCH ?= $(DEMO_DIR)
ESP32_DIR := $(BUILD_DIR)/esp32/$(notdir $(SKETCH))/$(subst :,_,$(FQBN))
ARDUINO_LIBRARIES := --library $(CURDIR) --library $(LOVYANGFX_DIR) --library $(CLAY_DIR)

esp32-setup:
	$(ARDUINO_CLI) core update-index
	$(ARDUINO_CLI) core install esp32:esp32@$(ESP32_CORE_VERSION)

# Prints the pinned core version, for scripts such as the CI cache key.
esp32-core-version:
	@echo $(ESP32_CORE_VERSION)

esp32: | check-dep
	$(ARDUINO_CLI) compile --fqbn $(FQBN) $(ARDUINO_LIBRARIES) \
		--build-path $(CURDIR)/$(ESP32_DIR) $(SKETCH)

# Every sketch for every board the repository is tested on.
ESP32_SKETCHES := example/demo example/minimal
ESP32_BOARDS := m5stack_core m5stack_core2 m5stack_cores3

esp32-all: | check-dep
	$(foreach s,$(ESP32_SKETCHES),$(foreach b,$(ESP32_BOARDS),$(MAKE) esp32 SKETCH=$(s) FQBN=esp32:esp32:$(b) &&)) true

upload: esp32
	$(ARDUINO_CLI) upload --fqbn $(FQBN) --port $(PORT) --input-dir $(CURDIR)/$(ESP32_DIR) $(SKETCH)

monitor:
	$(ARDUINO_CLI) monitor --port $(PORT) --config baudrate=115200

# ---------------------------------------------------------------------------
# Maintenance.

doc:
	@mkdir -p $(BUILD_DIR)/doc
	doxygen Doxyfile

clean:
	rm -rf $(BUILD_DIR)

check-dep:
	@test -n "$(CLAY_DIR)" -a -f "$(CLAY_DIR)/clay.h" || { echo "CLAY_DIR does not point to Clay; run inside 'nix develop'"; exit 1; }
	@test -n "$(LOVYANGFX_DIR)" -a -d "$(LOVYANGFX_DIR)/src/lgfx" || { echo "LOVYANGFX_DIR does not point to LovyanGFX; run inside 'nix develop'"; exit 1; }

-include $(shell find $(BUILD_DIR) -name '*.d' 2>/dev/null)

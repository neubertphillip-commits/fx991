# PC-Simulator der Casio-Deck-Firmware (Linux, macOS, WSL).
#   make && ./casio-sim --host <handy-ip>
FW = ../casio_deck
FW_SRC = $(FW)/app.cpp $(FW)/calc.cpp $(FW)/screen.cpp $(FW)/keymap.cpp \
         $(FW)/multitap.cpp $(FW)/net.cpp $(FW)/wav.cpp
SIM_SRC = sim_main.cpp sim_hw.cpp sim_websocket.cpp display_ansi.cpp

JSON_VER = v7.2.0
JSON_DIR = .deps/ArduinoJson

CXXFLAGS ?= -O1 -g -Wall -Wextra
override CXXFLAGS += -std=gnu++17 -Ishim -isystem $(JSON_DIR)/src

casio-sim: $(FW_SRC) $(SIM_SRC) $(wildcard $(FW)/*.h shim/*.h *.h) | $(JSON_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(FW_SRC) $(SIM_SRC)

# ArduinoJson ist header-only und laeuft auch auf dem PC
$(JSON_DIR):
	git clone -q --depth 1 --branch $(JSON_VER) https://github.com/bblanchon/ArduinoJson.git $@

clean:
	rm -f casio-sim

.PHONY: clean

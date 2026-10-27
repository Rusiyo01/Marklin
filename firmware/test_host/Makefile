# Tests de la lógica portable (protocolo CAN, motor de scripts y red CS2) en el PC.
#   make -C firmware/test_host        compila y ejecuta
#   make -C firmware/test_host clean

CXX ?= g++
CXXFLAGS ?= -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all
INCLUDES = -I../lib/marklin -I../lib/automation -I../lib/cs2 -I.

SRCS = ../lib/marklin/marklin_can.cpp \
       ../lib/marklin/marklin_controller.cpp \
       ../lib/automation/automation.cpp \
       ../lib/cs2/cs2_lan.cpp \
       test_marklin.cpp test_automation.cpp test_cs2.cpp main.cpp

BUILD = build
BIN = $(BUILD)/tests

.PHONY: test clean
test: $(BIN)
	./$(BIN)

$(BIN): $(SRCS) $(wildcard ../lib/*/*.h) ../src/default_scripts.h minitest.h
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SRCS) -o $@

clean:
	rm -rf $(BUILD)

# DA HW1 / Project-1 (BPV)
#
# Local build (macOS, Boost headers unpacked under third_party/):
#     make
#     make run
#
# On the CentOS7 machine, where Boost sits on the system include path:
#     make BOOST_INC=/usr/include

BOOST_INC ?= third_party

CXX      ?= c++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -I$(BOOST_INC) -Isrc
# Boost.Geometry leans on plenty of deprecated Boost.MPL internals; silence the noise
CXXFLAGS += -Wno-deprecated-declarations -Wno-unused-local-typedefs

SRC := src/main.cpp src/parser.cpp src/blockage.cpp src/draw.cpp
OBJ := $(SRC:.cpp=.o)
BIN := bpv

.PHONY: all run parse-check clean

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp src/bpv.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

run: $(BIN)
	@mkdir -p out
	./$(BIN) cases/case1.txt out/case1

# case0 is the example from the handout's language definition (3 layers, diagonal
# edges, negative coordinates). It only exercises the parser and is not part of the
# deliverable figures. Run it with: make parse-check
parse-check: $(BIN)
	@mkdir -p out
	./$(BIN) cases/case0.txt out/case0

clean:
	rm -f $(OBJ) $(BIN)
	rm -rf out

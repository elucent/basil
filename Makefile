.PHONY: debug release

SRC := ./src
INCLUDE := ./include

SRCFILES := $(wildcard $(SRC)/*.cpp)
OBJFILES := $(patsubst $(SRC)/%.cpp,$(SRC)/%.o,$(SRCFILES))

CXX := clang++
CXXDEBUG := -std=c++14 -g3 -Wall -Wno-strict-aliasing -pedantic -I$(INCLUDE) 
CXXRELEASE := -std=c++14 -Wall -Wno-strict-aliasing -pedantic -s \
	-Os -fno-ident -fno-rtti -fno-exceptions -fmerge-all-constants -I$(INCLUDE)

CC := clang
CFLAGS := -std=gnu99 -Os -fno-stack-protector

debug: CXXFLAGS := $(CXXDEBUG)
debug: basil
debug: lib/core.o

release: CXXFLAGS := $(CXXRELEASE)
release: basil
release: lib/core.o

clean:
	rm $(wildcard $(SRC)/*.o) basil lib/core.o

basil: $(OBJFILES)
	$(CXX) $(CXXFLAGS) -o basil $^

$(SRC)/%.o: $(SRC)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

lib/core.o: lib/core.c
	$(CC) $(CFLAGS) -c $< -o $@
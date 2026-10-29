# tests/Makefile: runs the one-translation-unit guard FIRST, then builds and runs every plugin's core checks (tests/<plugin>/Makefile).
# `make test` at the repo root runs this. A failing program fails the run.
CXX ?= g++
DIRS := $(patsubst %/Makefile,%,$(wildcard */Makefile))

all: odr run

run: odr
	@for d in $(DIRS); do echo "== $$d"; $(MAKE) --no-print-directory -C $$d || exit 1; done
	@echo "all $(words $(DIRS)) test programs passed: $(DIRS)"

# Every core header in ONE translation unit: a type or function defined under the same name in two plugins fails here,
# not as a silent ODR bug at link time. Covers core/ headers only (not src/); a header that is not self-contained is
# not caught here, each test program compiles its own includes.
odr:
	@mkdir -p ../build
	@for h in $(notdir $(wildcard ../core/*.h ../core/*.hpp)); do echo "#include \"$$h\""; done > ../build/odr.cpp
	$(CXX) -std=c++11 -fsyntax-only -Wall -Wextra -I../core ../build/odr.cpp

.PHONY: all run odr

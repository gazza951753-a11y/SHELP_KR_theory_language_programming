# =============================================================================
# Makefile — LL(1) struct syntax analyzer
#
# Targets:
#   make          — build the analyzer executable
#   make run      — build and run (reads grammar.txt + input.txt → output.txt)
#   make test     — run three built-in test scenarios
#   make clean    — remove compiled files and the executable
# =============================================================================

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2

TARGET := analyzer
SRCS   := main.cpp lexer.cpp grammar.cpp parser.cpp semantic.cpp
OBJS   := $(SRCS:.cpp=.o)

# Default target
.PHONY: all
all: $(TARGET)

# Link
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^
	@echo "Build OK: ./$(TARGET)"

# Compile each .cpp
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Explicit header dependencies
main.o:     main.cpp     grammar.h lexer.h parser.h semantic.h
lexer.o:    lexer.cpp    lexer.h
grammar.o:  grammar.cpp  grammar.h
parser.o:   parser.cpp   parser.h grammar.h lexer.h
semantic.o: semantic.cpp semantic.h lexer.h

# Run
.PHONY: run
run: $(TARGET)
	./$(TARGET)
	@echo ""
	@echo "=== output.txt ==="
	@cat output.txt

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
.PHONY: test
test: $(TARGET)
	@echo ""
	@echo "======================================================================"
	@echo "  TEST 1: Correct struct (no errors expected → OK)"
	@echo "======================================================================"
	@printf 'struct Point {\n    int x;\n    double y;\n};\n' > input.txt
	@./$(TARGET)
	@cat output.txt

	@echo ""
	@echo "======================================================================"
	@echo "  TEST 2: Duplicate field name (→ Name conflict)"
	@echo "======================================================================"
	@printf 'struct Bad {\n    int x;\n    float x;\n};\n' > input.txt
	@./$(TARGET)
	@cat output.txt

	@echo ""
	@echo "======================================================================"
	@echo "  TEST 3: Syntax error – missing semicolon"
	@echo "======================================================================"
	@printf 'struct Broken {\n    int x\n};\n' > input.txt
	@./$(TARGET)
	@cat output.txt

	@echo ""
	@echo "======================================================================"
	@echo "  TEST 4: Array field + user-defined type (→ OK)"
	@echo "======================================================================"
	@printf 'struct Grid {\n    int data[10];\n    Point origin;\n};\n' > input.txt
	@./$(TARGET)
	@cat output.txt

	@echo ""
	@echo "======================================================================"
	@echo "  TEST 5: Multiple structs, second one has conflict"
	@echo "======================================================================"
	@printf 'struct A {\n    int x;\n};\nstruct B {\n    bool flag;\n    bool flag;\n};\n' > input.txt
	@./$(TARGET)
	@cat output.txt

	@echo ""
	@echo "Restoring original input.txt..."
	@printf 'struct Point {\n    int x;\n    double y;\n    float z;\n};\n\nstruct Rectangle {\n    Point topLeft;\n    Point bottomRight;\n    int width;\n    int width;\n};\n\nstruct Empty {};\n' > input.txt
	@echo "Done."

# Clean
.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)
	@echo "Clean done"

# =============================================================================
# Makefile — сборка парсера арифметических выражений (ДМПА)
#
# Цели:
#   make          — собрать исполняемый файл parser
#   make run      — собрать и запустить (читает input.txt, пишет output.txt)
#   make clean    — удалить объектные файлы и бинарник
#   make check    — запустить на трёх тестовых примерах
# =============================================================================

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2

TARGET := parser
SRCS   := main.cpp lexer.cpp parser.cpp codegen.cpp
OBJS   := $(SRCS:.cpp=.o)

# Правило по умолчанию
.PHONY: all
all: $(TARGET)

# Компоновка
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^
	@echo "Build OK: ./$(TARGET)"

# Компиляция каждого .cpp
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Зависимости заголовков
main.o:    main.cpp    ast.h lexer.h parser.h codegen.h
lexer.o:   lexer.cpp   lexer.h
parser.o:  parser.cpp  parser.h ast.h lexer.h
codegen.o: codegen.cpp codegen.h ast.h

# Запуск
.PHONY: run
run: $(TARGET)
	./$(TARGET)
	@echo ""
	@echo "=== output.txt ==="
	@cat output.txt

# Тесты
.PHONY: check
check: $(TARGET)
	@echo "--- Тест 1: базовый (переменные и числа) ---"
	@echo "result = (a + 3) * (b + 2 + 5)" > input.txt
	@./$(TARGET)
	@cat output.txt
	@echo ""
	@echo "--- Тест 2: свёртка констант ---"
	@echo "x = (2 + 3) * (4 + 5)" > input.txt
	@./$(TARGET)
	@cat output.txt
	@echo ""
	@echo "--- Тест 3: числа в научной нотации ---"
	@echo "val = mass * 1e+18 + offset" > input.txt
	@./$(TARGET)
	@cat output.txt
	@echo ""
	# Восстановить исходный input.txt
	@echo "result = (a + 3) * (b + 2 + 5)" > input.txt

# Очистка
.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)
	@echo "Clean done"

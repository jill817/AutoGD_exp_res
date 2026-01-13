CC := g++ -std=c++17

TARGET := solver

SRC_DIR := src
GEN_DIR := $(SRC_DIR)/generatelp

OBJS := \
	$(SRC_DIR)/assignment.o \
	$(SRC_DIR)/formula.o \
	$(SRC_DIR)/instance.o \
	$(SRC_DIR)/lsearch.o \
	$(SRC_DIR)/main.o \
	$(SRC_DIR)/utils.o \
	$(GEN_DIR)/generatelp.o \
	$(GEN_DIR)/sls_solver/model.o \
	$(GEN_DIR)/sls_solver/solver.o

$(TARGET): $(OBJS)
	$(CC) -O3 -o $@ $^

%.o: %.cpp
	$(CC) -c -O3 -o $@ $<

clean:
	find $(SRC_DIR) -name "*.o" | xargs rm -f
	rm -f $(TARGET)

.PHONY: clean

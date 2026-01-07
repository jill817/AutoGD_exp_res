# 简单 Makefile，仅编译 RAP.cpp
TARGET = RAP

CXX = g++
CXXFLAGS = -std=c++17 -O2 
INCLUDES = -I./sls_solver
SRCDIR = .
SOLVERDIR = ./sls_solver

# 自动查找所有cpp文件
SRCS = $(SRCDIR)/model_and_RAP.cpp \
	$(SOLVERDIR)/model.cpp \
	$(SOLVERDIR)/solver.cpp

OBJS = $(SRCS:.cpp=.o)

TARGET = model_and_RAP

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(OBJS)

$(SRCDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(SOLVERDIR)/%.o: $(SOLVERDIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(SOLVERDIR)/*.o $(SRCDIR)/*.o $(TARGET)

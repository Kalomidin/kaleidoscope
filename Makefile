# Compiler and flags
CXX = clang++
CXXFLAGS = -Wall -g

# Find all .cpp files and corresponding .o files
SRCS = $(wildcard *.cpp)
OBJS = $(SRCS:.cpp=.o)

# Executable name
TARGET = main.exe

# Rule to build the executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

# Rule to compile .cpp files into .o files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $<

# Clean rule to remove compiled files
clean:
	rm -f $(OBJS) $(TARGET)

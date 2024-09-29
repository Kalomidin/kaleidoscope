# Compiler and flags
CXX = clang++
CXXFLAGS = -Wall -g

# Find all .cpp files and corresponding .o files
SRCS = $(wildcard *.cpp)
OBJS = $(SRCS:.cpp=.o)

# Executable name
TARGET = main.exe

# Rule to build the executable if the .o files are up to date
# Then run the executable even if there is no change in the .o files
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)
	@echo "Running the executable..."
	./$(TARGET)


# Rule to compile .cpp files into .o files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $<

# Clean rule to remove compiled files
clean:
	rm -f $(OBJS) $(TARGET)

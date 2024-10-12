# Compiler and flags
CXX = clang++
LLVM_DEPS = `llvm-config --cxxflags --ldflags --system-libs --libs all`
CXXFLAGS = -Wall -g $(LLVM_DEPS)
# Find all .cpp files and corresponding .o files
SRCS = $(wildcard *.cpp)
OBJS = $(SRCS:.cpp=.o)
MACOS_VERSION = -lSystem -mmacosx-version-min=13.6

# Executable name
TARGET = kaleidoscope

# Rule to build the executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

run:
	@echo "Running the executable..."
	./$(TARGET)

setup:
	@echo "Installing LLVM..."
	@if command -v llvm-config &> /dev/null; then \
		echo "LLVM is already installed."; \
	else \
		echo "Installing LLVM..."; \
		mkdir -p ~/Developer/llvm-project; \
		cd ~/Developer/llvm-project; \
		git clone https://github.com/llvm/llvm-project.git .; \
		mkdir build && cd build; \
		cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=~/Developer/llvm-install ../llvm; \
		make -j$(sysctl -n hw.logicalcpu); \
		make install; \
		echo 'export PATH="$PATH:~/Developer/llvm-install/bin"' >> ~/.zshrc; \
		source ~/.zshrc; \
	fi

install:
	@echo "Checking if the executable exists..."
	@if [ ! -f $(TARGET) ]; then \
		echo "Executable does not exist. Building..."; \
		make; \
	fi
	@echo "Installing the executable..."
	cp $(TARGET) /usr/local/bin/$(TARGET)

fmt:
	@echo "Formatting code..."
	clang-format -i $(SRCS)

tidy:
	@echo "Tidying code..."
	clang-tidy $(SRCS) -- $(CXXFLAGS)

# Rule to compile .cpp files into .o files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $<

# Clean rule to remove compiled files
clean:
	rm -f $(OBJS) $(TARGET)
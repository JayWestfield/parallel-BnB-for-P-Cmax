# Define compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -I/usr/include/tbb 
LDFLAGS = -ltbb

# Define the source and target files
SOURCES = src/main.cpp src/BnB/BnB.cpp src/BnB/STImpl.cpp
TARGET = dst/parallel_solver

# Define the rule to build the target executable
$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)

# Define a rule for running the program
run: $(TARGET)
	./$(TARGET)

# Define a clean rule to remove compiled files
clean:
	rm -f $(TARGET)

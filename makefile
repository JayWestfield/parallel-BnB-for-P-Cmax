# Define compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -I/usr/include/tbb -g -ggdb -O0
LDFLAGS = -ltbb

# Define the source and target files
SOURCES = src/main.cpp src/BnB/BnB.cpp src/BnB/STImpl.cpp src/BnB/BnB_base.cpp src/BnB/BnB_base.h 
TARGET = dst/parallel_solver
SOURCESEXP = src/BnB/STImpl.cpp src/BnB/BnB_base.cpp src/BnB/BnB_base.h src/experiments/lawrinenko_test.cpp
TARGETEXP = dst/experiment

# Define the rule to build the target executable
$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)

# Define a rule for running the program
run: $(TARGET)
	./$(TARGET)

$(TARGETEXP): $(SOURCESEXP)
	$(CXX) $(CXXFLAGS) -o $(TARGETEXP) $(SOURCESEXP) $(LDFLAGS)

exp: $(TARGETEXP)
	./$(TARGETEXP) $(filter-out $@,$(MAKECMDGOALS)) > results/exp12

debugger: 
	bash ./src/debugging/debugger.sh $(TARGET)
plot:  
	python3 src/plotting/plotter.py results/exp12
# Define a clean rule to remove compiled files
clean:
	rm -f $(TARGET)
	rm -f $(TARGETEXP)

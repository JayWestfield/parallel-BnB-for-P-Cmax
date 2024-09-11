# Define compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -I/usr/include/tbb -g -ggdb -DNDEBUG -O3
LDFLAGS = -ltbb 

# Define the source and target files
BASEFILES  = src/BnB/BnB_base.cpp src/experiments/readData/readData.cpp src/BnB/STImpl.cpp src/BnB/STImplSimpl.cpp src/BnB/threadLocal/threadLocal.cpp
SOURCES = src/main.cpp src/BnB/BnB.cpp src/BnB/STImpl.cpp src/BnB/BnB_base.cpp src/BnB/BnB_base.h 
TARGET = dst/parallel_solver
SOURCESEXP = src/BnB/STImpl.cpp src/BnB/BnB_base.cpp src/BnB/BnB_base.h src/experiments/lawrinenko_test.cpp
TARGETEXP = dst/experiment
FILTERALL = dst/runAllFast
SOURCESALL = src/BnB/STImpl.cpp src/BnB/BnB_base.cpp src/BnB/BnB_base.h src/experiments/test_to_filter_benchmark.cpp

PROFILE_SRC = src/BnB/STImpl.cpp src/BnB/BnB_base.cpp src/BnB/BnB_base.h src/experiments/Profiler.cpp 
PROFILE_DST = dst/profiler


# Define the rule to build the target executable
$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)

# Define a rule for running the program
run: $(TARGET)
	./$(TARGET)

$(TARGETEXP): $(SOURCESEXP)
	$(CXX) $(CXXFLAGS) $(BASEFILES) -o $(TARGETEXP) $(SOURCESEXP) $(LDFLAGS)

exp: $(TARGETEXP)


$(FILTERALL): $(SOURCESALL)
	$(CXX) $(CXXFLAGS) -o $(FILTERALL) $(SOURCESALL) $(LDFLAGS)

all: $(FILTERALL)

debugger: 
	bash ./src/debugging/debugger.sh $(TARGET)

plot:  
	python3 src/plotting/plotter.py results/experiment_lawrinenko_improved_3.txt plots/improved_3.png
	code plots/improved_3.png

plot2:
	python3 src/plotting/compare_executions.py results/experiment_lawrinenko_base_prevO3.txt  results/experiment_lawrinenko_improved_3.txt plots/compare2.png
	code plots/compare2.png

$(PROFILE_DST): $(PROFILE_SRC)
	$(CXX) $(CXXFLAGS) -o $(PROFILE_DST) $(PROFILE_SRC) $(BASEFILES) $(LDFLAGS)

profile: $(PROFILE_DST)

viewProfile: 
	google-pprof --pdf  ./dst/profiler profile.prof > outputq.pdf
	code outputq.pdf
# Define a clean rule to remove compiled files
clean:
	rm -f $(TARGET)
	rm -f $(TARGETEXP)
	rm -f $(FILTERALL)
	rm -f $(PROFILE_DST)

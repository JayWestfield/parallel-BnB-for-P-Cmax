# Define compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -I/usr/include/tbb   -g -ggdb  -DNDEBUG -funroll-loops -O3 # -Igrowt  #-I/usr/include/folly #-I/usr/include/junction -I -Ifolly/folly -Iturf -Ijunction
LDFLAGS = -ltbb #-lfolly

# Define the source and target files
BASEFILES  = src/BnB/BnB_base.cpp src/experiments/readData/readData.cpp src/BnB/STImpl.cpp  src/BnB/STImplSimplCustomLock.cpp  src/BnB/threadLocal/threadLocal.cpp src/BnB/hashmap/SingleThreadedHashMap/stdHashMap.cpp src/BnB/LowerBounds/lowerBounds_ret.cpp 
SOURCES = src/main.cpp src/BnB/BnB.cpp src/BnB/STImpl.cpp src/BnB/BnB_base.cpp src/BnB/BnB_base.h 
TARGET = dst/parallel_solver
SOURCESEXP = src/BnB/STImpl.cpp src/BnB/BnB_base.cpp src/BnB/BnB_base.h src/experiments/lawrinenko_test.cpp 
TARGETEXP = dst/experiment
FILTERALL = dst/runAllFast
SOURCESALL = src/BnB/STImpl.cpp src/BnB/BnB_base.cpp src/BnB/BnB_base.h src/experiments/test_to_filter_benchmark.cpp

PROFILE_SRC =  src/BnB/STImpl.cpp src/BnB/BnB_base.cpp src/BnB/BnB_base.h src/experiments/Profiler.cpp src/BnB/BnB_base_custom_work_stealing.cpp -ltcmalloc_and_profiler -lprofiler
PROFILE_DST = dst/profiler 

BASEFILES2 = src/customBnB/customBnB_base.hpp src/experiments/readData/readData.cpp src/customBnB/threadLocal/threadLocal.cpp 
TARGETEXP2 = dst/experiment2
SOURCESEXP2 = src/experiments/lawrinenko_test2.cpp 
CUSTOMFiles = src/customBnB/customBnB_base.hpp src/experiments/readData/readData.cpp src/customBnB/threadLocal/threadLocal.cpp src/experiments/Profiler.cpp -ltcmalloc_and_profiler -lprofiler
CUSTOMDst = dst/custom
$(CUSTOMDst): $(CUSTOMFiles) 
	$(CXX) $(CXXFLAGS) -o $(CUSTOMDst)  $(CUSTOMFiles) $(LDFLAGS)

custom: $(CUSTOMDst)
	rm -f ./profiling_results/profile_*.prof
	@./$(CUSTOMDst) $(filter-out custom,$(MAKECMDGOALS))
	google-pprof --callgrind ./dst/custom ./profiling_results/profile_*.prof > ./profiling_results/transformed_gperf.out
	kcachegrind ./profiling_results/transformed_gperf.out

# Define the rule to build the target executable
$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)


$(TARGETEXP): $(SOURCESEXP) $(BASEFILES)
	$(CXX) $(CXXFLAGS) $(BASEFILES) -o $(TARGETEXP) $(SOURCESEXP) $(LDFLAGS)

exp: $(TARGETEXP)


$(TARGETEXP2): $(SOURCESEXP2) $(BASEFILES2)
	$(CXX) $(CXXFLAGS) $(BASEFILES2) -o $(TARGETEXP2) $(SOURCESEXP2) $(LDFLAGS)

exp2: $(TARGETEXP2)


$(FILTERALL): $(SOURCESALL)
	$(CXX) $(CXXFLAGS) -o $(FILTERALL) $(SOURCESALL) $(LDFLAGS)

all: $(FILTERALL)

debugger: 
	bash ./src/debugging/debugger.sh $(TARGET)

plot:  
	python3 src/plotting/plotter.py ./results/customLock_fixed.txt plots/test6.png
	code plots/test6.png
localplot:  
	python3 src/plotting/plotter.py o2.txt plots/local.png
	code plots/local.png

localplot2:  
	python3 src/plotting/compare_executions.py ./o.txt  ./o2.txt plots/localcompare2.png
	code plots/localcompare2.png
plot2:
	python3 src/plotting/compare_executions.py ./results/customLock_fixed.txt  ./results/improveLowerBounds.txt plots/easySuspend.png
	code plots/compare3.png

$(PROFILE_DST): $(PROFILE_SRC) $(BASEFILES)
	$(CXX) $(CXXFLAGS) -o $(PROFILE_DST) $(PROFILE_SRC) $(BASEFILES) $(LDFLAGS)

# arguments threads repeat STVersion (instance) (benchmark)
runProfile: $(PROFILE_DST)
	rm -f ./profiling_results/profile_*.prof
	@./$(PROFILE_DST) $(filter-out runProfile,$(MAKECMDGOALS))
	google-pprof --callgrind ./dst/profiler ./profiling_results/profile_*.prof > ./profiling_results/transformed_gperf.out
	kcachegrind ./profiling_results/transformed_gperf.out

viewProfile: 
	google-pprof --callgrind ./dst/profiler ./profiling_results/profile_*.prof > ./profiling_results/transformed_gperf.out
	kcachegrind ./profiling_results/transformed_gperf.out
# Define a clean rule to remove compiled files
clean:
	rm -f $(TARGET)
	rm -f $(TARGETEXP)
	rm -f $(FILTERALL)
	rm -f $(PROFILE_DST)
	rm -f $(CUSTOMDst)

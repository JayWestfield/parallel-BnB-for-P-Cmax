#include <iostream>
#include "BnB/BnB.h"

int main() {
    int numMachines = 4;
    std::vector<int> jobDurations = {2, 14, 4, 16, 6, 5, 3};

    BnBSolver solver;
    int result = solver.solve(numMachines, jobDurations);
    
    std::cout << "The optimal solution is: " << result << std::endl;
    std::cout << "With nodes visited: " << solver.visitedNodes << std::endl;
    
    std::vector<int> firstBerndt={149,168,147,115,127,193,157,121,130,119,183,123,160,136};
    result = solver.solve(numMachines, firstBerndt);

    std::cout << "The optimal solution is: " << result << std::endl;
    std::cout << "With nodes visited: " << solver.visitedNodes << std::endl;
    return 0;
}

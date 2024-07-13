#include <iostream>
#include "BnB/BnB.h"

int main() {
    int numMachines = 3;
    std::vector<int> jobDurations = {2, 14, 4, 16, 6, 5, 3, 2};

    BnBSolver solver;
    int result = solver.solve(numMachines, jobDurations);
    
    std::cout << "The optimal solution is: " << result << std::endl;
    std::cout << "With nodes visited: " << solver.visitedNodes << std::endl;
    
    std::vector<int> firstBerndt={24,35,33,23,50,46,21,33,29,35,28,47,48,33,45,0};
    result = solver.solve(3, firstBerndt);

    std::cout << "The optimal solution is: " << result << std::endl;
    std::cout << "With nodes visited: " << solver.visitedNodes << std::endl;
    return 0;
}

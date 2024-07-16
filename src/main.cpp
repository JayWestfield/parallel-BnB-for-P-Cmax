#include <iostream>
#include "BnB/BnB.h"

int main() {
    int numMachines = 3;
    std::vector<int> jobDurations = {2, 14, 4, 16, 6, 5, 3, 2,4};

    BnBSolver solver;
    int result = solver.solve(numMachines, jobDurations);
    
    std::cout << "The optimal solution is: " << result << std::endl;
    std::cout << "With nodes visited: " << solver.visitedNodes << std::endl;
    
    std::vector<int> plantedKnownOptimal={87,74,71,62,29,23,15,15,14,11,0};
    result = solver.solve(4, plantedKnownOptimal);

    std::cout << "The optimal solution is: " << 101 << std::endl;
    std::cout << "The best found solution is: " << result << std::endl;
    std::cout << "With nodes visited: " << solver.visitedNodes << std::endl;

    std::vector<int> plantedKnownOptimal2={117,117,115,114,101,95,95,84,78,76,75,75,71,67,66,60,54,53,52,52,51,50,48,46,43,43,42,41,41,40,39,38,38,37,35,34,33,29,28,28,27,26,26,24,22,22,22,18,17,17,16,15,15,15,15,15,14,14,13,13,12,12,12,11,9,9,9,9,9,9,8,8,8,8,7,7,7,7,6,6,6,6,5,5,5,5,4,4,4,4,4,4,3,2,2,2,2,1,1,1,0
};
    result = solver.solve(10, plantedKnownOptimal2);

    std::cout << "The optimal solution is: " << 301 << std::endl;
    std::cout << "The best found solution is: " << result << std::endl;
    std::cout << "With nodes visited: " << solver.visitedNodes << std::endl;
    return 0;
}

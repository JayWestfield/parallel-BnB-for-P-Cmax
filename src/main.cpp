#include <iostream>
#include "BnB/BnB_base.cpp"

#include <fstream>
#include <sstream>
#include <chrono>
#include <tbb/tbb.h>
#include <future>
#include "BnB/BnB.h"
void readInstance(const std::string &filename, int &numJobs, int &numMachines, std::vector<int> &jobDurations)
{
    std::ifstream infile(filename);
    if (!infile)
    {
        throw std::runtime_error("Unable to open file");
    }
    std::string line;

    // Read the first line
    if (std::getline(infile, line))
    {
        std::istringstream iss(line);
        std::string problemType;
        iss >> problemType >> problemType >> numJobs >> numMachines;
    }

    // Read the second line
    if (std::getline(infile, line))
    {
        std::istringstream iss(line);
        int duration;
        while (iss >> duration)
        {
            jobDurations.push_back(duration);
        }
    }
}
void readOptimalSolutions(const std::string &filename, std::unordered_map<std::string, int> &optimalSolutions)
{
    std::ifstream infile(filename);
    if (!infile)
    {
        throw std::runtime_error("Unable to open file");
    }

    std::string line;
    while (std::getline(infile, line))
    {
        std::istringstream iss(line);
        std::string name;
        int optimalMakespan;
        if (iss >> name >> optimalMakespan)
        {
            optimalSolutions[name] = optimalMakespan;
        }
    }
}

double geometricMean(const std::vector<double> &values)
{
    double product = 1.0;
    for (const auto &val : values)
    {
        product *= val;
    }
    return std::pow(product, 1.0 / values.size());
}

double median(std::vector<double> &values)
{
    size_t size = values.size();
    std::sort(values.begin(), values.end());
    if (size % 2 == 0)
    {
        return (values[size / 2 - 1] + values[size / 2]) / 2.0;
    }
    else
    {
        return values[size / 2];
    }
}


int main()
{
 
    try
    {
        std::vector<int> jobDurations;
        std::unordered_map<std::string, int> optimalSolutions;
        std::vector<std::string> failed;
        std::vector<std::string> succesfull;
        // BnBSolver solver(true, true, true);
        uint64_t totalNodesVisited = 0;
        tbb::global_control global_limit(tbb::global_control::max_allowed_parallelism, 12);

        int wrong = 0;
        std::vector<int> plantedKnownOptimal = {97, 93, 82, 50, 95, 96, 56, 93};

        BnB_base_Impl solver(true, true, true, true, false);
        for (int i = 0; i < 100; i++)
        {

            BnB_base_Impl solver(true, true, true, true, false);
            std::cout << solver.solve(3, plantedKnownOptimal) << std::endl; // really weird error if this is not 231 then there will be error instances so it has to be an missing initializer or sth like that i have no idea
        }
        for (int i = 0; i < 10; i++)
        {
            tbb::global_control global_limit(tbb::global_control::max_allowed_parallelism, 12);
            BnB_base_Impl solver(true, true, true, true, false);

            // BnBSolver solver(true, true, true);
            std::vector<int> plantedKnownOptimal = {97, 93, 82, 50, 95, 96, 56, 93, 91, 81, 64, 89, 63, 58, 96, 76, 80, 54, 56, 50, 100, 79, 86, 63, 76, 68, 60, 83, 91, 67, 89, 86, 84, 90, 61, 69};
            int result = solver.solve(3, plantedKnownOptimal);
            // std::cout << result << std::endl; // really weird error if this is not 231 then there will be error instances so it has to be an missing initializer or sth like that i have no idea
            if (result != 924)
            {
                std::cout << "Error This would be a false run " << i << "with " << result << std::endl;
                wrong++;
                return 0;
            }
        }
        // return 0;
        // std::cout << wrong << " equals " << (double) wrong * 100 / (double) 20 << "%" << std::endl;
        /*std::vector<int> plantedKnownOptimal2={188,202,162,137,116,214,189,112,192,126,135,98,111,174,130,174,200,162,131,132,154,161,122,144,124,134,180,101,211,59,117,136,114,107,77,112};
        int result = solver.solve(12,plantedKnownOptimal2);
        std::cout << "The optimal solution is: " << 429 << std::endl; // really weird error if this is
        std::cout << "The best found solution is: " << result << std::endl;
        std::cout << "nodes visited: " << solver.visitedNodes << std::endl;  */
        // Read the known optimal solutions
        std::string optimalSolutionsFile = "benchmarks/opt-known-instances-lawrinenko.txt";
        std::vector<std::string> interestingFiles;
        readOptimalSolutions(optimalSolutionsFile, optimalSolutions);
        int testInstances = optimalSolutions.size();
        const int excludeLast = 0;
        auto it = optimalSolutions.begin();
        for (int s = 0; s < excludeLast; s++)
        {
            it++;
        }
        std::vector<std::string> selectedInstances = {
            "p_cmax-class1-n120-m60-minsize1-maxsize100-seed9174.txt",
            "p_cmax-class1-n180-m72-minsize1-maxsize100-seed10580.txt",
            "p_cmax-class1-n90-m40-minsize1-maxsize100-seed13008.txt",
            "p_cmax-class1-n108-m48-minsize1-maxsize100-seed25594.txt",
            "p_cmax-class1-n180-m80-minsize1-maxsize100-seed8550.txt",
            "p_cmax-class1-n160-m80-minsize1-maxsize100-seed21321.txt",
            "p_cmax-class1-n44-m16-minsize1-maxsize100-seed32230.txt",
            "p_cmax-class1-n200-m100-minsize1-maxsize100-seed2228.txt",
            "p_cmax-class1-n100-m50-minsize1-maxsize100-seed17298.txt",
            "p_cmax-class1-n44-m16-minsize1-maxsize100-seed31821.txt",
            "p_cmax-class1-n60-m30-minsize1-maxsize100-seed10583.txt",
            "p_cmax-class1-n90-m40-minsize1-maxsize100-seed3390.txt",
            "p_cmax-class1-n60-m24-minsize1-maxsize100-seed28346.txt",
            "p_cmax-class1-n180-m90-minsize1-maxsize100-seed1244.txt",
            "p_cmax-class1-n100-m50-minsize1-maxsize100-seed23471.txt",
            "p_cmax-class1-n162-m72-minsize1-maxsize100-seed30111.txt",
            "p_cmax-class1-n180-m80-minsize1-maxsize100-seed24676.txt",
            "p_cmax-class1-n40-m20-minsize1-maxsize100-seed25125.txt",
            "p_cmax-class1-n100-m40-minsize1-maxsize100-seed27405.txt",
            "p_cmax-class1-n200-m100-minsize1-maxsize100-seed31417.txt",
            "p_cmax-class1-n198-m88-minsize1-maxsize100-seed4176.txt",
            "p_cmax-class1-n144-m64-minsize1-maxsize100-seed32473.txt",
            "p_cmax-class1-n160-m64-minsize1-maxsize100-seed25916.txt",
            "p_cmax-class1-n54-m24-minsize1-maxsize100-seed6231.txt",
            "p_cmax-class1-n126-m56-minsize1-maxsize100-seed4180.txt",
            "p_cmax-class1-n144-m64-minsize1-maxsize100-seed23990.txt",
            "p_cmax-class1-n140-m70-minsize1-maxsize100-seed24487.txt",
            "p_cmax-class1-n54-m24-minsize1-maxsize100-seed15497.txt",
            "p_cmax-class1-n140-m70-minsize1-maxsize100-seed10529.txt",
            "p_cmax-class1-n144-m64-minsize1-maxsize100-seed10084.txt",
            "p_cmax-class1-n60-m24-minsize1-maxsize100-seed18349.txt",
            "p_cmax-class1-n100-m50-minsize1-maxsize100-seed5050.txt",
            "p_cmax-class1-n40-m20-minsize1-maxsize100-seed29879.txt",
            "p_cmax-class1-n108-m48-minsize1-maxsize100-seed18680.txt",
            "p_cmax-class1-n140-m70-minsize1-maxsize100-seed11379.txt",
            "p_cmax-class1-n90-m40-minsize1-maxsize100-seed8295.txt",
            "p_cmax-class1-n200-m100-minsize1-maxsize100-seed9414.txt",
            "p_cmax-class1-n144-m64-minsize1-maxsize100-seed9204.txt",
            "p_cmax-class1-n200-m100-minsize1-maxsize100-seed6948.txt",
            "p_cmax-class1-n40-m20-minsize1-maxsize100-seed19609.txt",
            "p_cmax-class1-n40-m16-minsize1-maxsize100-seed20292.txt",
            "p_cmax-class1-n108-m48-minsize1-maxsize100-seed13845.txt",
            "p_cmax-class1-n90-m40-minsize1-maxsize100-seed11575.txt",
            "p_cmax-class1-n100-m50-minsize1-maxsize100-seed515.txt",
            "p_cmax-class1-n100-m40-minsize1-maxsize100-seed20109.txt",
            "p_cmax-class1-n120-m60-minsize1-maxsize100-seed17275.txt",
            "p_cmax-class1-n40-m16-minsize1-maxsize100-seed5275.txt",
            "p_cmax-class1-n126-m56-minsize1-maxsize100-seed13540.txt",
            "p_cmax-class1-n180-m80-minsize1-maxsize100-seed18108.txt",
            "p_cmax-class1-n126-m56-minsize1-maxsize100-seed2902.txt",
            "p_cmax-class1-n72-m32-minsize1-maxsize100-seed23983.txt",
            "p_cmax-class1-n54-m24-minsize1-maxsize100-seed1796.txt",
            "p_cmax-class1-n160-m80-minsize1-maxsize100-seed28446.txt",
            "p_cmax-class1-n126-m56-minsize1-maxsize100-seed9277.txt",
            "p_cmax-class1-n144-m64-minsize1-maxsize100-seed13809.txt",
            "p_cmax-class1-n120-m60-minsize1-maxsize100-seed6431.txt",
            "p_cmax-class1-n72-m32-minsize1-maxsize100-seed12671.txt",
            "p_cmax-class1-n180-m80-minsize1-maxsize100-seed31215.txt",
            "p_cmax-class1-n162-m72-minsize1-maxsize100-seed8621.txt",
            "p_cmax-class1-n100-m40-minsize1-maxsize100-seed18064.txt",
            "p_cmax-class1-n60-m30-minsize1-maxsize100-seed6523.txt",
            "p_cmax-class1-n90-m40-minsize1-maxsize100-seed22431.txt",
            "p_cmax-class1-n72-m32-minsize1-maxsize100-seed17048.txt",
            "p_cmax-class1-n72-m32-minsize1-maxsize100-seed27485.txt",
            "p_cmax-class1-n40-m16-minsize1-maxsize100-seed14476.txt",
            "p_cmax-class1-n90-m40-minsize1-maxsize100-seed19174.txt",
            "p_cmax-class1-n198-m88-minsize1-maxsize100-seed12659.txt",
            "p_cmax-class1-n20-m8-minsize1-maxsize100-seed1448.txt",
            "p_cmax-class1-n20-m8-minsize1-maxsize100-seed30901.txt",
            "p_cmax-class1-n20-m10-minsize1-maxsize100-seed14707.txt",
            "p_cmax-class1-n40-m20-minsize1-maxsize100-seed26663.txt",
            "p_cmax-class1-n180-m90-minsize1-maxsize100-seed14416.txt",
            "p_cmax-class1-n36-m12-minsize1-maxsize100-seed28719.txt",
            "p_cmax-class1-n40-m16-minsize1-maxsize100-seed28080.txt",
            "p_cmax-class1-n36-m12-minsize1-maxsize100-seed776.txt",
            "p_cmax-class1-n44-m16-minsize1-maxsize100-seed4304.txt",
            "p_cmax-class1-n40-m16-minsize1-maxsize100-seed30459.txt",
            "p_cmax-class1-n200-m100-minsize1-maxsize100-seed8221.txt",
            "p_cmax-class1-n60-m24-minsize1-maxsize100-seed21912.txt",
            "p_cmax-class1-n160-m80-minsize1-maxsize100-seed22312.txt",
            "p_cmax-class1-n60-m24-minsize1-maxsize100-seed17780.txt",
            "p_cmax-class1-n60-m24-minsize1-maxsize100-seed18724.txt",
            "p_cmax-class1-n160-m80-minsize1-maxsize100-seed19991.txt",
            "p_cmax-class1-n36-m12-minsize1-maxsize100-seed24119.txt",
            "p_cmax-class1-n90-m40-minsize1-maxsize100-seed18666.txt",
            "p_cmax-class1-n80-m32-minsize1-maxsize100-seed236.txt",
            "p_cmax-class1-n162-m72-minsize1-maxsize100-seed25458.txt",
            "p_cmax-class1-n198-m88-minsize1-maxsize100-seed30243.txt",
            "p_cmax-class1-n80-m40-minsize1-maxsize100-seed19262.txt",
            "p_cmax-class1-n100-m50-minsize1-maxsize100-seed6356.txt",
            "p_cmax-class1-n60-m30-minsize1-maxsize100-seed6413.txt",
            "p_cmax-class1-n100-m50-minsize1-maxsize100-seed25986.txt",
            "p_cmax-class1-n90-m40-minsize1-maxsize100-seed21578.txt",
            "p_cmax-class1-n72-m32-minsize1-maxsize100-seed15115.txt",
            "p_cmax-class1-n20-m8-minsize1-maxsize100-seed16507.txt",
            "p_cmax-class1-n100-m40-minsize1-maxsize100-seed15674.txt",
            "p_cmax-class2-n72-m32-minsize20-maxsize100-seed1703.txt",
            "p_cmax-class2-n20-m10-minsize20-maxsize100-seed6389.txt",
            "p_cmax-class2-n120-m60-minsize20-maxsize100-seed26366.txt",
            "p_cmax-class2-n90-m40-minsize20-maxsize100-seed29918.txt",
            "p_cmax-class2-n40-m20-minsize20-maxsize100-seed12089.txt",
            "p_cmax-class2-n80-m40-minsize20-maxsize100-seed15780.txt",
            "p_cmax-class2-n72-m32-minsize20-maxsize100-seed14756.txt",
            "p_cmax-class2-n60-m30-minsize20-maxsize100-seed5041.txt",
            "p_cmax-class2-n60-m30-minsize20-maxsize100-seed18048.txt",
            "p_cmax-class2-n60-m30-minsize20-maxsize100-seed15557.txt",
            "p_cmax-class2-n40-m16-minsize20-maxsize100-seed26927.txt",
            "p_cmax-class2-n54-m24-minsize20-maxsize100-seed22969.txt",
            "p_cmax-class2-n20-m10-minsize20-maxsize100-seed12351.txt",
            "p_cmax-class2-n54-m24-minsize20-maxsize100-seed13532.txt",
            "p_cmax-class2-n40-m16-minsize20-maxsize100-seed4060.txt",
            "p_cmax-class2-n80-m40-minsize20-maxsize100-seed496.txt",
            "p_cmax-class2-n36-m16-minsize20-maxsize100-seed5432.txt",
            "p_cmax-class2-n72-m32-minsize20-maxsize100-seed14584.txt",
            "p_cmax-class2-n40-m16-minsize20-maxsize100-seed25356.txt",
            "p_cmax-class2-n22-m8-minsize20-maxsize100-seed26525.txt",
            "p_cmax-class2-n22-m8-minsize20-maxsize100-seed24630.txt",
            "p_cmax-class2-n22-m8-minsize20-maxsize100-seed23557.txt",
            "p_cmax-class2-n22-m8-minsize20-maxsize100-seed16948.txt",
            "p_cmax-class2-n200-m100-minsize20-maxsize100-seed4358.txt",
            "p_cmax-class2-n20-m8-minsize20-maxsize100-seed21139.txt",
            "p_cmax-class2-n20-m10-minsize20-maxsize100-seed26176.txt",
            "p_cmax-class2-n20-m8-minsize20-maxsize100-seed18571.txt",
            "p_cmax-class2-n36-m16-minsize20-maxsize100-seed11619.txt",
            "p_cmax-class2-n180-m90-minsize20-maxsize100-seed5294.txt",
            "p_cmax-class2-n40-m16-minsize20-maxsize100-seed25578.txt",
            "p_cmax-class2-n160-m80-minsize20-maxsize100-seed12610.txt",
            "p_cmax-class2-n40-m20-minsize20-maxsize100-seed24446.txt",
            "p_cmax-class2-n20-m10-minsize20-maxsize100-seed24498.txt",
            "p_cmax-class2-n160-m80-minsize20-maxsize100-seed26994.txt",
            "p_cmax-class2-n36-m16-minsize20-maxsize100-seed19993.txt",
            "p_cmax-class2-n54-m24-minsize20-maxsize100-seed28902.txt",
            "p_cmax-class2-n144-m64-minsize20-maxsize100-seed1411.txt",
            "p_cmax-class2-n100-m50-minsize20-maxsize100-seed18908.txt",
            "p_cmax-class2-n140-m70-minsize20-maxsize100-seed24799.txt",
            "p_cmax-class2-n140-m70-minsize20-maxsize100-seed20523.txt",
            "p_cmax-class2-n140-m70-minsize20-maxsize100-seed17618.txt",
            "p_cmax-class2-n22-m8-minsize20-maxsize100-seed25434.txt",
            "p_cmax-class2-n20-m8-minsize20-maxsize100-seed15800.txt",
            "p_cmax-class2-n200-m100-minsize20-maxsize100-seed28583.txt",
            "p_cmax-class2-n36-m16-minsize20-maxsize100-seed6300.txt",
            "p_cmax-class2-n120-m60-minsize20-maxsize100-seed5478.txt",
            "p_cmax-class2-n120-m60-minsize20-maxsize100-seed32110.txt",
            "p_cmax-class2-n120-m60-minsize20-maxsize100-seed246.txt",
            "p_cmax-class2-n36-m16-minsize20-maxsize100-seed13780.txt",
            "p_cmax-class2-n54-m24-minsize20-maxsize100-seed25518.txt",
            "p_cmax-class2-n40-m20-minsize20-maxsize100-seed14750.txt",
            "p_cmax-class2-n22-m8-minsize20-maxsize100-seed27190.txt",
            "p_cmax-class2-n180-m90-minsize20-maxsize100-seed13149.txt",
            "p_cmax-class2-n22-m8-minsize20-maxsize100-seed19010.txt",
            "p_cmax-class2-n72-m32-minsize20-maxsize100-seed13643.txt",
            "p_cmax-class2-n160-m80-minsize20-maxsize100-seed1683.txt",
            "p_cmax-class2-n20-m10-minsize20-maxsize100-seed25622.txt",
            "p_cmax-class2-n140-m70-minsize20-maxsize100-seed25387.txt",
            "p_cmax-class2-n108-m48-minsize20-maxsize100-seed14122.txt",
            "p_cmax-class2-n54-m24-minsize20-maxsize100-seed30841.txt",
            "p_cmax-class2-n200-m100-minsize20-maxsize100-seed17851.txt",
            "p_cmax-class2-n100-m50-minsize20-maxsize100-seed3032.txt",
            "p_cmax-class2-n120-m60-minsize20-maxsize100-seed24473.txt",
            "p_cmax-class2-n40-m16-minsize20-maxsize100-seed11557.txt",
            "p_cmax-class2-n100-m50-minsize20-maxsize100-seed20111.txt",
            "p_cmax-class2-n20-m8-minsize20-maxsize100-seed22725.txt",
            "p_cmax-class2-n160-m80-minsize20-maxsize100-seed2079.txt",
            "p_cmax-class2-n22-m8-minsize20-maxsize100-seed20480.txt",
            "p_cmax-class2-n90-m40-minsize20-maxsize100-seed6258.txt",
            "p_cmax-class2-n72-m32-minsize20-maxsize100-seed2959.txt",
            "p_cmax-class2-n20-m8-minsize20-maxsize100-seed3207.txt",
            "p_cmax-class2-n100-m50-minsize20-maxsize100-seed20116.txt",
            "p_cmax-class2-n54-m24-minsize20-maxsize100-seed23159.txt",
            "p_cmax-class2-n80-m40-minsize20-maxsize100-seed19545.txt",
            "p_cmax-class2-n22-m8-minsize20-maxsize100-seed19248.txt",
            "p_cmax-class2-n36-m16-minsize20-maxsize100-seed19338.txt",
            "p_cmax-class2-n20-m8-minsize20-maxsize100-seed18194.txt",
            "p_cmax-class2-n54-m24-minsize20-maxsize100-seed16262.txt",
            "p_cmax-class2-n40-m20-minsize20-maxsize100-seed2794.txt",
            "p_cmax-class2-n200-m100-minsize20-maxsize100-seed16393.txt",
            "p_cmax-class2-n100-m50-minsize20-maxsize100-seed12488.txt",
            "p_cmax-class2-n100-m50-minsize20-maxsize100-seed17999.txt",
            "p_cmax-class2-n140-m70-minsize20-maxsize100-seed26399.txt",
            "p_cmax-class2-n54-m24-minsize20-maxsize100-seed19502.txt",
            "p_cmax-class2-n54-m24-minsize20-maxsize100-seed13118.txt",
            "p_cmax-class2-n60-m30-minsize20-maxsize100-seed18900.txt",
            "p_cmax-class2-n80-m40-minsize20-maxsize100-seed2845.txt",
            "p_cmax-class2-n180-m90-minsize20-maxsize100-seed31956.txt",
            "p_cmax-class2-n20-m10-minsize20-maxsize100-seed11504.txt",
            "p_cmax-class2-n140-m70-minsize20-maxsize100-seed29324.txt",
            "p_cmax-class2-n54-m24-minsize20-maxsize100-seed8824.txt",
            "p_cmax-class2-n160-m80-minsize20-maxsize100-seed10999.txt",
            "p_cmax-class2-n60-m30-minsize20-maxsize100-seed16072.txt",
            "p_cmax-class2-n180-m90-minsize20-maxsize100-seed21043.txt",
        };
        std::vector<std::vector<double>> speedups;
        for (int i = 0; i < testInstances; i++)
        {
            const auto instanceFile = it->first;
            const auto knownOptimal = it++->second;
            // if (instanceFile.find("n22-") == -1 ) {continue;} // only test specific instances n22 && instanceFile.find("n36-m16") == -1  && instanceFile.find("n20-") == -1
            // if (instanceFile.find("class2-") == -1 && instanceFile.find("class1-") == -1) continue;
            if (std::find(selectedInstances.begin(), selectedInstances.end(), instanceFile) == selectedInstances.end())
            {
                continue;
            }
            int numJobs, numMachines;
            std::vector<int> jobDurations;

            std::string instanceFilePath = "benchmarks/lawrinenko/" + instanceFile;
            readInstance(instanceFilePath, numJobs, numMachines, jobDurations);

            std::vector<std::chrono::duration<double>> runtimes;
            std::vector<int> results = {0, 0, 0, 0, 0};
            std::vector<int> visitedNodes;
            std::cout << "Instance: " << instanceFile << std::endl;
            std::mutex mtx;
            std::condition_variable cv;
            bool timerExpired = false;

            std::vector<std::future<void>> futures;
            std::vector<int> threadsUsed = {1, 2, 4, 8, 12};
            /*tbb::global_control global_limit(tbb::global_control::max_allowed_parallelism, 1);
            futures.push_back(std::async(std::launch::async, [&solver, &results, &canceled,  &cv, &mtx, &timerExpired]() {
                    std::unique_lock<std::mutex> lock(mtx);
                    if(cv.wait_for(lock, std::chrono::milliseconds(30000), [&timerExpired]{ return timerExpired; })) {
                        return;
                    }
                    if (results[0] == 0) {
                        std::cout << " Aborted" << std::endl;
                        solver.endExecution = true;
                        canceled = true;
                        }
                }));
            auto start = std::chrono::high_resolution_clock::now();
            results[0] = solver.solve(numMachines, jobDurations);
            auto end = std::chrono::high_resolution_clock::now();

            runtimes.push_back(end - start);
            visitedNodes.push_back(solver.visitedNodes);
            totalNodesVisited += solver.visitedNodes;
            // Timer beenden, wenn die Aufgabe abgeschlossen ist
            {
                std::lock_guard<std::mutex> lock(mtx);
                timerExpired = true;
            }
            cv.notify_all();
            for (auto& future : futures) {
                future.get();
            }
            if(canceled) continue;
*/
            bool canceled = false;

            for (std::vector<double>::size_type l = 0; l < threadsUsed.size(); l++)
            { // i have locally "only" 12 threads
                int result = 0;
                tbb::global_control global_limit(tbb::global_control::max_allowed_parallelism, threadsUsed[l]);

                // Timer asynchron starten
                // std::cout << "start run with: " << threadsUsed[l] << std::endl;
                std::future<void> canceler;
                BnB_base_Impl solver(true, true, true, true, false);
                canceler = std::async(std::launch::async, [&solver, &results, &l, &canceled, &cv, &mtx, &timerExpired]()
                                      {
                    std::unique_lock<std::mutex> lock(mtx);
                    if(cv.wait_for(lock, std::chrono::seconds(10), [&timerExpired]{ return timerExpired; })) {
                        return;
                    }
                    if (results[l] == 0) {
                        std::cout << " Aborted" << std::endl;
                        solver.cancelExecution();
                        } });
                auto start = std::chrono::high_resolution_clock::now();
                result = solver.solve(numMachines, jobDurations);
                auto end = std::chrono::high_resolution_clock::now();
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    timerExpired = true;
                }
                cv.notify_all();
                canceler.get();
                if (result == 0)
                {
                    canceled = true;
                    break;
                }
                results[l] = result;

                runtimes.push_back(end - start);
                visitedNodes.push_back(solver.visitedNodes);
                totalNodesVisited += solver.visitedNodes;
            }
            if (canceled)
                continue;
            if (runtimes[0] > std::chrono::milliseconds(100))
                interestingFiles.push_back(instanceFile); // collect instances with a runtime of at least 0,1 seconds runtime

            std::cout << "runtimes: ";
            for (auto time : runtimes)
            {
                std::cout << time.count() << " ";
            }
            std::cout << std::endl
                      << "speedup: "; // todo safe speedups and return the biggest and smalles in the end
            std::vector<double> ewst;
            speedups.push_back(ewst);
            for (auto time : runtimes)
            {
                speedups[speedups.size() - 1].push_back(runtimes[0].count() / time.count());
                std::cout << runtimes[0].count() / time.count() << " ";
            }
            std::cout << std::endl
                      << "Visited Nodes : ";
            for (auto visited : visitedNodes)
            {
                std::cout << visited << " ";
            }
            std::cout << std::endl
                      << "increase Nodes: ";
            for (auto visited : visitedNodes)
            {
                std::cout << (double)visited / (double)visitedNodes[0] << " ";
            }
            std::cout << std::endl;
            std::vector<int> opt(results.size(), knownOptimal);
            if (results == opt)
            { // TODO curretnly onnly the last resolt with 12 threads is checked the others are ignored
                succesfull.push_back("" + instanceFilePath);
                std::cout << "Success: Found the optimal makespan of " << visitedNodes[4] << std::endl;
            }
            else
            {
                failed.push_back(instanceFilePath);
                std::cout << "Error: The calculated makespan of ";
                for (auto vla : results)
                    std::cout << vla;
                std::cout << " does not match the known optimal value of " << knownOptimal << std::endl;
            }
            std::cout << "--------------------------------------------------------------------------------------------------------" << std::endl;
        }
        if (testInstances > 0)
        {
            std::cout << "Failed instances: " << failed.size() << "/" << failed.size() + succesfull.size() << " = " << (double)failed.size() * 100 / (double)(failed.size() + succesfull.size()) << "%" << std::endl;
            if (failed.size() > 0)
                std::cout << "Failed instances: " << std::endl;
            for (auto inst : failed)
            {
                std::cout << inst << std::endl;
            }
            std::vector<double> best(5, 0);
            std::vector<double> worst(5, 7);
            std::vector<double> average(5, 0);
            int speedupInstances = 0;
            int speedupInstancesWeak = 0;
            for (auto speedup : speedups)
            {
                if (speedup[speedup.size() - 1] > best[4])
                    best = speedup;
                if (speedup[speedup.size() - 1] < worst[4])
                    worst = speedup;
                for (std::vector<double>::size_type i = 0; i < average.size(); i++)
                    average[i] += speedup[i];
                bool better = true;
                for (std::vector<double>::size_type i = 1; i < speedup.size(); i++)
                    better &= (speedup[i] > 1.0002);
                if (better)
                    speedupInstances++;
                better = false;
                for (std::vector<double>::size_type i = 1; i < speedup.size(); i++)
                    better |= (speedup[i] > 1);
                if (better)
                    speedupInstancesWeak++;
            }
            for (std::vector<double>::size_type i = 0; i < average.size(); i++)
                average[i] = average[i] / speedups.size();

            std::vector<double> geoMeans(5, 0);
            std::vector<double> medians(5, 0);

            for (std::vector<double>::size_type i = 0; i < speedups[0].size(); i++)
            {
                std::vector<double> threadSpeedups;
                for (const auto &speedup : speedups)
                {
                    threadSpeedups.push_back(speedup[i]);
                }
                geoMeans[i] = geometricMean(threadSpeedups);
                medians[i] = median(threadSpeedups);
            }

            std::cout << "worst found Speedups: ";
            for (auto time : worst)
                std::cout << time << " ";

            std::cout << std::endl
                      << "best found Speedups: ";
            for (auto time : best)
                std::cout << time << " ";

            std::cout << std::endl
                      << "average found Speedups: ";
            for (auto time : average)
                std::cout << time << " ";

            std::cout << std::endl
                      << "geometric means of Speedups: ";
            for (auto mean : geoMeans)
                std::cout << mean << " ";

            std::cout << std::endl
                      << "medians of Speedups: ";
            for (auto med : medians)
                std::cout << med << " ";

            std::cout << std::endl
                      << "Instances with a speedup for all threads: ";
            std::cout << ((double)speedupInstances / speedups.size()) * 100 << "%";

            std::cout << std::endl
                      << "Instances with a speedup for at least one threads: ";
            std::cout << ((double)speedupInstancesWeak / speedups.size()) * 100 << "%";
            std::cout << std::endl;

            std::cout << "Total number Of Nodes Visited: " << totalNodesVisited << std::endl;

            std::cout << "Interesting instances (single threaded between 10 and 120000 millisecond):" << std::endl;
            for (auto interest : interestingFiles)
            {
                std::cout << interest << std::endl;
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

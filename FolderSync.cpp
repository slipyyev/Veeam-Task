#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <unistd.h>

namespace fs = std::filesystem; 

#define ARGUMENTS_AMOUNT 5
#define MICROSECONDS_IN_ONE_SECOND 1000000

const std::string ARGUMENTS_DESCRIPTION =
    "First argument — source file relative path\n"
    "Second argument — replica file relative path\n"
    "Third argument — log file relative path\n"
    "Fourth argument — synchronization interval in seconds\n";




void runSynchronisationLoop(const fs::path sourcePath, const fs::path replicaPath, const fs::path logPath, int period) {

    if (!fs::exists(sourcePath)) {
        std::cout << "Source directory didn't exist, one was created" << std::endl;
        fs::create_directory(sourcePath);
    }
    
    if (!fs::exists(replicaPath)) {
        std::cout << "Source directory didn't exist, one was created" << std::endl;
        fs::create_directory(replicaPath);
    }

    if (!fs::exists(logPath.parent_path())) {
        std::cout << "Log path didn't exist, one was created" << std::endl;
        fs::create_directory(logPath.parent_path());
    }

    std::ifstream logFileTest (logPath);

    if (logFileTest.is_open()) {
        logFileTest.close();
        std::cout << "Log file existssss" << std::endl;
    }
    else {
        std::cout << "Log file didn't exist, one was created" << std::endl;
    }

    std::ofstream outfile;
    outfile.open (logPath, std::ofstream::out | std::ofstream::app);


    while (true) {

        std::cout << "Hello" << std::endl;

        usleep(period * MICROSECONDS_IN_ONE_SECOND);
    }
}



int main(int argc, char* argv[]) {

    if (argc != ARGUMENTS_AMOUNT) {
        std::cout << "Number of arguments provided: " << argc << ", agruments needed: " << ARGUMENTS_AMOUNT << std::endl;
        std::cout << ARGUMENTS_DESCRIPTION << std::endl;
        return 1;
    }

    for (int i = 0; i < argc; ++i) {
        std::cout << "argv[" << i << "] = " << argv[i] << std::endl;
    }

    int period;

    fs::path sourcePath = argv[1]; 
    fs::path replicaPath = argv[2]; 
    fs::path logPath = argv[3];

    if (!sourcePath.is_relative() || sourcePath.empty() || sourcePath != argv[1]) {
        std::cerr << "Source path is wrong" << '\n';
        return 1;
    }

    if (!replicaPath.is_relative() || replicaPath.empty() || replicaPath != argv[2]) {
        std::cerr << "Replica path is wrong" << '\n';
        return 1;
    }

    if (!logPath.is_relative() || logPath.empty() || logPath != argv[3]) {
        std::cerr << "Log path is wrong" << '\n';
        return 1;
    }

    try {
        period = std::stoi(argv[4]);
    }
    catch(const std::exception& e) {
        std::cerr << "Synchronization interval is wrong" << '\n';
        std::cout << ARGUMENTS_DESCRIPTION << std::endl;
        return 1;
    }

    if (period <= 0) {
        std::cerr << "Synchronization interval is wrong" << '\n';
    }

    std::cout << sourcePath << " " << replicaPath << " " << logPath << " " << period << std::endl;

    runSynchronisationLoop(sourcePath, replicaPath, logPath, period);

    return 0;
}
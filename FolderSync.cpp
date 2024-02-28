#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <set>
#include <algorithm>
#include <unistd.h>
#include <openssl/evp.h>
#include <openssl/md5.h>
#include <ctime>

namespace fs = std::filesystem; 

#define ARGUMENTS_AMOUNT 5
#define MICROSECONDS_IN_ONE_SECOND 1000000

const std::string ARGUMENTS_DESCRIPTION =
    "First argument — source file relative path\n"
    "Second argument — replica file relative path\n"
    "Third argument — log file relative path\n"
    "Fourth argument — synchronization interval in seconds\n";

void syncFolders(const fs::path & sourcePath, const fs::path & replicaPath, const fs::path & logPath);


std::string calculateMD5(const std::string& filename) {
    std::ifstream file(filename, std::ifstream::binary);
    if (!file) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return "";
    }

    EVP_MD_CTX* mdContext = EVP_MD_CTX_new();
    EVP_DigestInit_ex(mdContext, EVP_md5(), NULL);

    char buffer[1024];
    while (file.good()) {
        file.read(buffer, sizeof(buffer));
        EVP_DigestUpdate(mdContext, buffer, file.gcount());
    }

    unsigned char hash[MD5_DIGEST_LENGTH];
    unsigned int hashLength;
    EVP_DigestFinal_ex(mdContext, hash, &hashLength);

    EVP_MD_CTX_free(mdContext);

    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < hashLength; ++i) {
        ss << std::setw(2) << static_cast<unsigned>(hash[i]);
    }

    return ss.str();
}

bool filesHaveSameHash(const fs::path & sourcePath, const fs::path & replicaPath) {
    
    std::string sourceMD5Hash = calculateMD5(sourcePath.string());
    std::string replicaMD5Hash = calculateMD5(replicaPath.string());
    
    if (sourceMD5Hash != replicaMD5Hash)
        return false;

    return true;
}


bool syncSameFiles(const fs::path & sourcePath, const fs::path & replicaPath, const fs::path & logPath, const std::set<fs::path> & sameFiles) {
    
    
    for (const fs::path & f : sameFiles) {
        if (fs::is_directory(sourcePath / f)) {
            syncFolders(sourcePath / f, replicaPath / f, logPath);
        }
        else if (!filesHaveSameHash(sourcePath / f, replicaPath / f)) {
            fs::remove_all(replicaPath / f);
            fs::copy(sourcePath / f, replicaPath / f);
            std::cout << "File " << f << " was updated from the replica folder" << std::endl;
        }
    }

    return true;
}

bool eraseDeletedFiles(const fs::path & replicaPath, const fs::path & logPath, const std::set<fs::path> & inReplicaOnly) {

    for (const fs::path & f : inReplicaOnly) {
        fs::remove_all(replicaPath / f);
        std::cout << "File " << f << " removed from the replica folder" << std::endl;
    }

    return true;
}

bool addNewlyCreatedFiles(const fs::path & sourcePath, const fs::path & replicaPath, const fs::path & logPath, const std::set<fs::path> & inSourceOnly) {

    for (const fs::path & f : inSourceOnly) {
        fs::copy(sourcePath / f, replicaPath / f, fs::copy_options::recursive);
        std::cout << "File " << f << " added to the replica folder" << std::endl;
    }

    return true;
}



void syncFolders(const fs::path & sourcePath, const fs::path & replicaPath, const fs::path & logPath) {
    
    std::set<fs::path> filesInSource;
    std::set<fs::path> filesInReplica;


    for(const fs::directory_entry & dirEntry: fs::directory_iterator(sourcePath))
        filesInSource.insert(dirEntry.path().filename());

    for(const fs::directory_entry & dirEntry: fs::directory_iterator(replicaPath))
        filesInReplica.insert(dirEntry.path().filename());

    std::cout << "-------------------------------" << std::endl;
    std::cout << "Files in source:" << std::endl;

    for (auto p : filesInSource) {
                std::cout << p << std::endl;
    }


    std::cout << "-------------------------------" << std::endl;
    std::cout << "Files in replica:" << std::endl;

    for (auto p : filesInReplica) {
                std::cout << p << std::endl;
    }

    std::cout << "-------------------------------" << std::endl;

    std::set<fs::path> sameFiles;

    std::set_intersection(
        filesInSource.begin(), filesInSource.end(), 
        filesInReplica.begin(), filesInReplica.end(),                   
        std::inserter(sameFiles, sameFiles.begin())
    );

    std::set<fs::path> inSourceOnly;
    std::set<fs::path> inReplicaOnly;

    std::set_difference(
        filesInSource.begin(), filesInSource.end(), 
        sameFiles.begin(), sameFiles.end(), 
        std::inserter(inSourceOnly, inSourceOnly.begin())
    );

    std::set_difference(
        filesInReplica.begin(), filesInReplica.end(), 
        sameFiles.begin(), sameFiles.end(), 
        std::inserter(inReplicaOnly, inReplicaOnly.begin())
    );
    

    for (const auto & a : sameFiles ) {
        std::cout << "Same file: " << a << std::endl;
    }

    syncSameFiles(sourcePath, replicaPath, logPath, sameFiles);
    eraseDeletedFiles(replicaPath, logPath, inReplicaOnly);
    addNewlyCreatedFiles(sourcePath, replicaPath, logPath, inSourceOnly);
}


void runSynchronisationLoop(const fs::path & sourcePath, const fs::path & replicaPath, const fs::path & logPath, int period) {

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
    }
    else {
        std::cout << "Log file didn't exist, one was created" << std::endl;
    }

    std::ofstream outfile;
    outfile.open (logPath, std::ofstream::out | std::ofstream::app);

    std::time_t currentTime = std::time(nullptr);
    std::string timeString;

    while (true) {

        timeString = std::ctime(&currentTime);
        // Erasing the newline at the end of string
        timeString.erase(timeString.length() - 1);
        std::cout << "[" << timeString << "] Sync happens" << std::endl;

        syncFolders(sourcePath, replicaPath, logPath);

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
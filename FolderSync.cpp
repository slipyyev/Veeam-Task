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

const int ARGUMENTS_AMOUNT = 5;
const int MICROSECONDS_IN_ONE_SECOND = 1000000;

const std::string ARGUMENTS_DESCRIPTION =
    "First argument — source file relative path\n"
    "Second argument — replica file relative path\n"
    "Third argument — log file relative path\n"
    "Fourth argument — synchronization interval in seconds\n";

const std::string SOURCE_DOESNT_EXIST = "Source directory didn't exist, one was created";
const std::string REPLICA_DOESNT_EXIST = "Replica directory didn't exist, one was created";
const std::string LOG_PATH_DOESNT_EXIST = "Log path didn't exist, one was created";
const std::string LOG_FILE_DOESNT_EXIST = "Log file didn't exist, one was created";

const std::string SOURCE_CREATING_ERROR = "An exception occured when creating a source folder: ";
const std::string REPLICA_CREATING_ERROR = "An exception occured when creating a replica folder: ";
const std::string LOG_PATH_CREATING_ERROR = "An exception occured when creating a logging folder: ";


void syncFolders(const fs::path & sourcePath, const fs::path & replicaPath, std::stringstream & logStream);


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


bool syncSameFiles(const fs::path & sourcePath, const fs::path & replicaPath, std::stringstream & logStream, const std::set<fs::path> & sameFiles) {
    
    
    for (const fs::path & f : sameFiles) {
        if (fs::is_directory(sourcePath / f)) {
            syncFolders(sourcePath / f, replicaPath / f, logStream);
        }
        else if (!filesHaveSameHash(sourcePath / f, replicaPath / f)) {
            fs::remove_all(replicaPath / f);
            fs::copy(sourcePath / f, replicaPath / f);
            logStream << "File " << f << " was updated in the replica folder" << std::endl;
            return false;
        }
    }

    return true;
}

bool eraseDeletedFiles(const fs::path & replicaPath, std::stringstream & logStream, const std::set<fs::path> & inReplicaOnly) {

    for (const fs::path & f : inReplicaOnly) {
        fs::remove_all(replicaPath / f);
        logStream << "File " << f << " was removed from the replica folder" << std::endl;
    }

    return true;
}

bool addNewlyCreatedFiles(const fs::path & sourcePath, const fs::path & replicaPath, std::stringstream & logStream, const std::set<fs::path> & inSourceOnly) {

    for (const fs::path & f : inSourceOnly) {
        fs::copy(sourcePath / f, replicaPath / f, fs::copy_options::recursive);
        logStream << "File " << f << " was added to the replica folder" << std::endl;
    }

    return true;
}


bool checkIfAllFoldersExist(const fs::path & sourcePath, const fs::path & replicaPath, const fs::path & logPath, std::stringstream & logStream) {
    
    if (!fs::exists(sourcePath)) {
        try {
            fs::create_directories(sourcePath);
            logStream << SOURCE_DOESNT_EXIST << std::endl;

        } catch (const std::exception& e) {
            logStream << SOURCE_CREATING_ERROR << e.what() << std::endl;
            return false;
        }
    }

    
    if (!fs::exists(replicaPath)) {
        try {
            fs::create_directories(replicaPath);
            logStream << REPLICA_DOESNT_EXIST << std::endl;

        } catch (const std::exception& e) {
            logStream << REPLICA_CREATING_ERROR << e.what() << std::endl;
            return false;
        }
    }

    if (!fs::exists(logPath.parent_path())) {
        try {
            fs::create_directories(logPath.parent_path());
            logStream << LOG_PATH_DOESNT_EXIST << std::endl;

        } catch (const std::exception& e) {
            logStream << LOG_PATH_CREATING_ERROR << e.what() << std::endl;
            return false;
        }
    }

    std::ifstream logFileTest (logPath);

    if (logFileTest.is_open()) {
        logFileTest.close();
    }
    else {
        logStream << LOG_FILE_DOESNT_EXIST << std::endl;
    }

    return true;
}

void syncFolders(const fs::path & sourcePath, const fs::path & replicaPath, std::stringstream & logStream) {
    
    std::set<fs::path> filesInSource;
    std::set<fs::path> filesInReplica;

    for(const fs::directory_entry & dirEntry: fs::directory_iterator(sourcePath))
        filesInSource.insert(dirEntry.path().filename());

    for(const fs::directory_entry & dirEntry: fs::directory_iterator(replicaPath))
        filesInReplica.insert(dirEntry.path().filename());

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

    syncSameFiles(sourcePath, replicaPath, logStream, sameFiles);
    eraseDeletedFiles(replicaPath, logStream, inReplicaOnly);
    addNewlyCreatedFiles(sourcePath, replicaPath, logStream, inSourceOnly);
}

std::string getTimestamp() {

    std::time_t currentTime = std::time(nullptr);
    std::string timeString;
    timeString = std::ctime(&currentTime);
    // Erasing the newline at the end of string
    timeString.erase(timeString.length() - 1);

    return "[" + timeString + "]";
}

void addReportToLogAndConsole(std::stringstream & logStream, const std::string & startTimestamp, const fs::path & logPath) {

    if (logStream.str().empty())
        return;

    std::string content = logStream.str();
    logStream.str(std::string());
    logStream << startTimestamp << " Synchronization iteration has started" << std::endl;
    logStream << content;
    logStream << getTimestamp() << " Synchronization iteration has ended" << std::endl;
    logStream << "-------------------------------------------------------------------" << std::endl;

    std::ofstream file;
    file.open (logPath, std::ofstream::out | std::ofstream::app);

    if (!file) {
        logStream << "Didn't manage to write to the log file \"" << logPath.filename() << "\"" << std::endl;
    }
    else {
        std::cout << logStream.str();
        file << logStream.str();
    }

    file.close();
    // clear the stringstream
    logStream.str(std::string());
}

bool runSynchronisationLoop(const fs::path & sourcePath, const fs::path & replicaPath, const fs::path & logPath, int period) {

    std::stringstream logStream;
    std::string syncStartTimestamp;

    while (true) {

        syncStartTimestamp = getTimestamp();
        if (!checkIfAllFoldersExist(sourcePath, replicaPath, logPath, logStream)) {
            addReportToLogAndConsole(logStream, syncStartTimestamp, logPath);
            return false;    
        }
        syncFolders(sourcePath, replicaPath, logStream);
        addReportToLogAndConsole(logStream, syncStartTimestamp, logPath);
        usleep(period * MICROSECONDS_IN_ONE_SECOND);
    }

    return true;
}

bool validateUserInput(const fs::path & sourcePath, const fs::path & replicaPath, const fs::path & logPath, int & period, char * argv[]) {
    
    if (sourcePath.empty() || sourcePath != argv[1]) {
        std::cerr << "Source path is wrong" << std::endl;
        return false;
    }

    if (replicaPath.empty() || replicaPath != argv[2]) {
        std::cerr << "Replica path is wrong" << std::endl;
        return false;
    }

    if (logPath.empty() || logPath.filename().empty()  || logPath != argv[3]) {
        std::cerr << "Log path is wrong" << std::endl;
        return false;
    }

    try {
        period = std::stoi(argv[4]);
    }
    catch(const std::exception& e) {
        std::cerr << "Synchronization interval is wrong" << std::endl;
        std::cerr << ARGUMENTS_DESCRIPTION << std::endl;
        return false;
    }

    if (period <= 0) {
        std::cerr << "Synchronization interval is wrong" << std::endl;
        return false;
    }

    return true;
}



int main(int argc, char* argv[]) {

    if (argc != ARGUMENTS_AMOUNT) {
        std::cerr << "Number of arguments provided: " << argc << ", agruments needed: " << ARGUMENTS_AMOUNT << std::endl;
        std::cerr << ARGUMENTS_DESCRIPTION << std::endl;
        return 1;
    }

    int period;

    fs::path sourcePath = argv[1]; 
    fs::path replicaPath = argv[2]; 
    fs::path logPath = argv[3];

    if (!validateUserInput(sourcePath, replicaPath, logPath, period, argv)) {
        return 1;
    }

    if (!runSynchronisationLoop(sourcePath, replicaPath, logPath, period)) {
        return 1;
    }

    return 0;
}
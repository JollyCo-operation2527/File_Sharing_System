#include <iostream>
#include <string>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>
#include <filesystem>
#include <vector>
#include <thread>
#include <sstream>
#include <ctime>
#include <mutex>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 6000

#define maxBufferLength 4096

void createDownloadFolder();
int uploadFile(int, std::string);
void handleUpload(int);
void handleDownload(int);
int downloadFile(int, std::string);
void log(const std::string&);

// Structure declaration
struct FileHeader{
    char fn[128];  // File's name
    int fs;        // File's size
};

void createDownloadFolder(){
    std::filesystem::path dir = "./download/";

    if(!std::filesystem::exists(dir)){
        std::filesystem::create_directories(dir);
    }
}

// Declare logFile
std::ofstream logFile("logClient.txt", std::ios::app);
std::mutex logMutex;

int main(){
    int clientSocket;
    struct sockaddr_in  serverAddress;
    int status, bytesRcv;
    std::string inStr;   // Store input from keyboard
    char buffer[80];     // Store input from keyboard

    // Create a download folder if there is none
    createDownloadFolder();

    // This is the start of new log
    log("===============================================================================");

    // Create a client socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (clientSocket < 0){
        log("CLIENT ERROR: Could not open socket");
        exit(-1);
    }

    // Setup address
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = inet_addr(SERVER_IP);
    serverAddress.sin_port = htons((unsigned short) SERVER_PORT);

    // Connect to server
    status = connect(clientSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress));

    if (status < 0){
        log("CLIENT ERROR: Could not connect to server");
        exit(-1);
    }

    // Go into loop to talk to the server
    while(1){
        std::cout << "CLIENT: Enter message to send to server ..." << std::endl;
        std::getline(std::cin, inStr);

        // THis is to prevent client stuck when hitting enter 2 times in a row without sending anything
        if (inStr.empty()){
            continue;
        }

        // Send message string to server
        strcpy(buffer, inStr.c_str());
        std::cout << "CLIENT: Sending \"" << buffer << "\" to the server" << std::endl;
        log("CLIENT: Sending \"" + std::string(buffer) + "\" to the server");

        int bytesSend = send(clientSocket, buffer, strlen(buffer), 0);

        if(bytesSend < 0){
            log("CLIENT ERROR: bytesSend < 0 error");
        }

        if (strcmp(buffer, "stop") == 0){
            break;
        }

        else if (strcmp(buffer, "upload") == 0){
            // Call the handleUpload function
            handleUpload(clientSocket);
        }

        else if (strcmp(buffer, "download") == 0){
            // Call the handleDownload function
            handleDownload(clientSocket);
        }

        // Get response from the server (expecting "OK")
        bytesRcv = recv(clientSocket, buffer, 80, 0);

        // If receive less than or equal 0 bytes from server, it means the server connection is closed
        if (bytesRcv <= 0){
            std::cout << "CLIENT: Server connection closed" << std::endl;
            log("CLIENT: Server connection closed");
            break;
        }

        buffer[bytesRcv] = 0;  // Put a 0 at the end so we can display the string
        std::cout << "CLIENT: Got back response \"" << buffer << "\" from the server" << std::endl;
        log("CLIENT: Got back response \"" + std::string(buffer) + "\" from the server");
    }

    close(clientSocket);
    std::cout << "CLIENT: Shutting down" << std::endl;
    log("CLIENT: Shutting down");

    return 0;
}

void handleUpload(int clientSocket){
    std::string input, file;
    std::vector<std::string> filesToUpload;

    // Get the user to specify the file's name
    std::cout << "CLIENT: Enter file's name: ";
    std::getline(std::cin, input);  
    log("CLIENT: Enter file's name: " + input);

    // Create a string stream object to input
    std::stringstream ss(input);
    // Split by ','
    while (std::getline(ss, file, ',')){
        filesToUpload.push_back(file);
    }

    // Add a string "done" to signal the end of list of files to upload
    if(input != "abort"){
        filesToUpload.push_back("done");
    }
    
    // Loop through the vector
    for(const std::string& str : filesToUpload){
        // CHeck if the file is regular and exists
        if(input == "abort" || str == "done" || (std::filesystem::exists(str) && std::filesystem::is_regular_file(str))){
            uploadFile(clientSocket, str);
        }else{
            std::cout << "CLIENT ERROR: No such file or file irregular" << std::endl;
            log("CLIENT ERROR: No such file or file irregular");
            continue;
        }
    }
}

// This function will upload 1 file
int uploadFile(int clientSocket, std::string input){
    char buffer[maxBufferLength];
    FileHeader myFile;

    // If the input is "abort"
    if(input == "abort"){
        std::cout << "CLIENT: Abort uploading mode" << std::endl;
        log("CLIENT: Abort uploading mode");
        // The file's name is "abort". No real file is named like this 
        strcpy(myFile.fn, "abort");
        myFile.fs = 0;
        // Send this fake file to the server. The server will recognize "abort" as a fake file and abort the upload mode
        int bytesSend = send(clientSocket, &myFile, sizeof(myFile), 0);
        return 0;
    }else if (input == "done"){
        // The file's name is "done". No real file is named like this 
        strcpy(myFile.fn, "done");
        myFile.fs = 0;
        // Send this fake file to the server. The server will recognize "done" as the end of the list of files to upload
        std::cout << "CLIENT: Sending \"Done\"" << std::endl;
        log("CLIENT: Sending \"Done\"");

        int bytesSend = send(clientSocket, &myFile, sizeof(myFile), 0);
        return 0;
    }
    // Otherwise, continue as normal
    // Open the file (or try to, at least)
    std::ifstream inFile(input, std::ios::binary);
    
    // If cannot open inFile
    if (!inFile){
        log("CLIENT ERROR: Could not open inFile");
        return -1;
    }

    // Get the size of the file
    inFile.seekg(0, inFile.end);
    uint32_t fileSize = inFile.tellg();
    inFile.seekg(0, inFile.beg);
    
    // Send file's meta data to server
    strcpy(myFile.fn, input.c_str());
    myFile.fs = fileSize;
    
    int bytesSend = send(clientSocket, &myFile, sizeof(myFile), 0);

    std::cout << "File: " << myFile.fn << " - " << myFile.fs << std::endl;
    log("File: " + std::string(myFile.fn) + " - " + std::to_string(myFile.fs));

    if(bytesSend < 0){
        log("CLIENT ERROR: bytesSend < 0 error (1)");
    }

    // Read inFile into buffer
    while (inFile.read(buffer, maxBufferLength) || inFile.gcount() > 0){
        std::streamsize bytesRead = inFile.gcount();

        //std::cout << bytesRead << std::endl;

        // Sending the data to the server
        bytesSend = send(clientSocket, buffer, bytesRead, 0);

        if(bytesSend < 0){
            log("CLIENT: bytesSend < 0 error (2)");
        }
    }

    std::cout << "File: " << input << " - Done" << std::endl;
    log("File: " + input + " - Done");

    // Wait for the acknowledgement from the server
    char ackBuffer[80];
    int ackBytes = recv(clientSocket, ackBuffer, sizeof(ackBuffer), 0);
    if (ackBytes > 0){
        ackBuffer[ackBytes] = '\0';
        std::cout << "CLIENT: Server has received file: " << myFile.fn << std::endl;
        log("CLIENT: Server has received file: " + std::string(myFile.fn));
    }

    return 0;
}

void handleDownload(int clientSocket){
    std::string input, file;
    std::vector<std::string> filesToDownload;

    // Get the user to specify the file's name
    std::cout << "CLIENT: Enter file's name: ";
    std::getline(std::cin, input);  
    log("CLIENT: Enter file's name: " + input);

    // Create a string stream object to input
    std::stringstream ss(input);
    // Split by ','
    while (std::getline(ss, file, ',')){
        filesToDownload.push_back(file);
    }

    // Add a string "done" to signal the end of list of files to download
    if(input != "abort"){
        filesToDownload.push_back("done");
    }
    
    bool flag = true;
    // Loop through the vector
    for(const std::string& str : filesToDownload){
        if(str == "abort"){
            std::cout << "CLIENT: Aborting Download Mode" << std::endl;
            log("CLIENT: Aborting Download Mode");
            // Send "abort" string to the server
            send(clientSocket, str.c_str(), str.size(), 0);
            break;
        }else{
            // Do this only in the first loop to deal with the "OK" response from server
            if(flag){
                char okBuffer[10];
                int okBytes = recv(clientSocket, okBuffer, sizeof(okBuffer) - 1, 0);

                if (okBytes > 0){
                    okBuffer[okBytes] = '\0';
                    std::cout << "CLIENT: Receive response from server: " << okBuffer << std::endl;
                    log("CLIENT: Receive response from server: " + std::string(okBuffer));
                }
                flag = 0;
            }
            downloadFile(clientSocket, str);
        }
    }
}

// This function will download 1 file
int downloadFile(int clientSocket, std::string input){
    int totalBytesRead = 0;
    char buffer[maxBufferLength];
    FileHeader recvFile;

    // Send the file's name to the server to request that file
    int requestFile = send(clientSocket, input.c_str(), input.size() + 1, 0);  

    if (requestFile < 0){
        log("CLIENT ERROR: requestFile < 0 error");
    }

    // if input is "done", this is the end of filesToDownload
    if (input == "done"){
        return 0;
    }

    // Receive the file's meta data
    int bytesRcv = recv(clientSocket, &recvFile, sizeof(recvFile), 0);

    // If fake file, return
    if (strcmp(recvFile.fn, "done") == 0){
        std::cout << "CLIENT: File requested does not exist or inaccessible" << std::endl;
        log("CLIENT: File requested does not exist or inaccessible");
        return 0;
    }

    std::string fullPath = "./download/";
    fullPath += recvFile.fn;

    std::cout << "CLIENT: Client downloading" << std::endl;
    log("CLIENT: Client downloading");

    // Debugging purpose
    log("CLIENT: File's name is: " + std::string(recvFile.fn));
    log("CLIENT: File's size is: " + std::to_string(recvFile.fs));
    // Debugging purpose

    std::ofstream downFile(fullPath, std::ofstream::binary);

    if (!downFile){
        log("CLIENT ERROR: Could not write downFile");
        return -1;
    }

    // Write the file to download folder
    while(totalBytesRead < recvFile.fs){
        int bytesToRead = std::min(maxBufferLength, recvFile.fs - totalBytesRead);
        int bytesRead = recv(clientSocket, buffer, bytesToRead, 0);

        if (bytesRead < 0){
            log("CLIENT ERROR: Writing error");
            break;
        }

        //std::cout << "total bytes read = " << totalBytesRead << std::endl; 

        downFile.write(buffer, bytesRead);
        totalBytesRead += bytesRead;
    }

    // Send the server acknowledgement that the file was received
    const char* ackMsg = "file_received"; 
    int ackBytes = send(clientSocket, ackMsg, strlen(ackMsg), 0);
    if (ackBytes > 0){
        std::cout << "CLIENT: File " << recvFile.fn << " received" << std::endl;
        log("CLIENT: File " + std::string(recvFile.fn) + " received");
    }

    downFile.close();
    std::cout << "CLIENT: File closed" << std::endl;
    log("CLIENT: File closed");
    return 0;
}

// This function handles logging
void log(const std::string& message){
    std::lock_guard<std::mutex> guard(logMutex);

    std::time_t now = std::time(nullptr);
    logFile << std::ctime(&now) << ">> " << message << std::endl;
}
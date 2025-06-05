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

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 6000

#define maxBufferLength 4096

int uploadFile(int, std::string);
void handleUpload(int);

// Structure declaration
struct FileHeader{
    char fn[128];  // File's name
    uint32_t fs;   // File's size
};

int main(){
    int clientSocket;
    struct sockaddr_in  serverAddress;
    int status, bytesRcv;
    std::string inStr;   // Store input from keyboard
    char buffer[80];     // Store input from keyboard

    // Create a client socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (clientSocket < 0){
        std::cout << "CLIENT ERROR: Could not open socket" << std::endl;
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
        std::cout << "CLIENT ERROR: Could not connect to server" << std::endl;
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
        int bytesSend = send(clientSocket, buffer, strlen(buffer), 0);

        if(bytesSend < 0){
            std::cout << "CLIENT: bytesSend < 0 error" << std::endl;
        }

        if (strcmp(buffer, "stop") == 0){
            break;
        }

        else if (strcmp(buffer, "upload") == 0){
            // Call the handleUpload function
            handleUpload(clientSocket);
        }

        // Get response from the server (expecting "OK")
        bytesRcv = recv(clientSocket, buffer, 80, 0);

        // If receive less than or equal 0 bytes from server, it means the server connection is closed
        if (bytesRcv <= 0){
            std::cout << "CLIENT: Server connection closed" << std::endl;
            break;
        }

        buffer[bytesRcv] = 0;  // Put a 0 at the end so we can display the string
        std::cout << "CLIENT: Got back response \"" << buffer << "\" from the server" << std::endl;
    }

    close(clientSocket);
    std::cout << "CLIENT: Shutting down" << std::endl;
}

void handleUpload(int clientSocket){
    std::string input, file;
    std::vector<std::string> filesToUpload;

    // Get the user to specify the file's name
    std::cout << "CLIENT: Enter file's name: ";
    std::getline(std::cin, input);  

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
        int bytesSend = send(clientSocket, &myFile, sizeof(myFile), 0);
        return 0;
    }
    // Otherwise, continue as normal
    // Open the file (or try to, at least)
    std::ifstream inFile(input, std::ios::binary);
    
    // If cannot open inFile
    if (!inFile){
        std::cout << "CLIENT: Could not open inFile" << std::endl;
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

    if(bytesSend < 0){
        std::cout << "CLIENT: bytesSend < 0 error" << std::endl;
    }

    // Read inFile into buffer
    while (inFile.read(buffer, maxBufferLength) || inFile.gcount() > 0){
        std::streamsize bytesRead = inFile.gcount();

        //std::cout << bytesRead << std::endl;

        // Sending the data to the server
        bytesSend = send(clientSocket, buffer, bytesRead, 0);

        if(bytesSend < 0){
            std::cout << "CLIENT: bytesSend < 0 error" << std::endl;
        }
    }

    std::cout << "File: " << input << " - Done" << std::endl;

    // Wait for the acknowledgement from the server
    char ackBuffer[80];
    int ackBytes = recv(clientSocket, ackBuffer, sizeof(ackBuffer), 0);
    if (ackBytes > 0){
        ackBuffer[ackBytes] = '\0';
        std::cout << "CLIENT: Server has received file: " << myFile.fn << std::endl;
    }

    return 0;
}
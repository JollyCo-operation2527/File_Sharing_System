#include <iostream>
#include <string>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>
//#include <vector>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 6000

#define maxBufferLength 4096

int upload(int);

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
        send(clientSocket, buffer, strlen(buffer), 0);

        if ((strcmp(buffer, "done") == 0) || (strcmp(buffer, "stop") == 0)){
            break;
        }

        else if (strcmp(buffer, "upload") == 0){
            upload(clientSocket);
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
        
        // This block is moved up
        /*if((strcmp(buffer, "done") == 0) || (strcmp(buffer, "stop") == 0)){
            break;
        }*/ 
    }

    close(clientSocket);
    std::cout << "CLIENT: Shutting down" << std::endl;
}


int upload(int clientSocket){
    char buffer[maxBufferLength];
    std::string input;

    // Create a struct that include file meta data
    struct FileHeader{
        char fn[128];
        int fs;
    };

    // Get the user to specify the file's name
    std::cout << "CLIENT: Enter file's name: ";
    std::getline(std::cin, input);  
 
    // Open the file (or try to, at least)
    std::ifstream inFile(input, std::ios::binary);
    
    // If cannot open inFile
    if (!inFile){
        std::cout << "CLIENT: Could not open inFile" << std::endl;
        return -1;
    }

    // Get the size of the file
    inFile.seekg(0, inFile.end);
    int fileSize = inFile.tellg();
    inFile.seekg(0, inFile.beg);
    
    // Send file's meta data to server
    FileHeader myFile; 
    strcpy(myFile.fn, input.c_str());
    myFile.fs = fileSize;
    send(clientSocket, &myFile, sizeof(myFile), 0);

    // Read inFile into buffer
    while (inFile.read(buffer, maxBufferLength) || inFile.gcount() > 0){
        std::streamsize bytesRead = inFile.gcount();

        std::cout << bytesRead << std::endl;
        std::cout << buffer << std::endl;

        // Sending the data to the server
        send(clientSocket, buffer, bytesRead, 0);
    }

    return 0;
}
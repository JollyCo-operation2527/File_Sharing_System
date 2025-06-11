#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fstream>
#include <sys/time.h>
#include <thread>
#include <filesystem>
#include <csignal>
#include <atomic>

#define SERVER_PORT 6000

#define maxBufferLength 4096

int handleClient(int);
int clientUpload(int, const char[], int);
int handleUpload(int);
void createOutputFolder();
void signalHandler(int);
int clientDownload(int, const char[], int);
int handleDownload(int);

// Structure declaration
struct FileHeader{
    char fn[128];  // File's name
    int fs;        // File's size
};

// Initialize serverSocket to -1 (to safely handle handle in the signalHangler() function)
int serverSocket = -1;

// create an atomic flag to keep the server running in main
// This flag should be atomic because "Ctrl + C"  could be pressed at any moment
std::atomic<bool> keepRunning(true);

void signalHandler(int signum){
    std::cout << "SERVER: SIGINT detected. Shutting down server" << std::endl;
    keepRunning = false;
    // Only shutdown server if serverSocket has been properly created
    if(serverSocket != -1){
        shutdown(serverSocket, SHUT_RDWR);  // Force shutdown so that the code doesn't wait for accept() in main
        close(serverSocket);
    }
}

void createOutputFolder(){
    std::filesystem::path dir = "./output/";

    if(!std::filesystem::exists(dir)){
        std::filesystem::create_directories(dir);
    }
}

int main(){
    int clientSocket;
    struct sockaddr_in  serverAddress, clientAddress;
    int status, bytesRcv;
    socklen_t addrSize;

    // Register the SIGINT handler
    signal(SIGINT, signalHandler);

    // Create output folder if there isn't any
    createOutputFolder();

    // Create a server socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSocket < 0){
        std::cout << "SERVER ERROR: Could not open socket" << std::endl;
        exit(-1);
    }

    // Setup server address
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons((unsigned short) SERVER_PORT);

    // Bind the server socket
    status = bind(serverSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress));

    if (status < 0){
        std::cout << "SERVER ERROR: Could not bind socket" << std::endl;
        exit(-1);
    }

    // Listen up to 10 clients
    status = listen(serverSocket, 10);

    if (status < 0){
        std::cout << "SERVER ERROR: Could not listen on socket" << std::endl;
        exit(-1);
    }

    std::cout << "SERVER: READY" << std::endl;

    // Wait for clients
    while(keepRunning){
        addrSize = sizeof(clientAddress);
        clientSocket = accept(serverSocket, (struct sockaddr *) &clientAddress, &addrSize);

        if (clientSocket < 0){
            if(!keepRunning){
                break;
            }
            std::cout << "SERVER ERROR: Could not accept incoming client connection" << std::endl;
            continue;
        }
        // If clientSocket != -1
        std::thread(handleClient, clientSocket).detach();
    }

    // Server shutdown after SIGINT
    std::cout << "SERVER: Server shutdown" << std::endl;
}

// Handle each client separately
int handleClient(int clientSocket){
    int bytesRcv;
    char buffer[30];
    char response[] = "OK" ;

    std::cout << "SERVER: Received client connection" << std::endl;

    // Infinite loop to talk to client
    while(1) {
        // Receive messages from the client
        bytesRcv = recv(clientSocket, buffer, sizeof(buffer), 0);

        if (bytesRcv < 0){
            std::cout << "SERVER: bytesRcv < 0 error" << std::endl;
            break;
        }

        buffer[bytesRcv] = 0;  // Put a 0 at the end so we can display the string
        std::cout << "SERVER: Received client request: " << buffer << std::endl;

        // Respond with "OK" message
        std::cout << "SERVER: Sending \"" << response << "\" to client" << std::endl;
        send(clientSocket, response, strlen(response), 0);

        if((strcmp(buffer, "done") == 0) || (strcmp(buffer, "stop") == 0)){
            break;
        }

        else if (strcmp(buffer, "upload") == 0){
            // Call the handleUpload function
            std::cout << "SERVER: --------------------------------" << std::endl;
            std::cout << "SERVER: --- < Entering Upload Mode > ---" << std::endl;

            handleUpload(clientSocket);
        }

        else if (strcmp(buffer, "download") == 0){
            // Call the handleDownload function
            std::cout << "SERVER: --------------------------------" << std::endl;
            std::cout << "SERVER: --- < Entering Download Mode > ---" << std::endl;

            handleDownload(clientSocket);
        }
    }

    std::cout << "SERVER: Closing client connection" << std::endl;
    close(clientSocket);
    return 0;
}

int handleUpload(int clientSocket){
    // While loop to keep accepting new files being uploaded
    // Without this loop, server only receives 1 file
    while(1){
        FileHeader recvFile;
        int bytesRcv = recv(clientSocket, &recvFile, sizeof(recvFile), 0);

        //std::cout << "bytesRcv = " << bytesRcv << std::endl;

        if (bytesRcv < 0){
            std::cout << "SERVER: bytesRcv < 0 error" << std::endl;
        }

        if (strcmp(recvFile.fn, "abort") == 0){
            std::cout << "SERVER: --- < Aborting Upload Mode > ---" << std::endl;
            return 0;
        }

        if (strcmp(recvFile.fn, "done") == 0){
            std::cout << "SERVER: --- < Finishing Upload Mode > ---" << std::endl;
            return 0;
        }

        std::cout << "SERVER: File's name is: " << recvFile.fn << std::endl;
        std::cout << "SERVER: File's size is: " << recvFile.fs << std::endl;

        clientUpload(clientSocket, recvFile.fn, recvFile.fs); 
    }
    return 0;
}

// This function handles upload of 1 file
int clientUpload(int clientSocket, const char fileName[128], int fileSize){
    int totalBytesRead = 0;
    char buffer[maxBufferLength];
    std::string fullPath = "./output/";
    fullPath += fileName;

    std::cout << "SERVER: Client uploading" << std::endl;

    std::ofstream outFile(fullPath, std::ofstream::binary);

    if (!outFile){
        std::cout << "SERVER: Could not write outFile" << std::endl;
        return -1;
    }

    while(totalBytesRead < fileSize){
        int bytesToRead = std::min(maxBufferLength, fileSize - totalBytesRead);
        int bytesRead = recv(clientSocket, buffer, bytesToRead, 0);

        if (bytesRead < 0){
            std::cout << "SERVER: Writing error" << std::endl;
            break;
        }
        
        // What if bytesRead < 0 (could happen due to error)

        //std::cout << "total bytes read = " << totalBytesRead << std::endl; 

        outFile.write(buffer, bytesRead);
        totalBytesRead += bytesRead;
    }

    // Send the client acknowledgement that the file was received
    const char* ackMsg = "file_received"; 
    int ackBytes = send(clientSocket, ackMsg, strlen(ackMsg), 0);
    if (ackBytes > 0){
        std::cout << "SERVER: Server has received file: " << fileName << std::endl;
    }

    outFile.close();
    std::cout << "SERVER: File closed" << std::endl;
    return 0;
}

//
int handleDownload(int clientSocket){
    // While loop to keep accepting new files being downloaded from clients
    // Without this loop, server only processes 1 file
    while(1){
        
    }
    return 0;
}

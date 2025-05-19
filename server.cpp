#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fstream>
#include <sys/time.h>
#include <thread>

#define SERVER_PORT 6000

#define maxBufferLength 4096

int clientUpload(int, const char[], int);
int handleUpload(int);

// Structure declaration
struct FileHeader{
    char fn[128];  // File's name
    uint32_t fs;   // File's size
};

int main(){
    int serverSocket, clientSocket;
    struct sockaddr_in  serverAddress, clientAddress;
    int status, bytesRcv;
    socklen_t addrSize;
    char buffer[30];
    char response[] = "OK" ;

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
    while(1){
        addrSize = sizeof(clientAddress);
        clientSocket = accept(serverSocket, (struct sockaddr *) &clientAddress, &addrSize);

        if (clientSocket < 0){
            std::cout << "SERVER ERROR: Could not accept incoming client connection" << std::endl;
            exit(-1);
        }
        // If clientSocket != -1
        std::cout << "SERVER: Received client connection" << std::endl;

        // Infinite loop to talk to client
        while(1) {
            // Receive messages from the client
            bytesRcv = recv(clientSocket, buffer, sizeof(buffer), 0);

            if (bytesRcv < 0){
                std::cout << "SERVER: bytesRcv < 0 error" << std::endl;
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
                std::thread(handleUpload, serverSocket).detach();
            }
        }

        std::cout << "SERVER: Closing client connection" << std::endl;
        close(clientSocket);

        if (strcmp(buffer, "stop") == 0){
            break;
        }
    }

    // Close the server socket
    close(serverSocket);
    std::cout << "SERVER: Shutting down" << std::endl;
}

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
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);

        if (bytesRead < 0){
            std::cout << "SERVER: Writing error" << std::endl;
            break;
        }
        
        // What if bytesRead < 0 (could happen due to error)

        //std::cout << "total bytes read = " << totalBytesRead << std::endl; 

        outFile.write(buffer, bytesRead);
        totalBytesRead += bytesRead;
    }

    outFile.close();
    std::cout << "SERVER: File closed" << std::endl;
    return 0;
}

int handleUpload(int serverSocket){
    // While loop to keep accepting new files being uploaded
    // Without this loop, server only receives 1 file
    while(1){
        struct sockaddr_in uploadAddress;
        socklen_t uploadSize;

        std::cout << "SERVER: --------------------------------" << std::endl;
        std::cout << "SERVER: --- < Entering Upload Mode > ---" << std::endl;

        // Create a file descriptor
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(serverSocket, &readfds);
        struct timeval timeout;
        int originalTimeout = 15;  // Wait 15 seconds for the uploadSocket to connect
        timeout.tv_sec = originalTimeout;    
        timeout.tv_usec = 0;

        int activity = select(serverSocket + 1, &readfds, NULL, NULL, &timeout);

        // If some socket is ready (activity > 0) and serverSokcet is one of them
        if(activity > 0 && FD_ISSET(serverSocket, &readfds)){
            uploadSize = sizeof(uploadAddress);
            int uploadSocket = accept(serverSocket, (struct sockaddr *) &uploadAddress, &uploadSize);

            if (uploadSocket < 0){
                std::cout << "SERVER ERROR: Could not accept incoming client connection" << std::endl;
                return -1;
            }

            std::cout << "SERVER: Thread connected" << std::endl;

            FileHeader recvFile;
            int bytesRcv = recv(uploadSocket, &recvFile, sizeof(recvFile), 0);

            if (bytesRcv < 0){
                std::cout << "SERVER: bytesRcv < 0 error" << std::endl;
            }

            std::cout << "SERVER: File's name is: " << recvFile.fn << std::endl;
            std::cout << "SERVER: File's size is: " << recvFile.fs << std::endl;

            // Spawn a new thread wrapping up in a lambda function and call the clientUpload
            std::thread([=](){
                clientUpload(uploadSocket, recvFile.fn, recvFile.fs);
                close(uploadSocket);
            }).detach();
            
        }else{
            std::cout << "SERVER: No upload attempt within " << originalTimeout << " seconds. Exiting upload mode" << std::endl;
            return -1;
        }
    }
}
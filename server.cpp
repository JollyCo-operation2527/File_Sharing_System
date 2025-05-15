#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fstream>

#define SERVER_PORT 6000

int clientUpload();

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

    // Listen up to 5 clients
    status = listen(serverSocket, 5);

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
            buffer[bytesRcv] = 0;  // Put a 0 at the end so we can display the string
            std::cout << "SERVER: Received client request: " << buffer << std::endl;

            // Respond with "OK" message
            std::cout << "SERVER: Sending \"" << response << "\" to client" << std::endl;
            send(clientSocket, response, strlen(response), 0);

            if((strcmp(buffer, "done") == 0) || (strcmp(buffer, "stop") == 0)){
                break;
            }
        }

        std::cout << "SERVER: Closing client connection" << std::endl;
        close(clientSocket);

        if(strcmp(buffer, "stop") == 0){
            break;
        }
    }

    // Close the server socket
    close(serverSocket);
    std::cout << "SERVER: Shutting down" << std::endl;
}

int clientUpload(){
    return 0;
}
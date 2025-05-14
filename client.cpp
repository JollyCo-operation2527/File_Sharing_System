#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(){
    int clientSocket;
    struct sockaddr_in  serverAddress;
    int status, bytesRcv;
    char inStr[80];      // Store input from keyboard
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
    serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddress.sin_port = htons((unsigned short) 6000);

    // Connect to server
    status = connect(clientSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress));

    if (status < 0){
        std::cout << "CLIENT ERROR: Could not connect to server" << std::endl;
        exit(-1);
    }

    // Go into loop to talk to the server
    while(1){
        std::cout << "CLIENT: Enter message to send to server ..." << std::endl;
        std::cin >> inStr;

        // Send message string to server
        strcpy(buffer, inStr);
        std::cout << "CLIENT: Sending \" " << buffer << " \" to the server" << std::endl;
        send(clientSocket, buffer, strlen(buffer), 0);

        // Get response from the server (expecting "OK")
        bytesRcv = recv(clientSocket, buffer, 80, 0);
        buffer[bytesRcv] = 0;  // Put a 0 at the end so we can display the string
        std::cout << "CLIENT: Got back response \"" << buffer << "\" from the server" << std::endl;
        
        if((strcmp(buffer, "done") == 0) || (strcmp(buffer, "stop") == 0)){
            break;
        }
    }

    close(clientSocket);
    std::cout << "CLIENT: Shutting down" << std::endl;
}

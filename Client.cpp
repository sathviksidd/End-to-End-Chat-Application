#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

void receive_messages(SOCKET server_socket);

int main()
{
    // Initialize Winsock
    WSADATA wsaData;
    int iResult;

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0)
    {
        std::cerr << "WSAStartup failed with error: " << iResult << std::endl;
        return EXIT_FAILURE;
    }

    // Create a socket for connecting to server
    SOCKET connect_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connect_socket == INVALID_SOCKET)
    {
        std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return EXIT_FAILURE;
    }

    // Setup server address and port
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(54000);

    // Convert IPv4 address from text to binary form
    iResult = inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);
    if (iResult <= 0)
    {
        std::cerr << "Invalid address or address not supported." << std::endl;
        closesocket(connect_socket);
        WSACleanup();
        return EXIT_FAILURE;
    }

    // Connect to server
    iResult = connect(connect_socket, (sockaddr*)&server_addr, sizeof(server_addr));
    if (iResult == SOCKET_ERROR)
    {
        std::cerr << "Connection failed with error: " << WSAGetLastError() << std::endl;
        closesocket(connect_socket);
        WSACleanup();
        return EXIT_FAILURE;
    }

    // Prompt the user for their name
    std::string username;
    std::cout << "Enter your name: ";
    std::getline(std::cin, username);

    // Send the username to the server
    int sendResult = send(connect_socket, username.c_str(), static_cast<int>(username.size()), 0);
    if (sendResult == SOCKET_ERROR)
    {
        std::cerr << "Failed to send username with error: " << WSAGetLastError() << std::endl;
        closesocket(connect_socket);
        WSACleanup();
        return EXIT_FAILURE;
    }

    std::cout << "Connected to server!" << std::endl;

    // Start a thread to receive messages from the server
    std::thread recv_thread(receive_messages, connect_socket);
    recv_thread.detach();

    // Send messages to server
    std::string message;
    while (true)
    {
        std::getline(std::cin, message);
        if (message == "/quit")
        {
            break;
        }

        sendResult = send(connect_socket, message.c_str(), static_cast<int>(message.size()), 0);
        if (sendResult == SOCKET_ERROR)
        {
            std::cerr << "Send failed with error: " << WSAGetLastError() << std::endl;
            break;
        }
    }

    // Shutdown the connection since no more data will be sent
    iResult = shutdown(connect_socket, SD_SEND);
    if (iResult == SOCKET_ERROR)
    {
        std::cerr << "Shutdown failed with error: " << WSAGetLastError() << std::endl;
    }

    closesocket(connect_socket);
    WSACleanup();

    return EXIT_SUCCESS;
}

void receive_messages(SOCKET server_socket)
{
    char recvbuf[512];
    int recvbuflen = sizeof(recvbuf);

    while (true)
    {
        int iResult = recv(server_socket, recvbuf, recvbuflen, 0);
        if (iResult > 0)
        {
            recvbuf[iResult] = '\0';
            std::cout << recvbuf << std::endl;
        }
        else if (iResult == 0)
        {
            std::cout << "Server closed the connection." << std::endl;
            break;
        }
        else
        {
            std::cerr << "Receive failed with error: " << WSAGetLastError() << std::endl;
            break;
        }
    }
}

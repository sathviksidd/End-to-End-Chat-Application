#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <algorithm>
#include <functional>

#pragma comment(lib, "Ws2_32.lib")

// Structure to hold client information
struct ClientInfo
{
    SOCKET socket;
    std::string name;
};

// Global variables
std::vector<ClientInfo> clients;
std::mutex clients_mutex;

// Function to handle individual client communication
void handle_client(SOCKET client_socket);

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

    // Create a socket for listening
    SOCKET listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == INVALID_SOCKET)
    {
        std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return EXIT_FAILURE;
    }

    // Bind the socket
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Accept connections on any IP address
    server_addr.sin_port = htons(54000);      // Port number

    iResult = bind(listen_socket, (sockaddr*)&server_addr, sizeof(server_addr));
    if (iResult == SOCKET_ERROR)
    {
        std::cerr << "Bind failed with error: " << WSAGetLastError() << std::endl;
        closesocket(listen_socket);
        WSACleanup();
        return EXIT_FAILURE;
    }

    // Listen on the socket
    iResult = listen(listen_socket, SOMAXCONN);
    if (iResult == SOCKET_ERROR)
    {
        std::cerr << "Listen failed with error: " << WSAGetLastError() << std::endl;
        closesocket(listen_socket);
        WSACleanup();
        return EXIT_FAILURE;
    }

    std::cout << "Server is listening on port 54000..." << std::endl;

    // Accept and handle incoming connections
    while (true)
    {
        sockaddr_in client_addr;
        int client_size = sizeof(client_addr);

        SOCKET client_socket = accept(listen_socket, (sockaddr*)&client_addr, &client_size);
        if (client_socket == INVALID_SOCKET)
        {
            std::cerr << "Accept failed with error: " << WSAGetLastError() << std::endl;
            continue;
        }

        // Start a new thread to handle the client
        std::thread client_thread(handle_client, client_socket);
        client_thread.detach();
    }

    // Cleanup
    closesocket(listen_socket);
    WSACleanup();

    return EXIT_SUCCESS;
}

void handle_client(SOCKET client_socket)
{
    char recvbuf[512];
    int recvbuflen = sizeof(recvbuf);

    // Receive the client's name
    int iResult = recv(client_socket, recvbuf, recvbuflen, 0);
    if (iResult <= 0)
    {
        std::cerr << "Failed to receive username." << std::endl;
        closesocket(client_socket);
        return;
    }
    recvbuf[iResult] = '\0';
    std::string client_name = recvbuf;

    // Add the client to the list with their name
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients.push_back({ client_socket, client_name });
    }

    std::cout << client_name << " has connected." << std::endl;

    // Notify all clients about the new connection
    std::string join_message = client_name + " has joined the chat.";
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        for (const auto& client : clients)
        {
            if (client.socket != client_socket)
            {
                send(client.socket, join_message.c_str(), static_cast<int>(join_message.size()), 0);
            }
        }
    }

    // Continue to receive messages from the client
    while (true)
    {
        iResult = recv(client_socket, recvbuf, recvbuflen, 0);
        if (iResult > 0)
        {
            recvbuf[iResult] = '\0';
            std::string message = recvbuf;

            // Prepend the client's name to the message
            std::string full_message = client_name + ": " + message;

            // Broadcast the message to all connected clients
            std::lock_guard<std::mutex> lock(clients_mutex);
            for (const auto& client : clients)
            {
                if (client.socket != client_socket)
                {
                    int sendResult = send(client.socket, full_message.c_str(), static_cast<int>(full_message.size()), 0);
                    if (sendResult == SOCKET_ERROR)
                    {
                        std::cerr << "Send failed with error: " << WSAGetLastError() << std::endl;
                    }
                }
            }
        }
        else if (iResult == 0)
        {
            std::cout << client_name << " has disconnected." << std::endl;
            break;
        }
        else
        {
            std::cerr << "Recv failed with error: " << WSAGetLastError() << std::endl;
            break;
        }
    }

    // Remove the client from the list
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients.erase(std::remove_if(clients.begin(), clients.end(),
            [client_socket](const ClientInfo& client) {
                return client.socket == client_socket;
            }), clients.end());
    }

    // Notify all clients about the disconnection
    std::string leave_message = client_name + " has left the chat.";
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        for (const auto& client : clients)
        {
            send(client.socket, leave_message.c_str(), static_cast<int>(leave_message.size()), 0);
        }
    }

    closesocket(client_socket);
}

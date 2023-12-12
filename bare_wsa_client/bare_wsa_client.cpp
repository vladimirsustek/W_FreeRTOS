#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>


// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

#define WSAEWOULDBLOCK 10035

static void socketSetNonBlocking(SOCKET s)
{
    /* Mode = '1' for non-blocking*/
    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);
}

int app(int argc, char** argv)
{
    WSADATA wsaData;
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct addrinfo* result = NULL,
        * ptr = NULL,
        hints;
    char recvbuf[DEFAULT_BUFLEN] = { 0 };
    int iResult;
    int recvbuflen = DEFAULT_BUFLEN;

    // Validate the parameters
    if (argc != 2) {
        printf("usage: %s server-name\n", argv[0]);
        return 1;
    }

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo(argv[1], DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    ptr = result;

    // Create a SOCKET for connecting to server
    ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
        ptr->ai_protocol);

    if (ConnectSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    socketSetNonBlocking(ConnectSocket);

    // Attempt to connect to an address until one succeeds
    int non_block_conn_err = 1;
    fd_set wr_set;
    struct timeval timeout = { 0,(10*1000) }; //10*1000 us
    int num_of_sockets = 0;

    FD_ZERO(&wr_set);
    FD_SET(ConnectSocket, &wr_set);

    iResult = connect(ConnectSocket, ptr->ai_addr, ptr->ai_addrlen);

    non_block_conn_err = WSAGetLastError();

    if (non_block_conn_err != WSAEWOULDBLOCK && non_block_conn_err != 0)
    {
        printf("Other problem occured: %d\n", non_block_conn_err);
    }

    printf("Client is waiting to finish connect operation\n");

    while (1)
    {
        static int loops = 1;
        /* Use select to determine the completion of the connection request
            by checking if the socket is writeable
            As only one socket is created and passed as argument, the result
            mus be 1*/
        if (1 == select(ConnectSocket, 0, &wr_set, 0, &timeout))
        {
            printf("1 Socket to write is available - connected, looped: %ld [ms]\n", loops*timeout.tv_usec/1000);
            break;
        }
        loops++;
    }

    freeaddrinfo(result);

    // Send an initial buffer
    iResult = send(ConnectSocket, "1st message from client\n", (int)strlen("1st message from client\n"), 0);
    if (iResult == SOCKET_ERROR) {
        printf("send failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }

    printf("Bytes Sent: %ld\n", iResult);

    // shutdown the connection since no more data will be sent
    iResult = shutdown(ConnectSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }

    // Receive until the peer closes the connection
    do {

        iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
        if (iResult < 0 && WSAGetLastError() == WSAEWOULDBLOCK)
        {
            int select_result = 0;
            fd_set rd_set;
            FD_ZERO(&rd_set);
            FD_SET(ConnectSocket, &rd_set);
            while (1)
            {
                static int loops = 1;
                /* Use select to determine the completion of the receive request
                    by checking if the socket is readable
                    As only one socket is created and passed as argument, the result
                    mus be 1*/
                if (1 == select(ConnectSocket, &rd_set, NULL, 0, &timeout))
                {
                    iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
                    printf("1 Socket to read is available - connected, looped: %ld [ms]\n", loops * timeout.tv_usec / 1000);
                    break;
                }
            }

        }
        if (iResult > 0)
        {
            printf("Bytes received: %d\n", iResult);
            printf(recvbuf);
            memset(recvbuf, 0, recvbuflen);
        }
        else if (iResult == 0)
            printf("Connection closed\n");
        else
            printf("recv failed with error: %d\n", WSAGetLastError());

    } while (iResult > 0);

    // cleanup
    closesocket(ConnectSocket);
    WSACleanup();

    return 0;
}

int __cdecl main(int argc, char** argv)
{
    Sleep(1000);

    for(int i = 0; i < 5; i++)
    {
        app(argc, argv);
        Sleep(10);
    }
}
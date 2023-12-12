#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN (1 << 15)
#define DEFAULT_PORT "27015"

static void socketSetNonBlocking(SOCKET s)
{
	/* Mode = '1' for non-blocking*/
	u_long mode = 1;
	ioctlsocket(s, FIONBIO, &mode);
}

int __cdecl app(void)
{
	WSADATA wsaData;
	int iResult;
	int non_block_conn_err = 0;

	SOCKET ListenSocket = INVALID_SOCKET;
	SOCKET ClientSocket = INVALID_SOCKET;

	socketSetNonBlocking(ListenSocket);

	struct addrinfo* result = NULL;
	struct addrinfo hints;

	int iSendResult;
	char recvbuf[DEFAULT_BUFLEN] = { 0 };
	int recvbuflen = DEFAULT_BUFLEN;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	// Create a SOCKET for the server to listen for client connections.
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	// Setup the TCP listening socket
	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	freeaddrinfo(result);

	iResult = listen(ListenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	printf("Server is waiting to finish accept operation\n");

	int select_result = 0;
	while (1)
	{
		/* Use select to determine the completion of the connection request
			by checking if the socket is writeable
			As only one socket is created and passed as argument, the result
			mus be 1*/
			// Check for incoming connections using select
		fd_set readSet;
		FD_ZERO(&readSet);
		FD_SET(ListenSocket, &readSet);

		timeval timeout;
		timeout.tv_sec = 1; // 1 second timeout
		timeout.tv_usec = 0;

		int result = select(0, &readSet, nullptr, nullptr, &timeout);

		if (result == SOCKET_ERROR) {
			printf("select failed with error: %d\n", WSAGetLastError());
			break;
		}

		if (result > 0) {
			// Accept the incoming connection
			ClientSocket = accept(ListenSocket, nullptr, nullptr);
			if (ClientSocket == INVALID_SOCKET) {
				printf("Accept failed with error: %d\n ", WSAGetLastError());
			}
			else {
				printf("Connection accepted.\n");
			}
		}
	}


	// No longer need server socket
	closesocket(ListenSocket);

	// Receive until the peer shuts down the connection
	do {

		iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
		if (iResult > 0) {
			printf("Bytes received: %d\n", iResult);
			printf(recvbuf);
			const char msg[] = "2nd echo from server: ";
			// Echo the buffer back to the sender
			memcpy(recvbuf + strlen(msg), recvbuf, iResult);
			memcpy(recvbuf, msg, strlen(msg));
			iSendResult = send(ClientSocket, recvbuf, iResult + strlen(msg), 0);
			if (iSendResult == SOCKET_ERROR) {
				printf("send failed with error: %d\n", WSAGetLastError());
				closesocket(ClientSocket);
				WSACleanup();
				return 1;
			}
			printf("Bytes sent: %d\n", iSendResult);
		}
		else if (iResult == 0)
			printf("Connection closing...\n");
		else {
			printf("recv failed with error: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			WSACleanup();
			return 1;
		}

	} while (iResult > 0);

	// shutdown the connection since we're done
	iResult = shutdown(ClientSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		WSACleanup();
		return 1;
	}

	// cleanup
	closesocket(ClientSocket);
	WSACleanup();

	return 0;
}

int __cdecl main(int argc, char** argv)
{
	for (int i = 0; i < 5; i++)
	{
		app();
	}
	(void)getchar();
}
#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN (1 << 10)
#define DEFAULT_PORT "27015"

#define ONE_SOCKET_AVAILABLE 1
#define APP_ERR -1

static void socketSetNonBlocking(SOCKET s)
{
	/* Mode = '1' for non-blocking*/
	u_long mode = 1;
	ioctlsocket(s, FIONBIO, &mode);
}

static void timeStampPrint(const char* format, ...)
{
	static int already_called = 0;
	va_list argptr;

	already_called++;

	/* Guard to ensure recursion is not end-less */
	assert(already_called >= 0 && already_called <= 2);

	/* Use recursion only on first call */
	if (already_called == 1)
	{
		SYSTEMTIME localTime;
		GetLocalTime(&localTime);
		timeStampPrint("%02d:%02d:%02d:%03d ", localTime.wHour, localTime.wMinute,
			localTime.wSecond, localTime.wMilliseconds);
	}

	va_start(argptr, format);
	vfprintf(stdout, format, argptr);
	va_end(argptr);

	already_called--;
}

int __cdecl app(void)
{
	WSADATA wsaData;

	SOCKET ListenSocket = INVALID_SOCKET;
	SOCKET ClientSocket = INVALID_SOCKET;

	addrinfo* clientAdr = NULL;
	addrinfo clientAdrHints;

	fd_set readOpAcceptFDset;
	fd_set readOpRecvFDset;

	timeval nonBlockOpMaxTimeout;

	char serverRecvBuff[DEFAULT_BUFLEN] = { 0 };
	int sendOpResult = -1;
	int serverRecvBuffSize = DEFAULT_BUFLEN;
	int sockOpResult = -1;
	int recentWSAError = -1;
	int selectResult = 0;
	bool connection_accepted = false;

	const char serverMsg[] = "2nd echo from server: ";

	nonBlockOpMaxTimeout.tv_sec = 0;
	nonBlockOpMaxTimeout.tv_usec = 10 * 1000;

	/* Yes, used to indicate how many times
	must be function call to do desired operation */
	int nonBlockingPollingLoops = 0;

	socketSetNonBlocking(ListenSocket);

	// Initialize Winsock
	sockOpResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (sockOpResult != 0) {
		timeStampPrint("WSAStartup failed with error: %d\n", sockOpResult);
		return APP_ERR;
	}

	ZeroMemory(&clientAdrHints, sizeof(clientAdrHints));
	clientAdrHints.ai_family = AF_INET;
	clientAdrHints.ai_socktype = SOCK_STREAM;
	clientAdrHints.ai_protocol = IPPROTO_TCP;
	clientAdrHints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	sockOpResult = getaddrinfo("localhost", DEFAULT_PORT, &clientAdrHints, &clientAdr);
	if (sockOpResult != 0) {
		timeStampPrint("getaddrinfo failed with error: %d\n", sockOpResult);
		WSACleanup();
		return APP_ERR;
	}

	// Create a SOCKET for the server to listen for client connections.
	ListenSocket = socket(clientAdr->ai_family, clientAdr->ai_socktype, clientAdr->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		timeStampPrint("socket create failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(clientAdr);
		WSACleanup();
		return APP_ERR;
	}

	// Setup the TCP listening socket
	sockOpResult = bind(ListenSocket, clientAdr->ai_addr, (int)clientAdr->ai_addrlen);
	if (sockOpResult == SOCKET_ERROR) {
		timeStampPrint("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(clientAdr);
		closesocket(ListenSocket);
		WSACleanup();
		return APP_ERR;
	}

	freeaddrinfo(clientAdr);

	sockOpResult = listen(ListenSocket, SOMAXCONN);
	if (sockOpResult == SOCKET_ERROR) {
		timeStampPrint("listen failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return APP_ERR;
	}

	timeStampPrint("Server is waiting to finish accept operation\n");
	while ((selectResult == 0 && ClientSocket == INVALID_SOCKET))
	{
		/* Use select to determine the completion of the connection request
			by checking if the socket is writeable
			As only one socket is created and passed as argument, the result
			mus be 1*/
			// Check for incoming connections using select
		FD_ZERO(&readOpAcceptFDset);
		FD_SET(ListenSocket, &readOpAcceptFDset);

		selectResult = select(0, &readOpAcceptFDset, nullptr, nullptr, &nonBlockOpMaxTimeout);

		if (selectResult == SOCKET_ERROR) {
			timeStampPrint("select failed with error: %d\n", WSAGetLastError());
			break;
		}

		if (selectResult > 0) {
			// Accept the incoming connection
			ClientSocket = accept(ListenSocket, nullptr, nullptr);
			if (ClientSocket == INVALID_SOCKET) {
				timeStampPrint("accept failed with error: %d\n ", WSAGetLastError());
			}
			else {
				timeStampPrint("Connection accepted, %d\n", WSAGetLastError());
			}
		}
	}

	// No longer need server socket
	closesocket(ListenSocket);

	timeStampPrint("Server is waiting to receive \n");

	do {

		sockOpResult = recv(ClientSocket, serverRecvBuff, serverRecvBuffSize, 0);
		if (sockOpResult < 0 && WSAGetLastError() == WSAEWOULDBLOCK)
		{
			FD_ZERO(&readOpRecvFDset);
			FD_SET(ClientSocket, &readOpRecvFDset);
			while (1)
			{
				nonBlockingPollingLoops = 1;
				/* Use select to determine the completion of the receive request
					by checking if the socket is readable
					As only one socket is created and passed as argument, the result
					mus be 1*/
				if (ONE_SOCKET_AVAILABLE == select(ClientSocket, &readOpRecvFDset, NULL, 0, &nonBlockOpMaxTimeout))
				{
					sockOpResult = recv(ClientSocket, serverRecvBuff, serverRecvBuffSize, 0);
					timeStampPrint("1 Socket to read is available - connected, looped: %ld [ms]\n", nonBlockingPollingLoops * nonBlockOpMaxTimeout.tv_usec / 1000);
				}
			}

		}
		if (sockOpResult > 0)
		{
			timeStampPrint("Bytes received: %d\n", sockOpResult);
			timeStampPrint("RX: %s", serverRecvBuff);
			// Echo the buffer back to the sender
			memcpy(serverRecvBuff + strlen(serverMsg), serverRecvBuff, sockOpResult);
			memcpy(serverRecvBuff, serverMsg, strlen(serverMsg));
			sendOpResult = send(ClientSocket, serverRecvBuff, sockOpResult + strlen(serverMsg), 0);
			if (sendOpResult == SOCKET_ERROR) {
				timeStampPrint("send failed with error: %d\n", WSAGetLastError());
				closesocket(ClientSocket);
				WSACleanup();
				return APP_ERR;
			}
			timeStampPrint("Bytes sent: %d\n", sendOpResult);
		}
		else if (sockOpResult == 0)
			timeStampPrint("Connection closed\n");
		else
			timeStampPrint("recv failed with error: %d\n", WSAGetLastError());



	} while (sockOpResult > 0);

	// shutdown the connection since we're done
	sockOpResult = shutdown(ClientSocket, SD_SEND);
	if (sockOpResult == SOCKET_ERROR) {
		timeStampPrint("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		WSACleanup();
		return APP_ERR;
	}

	// cleanup
	closesocket(ClientSocket);
	WSACleanup();

	return 0;
}

int __cdecl main(int argc, char** argv)
{
	timeStampPrint("I am server\n");

	timeStampPrint("One\n");
	timeStampPrint("Two\n");
	timeStampPrint("Three\n");
	timeStampPrint("Now!\n");

	Sleep(0);

	for (int i = 0; i < 1; i++)
	{
		app();
	}
}
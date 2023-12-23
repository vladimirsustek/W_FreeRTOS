#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


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

int app(int argc, char** argv)
{
	WSADATA wsaData;
	SOCKET ConnectSocket = INVALID_SOCKET;

	addrinfo* serverAdr = NULL;
	addrinfo serverAdrHints;

	fd_set writeOpConnectFDset;
	fd_set readOpRecvFDset;

	/* Initialized as 10 * 1000us = 1ms */
	timeval nonBlockOpMaxTimeout = { 0,(10 * 1000) };

	/* Huge buffer, may be moved to be a static global */
	char clientRecvBuff[DEFAULT_BUFLEN] = { 0 };
	int sockOpResult;
	int clientRecvBuffSize = DEFAULT_BUFLEN;
	int recentWSAError = -1;

	/* Yes, used to indicate how many times
	must be function call to do desired operation */
	int nonBlockingPollingLoops = 0;

	const char clientMsg[] = "1st message from client\n";

	bool clientConnected = false;

	// Validate the parameters
	if (argc != 2) {
		timeStampPrint("usage: %s client-name\n", argv[0]);
		return APP_ERR;
	}

	// Initialize Winsock
	sockOpResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (sockOpResult != 0) {
		timeStampPrint("WSAStartup failed with error: %d\n", sockOpResult);
		return APP_ERR;
	}

	ZeroMemory(&serverAdrHints, sizeof(serverAdrHints));
	serverAdrHints.ai_family = AF_UNSPEC;
	serverAdrHints.ai_socktype = SOCK_STREAM;
	serverAdrHints.ai_protocol = IPPROTO_TCP;

	while (clientConnected == false)
	{
		// Resolve the server address and port
		sockOpResult = getaddrinfo(argv[1], DEFAULT_PORT, &serverAdrHints, &serverAdr);
		if (sockOpResult != 0) {
			timeStampPrint("getaddrinfo failed with error: %d\n", sockOpResult);
			WSACleanup();
			return APP_ERR;
		}

		// Create a SOCKET for connecting to server
		ConnectSocket = socket(serverAdr->ai_family, serverAdr->ai_socktype,
			serverAdr->ai_protocol);

		socketSetNonBlocking(ConnectSocket);
		FD_ZERO(&writeOpConnectFDset);
		FD_SET(ConnectSocket, &writeOpConnectFDset);

		if (ConnectSocket == INVALID_SOCKET) {
			timeStampPrint("socket create failed with error: %ld\n", WSAGetLastError());
			WSACleanup();
			return APP_ERR;
		}
		recentWSAError = WSAGetLastError();

		sockOpResult = connect(ConnectSocket, serverAdr->ai_addr, serverAdr->ai_addrlen);

		if (recentWSAError != WSAEWOULDBLOCK && recentWSAError != 0)
		{
			timeStampPrint("connect problem occured: %d\n", recentWSAError);
		}

		timeStampPrint("Client is waiting to finish connect operation\n");

		nonBlockingPollingLoops = 1;
		while (1)
		{
			/* Use select to determine the completion of the connection request
				by checking if the socket is writeable. As only one socket is created and
				passed as argument, the result must be 1*/
			if (ONE_SOCKET_AVAILABLE == select(ConnectSocket, 0, &writeOpConnectFDset, 0, &nonBlockOpMaxTimeout))
			{
				timeStampPrint("1 Socket to write is available - connected, loop-ed: %ld [ms]\n", nonBlockingPollingLoops * nonBlockOpMaxTimeout.tv_usec / 1000);
				clientConnected = true;
				break;
			}

			Sleep(10);

			if (nonBlockingPollingLoops < 100)
			{
				nonBlockingPollingLoops++;
			}
			else
			{
				closesocket(ConnectSocket);
				break;
			}
		}
	}

	freeaddrinfo(serverAdr);

	// Send an initial buffer
	sockOpResult = send(ConnectSocket, clientMsg, (int)strlen(clientMsg), 0);
	if (sockOpResult == SOCKET_ERROR) {
		timeStampPrint("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return APP_ERR;
	}

	timeStampPrint("Bytes Sent: %ld\n", sockOpResult);

	// shutdown the connection since no more data will be sent
	sockOpResult = shutdown(ConnectSocket, SD_SEND);
	if (sockOpResult == SOCKET_ERROR) {
		timeStampPrint("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return APP_ERR;
	}

	timeStampPrint("Client is waiting to receive \n");

	// Receive until the peer closes the connection
	do {

		sockOpResult = recv(ConnectSocket, clientRecvBuff, clientRecvBuffSize, 0);
		if (sockOpResult < 0 && WSAGetLastError() == WSAEWOULDBLOCK)
		{
			FD_ZERO(&readOpRecvFDset);
			FD_SET(ConnectSocket, &readOpRecvFDset);
			nonBlockingPollingLoops = 1;
			while (1)
			{
				/* Use select to determine the completion of the receive request
				by checking if the socket is readable. As only one socket is created and
				passed as argument, the result must be 1*/
				if (ONE_SOCKET_AVAILABLE == select(ConnectSocket, &readOpRecvFDset, NULL, 0, &nonBlockOpMaxTimeout))
				{
					sockOpResult = recv(ConnectSocket, clientRecvBuff, clientRecvBuffSize, 0);
					timeStampPrint("1 Socket to read is available - something received, loop-ed: %ld [ms]\n", nonBlockingPollingLoops * nonBlockOpMaxTimeout.tv_usec / 1000);
					break;
				}
				nonBlockingPollingLoops++;
			}

		}
		if (sockOpResult > 0)
		{
			timeStampPrint("Bytes received: %d\n", sockOpResult);
			timeStampPrint("RX: %s", clientRecvBuff);
			memset(clientRecvBuff, 0, clientRecvBuffSize);
		}
		else if (sockOpResult == 0)
			timeStampPrint("Connection closed\n");
		else
			timeStampPrint("recv failed with error: %d\n", WSAGetLastError());

	} while (sockOpResult > 0);

	// cleanup
	closesocket(ConnectSocket);
	WSACleanup();

	return 0;
}

int __cdecl main(int argc, char** argv)
{
	timeStampPrint("I am client\n");

	timeStampPrint("One\n");
	timeStampPrint("Two\n");
	timeStampPrint("Three\n");
	timeStampPrint("Now!\n");

	Sleep(1000);

	for (int i = 0; i < 10; i++)
	{
		app(argc, argv);
		Sleep(100);
	}
}
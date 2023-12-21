#undef UNICODE

#define WIN32_LEAN_AND_MEAN

/* Standard includes. */
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <intrin.h>
#include <assert.h>

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* Standard demo includes */
#include "StaticAllocation.h"

/* Static windows FreeRTOS sub-functions */
#include "static_services.h"

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#include <ws2def.h>

// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#include "portmacro.h"

#define FATAL_ERROR() while(1)

#define SOCKET_OPERATION(exp) if((exp) == SOCKET_ERROR){safePrint("WSA Failed %s line: %d\n", __FILE__, __LINE__); WSACleanup();} __nop()
#define SOCKET_CREATION(exp) if(exp == INVALID_SOCKET) {safePrint("SOCKET Failed %s line: %d\n", __FILE__, __LINE__); WSACleanup();} __nop()

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

void winSockClientTask(void* pvParameters)
{

	vTaskDelay(1000);

	for(;;)
	{
		WSADATA wsaData;
		SOCKET ConnectSocket = INVALID_SOCKET;

		struct addrinfo* serverAdr = NULL;
		struct addrinfo serverAdrHints;

		fd_set writeOpConnectFDset;
		fd_set readOpRecvFDset;

		/* Initialized as 10 * 1000us = 1ms */
		struct timeval nonBlockOpMaxTimeout = { 0,(10 * 1000) };

		/* Huge buffer, may be moved to be a static global */
		char clientRecvBuff[DEFAULT_BUFLEN] = { 0 };
		int sockOpResult;
		int clientRecvBuffSize = DEFAULT_BUFLEN;
		int recentWSAError = -1;

		/* Yes, used to indicate how many times
		must be function call to do desired operation */
		int nonBlockingPollingLoops = 0;

		const char clientMsg[] = "1st message from client\n";

		// Initialize Winsock
		sockOpResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (sockOpResult != 0) {
			printf("WSAStartup failed with error: %d\n", sockOpResult);
			return APP_ERR;
		}

		ZeroMemory(&serverAdrHints, sizeof(serverAdrHints));
		serverAdrHints.ai_family = AF_UNSPEC;
		serverAdrHints.ai_socktype = SOCK_STREAM;
		serverAdrHints.ai_protocol = IPPROTO_TCP;

		// Resolve the server address and port
		sockOpResult = getaddrinfo("localhost", DEFAULT_PORT, &serverAdrHints, &serverAdr);
		if (sockOpResult != 0) {
			printf("getaddrinfo failed with error: %d\n", sockOpResult);
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
			printf("socket failed with error: %ld\n", WSAGetLastError());
			WSACleanup();
			return APP_ERR;
		}

		sockOpResult = connect(ConnectSocket, serverAdr->ai_addr, serverAdr->ai_addrlen);

		recentWSAError = WSAGetLastError();

		if (recentWSAError != WSAEWOULDBLOCK && recentWSAError != 0)
		{
			printf("Other problem occured: %d\n", recentWSAError);
		}

		printf("Client is waiting to finish connect operation\n");

		nonBlockingPollingLoops = 1;
		while (1)
		{
			/* Use select to determine the completion of the connection request
				by checking if the socket is writeable. As only one socket is created and
				passed as argument, the result must be 1*/
			if (ONE_SOCKET_AVAILABLE == select(ConnectSocket, 0, &writeOpConnectFDset, 0, &nonBlockOpMaxTimeout))
			{
				printf("1 Socket to write is available - connected, loop-ed: %ld [ms]\n", nonBlockingPollingLoops * nonBlockOpMaxTimeout.tv_usec / 1000);
				break;
			}

			vTaskDelay(10);

			nonBlockingPollingLoops++;
		}

		freeaddrinfo(serverAdr);

		// Send an initial buffer
		sockOpResult = send(ConnectSocket, clientMsg, (int)strlen(clientMsg), 0);
		if (sockOpResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(ConnectSocket);
			WSACleanup();
			return APP_ERR;
		}

		printf("Bytes Sent: %ld\n", sockOpResult);

		// shutdown the connection since no more data will be sent
		sockOpResult = shutdown(ConnectSocket, SD_SEND);
		if (sockOpResult == SOCKET_ERROR) {
			printf("shutdown failed with error: %d\n", WSAGetLastError());
			closesocket(ConnectSocket);
			WSACleanup();
			return APP_ERR;
		}

		printf("Client is waiting to receive \n");

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
						printf("1 Socket to read is available - something received, loop-ed: %ld [ms]\n", nonBlockingPollingLoops * nonBlockOpMaxTimeout.tv_usec / 1000);
						break;
					}

					vTaskDelay(10);

					nonBlockingPollingLoops++;
				}

			}
			if (sockOpResult > 0)
			{
				printf("Bytes received: %d\n", sockOpResult);
				printf("RX: %s", clientRecvBuff);
				memset(clientRecvBuff, 0, clientRecvBuffSize);
			}
			else if (sockOpResult == 0)
				printf("Connection closed\n");
			else
				printf("recv failed with error: %d\n", WSAGetLastError());

		} while (sockOpResult > 0);
	}

}
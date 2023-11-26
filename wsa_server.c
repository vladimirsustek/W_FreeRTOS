#undef UNICODE

#define WIN32_LEAN_AND_MEAN

/* Standard includes. */
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <intrin.h>

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

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#include "portmacro.h"

void safePrint(const char* format, ...)
{
	portENTER_CRITICAL();
	va_list argptr;
	va_start(argptr, format);
	vfprintf(stdout, format, argptr);
	va_end(argptr);
	portEXIT_CRITICAL();
}

#define FATAL_ERROR() while(1)

#define SOCKET_OPERATION(exp) if((exp) == SOCKET_ERROR){safePrint("WSA Failed %s line: %d\n", __FILE__, __LINE__); WSACleanup();} __nop()
#define SOCKET_CREATION(exp) if(exp == INVALID_SOCKET) {safePrint("SOCKET Failed %s line: %d\n", __FILE__, __LINE__); WSACleanup();} __nop()

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

static void socketSetNonBlocking(SOCKET s)
{
	/* Mode = '1' for non-blocking*/
	u_long mode = 1;
	ioctlsocket(s, FIONBIO, &mode);
}

void winSockServerTask(void* pvParameters)
{
	int iResult;

	struct addrinfo* result = NULL;
	struct addrinfo hints;

	int iSendResult;
	char recvbuf[DEFAULT_BUFLEN] = { 0 };
	int recvbuflen = DEFAULT_BUFLEN;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	for (;;)
	{
		SOCKET ListenSocket = INVALID_SOCKET;
		SOCKET ClientSocket = INVALID_SOCKET;

		SOCKET_OPERATION(getaddrinfo("localhost", DEFAULT_PORT, &hints, &result));
		ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
		SOCKET_CREATION(ListenSocket);
		SOCKET_OPERATION(bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen));
		SOCKET_OPERATION(listen(ListenSocket, SOMAXCONN));
		socketSetNonBlocking(ListenSocket);

		do
		{
			// Accept a client socket (non-blocking mode
			ClientSocket = accept(ListenSocket, NULL, NULL);
			vTaskDelay(100);
		} while (ClientSocket == INVALID_SOCKET);

		// No longer need server socket
		SOCKET_OPERATION(closesocket(ListenSocket));

		// Receive until the peer shuts down the connection
		do {

			iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
			if (iResult > 0) {
				safePrint("Length: %d, Content: %s\n%", iResult, recvbuf);
				iSendResult = send(ClientSocket, recvbuf, iResult, 0);
				SOCKET_OPERATION(iSendResult);
				safePrint("Bytes sent: %d\n", iSendResult);
			}
			else if (iResult == 0)
				safePrint("Connection closing...\n");
			else {
				safePrint("recv failed with error: 0x%x\n", WSAGetLastError());
				FATAL_ERROR();
			}
			vTaskDelay(100);

		} while (iResult > 0);

		// shutdown the connection since we're done
		SOCKET_OPERATION(shutdown(ClientSocket, SD_SEND));
		closesocket(ClientSocket);

	}
}
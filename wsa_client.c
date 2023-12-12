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

// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


#include "portmacro.h"

#define FATAL_ERROR() while(1)

#define SOCKET_OPERATION(exp) if((exp) == SOCKET_ERROR){safePrint("WSA Failed %s line: %d\n", __FILE__, __LINE__); WSACleanup();} __nop()
#define SOCKET_CREATION(exp) if(exp == INVALID_SOCKET) {safePrint("SOCKET Failed %s line: %d\n", __FILE__, __LINE__); WSACleanup();} __nop()

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"
#define MAX_CONNECT_ATTEMPTS 3

static void socketSetNonBlocking(SOCKET s)
{
	/* Mode = '1' for non-blocking*/
	u_long mode = 1;
	ioctlsocket(s, FIONBIO, &mode);
}

void winSockClientTask(void* pvParameters)
{

	vTaskDelay(3000);

	SOCKET ConnectSocket = INVALID_SOCKET;
	struct addrinfo* result = NULL, * ptr = NULL, hints;
	const char* sendbuf = "Message from client\0";
	struct timeval select_timeout = { 0, 100 * 1000 };
	char recvbuf[DEFAULT_BUFLEN];
	int iResult;
	int recvbuflen = DEFAULT_BUFLEN;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	WSADATA wsaData;
	assert(0 == WSAStartup(MAKEWORD(2, 2), &wsaData));

	SOCKET_OPERATION(getaddrinfo("localhost", DEFAULT_PORT, &hints, &result));


	for (;;)
	{
		for (ptr = result; ptr != NULL; ptr = ptr->ai_next)
		{

			portENTER_CRITICAL();
			ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
			//socketSetNonBlocking(ConnectSocket);
			iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);

			if (iResult != 0)
			{
				safePrint("WSA_ERR: %d\n", WSAGetLastError());
			}
			portEXIT_CRITICAL();

			if (iResult == 0)
			{
				break;
			}
		}
		if (ConnectSocket != SOCKET_ERROR && iResult == 0)
		{
			break;
		}
		vTaskDelay(100);

		safePrint("Client connected\n");

		freeaddrinfo(result);

		portENTER_CRITICAL();
		iResult = send(ConnectSocket, sendbuf, (int)strlen(sendbuf), 0);
		portEXIT_CRITICAL();

		safePrint("Client: Bytes Sent %d\n", iResult);
		SOCKET_OPERATION(shutdown(ConnectSocket, SD_SEND));

		do {

			iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
			if (iResult > 0)
				printf("Bytes received: %d\n", iResult);
			else if (iResult == 0)
				printf("Connection closed\n");
			else
				printf("recv failed with error: %d\n", WSAGetLastError());

		} while (iResult > 0);

		closesocket(ConnectSocket);
	}

}
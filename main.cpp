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
void clien_connect(void);


extern "C" {


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

#define INOUT_QUEUE_SIZE_MAX 128

	static StaticTask_t xCheckTask;
	static StackType_t ucTaskStack[configMINIMAL_STACK_SIZE * sizeof(StackType_t)];

	static StaticTask_t xConsoleInTask;
	static StackType_t ucConsoleInTaskStack[configMINIMAL_STACK_SIZE * sizeof(StackType_t)];

	static StaticTask_t xConsoleOutTask;
	static StackType_t ucConsoleOutTaskStack[configMINIMAL_STACK_SIZE * sizeof(StackType_t)];

	static StaticTask_t xWinSockClientTask;
	static StackType_t ucWinSockClientTaskStack[configMINIMAL_STACK_SIZE * sizeof(StackType_t)];

	static StaticTask_t xWinSockServer;
	static StackType_t ucWinSockServertStack[configMINIMAL_STACK_SIZE * sizeof(StackType_t)];

	static StaticQueue_t inoutQueue;
	static QueueHandle_t inoutQueueHandle;
	static char queueStorage[INOUT_QUEUE_SIZE_MAX];


	void prvCheckTask(void* pvParameters);
	void consoleInTask(void* pvParameters);
	void consoleOutTask(void* pvParameters);
	void winSockServerTask(void* pvParameters);
	void winSockClientTask(void* pvParameters);

	/* Force C-calling main (ensures cleaning e.g. WSA) */
	int __cdecl main()
	{
		vStartStaticallyAllocatedTasks();

		clien_connect();

		xTaskCreateStatic(prvCheckTask, "Check", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, ucTaskStack, &xCheckTask);
		xTaskCreateStatic(consoleInTask, "Send", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, ucConsoleInTaskStack, &xConsoleInTask);
		xTaskCreateStatic(consoleOutTask, "Receive", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 3, ucConsoleOutTaskStack, &xConsoleOutTask);
		//xTaskCreateStatic(winSockClientTask, "Client", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 4, ucWinSockClientTaskStack, &xWinSockClientTask);

		inoutQueueHandle = xQueueCreateStatic(sizeof(uint8_t), INOUT_QUEUE_SIZE_MAX, (uint8_t*)queueStorage, &inoutQueue);

		vTaskStartScheduler();

		/* Should be never reached */
		return -1;
	}

	/*-----------------------------------------------------------*/

	void prvCheckTask(void* pvParameters)
	{
		const TickType_t xCycleFrequency = pdMS_TO_TICKS(500);
		/* Just to remove compiler warning. */
		(void)pvParameters;

		for (;; )
		{
			/* Place this task in the blocked state until it is time to run again. */
			vTaskDelay(xCycleFrequency);
			/* Check the tasks that use static allocation are still executing. */
			if (xAreStaticAllocationTasksStillRunning() != pdPASS)
			{
				portENTER_CRITICAL();
				printf("xAreStaticAllocationTasksStillRunning == pdFALSE\r\n");
				__debugbreak();
				while (1);
			}
		}
	}

	void consoleInTask(void* pvParameters)
	{
		(void)pvParameters;

		for (;;)
		{
			static int num = 0;

			char internal_buffer[INOUT_QUEUE_SIZE_MAX] = { 0 };

			portENTER_CRITICAL();
			sprintf(internal_buffer, "Number is: %d\n\0", num++);
			portEXIT_CRITICAL();


			xQueueSend(inoutQueueHandle, internal_buffer, portMAX_DELAY);

			vTaskDelay(pdMS_TO_TICKS(200));
		}
	}

	void consoleOutTask(void* pvParameters)
	{
		(void)pvParameters;

		for (;;)
		{
			char internal_buffer[INOUT_QUEUE_SIZE_MAX] = { 0 };

			if (xQueueReceive(inoutQueueHandle, internal_buffer, portMAX_DELAY) == pdPASS)
			{
				portENTER_CRITICAL();
				printf("%s", internal_buffer);
				portEXIT_CRITICAL();
			}

		}
	}

	void winSockClientTask(void *pvParameters)
	{
		for (;;)
		{
			vTaskDelay(pdMS_TO_TICKS(1000));
		}

	}
}


void clien_connect(void)
{
	WSADATA wsaData;
	SOCKET ConnectSocket = INVALID_SOCKET;
	struct addrinfo* result = NULL,
		* ptr = NULL,
		hints;
	const char* sendbuf = "this is a test";
	char recvbuf[DEFAULT_BUFLEN];
	int iResult;
	int recvbuflen = DEFAULT_BUFLEN;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the server address and port
	iResult = getaddrinfo("localhost", DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
	}

	// Attempt to connect to an address until one succeeds
	for (ptr = result; ptr != NULL;ptr = ptr->ai_next) {

		// Create a SOCKET for connecting to server
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
			ptr->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET) {
			printf("socket failed with error: %ld\n", WSAGetLastError());
			WSACleanup();
		}

		// Connect to server.
		iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			closesocket(ConnectSocket);
			ConnectSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}

	freeaddrinfo(result);

	if (ConnectSocket == INVALID_SOCKET) {
		printf("Unable to connect to server!\n");
		WSACleanup();
	}

	// Send an initial buffer
	iResult = send(ConnectSocket, sendbuf, (int)strlen(sendbuf), 0);
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
	}

	printf("Bytes Sent: %ld\n", iResult);

	// shutdown the connection since no more data will be sent
	iResult = shutdown(ConnectSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
	}

	// Receive until the peer closes the connection
	do {

		iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
		if (iResult > 0)
			printf("Bytes received: %d\n", iResult);
		else if (iResult == 0)
			printf("Connection closed\n");
		else
			printf("recv failed with error: %d\n", WSAGetLastError());

	} while (iResult > 0);

	// cleanup
	closesocket(ConnectSocket);
	WSACleanup();
}

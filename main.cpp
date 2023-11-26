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

#define WSA_OK 0

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

#include <assert.h>

#define INOUT_QUEUE_SIZE_MAX 128

	static StaticTask_t xCheckTask;
	static StackType_t ucTaskStack[configMINIMAL_STACK_SIZE * sizeof(StackType_t)];

	static StaticTask_t xConsoleInTask;
	static StackType_t ucConsoleInTaskStack[configMINIMAL_STACK_SIZE * sizeof(StackType_t)];

	static StaticTask_t xConsoleOutTask;
	static StackType_t ucConsoleOutTaskStack[configMINIMAL_STACK_SIZE * sizeof(StackType_t)];

	static StaticTask_t xWinSockClientTask;
	static StackType_t ucWinSockClientTaskStack[configMINIMAL_STACK_SIZE * sizeof(StackType_t)];

	static StaticTask_t xWinSockServerTask;
	static StackType_t ucWinSockServerStack[configMINIMAL_STACK_SIZE * sizeof(StackType_t)];

	static StaticQueue_t inoutQueue;
	static QueueHandle_t inoutQueueHandle;
	static char queueStorage[INOUT_QUEUE_SIZE_MAX];

    WSADATA wsaData;

	void prvCheckTask(void* pvParameters);
	void consoleInTask(void* pvParameters);
	void consoleOutTask(void* pvParameters);

	extern void winSockServerTask(void* pvParameters);
	void winSockClientTask(void* pvParameters);

	/* Force C-calling main (ensures cleaning e.g. WSA) */
	int __cdecl main()
	{
		vStartStaticallyAllocatedTasks();
        
        // Initialize Winsock
		assert(0 == WSAStartup(MAKEWORD(2, 2), &wsaData));

		xTaskCreateStatic(prvCheckTask, "Check", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, ucTaskStack, &xCheckTask);
		xTaskCreateStatic(consoleInTask, "Send", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, ucConsoleInTaskStack, &xConsoleInTask);
		xTaskCreateStatic(consoleOutTask, "Receive", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 3, ucConsoleOutTaskStack, &xConsoleOutTask);
		xTaskCreateStatic(winSockServerTask, "Server", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 4, ucWinSockServerStack, &xWinSockServerTask);

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
			sprintf(internal_buffer, "Seconds running: %d\n\0", num++);
			portEXIT_CRITICAL();


			xQueueSend(inoutQueueHandle, internal_buffer, portMAX_DELAY);

			vTaskDelay(pdMS_TO_TICKS(500));
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
			/* To be done */
			vTaskDelay(pdMS_TO_TICKS(1000));
		}

	}
}


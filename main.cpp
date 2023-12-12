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

	static StaticTask_t xNotifiedTask;
	static StackType_t ucNotifiedTaskStack[configMINIMAL_STACK_SIZE * sizeof(StackType_t)];

	static StaticTask_t xConsoleInTask;
	static StackType_t ucConsoleInTaskStack[configMINIMAL_STACK_SIZE * sizeof(StackType_t)];

	static StaticTask_t xConsoleOutTask;
	static StackType_t ucConsoleOutTaskStack[configMINIMAL_STACK_SIZE * sizeof(StackType_t)];

	static StaticTask_t xWinSockClientTask;
	static StackType_t ucWinSockClientTaskStack[configMINIMAL_STACK_SIZE * sizeof(StackType_t)];

	static StaticTask_t xWinSockServerTask;
	static StackType_t ucWinSockServerTaskStack[configMINIMAL_STACK_SIZE * sizeof(StackType_t)];

	static StaticQueue_t inoutQueue;
	static QueueHandle_t inoutQueueHandle;
	static char queueStorage[INOUT_QUEUE_SIZE_MAX];

	TaskHandle_t notifiedTaskHandle;

	void prvCheckTask(void* pvParameters);
	void consoleInTask(void* pvParameters);
	void consoleOutTask(void* pvParameters);
	void notifiedTask(void* pvParameters);

	extern void winSockServerTask(void* pvParameters);
	extern void winSockClientTask(void* pvParameters);

	typedef enum {INVALID, SERVER, CLIENT} instance_type_e;

	/* Force C-calling main (ensures cleaning e.g. WSA) */
	int __cdecl main(int argc, char** argv)
	{
		instance_type_e instance_type = INVALID;

		// Validate the parameters
		if (argc != 2) {
			printf("usage: %s server-name\n", argv[0]);
			(void)getchar();
			return 1;
		}
		if (0 == memcmp(argv[1], "Server", strlen("Server")))
		{
			instance_type = SERVER;
		}
		else if (0 == memcmp(argv[1], "Client", strlen("Client")))
		{
			instance_type = CLIENT;
		}
		else
		{
			printf("Single argument \"Client\" or \"Server\" missing\n");
			(void)getchar();
			return -1;
		}

		printf("%s instance running\n", argv[1]);

		vStartStaticallyAllocatedTasks();
        
		xTaskCreateStatic(prvCheckTask, "Check", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, ucTaskStack, &xCheckTask);
		xTaskCreateStatic(consoleInTask, "Send", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, ucConsoleInTaskStack, &xConsoleInTask);
		xTaskCreateStatic(consoleOutTask, "Receive", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 3, ucConsoleOutTaskStack, &xConsoleOutTask);

		notifiedTaskHandle = xTaskCreateStatic(notifiedTask, "Notified", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES-5, ucNotifiedTaskStack, &xNotifiedTask);

		if (instance_type == CLIENT)
		{
			xTaskCreateStatic(winSockClientTask, "Client", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 4, ucWinSockClientTaskStack, &xWinSockClientTask);
		}
		else if (instance_type == SERVER)
		{
			xTaskCreateStatic(winSockServerTask, "Server", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 4, ucWinSockServerTaskStack, &xWinSockServerTask);
		}

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

		static uint32_t value = 0x55555555;

		for (;; )
		{
			/* Place this task in the blocked state until it is time to run again. */
			vTaskDelay(xCycleFrequency);

			xTaskNotify(notifiedTaskHandle, value, eSetValueWithOverwrite);
			/* Check the tasks that use static allocation are still executing. */
			if (xAreStaticAllocationTasksStillRunning() != pdPASS)
			{
				portENTER_CRITICAL();
				safePrint("xAreStaticAllocationTasksStillRunning == pdFALSE\r\n");
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
				safePrint("%s", internal_buffer);
			}

		}
	}

	void notifiedTask(void* pvParameters)
	{
		for (;;)
		{
			uint32_t notifiedTask = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
			safePrint("notifiedTask: 0x%08x\n", notifiedTask);
		}
	}
}


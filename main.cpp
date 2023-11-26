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

	static StaticQueue_t inoutQueue;
	static QueueHandle_t inoutQueueHandle;
	static char queueStorage[INOUT_QUEUE_SIZE_MAX];


	void prvCheckTask(void* pvParameters);
	void consoleInTask(void* pvParameters);
	void consoleOutTask(void* pvParameters);

	int main()
	{
		vStartStaticallyAllocatedTasks();

		xTaskCreateStatic(prvCheckTask, "Check", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, ucTaskStack, &xCheckTask);
		xTaskCreateStatic(consoleInTask, "Send", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, ucConsoleInTaskStack, &xConsoleInTask);
		xTaskCreateStatic(consoleOutTask, "Receive", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 3, ucConsoleOutTaskStack, &xConsoleOutTask);

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

			static char internal_buffer[INOUT_QUEUE_SIZE_MAX] = { 0 };

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
			static char internal_buffer[INOUT_QUEUE_SIZE_MAX] = { 0 };

			if (xQueueReceive(inoutQueueHandle, internal_buffer, portMAX_DELAY) == pdPASS)
			{
				portENTER_CRITICAL();
				printf("%s", internal_buffer);
				portEXIT_CRITICAL();
			}

		}
	}
}

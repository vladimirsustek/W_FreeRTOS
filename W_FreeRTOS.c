
#include <stdio.h>
#include <intrin.h>

/* Standard includes. */
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"

/* Standard demo includes. */
#include "StaticAllocation.h"

#include "static_services.h"


static StaticTask_t xCheckTask;
static StackType_t ucTaskStack[configMINIMAL_STACK_SIZE * sizeof(StackType_t)];

void prvCheckTask(void* pvParameters);

int main()
{
	vStartStaticallyAllocatedTasks();

	xTaskCreateStatic(prvCheckTask, "Check", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, ucTaskStack, &xCheckTask);

	vTaskStartScheduler();

	/* Should be never reached */
}

void prvStartCheckTask(void)
{
	/* Allocate the data structure that will hold the task's TCB.  NOTE:  This is
	declared static so it still exists after this function has returned. */
	static StaticTask_t xCheckTask;

	/* Allocate the stack that will be used by the task.  NOTE:  This is declared
	static so it still exists after this function has returned. */
	static StackType_t ucTaskStack[configMINIMAL_STACK_SIZE * sizeof(StackType_t)];

	/* Create the task, which will use the RAM allocated by the linker to the
	variables declared in this function. */
	xTaskCreateStatic(prvCheckTask, "Check", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, ucTaskStack, &xCheckTask);
}
/*-----------------------------------------------------------*/

void prvCheckTask(void* pvParameters)
{
	const TickType_t xCycleFrequency = pdMS_TO_TICKS(500);
	static char* pcStatusMessage = "No errors";

	/* Just to remove compiler warning. */
	(void)pvParameters;

	for (;; )
	{
		/* Place this task in the blocked state until it is time to run again. */
		vTaskDelay(xCycleFrequency);

		/* Check the tasks that use static allocation are still executing. */
		if (xAreStaticAllocationTasksStillRunning() != pdPASS)
		{
			pcStatusMessage = "Error: Static allocation";
		}

		/* This is the only task that uses stdout so its ok to call printf()
		directly. */
		printf("%s - tick count %d - number of tasks executing %d\r\n",
			pcStatusMessage,
			xTaskGetTickCount(),
			uxTaskGetNumberOfTasks());
	}
}



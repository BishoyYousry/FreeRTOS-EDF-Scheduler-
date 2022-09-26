/*
 * FreeRTOS Kernel V10.2.0
 * Copyright (C) 2019 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */

/* 
	NOTE : Tasks run in system mode and the scheduler runs in Supervisor mode.
	The processor MUST be in supervisor mode when vTaskStartScheduler is 
	called.  The demo applications included in the FreeRTOS.org download switch
	to supervisor mode prior to main being called.  If you are not using one of
	these demo application projects then ensure Supervisor mode is used.
*/


/*
 * Creates all the demo application tasks, then starts the scheduler.  The WEB
 * documentation provides more details of the demo application tasks.
 * 
 * Main.c also creates a task called "Check".  This only executes every three 
 * seconds but has the highest priority so is guaranteed to get processor time.  
 * Its main function is to check that all the other tasks are still operational.
 * Each task (other than the "flash" tasks) maintains a unique count that is 
 * incremented each time the task successfully completes its function.  Should 
 * any error occur within such a task the count is permanently halted.  The 
 * check task inspects the count of each task to ensure it has changed since
 * the last time the check task executed.  If all the count variables have 
 * changed all the tasks are still executing error free, and the check task
 * toggles the onboard LED.  Should any task contain an error at any time 
 * the LED toggle rate will change from 3 seconds to 500ms.
 *
 */

/* Standard includes. */
#include <stdlib.h>
#include <stdio.h>

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "lpc21xx.h"

/* Peripheral includes. */
#include "serial.h"
#include "GPIO.h"
#include "queue.h"

/*-----------------------------------------------------------*/

/* Constants to setup I/O and processor. */
#define mainBUS_CLK_FULL	( ( unsigned char ) 0x01 )

/* Constants for the ComTest demo application tasks. */
#define mainCOM_TEST_BAUD_RATE	( ( unsigned long ) 115200 )

/*Stack Size*/
#define STACK_SIZE										100

/*Tasks Macros*/

#define LOAD_1_SIMULATION_CAPACITY			37000
#define LOAD_2_SIMULATION_CAPACITY			89500	

/*Queue Macros*/
#define QUEUE_LEN						10			/*The queue stores 10 items*/
#define QUEUE_ITEM_SIZE			25			/*The size of the message is 25 byte*/

typedef struct
{
	char messageData[QUEUE_ITEM_SIZE];
	uint16_t messageLen;
}Message;
	

/*Declare Handles*/
QueueHandle_t QueueHandle;
xTaskHandle button1_handle = NULL, button2_handle = NULL, transmit_handle = NULL;
xTaskHandle uart_handle = NULL, load1_handle = NULL, load2_handle = NULL;

int btn1_in = 0, btn1_out = 0, btn1_total = 0;
int btn2_in = 0, btn2_out = 0, btn2_total = 0;
int transmit_in = 0, transmit_out = 0, transmit_total = 0;
int receive_in = 0, receive_out = 0, receive_total = 0;
int load1_in = 0, load1_out = 0, load1_total = 0;
int load2_in = 0, load2_out = 0, load2_total = 0;
int tick_total = 0;
int idle_total = 0;
int totalExecutionTime = 0;
float systemLoad = 0;

/*functions Prototypes*/
static void prvSetupHardware( void );
void Button_1_Monitor(void* btn1_msg);
void Button_2_Monitor(void* btn2_msg);
void Periodic_Transmitter(void* transmitMessage);
void Uart_Receiver(void);
void Load_1_Simulation(void);
void Load_2_Simulation(void);
																																				
/*-----------------------------------------------------------*/
int main( void )
{													
	Message *button1_message, *button2_message, *transmit_message;
	
	prvSetupHardware();

	/*Create Queue*/
	QueueHandle = xQueueCreate( QUEUE_LEN, sizeof(Message*) );
	
//	/* Create Tasks here */
//	xTaskCreate( Button_1_Monitor, "btn1", STACK_SIZE, (void*)&button1_message,
//											1,  &button1_handle );
//	xTaskCreate( Button_2_Monitor, "btn2", STACK_SIZE, (void*)&button2_message,
//											1, &button2_handle );
//	
//	xTaskCreate( Periodic_Transmitter, "TX", STACK_SIZE, (void*)&transmit_message,
//											1, &transmit_handle );
//	
//	xTaskCreate( Uart_Receiver, "RX", STACK_SIZE, NULL,
//											1, &uart_handle );
//	xTaskCreate( Load_1_Simulation, "load1", STACK_SIZE, NULL,
//											1, &load1_handle );
//	
//	xTaskCreate( Load_2_Simulation, "load2", STACK_SIZE, NULL,
//											1, &load2_handle );
	
	
	/*Create Tasks that used with EDF scheduler*/
	xTaskPeriodicCreate( Button_1_Monitor, "btn1", STACK_SIZE, (void*)&button1_message,
											1, &button1_handle, 50 );

	xTaskPeriodicCreate( Button_2_Monitor, "btn2", STACK_SIZE, (void*)&button2_message,
											1, &button2_handle, 50 );
	
	xTaskPeriodicCreate( Periodic_Transmitter, "TX", STACK_SIZE, (void*)&transmit_message,
											1, &transmit_handle, 100 );
	
	xTaskPeriodicCreate( Uart_Receiver, "RX", STACK_SIZE, NULL,
											1, &uart_handle, 20 );

	xTaskPeriodicCreate( Load_1_Simulation, "load1", STACK_SIZE, NULL,
											1, &load1_handle, 10 );
	
	xTaskPeriodicCreate( Load_2_Simulation, "load2", STACK_SIZE, NULL,
											1, &load2_handle, 100 );
	

	vTaskStartScheduler();

	for( ;; );
}
/*-----------------------------------------------------------*/

/* Function to reset timer 1 */
void timer1Reset(void)
{
	T1TCR |= 0x2;
	T1TCR &= ~0x2;
}

/* Function to initialize and start timer 1 */
static void configTimer1(void)
{
	T1PR = 1000;
	T1TCR |= 0x1;
}

static void prvSetupHardware( void )
{
	/* Perform the hardware setup required.  This is minimal as most of the
	setup is managed by the settings in the project file. */

	/* Configure UART */
	xSerialPortInitMinimal(mainCOM_TEST_BAUD_RATE);

	/* Configure GPIO */
	GPIO_init();
	
	/* Config trace timer 1 and read T1TC to get current tick */
	configTimer1();

	/* Setup the peripheral bus to be the same as the PLL output. */
	VPBDIV = mainBUS_CLK_FULL;
}
/*-----------------------------------------------------------*/

void vApplicationTickHook(void)
{
	tick_total++;
	GPIO_write(PORT_1, PIN0, PIN_IS_HIGH);
	GPIO_write(PORT_1, PIN0, PIN_IS_LOW);
}

void vApplicationIdleHook(void)
{
	GPIO_write(PORT_1, PIN7, PIN_IS_HIGH);
	idle_total++;
}

/*-----------------------------------------------------------*/

void Button_1_Monitor(void *btn1_msg)
{
	
	TickType_t xLastWakeTime = xTaskGetTickCount();
	Message *msg = (Message *)btn1_msg;
	pinState_t oldRead = PIN_IS_LOW, newRead;
	vTaskSetApplicationTaskTag(NULL, (void *) 1);
	while(1)
	{
		newRead = GPIO_read(PORT_0, PIN0);
		if( newRead != oldRead )
		{
			if(newRead == PIN_IS_HIGH)
			{
				strcpy(msg->messageData, "btn1 down\n");
				msg->messageLen = (uint8_t) strlen(msg->messageData);
			}
			else
			{
				strcpy(msg->messageData, "btn1 up\n");
				msg->messageLen = (uint8_t) strlen(msg->messageData);
			}
			xQueueSend(QueueHandle,(void *) &msg, portMAX_DELAY);
		}
		oldRead = newRead;
		vTaskDelayUntil(&xLastWakeTime,50);
	}
}

/*-----------------------------------------------------------*/

void Button_2_Monitor(void* btn2_msg)
{
	TickType_t xLastWakeTime = xTaskGetTickCount();
	Message *msg = (Message *)btn2_msg;
	pinState_t oldRead = PIN_IS_LOW, newRead;
	vTaskSetApplicationTaskTag(NULL, (void *) 2);
	while(1)
	{
		newRead = GPIO_read(PORT_0, PIN1);
		if(newRead != oldRead)
		{
			if(newRead == PIN_IS_HIGH)
			{
				strcpy(msg->messageData, "btn2 down\n");
				msg->messageLen = (uint8_t) strlen(msg->messageData);
			}
			else
			{
				strcpy(msg->messageData, "btn2 up\n");
				msg->messageLen = (uint8_t) strlen(msg->messageData);
			}
			xQueueSend(QueueHandle,(void *) &msg, portMAX_DELAY);
		}
		oldRead = newRead;
		vTaskDelayUntil(&xLastWakeTime,50);
	}
}

/*-----------------------------------------------------------*/

void Periodic_Transmitter(void* transmitMessage)
{
	Message *msg = (Message*)transmitMessage;
	TickType_t xLastWakeTime = xTaskGetTickCount();
	vTaskSetApplicationTaskTag(NULL, (void*)3);
	while(1)
	{
		strcpy(msg->messageData, "TX\n");
		msg->messageLen = (uint16_t)strlen(msg->messageData);
		xQueueSend(QueueHandle, (void*)&msg, portMAX_DELAY);
		vTaskDelayUntil(&xLastWakeTime, 100);
	}
}
/*-----------------------------------------------------------*/

void Uart_Receiver(void)
{
	TickType_t xLastWakeTime = xTaskGetTickCount();
	Message *receivedMessage;
	vTaskSetApplicationTaskTag(NULL, (void*) 4);
	while(1)
	{
		xQueueReceive(QueueHandle, (void*) &receivedMessage, portMAX_DELAY);
		vSerialPutString( receivedMessage->messageData, receivedMessage->messageLen );
		vTaskDelayUntil(&xLastWakeTime, 20);
	}
}
/*-----------------------------------------------------------*/

void Load_1_Simulation(void)
{
	int32_t cycles;
	TickType_t xLastWakeTime = xTaskGetTickCount();
	vTaskSetApplicationTaskTag( NULL, (void *) 5 );
	while(1)
	{
		for(cycles = 0; cycles < LOAD_1_SIMULATION_CAPACITY; cycles++);		/*5ms capacity*/
		vTaskDelayUntil(&xLastWakeTime, 10);
	}
}

/*-----------------------------------------------------------*/

void Load_2_Simulation(void)
{
	int32_t cycles;
	TickType_t xLastWakeTime = xTaskGetTickCount();
	vTaskSetApplicationTaskTag( NULL, (void *) 6 );
	while(1)
	{
		for(cycles = 0; cycles < LOAD_2_SIMULATION_CAPACITY; cycles++);		/*12ms capacity*/
		vTaskDelayUntil(&xLastWakeTime, 100);
	}
}





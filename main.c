#include <string.h>
#include <time.h>
#include <conio.h>
/* FreeRTOS.org includes. */
#include "include/FreeRTOS.h"
#include "include/task.h"
#include "include/queue.h"
#include "include/semphr.h"
#include "include/supporting_functions.h"


#define MAX_ITEMS_IN_EVENTS_QUEUE 12 //Max items a single queue can hold
#define NUM_TYPES_OF_EVENTS 4   //Police,Fire,Ambulance,Corona(Utility vehicle)
#define NUM_CONCURRENT_POLICE_TASKS 3 //Maxmium Num of concurrent police cars
#define NUM_CONCURRENT_FIRETRUCK_TASKS 2 //Maxmium Num of concurrent firetrucks
#define NUM_CONCURRENT_AMBULANCE_TASKS 4 //Maxmium Num of concurrent ambulances
#define NUM_CONCURRENT_UTILITY_VEHICLE_TASKS 4 //Maxmium Num of concurrent utility vehicles
#define MAX_VEHICLES_FOR_SINGLE_EVENT 8 //maximum number of vehicles that can be generated per event
#define MIN_VEHICLES_FOR_SINGLE_EVENT 1

/*Event Struct
	e_code = Event code (0=Police, 1=Firetruck, 2=Ambulance, 3=Utility vehicle)
	num= number of emergency car needed
*/

typedef struct Event
{
	int e_code;
	int num;
}t_event;

/*Event Car_Handle
	task_handle = Hold task handle
	car_num= no. of the car
*/

typedef struct Car_Handle
{
	xTaskHandle task_handle;
	int car_num;
}t_car_handle;

/*Shared Resources*/

//Tasks list
void vEventsGeneratorTask(void* pvParameters);
void vDispatcherTask(void* pvParameters);

void vPoliceTask(void* pvParameters);
void vFiretruckTask(void* pvParameters);
void vAmbulanceTask(void* pvParameters);
void vUtilityVechicleTask(void* pvParameters);


void vPolice_Car(t_car_handle* pvCarHandle);
void vFiretruckCar(t_car_handle* pvCarHandle);
void vAmbulanceCar(t_car_handle* pvCarHandle);
void vUtilityVehicle(t_car_handle* pvCarHandle);

//Queues handler
QueueHandle_t events_queue = NULL;

QueueHandle_t police_queue = NULL;
QueueHandle_t firetruck_queue = NULL;
QueueHandle_t ambulance_queue = NULL;
QueueHandle_t corona_queue = NULL;


//Tasks handler
TaskHandle_t xPoliceTaskHandle=NULL;
TaskHandle_t xAmbulanceTaskHandle=NULL;
TaskHandle_t xFiretruckTaskHandle=NULL;
TaskHandle_t xUtilityTaskHandle=NULL;

//Semaphores
SemaphoreHandle_t xPoliceSemaphore;
SemaphoreHandle_t xFiretruckSemaphore;
SemaphoreHandle_t xAmbulanceSemaphore;
SemaphoreHandle_t xCoronaSemaphore;

/*-----------------------------------------------------------*/

int main(void)
{

	//Instantiate events queue , MAX_ITEMS_IN_EVENTS_QUEUE=hold the maximum number of items in the queue , each item size== size of t_event
	events_queue = xQueueCreate(MAX_ITEMS_IN_EVENTS_QUEUE, sizeof(t_event));
	//Instantiate police queue\firetruct_queue\ambulance_queue\corona_queue , MAX_ITEMS_IN_EVENS_QUEUE=hold the maximum number of items in the queue , each item size== size of t_event
	police_queue = xQueueCreate(MAX_ITEMS_IN_EVENTS_QUEUE, sizeof(t_event));
	firetruck_queue = xQueueCreate(MAX_ITEMS_IN_EVENTS_QUEUE, sizeof(t_event));
	ambulance_queue = xQueueCreate(MAX_ITEMS_IN_EVENTS_QUEUE, sizeof(t_event));
	corona_queue = xQueueCreate(MAX_ITEMS_IN_EVENTS_QUEUE, sizeof(t_event));


	if (events_queue == NULL || police_queue==NULL || firetruck_queue==NULL || ambulance_queue==NULL || corona_queue==NULL)
	{
		//err-queue could not be created
		printf("err in creating one or more of the queues\n");
		return 1;
	}
	
	xPoliceSemaphore = xSemaphoreCreateCounting(NUM_CONCURRENT_POLICE_TASKS, NUM_CONCURRENT_POLICE_TASKS);
	xFiretruckSemaphore = xSemaphoreCreateCounting(NUM_CONCURRENT_FIRETRUCK_TASKS, NUM_CONCURRENT_FIRETRUCK_TASKS);
	xAmbulanceSemaphore = xSemaphoreCreateCounting(NUM_CONCURRENT_AMBULANCE_TASKS, NUM_CONCURRENT_AMBULANCE_TASKS);
	xCoronaSemaphore = xSemaphoreCreateCounting(NUM_CONCURRENT_UTILITY_VEHICLE_TASKS, NUM_CONCURRENT_UTILITY_VEHICLE_TASKS);
	
	if (xPoliceSemaphore == NULL || xFiretruckSemaphore==NULL || xAmbulanceSemaphore==NULL || xCoronaSemaphore==NULL)
	{
		//err-one or more of the semaphores could not created
		printf("err- one or more of the semaphores could not created \n");
		return 1;
	}

	//Create task the generate event every const interval
	xTaskCreate(vEventsGeneratorTask, "Task Events Generator", 1000, NULL, tskIDLE_PRIORITY + 1, NULL);
	xTaskCreate(vDispatcherTask, "Task Dispatcher", 1000, NULL, tskIDLE_PRIORITY+1, NULL);

	xTaskCreate(vPoliceTask, "Police Task", 1000, NULL, tskIDLE_PRIORITY + 1, &xPoliceTaskHandle);
	xTaskCreate(vFiretruckTask, "Firetruck Task", 1000, NULL, tskIDLE_PRIORITY + 1, &xFiretruckTaskHandle);
	xTaskCreate(vAmbulanceTask, "Ambulance Task", 1000, NULL, tskIDLE_PRIORITY + 1, &xAmbulanceTaskHandle);
	xTaskCreate(vUtilityVechicleTask, "Utility Task", 1000, NULL, tskIDLE_PRIORITY + 1, &xUtilityTaskHandle);
	
	//Suspend police,ambulance,firetruck and utility tasks
	vTaskSuspend(xPoliceTaskHandle);
	vTaskSuspend(xFiretruckTaskHandle);
	vTaskSuspend(xAmbulanceTaskHandle);
	vTaskSuspend(xUtilityTaskHandle);

	/* Start the scheduler to start the tasks executing. */
	vTaskStartScheduler();

	for (;; );
	return 0;
}
/*-----------------------------------------------------------*/

void vEventsGeneratorTask(void* pvParameters)
{
	//reset time for the rand() function
	srand(time(NULL));
	//2.5 seconds(converted to ticks)
	const TickType_t time_delay = pdMS_TO_TICKS(2500UL);
	//Will return if queue is able to accept more messages
	BaseType_t xStatus = 0;
	//Flag that indicates the message can still sent, default 1=true , message can still be send
	int send_flag = 1;
	//Instantiate event param
	t_event ev_t = { 0,0 };
	int num = 0;

	for (;; )
	{

		if (send_flag)
		{
			//queue is not full
			//generate new event
			
			//event code - 0=police,1=firetruck,2=ambulance,3=utility vehicle
			ev_t.e_code = rand() % NUM_TYPES_OF_EVENTS;
			//generate number of vehicles needed (between 1 and 8)
			ev_t.num = ((rand() % MAX_VEHICLES_FOR_SINGLE_EVENT) + MIN_VEHICLES_FOR_SINGLE_EVENT);

			xStatus = xQueueSendToBack(events_queue, &ev_t, 100UL);
			if (xStatus == pdPASS)
			{
				//queue is not full - message sent
				//Message sent , flag ==true
				send_flag = 1;
			}
			else
			{
				//queue is full
				//Message(event) did not sent , flag ==false
				//Do not generate new event num (Do not destroy old value of num)
				send_flag = 0;
			}

			//Send event task every 2 seconds(converted to ticks)
			vTaskDelay(time_delay);
		}
	}
}


void vDispatcherTask(void* pvParameters)
{
	//0.8 seconds(converted to ticks)
	const TickType_t time_delay = pdMS_TO_TICKS(800UL);
	//Will return if queue is able to accept more messages
	BaseType_t xStatus=0;
	BaseType_t xStatus_Send=0;

	//Instantiate event param
	t_event ev_t = { 0,0 };
	int received_code;
	

	for (;; )
	{
		xStatus = xQueueReceive(events_queue, &ev_t, 0);
		if (xStatus == pdPASS)
		{
			//Data was successfully received
			switch (ev_t.e_code)
			{
				case 0: //Police
					//1.Pass message(event) to police queue
					//2.Unblock Police task
					xStatus_Send== xQueueSendToBack(police_queue, &ev_t, 100UL);
					vTaskResume(xPoliceTaskHandle);
					break;
				case 1: //Firetruck
					//1.Pass message(event) to firetruck queue
					//2.Unblock Firetruck task
					xStatus_Send == xQueueSendToBack(firetruck_queue, &ev_t, 100UL);
					vTaskResume(xFiretruckTaskHandle);
					break;
				case 2: //Ambulance
					//1.Pass message(event) to ambulance_queue
					//2.Unblock Ambulance task
					xStatus_Send == xQueueSendToBack(ambulance_queue, &ev_t, 100UL);
					vTaskResume(xAmbulanceTaskHandle);
					break;
				default: //Corona=Utility Vehicle
					//1.Pass message(event) to corona_queue
					//2.Unblock Utility task
					xStatus_Send == xQueueSendToBack(corona_queue, &ev_t, 100UL);
					vTaskResume(xUtilityTaskHandle);
					break;
			}
			
		}
		else
		{
			//No Data in queue
		}
		//Read events from events_queue every 2 seconds(converted to ticks)
		vTaskDelay(time_delay);
	}
}

void vPoliceTask(void* pvParameters)
{
	//1 second(converted to ticks)
	const TickType_t time_delay = pdMS_TO_TICKS(1000UL);
	//Will return if queue is able to accept more messages
	BaseType_t xStatus = 0;

	//Instantiate event param
	t_event ev_t = { 0,0 };

	//current police car waiting
	int police_cars_counter = 0;
	//num of police car needed (read from message)
	int police_car_needed = 0;

	for (;; )
	{
		//Block this task until next time (and if queue is empty)
		if (uxQueueMessagesWaiting(police_queue) == 0)
		{
			vTaskSuspend(xPoliceTaskHandle);
		}
		
		//Read message(event) from police_queue
		xStatus = xQueueReceive(police_queue, &ev_t, 100);
		if (xStatus == pdPASS)
		{
			police_car_needed = ev_t.num;
			police_cars_counter = 0;
			
			while (police_cars_counter<police_car_needed)
			{
				//Get key
				if (xSemaphoreTake(xPoliceSemaphore, 100))
				{
					//Semaphore key took
					xTaskHandle task_handle = NULL;
					police_cars_counter++;
					//make a struct that holds both the car index and task handle
					t_car_handle police_car_task_handle= { task_handle,police_cars_counter };
					xTaskCreate(vPolice_Car, "Police Car", 1000, &police_car_task_handle, tskIDLE_PRIORITY + 4, &task_handle);
				}
				else
				{
					//no semaphores left
					//try again next iteration
					
				}
				vTaskDelay(time_delay);
			}
			

			//All police cars have been sent
			
		}
		else
		{
			//No message(event) is waiting in the police queue
		}
		//Read events from events_queue every 2 seconds(converted to ticks)
		vTaskDelay(time_delay);
	}
}


void vFiretruckTask(void* pvParameters)
{
	//1 second(converted to ticks)
	const TickType_t time_delay = pdMS_TO_TICKS(1000UL);
	//Will return if queue is able to accept more messages
	BaseType_t xStatus = 0;

	//Instantiate event param
	t_event ev_t = { 0,0 };

	//current firetrucks waiting
	int firetrucks_cars_counter = 0;
	//num of firetrucks needed (read from message)
	int firetrucks_car_needed = 0;
	
	for (;; )
	{
		//Block this task until next time (and if queue is empty)
		if (uxQueueMessagesWaiting(firetruck_queue) == 0)
		{
			vTaskSuspend(xFiretruckTaskHandle);
		}

		//Read message(event) from firetruck_queue
		xStatus = xQueueReceive(firetruck_queue, &ev_t, 100);
		if (xStatus == pdPASS)
		{
			firetrucks_car_needed = ev_t.num;
			firetrucks_cars_counter = 0;
			
			while (firetrucks_cars_counter < firetrucks_car_needed)
			{
				//Get key
				if (xSemaphoreTake(xFiretruckSemaphore, 100))
				{
					//Semaphore key took
					xTaskHandle task_handle = NULL;
					firetrucks_cars_counter++;
					//make a struct that holds both the car index and task handle
					t_car_handle firetrucks_car_task_handle = { task_handle,firetrucks_cars_counter };
					xTaskCreate(vFiretruckCar, "Firetruck Car", 1000, &firetrucks_car_task_handle, tskIDLE_PRIORITY + 4, &task_handle);
				}
				else
				{
					//no semaphores left
					//try again next iteration
					
				}
				vTaskDelay(time_delay);
			}


			//All firetrucks have been sent

		}
		else
		{
			//No message(event) is waiting in the firetruck_queue
		}
		//Read events from events_queue every 2 seconds(converted to ticks)
		vTaskDelay(time_delay);
	}
}


void vAmbulanceTask(void* pvParameters)
{
	//1 second(converted to ticks)
	const TickType_t time_delay = pdMS_TO_TICKS(1000UL);
	//Will return if queue is able to accept more messages
	BaseType_t xStatus = 0;

	//Instantiate event param
	t_event ev_t = { 0,0 };

	//current ambulance car waiting
	int ambulance_car_counter = 0;
	//num of ambulance cars needed (read from message)
	int ambulance_cars_needed = 0;

	for (;; )
	{
		//Block this task until next time (and if queue is empty)
		if (uxQueueMessagesWaiting(ambulance_queue) == 0)
		{
			vTaskSuspend(xAmbulanceTaskHandle);
		}

		//Read message(event) from ambulance_queue
		xStatus = xQueueReceive(ambulance_queue, &ev_t, 100);
		if (xStatus == pdPASS)
		{
			ambulance_cars_needed = ev_t.num;
			ambulance_car_counter = 0;
			
			while (ambulance_car_counter < ambulance_cars_needed)
			{
				//Get key
				if (xSemaphoreTake(xAmbulanceSemaphore, 100))
				{
					//Semaphore key took
					xTaskHandle task_handle = NULL;
					ambulance_car_counter++;
					//make a struct that holds both the car index and task handle
					t_car_handle ambulance_car_task_handle = { task_handle,ambulance_car_counter };
					xTaskCreate(vAmbulanceCar, "Ambulance Car", 1000, &ambulance_car_task_handle, tskIDLE_PRIORITY + 4, &task_handle);
				}
				else
				{
					//no semaphores left
					//try again next iteration
					
				}
				vTaskDelay(time_delay);
			}


			//All ambulance cars have been sent

		}
		else
		{
			//No message(event) is waiting in the ambulance_queue
		}
		//Read events from events_queue every 2 seconds(converted to ticks)
		vTaskDelay(time_delay);
	}
}

void vUtilityVechicleTask(void* pvParameters)
{
	//1 second(converted to ticks)
	const TickType_t time_delay = pdMS_TO_TICKS(1000UL);
	//Will return if queue is able to accept more messages
	BaseType_t xStatus = 0;

	//Instantiate event param
	t_event ev_t = { 0,0 };

	//current utility vehicle waiting
	int utility_vehicle_counter = 0;
	//num of utility vehicles needed (read from message)
	int utility_vehicles_needed = 0;

	for (;; )
	{
		//Block this task until next time (and if queue is empty)
		if (uxQueueMessagesWaiting(corona_queue) == 0)
		{
			vTaskSuspend(xUtilityTaskHandle);
		}

		//Read message(event) from corona_queue
		xStatus = xQueueReceive(corona_queue, &ev_t, 100);
		if (xStatus == pdPASS)
		{
			utility_vehicles_needed = ev_t.num;
			utility_vehicle_counter = 0;
			
			while (utility_vehicle_counter < utility_vehicles_needed)
			{
				//Get key
				if (xSemaphoreTake(xCoronaSemaphore, 100))
				{
					//Semaphore key took
					xTaskHandle task_handle = NULL;
					utility_vehicle_counter++;
					//make a struct that holds both the car index and task handle
					t_car_handle utility_vehicle_task_handle = { task_handle,utility_vehicle_counter };
					xTaskCreate(vUtilityVehicle, "Utility Vehicle", 1000, &utility_vehicle_task_handle, tskIDLE_PRIORITY + 4, &task_handle);
				}
				else
				{
					//no semaphores left
					//try again next iteration
					
				}
				vTaskDelay(time_delay);
			}


			//All utility vehicles have been sent

		}
		else
		{
			//No message(event) is waiting in the corona_queue
		}
		//Read events from events_queue every 2 seconds(converted to ticks)
		vTaskDelay(time_delay);
	}
}

void vPolice_Car(t_car_handle* pvCarHandle)
{
	//5 seconds(converted to ticks)
	const TickType_t time_delay = pdMS_TO_TICKS(5000UL);
	t_car_handle car_handle = *pvCarHandle;

	//Police car sent - print message
	printf("Police No.%d was sent.\r\n", car_handle.car_num);
	//Wait
	vTaskDelay(time_delay);
	//Police car returned - print message
	printf("Police No.%d has returned.\r\n", car_handle.car_num);
	//Release police car semaphore
	xSemaphoreGive(xPoliceSemaphore);
	//Release task's resources
	vTaskDelete(car_handle.task_handle);

}

void vFiretruckCar(t_car_handle* pvCarHandle)
{
	//3 seconds(converted to ticks)
	const TickType_t time_delay = pdMS_TO_TICKS(3000UL);
	t_car_handle car_handle = *pvCarHandle;

	//Firetruck sent - print message
	printf("Firetruck No.%d was sent.\r\n", car_handle.car_num);
	//Wait
	vTaskDelay(time_delay);
	//Firetruck returned - print message
	printf("Firetruck No.%d has returned.\r\n", car_handle.car_num);
	//Release Firetruck semaphore
	xSemaphoreGive(xFiretruckSemaphore);
	//Release task's resources
	vTaskDelete(car_handle.task_handle);

}

void vAmbulanceCar(t_car_handle* pvCarHandle)
{
	//4 seconds(converted to ticks)
	const TickType_t time_delay = pdMS_TO_TICKS(4000UL);
	t_car_handle car_handle = *pvCarHandle;

	//Ambulance car sent - print message
	printf("Ambulance No.%d was sent.\r\n", car_handle.car_num);
	//Wait
	vTaskDelay(time_delay);
	//Ambulance car returned - print message
	printf("Ambulance No.%d has returned.\r\n", car_handle.car_num);
	//Release Ambulance car semaphore
	xSemaphoreGive(xAmbulanceSemaphore);
	//Release task's resources
	vTaskDelete(car_handle.task_handle);

}

void vUtilityVehicle(t_car_handle* pvCarHandle)
{
	//3 seconds(converted to ticks)
	const TickType_t time_delay = pdMS_TO_TICKS(3000UL);
	t_car_handle car_handle = *pvCarHandle;

	//Utility vehicle sent - print message
	printf("Utility vehicle (City) No.%d was sent.\r\n", car_handle.car_num);
	//Wait
	vTaskDelay(time_delay);
	//Utility vehicle returned - print message
	printf("Utility vehicle (City) No.%d has returned.\r\n", car_handle.car_num);
	//Release Utility vehicle semaphore
	xSemaphoreGive(xCoronaSemaphore);
	//Release task's resources
	vTaskDelete(car_handle.task_handle);

}

/*-----------------------------------------------------------*/




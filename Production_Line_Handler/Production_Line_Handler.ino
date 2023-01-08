/**********************************************************************************************
Authors:
*** Mohamed Ahmed Mohanna   Mohamedmohanna504@gmail.com
*** Mahmoud Ahmed Elsheikh  Mahmoud.elsheikh@outlook.sa 
*** Eslam Elhusseini        ihussini2002@gmail.com

Description:
Controlling a Production Line that consists of three main subsystems (Feeding,Sorting,Storage)
Real Time Operating System while communicating with another arduino via ethernet communication
**/
#include "Arduino_FreeRTOS.h"
#include "semphr.h"
#include <EtherCard.h>
/*******************************************************************************************/
//Arrays of Positions
unsigned long long metalsteps[2][4] = {{15000,15000,26200,26200},{6700,12300,12300,6700}};
unsigned long long plasticsteps[2][4] = {{15000,15000,26200,26200},{5700,11000,11000,5700}};
/*******************************************************************************************/
/* Setting Addresses of LAN Netwotk Node */
// mac address
static byte mymac[] = { 0x1A,0x2B,0x3C,0x4D,0x5E,0x6F };
// ethernet interface ip address
static byte myip[] = { 192, 168, 1, 2 };
// gateway ip address
static byte gwip[] = { 192, 168, 1, 1 };
// subnet mask
static byte mask[] = { 255, 255, 255, 0 };
// destination ip-address
static byte dstIp[] = { 192, 168, 1, 3 };
// ports
const int dstPort PROGMEM = 1234;
const int srcPort PROGMEM = 4321;
/* Buffer and timer */
byte Ethernet::buffer[700];
static uint32_t timer;  // dummy incrementor that'll later be used for delays
/*******************************************************************************************/
//Global Variables
bool metal; //Variable Used to Determine The Type of Material
int metalcount = 0; 
int plasticcount = 0;
/*******************************************************************************************/
//Defining Handlers
TaskHandle_t HomingxHandler; 
TaskHandle_t HomingyHandler; 
TaskHandle_t HomingCheckHandler; 
TaskHandle_t Adjustyhandler;
TaskHandle_t SortHandler;
/*******************************************************************************************/
//Defining Semaphores
SemaphoreHandle_t FeedingSemaphore_extend;
SemaphoreHandle_t FeedingSemaphore_retract;
SemaphoreHandle_t HomingxSemphr;
SemaphoreHandle_t HomingySemphr;
SemaphoreHandle_t AdjustySemphr;
SemaphoreHandle_t SortSemphr;
SemaphoreHandle_t z_axis_suction_Semphr;
SemaphoreHandle_t feedingreadSemphr;
SemaphoreHandle_t metalsmphr;
SemaphoreHandle_t plasticsmphr;
SemaphoreHandle_t z_release_Semphr;
/*******************************************************************************************/
//Defining Pins
#define xenable 7      //Enable Pin of TB660 Driver
#define xdir 4         //Direction Pin of TB660 Driver
#define xstep 3        //Pulse Pin of TB660 Driver
#define ydir 17        //Direction Pin of A4988 Driver 
#define ystep 18       //Pulse Pin of A4988 Driver
#define IRF 19         //E18-D80NK Infrared Proximity Sensor Used To check if there is a product in the magazine
#define IRP 20         //FC51 InfraRed proximity used to determine if product reached the product stopper
#define xhoming 16     //X_Axis Opto coupler
#define yhoming 21     //Y_Axis Opto coupler
#define materialsort 8 //LJC18A3-H-Z/BX PNP Inductive Proximity Sensor
#define DCV_Feeding 14 //5/2 Feeding DCV
#define zDCV 2         //5/2 Z_axis DCV
#define suction 15     //2/2 Suction DCV
/*******************************************************************************************/
//Defining Neccessary Macros
#define ADJUST_STEPS 11500   //Number of Steps to Intially Adjust the Y_Axis
#define X_axis 0
#define Y_axis 1
#define ON 0
#define OFF 1
#define Home 0
#define Away 1
void setup() 
{
/*******************************************************************************************/
  //Intiallizing Pins
  pinMode(IRP,INPUT);
  pinMode(yhoming,INPUT);
  pinMode(materialsort,INPUT);
  pinMode(IRF,INPUT);
  pinMode(xhoming,INPUT);
  pinMode(DCV_Feeding,OUTPUT);
  pinMode(xenable,OUTPUT);
  pinMode(xdir,OUTPUT);
  pinMode(xstep,OUTPUT);
  pinMode(ydir,OUTPUT);
  pinMode(ystep,OUTPUT);
  pinMode(zDCV,OUTPUT);
  pinMode(suction,OUTPUT);
/*******************************************************************************************/
  //Setting Starting Conditions
  digitalWrite(xenable,ON);
  digitalWrite(zDCV,OFF);
  digitalWrite(DCV_Feeding,OFF);
  digitalWrite(suction,OFF);
/*******************************************************************************************/
  Serial.begin(9600);//For Debugging Purposes
/*******************************************************************************************/
  //Creating Semaphores
  FeedingSemaphore_extend = xSemaphoreCreateBinary();
  FeedingSemaphore_retract = xSemaphoreCreateBinary();
  HomingxSemphr = xSemaphoreCreateBinary();
  HomingySemphr = xSemaphoreCreateBinary();
  AdjustySemphr = xSemaphoreCreateBinary();
  SortSemphr = xSemaphoreCreateBinary();
  z_axis_suction_Semphr = xSemaphoreCreateBinary();
  feedingreadSemphr = xSemaphoreCreateBinary();
  metalsmphr = xSemaphoreCreateBinary();
  plasticsmphr = xSemaphoreCreateBinary();
  z_release_Semphr = xSemaphoreCreateBinary();
/*******************************************************************************************/
  //Creating Tasks
  xTaskCreate(Feedingread, "Feeding Read", 100, NULL, 1, NULL);
  xTaskCreate(Homingcheck, "HomingCheck", 100, NULL, 1, NULL);
  xTaskCreate(Feedingmove_retract, "Feeding Retraction", 100, NULL, 1, NULL);
  xTaskCreate(Sorting,"Sorting",100 ,NULL , 1, &SortHandler);
  xTaskCreate(Feedingmove_extend, "Feeding Extension", 100, NULL, 2, NULL);
  xTaskCreate(z_suction,"z_suction", 100, NULL, 2, NULL);
  xTaskCreate(movemetal,"movemetal", 100, NULL, 2, NULL);
  xTaskCreate(moveplastic,"moveplastic", 100, NULL, 2, NULL);
  xTaskCreate(z_release,"z_release", 100, NULL, 2, NULL);
  xTaskCreate(Adjusty, "Adjusty" , 100, NULL, 3, &Adjustyhandler);
  xTaskCreate(Homingx, "Homingx" , 100, NULL,4, &HomingxHandler);
  xTaskCreate(Homingy, "Homingy" , 100, NULL, 4, &HomingyHandler);
/*******************************************************************************************/
//EtherNet Intiallization 
// Begin Ethernet communication
  while ((ether.begin(sizeof Ethernet::buffer, mymac, SS) == 1)) {}
// Setup static IP address
  ether.staticSetup(myip, gwip, 0, mask);
}
void loop() {}
/*******************************************************************************************/
// Note That Each Number that is sent via ethernet determines the state of the system
/*******************************************************************************************/
// Function to move x_axis stepper motor
void xmove(unsigned long long steps, int dir)
{
  digitalWrite(xdir, dir);
  for(int i = 0; i < steps; i++)
  {
    digitalWrite(xstep,1);
    delayMicroseconds(300);
    digitalWrite(xstep,0);
    delayMicroseconds(300);
  }
}
/*******************************************************************************************/
// Function to move y_axis stepper motor
void ymove(unsigned long long steps, int dir)
{
  digitalWrite(ydir, dir);
  for(int i = 0; i < steps; i++)
  {
    digitalWrite(ystep,1);
    delayMicroseconds(400);
    digitalWrite(ystep,0);
    delayMicroseconds(400);
  }
}
/*******************************************************************************************/
//Function to check if the Cartesion Robot is set to its intial position
void Homingcheck(void* pvParameters)
{
  while(1)
  {
    int x = digitalRead(xhoming);
    int y = digitalRead(yhoming);
    if(x == 0)
    {
      xSemaphoreGive(HomingxSemphr);
    }
    else if(y == 0)
    {
      xSemaphoreGive(HomingySemphr);
    }
    else if(y == 1 && x == 1)
    {
      xSemaphoreGive(AdjustySemphr);
      vTaskDelete(HomingyHandler);
      vTaskSuspend(HomingxHandler);
      vTaskDelete(HomingCheckHandler);
 
    }
  }
}
/*******************************************************************************************/
//Moving X_axis Motor to its intial Position
void Homingx (void* pvParameters)
{
  while(1)
  {
    xSemaphoreTake(HomingxSemphr,portMAX_DELAY);
    ether.sendUdp("1", sizeof("1"), srcPort, dstIp, dstPort);
    xmove(50,Home);
  }
}
/*******************************************************************************************/
//Moving y_axis Motor to its intial Position
void Homingy (void* pvParameters)
{
  while(1)
  {
    xSemaphoreTake(HomingySemphr,portMAX_DELAY);
    ether.sendUdp("2", sizeof("2"), srcPort, dstIp, dstPort);
    ymove(50, Home);
  }
}
/*******************************************************************************************/
//Adjusting The Y_axis to the middle after each Cycle
void Adjusty (void* pvParameters)
{
  while(1)
  {
    xSemaphoreTake(AdjustySemphr,portMAX_DELAY);
    ether.sendUdp("3", sizeof("3"), srcPort, dstIp, dstPort);
    Serial.println("Adjust");
    if(metalcount == 0 && plasticcount == 0) //At The Start
    { 
      ymove(ADJUST_STEPS, Away);
    }
    else if (metal == 1) // After Placing A Metal Product
    {
      int x = digitalRead(xhoming);
      while(x == 0)
      {
      xmove(50,Home);
      x = digitalRead(xhoming);
      }
      ymove(metalsteps[Y_axis][metalcount - 1], Home);
    }
    else if (metal == 0) // After Placing A Plastic Product
    {     
      int x = digitalRead(xhoming);
      while(x == 0)
      {
        xmove(50,Home);
        x = digitalRead(xhoming);
      }
      ymove(plasticsteps[Y_axis][plasticcount - 1], Away);
    }
  
   ether.sendUdp("4", sizeof("4"), srcPort, dstIp, dstPort);
   xSemaphoreGive(feedingreadSemphr);
   vTaskResume(SortHandler);
  }
}
/*******************************************************************************************/
// Function to Determine whether to Feed a Product or Not
void Feedingread( void* pvParameters)
{ 
  while(1)
  {
    xSemaphoreTake(feedingreadSemphr,portMAX_DELAY);
    int proximity = digitalRead(IRP);
    int productstopper = digitalRead(IRF);
    if ( proximity == 0 && productstopper == 1 )
    {
      xSemaphoreGive(FeedingSemaphore_extend);
    }
    else
    {
      xSemaphoreGive(FeedingSemaphore_retract);
    }
  }
}
/*******************************************************************************************/
// Function to Open the Feeding DCV
void Feedingmove_extend( void* pvParameters)
{
  while(1)
  {
    xSemaphoreTake(FeedingSemaphore_extend,portMAX_DELAY);
    vTaskDelay(30);
    digitalWrite(DCV_Feeding,0);
    xSemaphoreGive(feedingreadSemphr);
  }
}
/*******************************************************************************************/
// Function to Close the Feeding DCV
void Feedingmove_retract( void* pvParameters)
{
  while(1)
  {
    xSemaphoreTake(FeedingSemaphore_retract,portMAX_DELAY);
    vTaskDelay(20);
    digitalWrite(DCV_Feeding,1);
    xSemaphoreGive(feedingreadSemphr);
    int productstopper = digitalRead(IRF);
    if(productstopper == 0)
    {
        xSemaphoreGive(SortSemphr);
    }
  }
}
/*******************************************************************************************/
// Function to Determine the Material of the Product
void Sorting(void * pvParameters)
{
  while(1)
  {
    xSemaphoreTake(SortSemphr,portMAX_DELAY);
    ether.sendUdp("5", sizeof("5"), srcPort, dstIp, dstPort);
    Serial.println("sort");
    int productstopper = digitalRead(IRF);
    if(productstopper == 0)
    {
      int material = digitalRead(materialsort);
      if(material == 0)
      {
        Serial.println("Plastic");
        metal = 0;
      }
      else
      {
        Serial.println("Metal");
        metal = 1;
      }
      xSemaphoreGive(z_axis_suction_Semphr);
      vTaskSuspend(NULL);
    }
  }} 
/*******************************************************************************************/
/* Function to Take the product from product stopper then depending on the material type
*  It will send a semaphor to one of the two functions responsible for the actuation of 
*  the X and Y axes
*/
void z_suction(void * pvParameters)
{
  while(1)
  {
    xSemaphoreTake(z_axis_suction_Semphr,portMAX_DELAY);
    Serial.println("Suction");
    digitalWrite(suction,0);
    digitalWrite(zDCV,0);
    delay(3000);
    digitalWrite(zDCV,1);
    delay(2000);
    if(metal == 0 && plasticcount < 4)
    {
      xSemaphoreGive(plasticsmphr);
    }
    else if(metal == 1 && metalcount < 4)
    { 
      xSemaphoreGive(metalsmphr);
    }  
  }
}
/*******************************************************************************************/
/* Function to move the X and Y axes if the Product is plastic where it fetch the number 
*  of steps required to move the X&Y axes from a 2D array where the element number to be
*  fetched is determined depending on the count of the product
*/
void moveplastic(void * pvParameters)
{
  while(1)
  {
    xSemaphoreTake(plasticsmphr,portMAX_DELAY); 
    ether.sendUdp("6", sizeof("6"), srcPort, dstIp, dstPort);
    vTaskDelay(160);
    plasticcount++;
    xmove(plasticsteps[X_axis][plasticcount - 1], Away);
    ymove(plasticsteps[Y_axis][plasticcount - 1], Home);
    xSemaphoreGive(z_release_Semphr);
  }
}
/*******************************************************************************************/
/* Function to move the X and Y axes if the Product is metal where it fetch the number 
*  of steps required to move the X&Y axes from a 2D array where the element number to be
*  fetched is determined depending on the count of the product
*/
void movemetal(void * pvParameters)
{
  while(1)
  {
    xSemaphoreTake(metalsmphr,portMAX_DELAY); 
    ether.sendUdp("7", sizeof("7"), srcPort, dstIp, dstPort);
    vTaskDelay(160);
    metalcount++;
    xmove(metalsteps[X_axis][metalcount - 1], Away);
    ymove(metalsteps[Y_axis][metalcount - 1],Away);
    xSemaphoreGive(z_release_Semphr);
  }
}
/*******************************************************************************************/
//Function to Place the Product in the storage Area
void z_release(void * pvParameters)
{
  while(1)
  {
    xSemaphoreTake(z_release_Semphr,portMAX_DELAY);
    ether.sendUdp("8", sizeof("8"), srcPort, dstIp, dstPort);
    Serial.println("release");
    digitalWrite(zDCV,0);
    delay(3000);
    digitalWrite(suction,1);
    vTaskDelay(10);
    digitalWrite(zDCV,1); 
    delay(1500);
    xSemaphoreGive(AdjustySemphr);
  }
}
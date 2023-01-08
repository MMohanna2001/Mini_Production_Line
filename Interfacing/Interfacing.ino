/**********************************************************************************************
Authors:
*** Mohamed Ahmed Mohanna   Mohamedmohanna504@gmail.com
*** Mahmoud Ahmed Elsheikh  Mahmoud.elsheikh@outlook.sa 
*** Eslam Elhusseini        ihussini2002@gmail.com

Description:
Receiving The String Sent from The main Controller then dispalying the states 
and product numbers on two LCDs
**/
#include <EtherCard.h>
#include <IPAddress.h>
#include <LiquidCrystal.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
/***************************************************************************************/
// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd2(0x27,16,2);
LiquidCrystal lcd1(A0, A1, A2, 9, 8, 7, 6, 5, 4, 3, 2);
int plastic = 0;
int metal = 0;
/***************************************************************************************/
/* Setting Addresses of LAN Netwotk Node */
// mac address
static byte mymac[] = { 0x70,0x69,0x69,0x2D,0x30,0x31 };
// ethernet interface ip address
static byte myip[] = { 192, 168, 1, 3 };
// gateway ip address
static byte gwip[] = { 192, 168, 1, 1 };
// subnet mask
static byte mask[] = { 255, 255, 255, 0 };
// ports
const int dstPort PROGMEM = 1234;
/* Buffer */
byte Ethernet::buffer[50];
/***************************************************************************************/
//Function to Reset Arduino
void(* resetFunc) (void) = 0;
void setup(){
  // Initialize Serial
  Serial.begin(9600);
/***************************************************************************************/
//LCDs Intiallization
  lcd1.begin(16,2);
  lcd1.clear();
  lcd2.begin();
  // Turn on the blacklight and print a message.
  lcd2.backlight();
  lcd2.setCursor(0,0);
  lcd2.print("Team 11");
  lcd2.setCursor(0,1);
  lcd2.print("Starting System");
/***************************************************************************************/
  // Initialize Ethernet
  initializeEthernet();
  //register udpSerialPrint() to destination port (callback function)
  ether.udpServerListenOnPort(&udpSerialPrint, dstPort);
}
void loop(){
  // receive packets, get ready for callbacks
  delay(1500);
  ether.packetLoop(ether.packetReceive());
}
void initializeEthernet()
{
  // Begin Ethernet communication
  if (ether.begin(sizeof Ethernet::buffer, mymac, SS) == 0)
  {
    Serial.println("Failed to access Ethernet controller");
    return;
  }
  // Setup static IP address
  ether.staticSetup(myip, gwip, 0, mask);
  // Log configuration
  Serial.println(F("\n[Receiver]"));
  ether.printIp("IP:  ", ether.myip);
  ether.printIp("GW:  ", ether.gwip);
}
/* callback that prints received packets to the serial port & Print on The LCDS */
void udpSerialPrint(uint16_t dest_port, uint8_t src_ip[IP_LEN], uint16_t src_port, const char *data, uint16_t len){
  IPAddress src(src_ip[0],src_ip[1],src_ip[2],src_ip[3]);

  Serial.println(data);
  
  if( data[0] == '1'){
    lcd1.setCursor(0,0);
    lcd1.print("HOMING_X        ");
    delay(150);
    resetFunc();
  }
  else if( data[0] == '2'){
    lcd1.setCursor(0,0);
    lcd1.print("HOMING_Y        ");
    delay(150);
     resetFunc(); 
  }
  else if( data[0] == '3'){
    lcd1.setCursor(0,0);
    lcd1.print("ADJUST ROBOT        ");
    lcd1.setCursor(0,1);
    lcd1.print("        ");
  }
  else if( data[0] == '4'){
    lcd1.setCursor(0,0);
    lcd1.print("FEEDING        ");
    lcd1.setCursor(0,1);
    lcd1.print("        ");
    delay(1500);    
  }
  else if( data[0] == '5'){
    lcd1.setCursor(0,0);
    lcd1.print("SORTING        ");
    lcd1.setCursor(0,1);
    lcd1.print("SUCTION        ");
  }
  else if( data[0] == '6'){
    lcd1.setCursor(0,0);
    lcd1.print("PACKEGING         ");
    lcd1.setCursor(0,1);
    lcd1.print("PLASTIC        ");    
    plastic++;
    lcd2.setCursor(0,0);
    lcd2.print("Plastic   Metal");
    lcd2.setCursor(0,1);
    lcd2.print(plastic);
    lcd2.print("         ");
    lcd2.print(metal);
    lcd2.print("              ");
  }
  else if( data[0] == '7'){
    lcd1.setCursor(0,0);
    lcd1.print("PACKEGING         ");
    lcd1.setCursor(0,1);
    lcd1.print("METAL        ");
    metal++;
    lcd2.setCursor(0,0);
    lcd2.print("Plastic   Metal");
    lcd2.setCursor(0,1);
    lcd2.print(plastic);
    lcd2.print("         ");
    lcd2.print(metal);
    lcd2.print("              ");
  }
  else if( data[0] == '8'){
    lcd1.setCursor(0,0);
    lcd1.print("RELEASING        ");  
  }
}
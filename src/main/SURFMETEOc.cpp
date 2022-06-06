#include <SPI.h>
#include <Ethernet.h>
#include "TimerOne.h"
#include <math.h>
#include "BME680.h"

#define Bucket_Size 0.01 // bucket size to trigger tip count
#define RG11_Pin 3 // digital pin RG11 connected to
#define TX_Pin 8 // used to indicate web data tx
#define WaterSensor_Pin (A5) // analog pin for water temperature sensor

#define WindSensor_Pin (2) // digital pin for wind speed sensor
#define WindVane_Pin (A2) // analog pin for wind direction sensor
#define VaneOffset 0 // define the offset for caclulating wind direction

volatile unsigned long tipCount; // bucket tip counter used in interrupt routine
volatile unsigned long contactTime; // Timer to manage any contact bounce in interrupt routine

volatile bool isSampleRequired; // this is set every 2.5sec to generate wind speed
volatile unsigned int timerCount; // used to count ticks for 2.5sec timer count
volatile unsigned long rotations; // cup rotation counter for wind speed calcs
volatile unsigned long contactBounceTime; // timer to avoid contact bounce in wind speed sensor
volatile float windSpeed;
volatile float totalRainfall; // total amount of rainfall detected

bool txState; // current led state for tx rx led
int vaneValue; // raw analog value from wind vane
int vaneDirection; // translated 0 - 360 wind direction
int calDirection; // calibrated direction after offset applied
int lastDirValue; // last recorded direction value

BME680 bme; // I2C using address 0x77

// Here we setup the web server. We are using a static ip address and a mac address
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 45);
EthernetServer server(80); // create a server listing on 192.168.1.45 port 80

void setup() {

  txState = HIGH;

  // setup rain sensor values
tipCount = 0;
totalRainfall = 0;

// setup wind sensor values
lastDirValue = 0;
rotations = 0;
isSampleRequired = false;

// setup water temp sensor
  
  
// setup timer values
timerCount = 0;
  

// start the Ethernet connection and server
Ethernet.begin(mac, ip);
server.begin();

bme.readSensor();

Serial.begin(9600)
Serial.println("BME Temp\tPressure\tRainfall\tSpeed\tDirection");

if (!bme.begin()) {
Serial.println("Could not find BME680 sensor, check wiring");
while (1);
}
pinMode(TX_Pin, OUTPUT);
pinMode(RG11_Pin, INPUT);
pinMode(WindSensor_Pin, INPUT);
attachInterrupt(digitalPinToInterrupt(RG11_Pin), isr_rg, FALLING);
attachInterrupt(digitalPinToInterrupt(WindSensor_Pin), isr_rotation, FALLING);

// setup the timer for 0.5 second
Timer1.initialize(500000);
Timer1.attachInterrupt(isr_timer);

sei();// Enable Interrupts
}

void loop() {
/ listen for incoming clients
EthernetClient client = server.available();
if (client) {
// an http request ends with a blank line
boolean currentLineIsBlank = true;
while (client.connected()) {
if (client.available()) {
char c = client.read();
Serial.write(c);
// if you've gotten to the end of the line (received a newline
// character) and the line is blank, the http request has ended,
// so you can send a reply
if (c == '\n' && currentLineIsBlank) {
// send a standard http response header
digitalWrite(TX_Pin,HIGH);
client.println("HTTP/1.1 200 OK");
client.println("Content-Type: text/html");
client.println("Connection: close"); // connection closed completion of response
client.println("Refresh: 10"); // refresh the page automatically every 5 sec
client.println();
client.println("<!DOCTYPE HTML>");
client.println("<html><body>");
digitalWrite(TX_Pin,HIGH); // Turn the TX LED on
client.print("<span style=\"font-size: 26px\";><br>  Temperatura: ");
client.print(ds.getTemperature_C());
client.println(" °C<br>");
client.print("%<br>  Cisnienie: ");
client.print(bme.getPressure_MB());
client.println(" mb%<br>");
client.print("%<br>  Predkosc wiatru: ");
client.print(windSpeed);
client.println(" mph<br>");
getWindDirection();
client.print("%<br>  Kierunek wiatru: ");
client.print(calDirection);
client.println(" °<br>");
client.print(getHeading);
client.println(" <br>");
client.print("%<br>  Opady: ");
client.print(totalRainfall);
client.println(" mm</span>");
client.println("</body></html>");
digitalWrite(TX_Pin,LOW); // Turn the TX LED off
break;
}
if (c == '\n') {
// you're starting a new line
currentLineIsBlank = true;
} else if (c != '\r') {
// you've gotten a character on the current line
currentLineIsBlank = false;
}
}
}
}

// give the web browser time to receive the data
delay(1);
// close the connection:
client.stop();

bme.readSensor();

if(isSampleRequired) {

getWindDirection();


Serial.print(bme.getTemperature_C()); Serial.print(" *C\t");
Serial.print(bme.getPressure_MB()); Serial.print(" mb\t");
Serial.print(totalRainfall); Serial.print(" mm\t\t");
Serial.print(windSpeed); Serial.print(" mph\t");
Serial.print(calDirection); 
getHeading(CalDirection);
isSampleRequired = false;
}
}

// Interrupt handler routine for timer interrupt
void isr_timer() {

timerCount++;

if(timerCount == 5) {
// convert to mp/h using the formula V=P(2.25/T)
// V = P(2.25/2.5) = P * 0.9
windSpeed = rotations * 0.9;
rotations = 0;
txState = !txState; // toggle the led state
digitalWrite(TX_Pin,txState);
isSampleRequired = true;
timerCount = 0;
}
}

// Interrupt handler routine to increment the rotation count for wind speed
void isr_rotation() {

if((millis() - contactBounceTime) > 15 ) { // debounce the switch contact
rotations++;
contactBounceTime = millis();
}
}

// Interrupt handler routine that is triggered when the rg-11 detects rain
void isr_rg() {

if((millis() - contactTime) > 15 ) { // debounce of sensor signal
tipCount++;
totalRainfall = tipCount * Bucket_Size;
contactTime = millis();
}
}

// Get Wind Direction
void getWindDirection() {

vaneValue = analogRead(WindVane_Pin);
vaneDirection = map(vaneValue, 0, 1023, 0, 359);
calDirection = vaneDirection + VaneOffset;

if(calDirection > 360)
calDirection = calDirection - 360;

if(calDirection > 360)
calDirection = calDirection - 360;
}
// Converts compass direction to heading
void getHeading(int direction) {

if(direction < 22)
Serial.print(" N");
else if (direction < 67)
Serial.print(" NE");
else if (direction < 112)
Serial.print(" E");
else if (direction < 157)
Serial.print(" SE");
else if (direction < 212)
Serial.print(" S");
else if (direction < 247)
Serial.print(" SW");
else if (direction < 292)
Serial.print(" W");
else if (direction < 337)
Serial.print(" NW");
else
Serial.print(" N");
}

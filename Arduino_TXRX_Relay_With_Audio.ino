/******************************************************************************************
 * TX/RX Relay
 * By Chris Dusseau - KE6FGY - CETsr motoapx1@gmail dot com
 * This will allow you to connect to a radio transmitter and program time intervals
 * for TX and RX. This will allow you to burn in a radio or test it for intermittent issues
 * when you dont have time to sit and key the microphone all day long.
 * 
 * The base of this code was provided by  Mishaux and can be found here 
 * https://github.com/Mishaux/ArduinoTimer
 * 
 * I have modified it to suit my needs and as such is free for anyone else to modify or
 * use as needed. Just give credit where it is due to both Mishaux anc myself.
 * 
 * 
 * 2/25/2016
 ******************************************************************************************/

// These are the libraries needed
#include <EEPROM.h>
#include <SPI.h> 
#include <Adafruit_GFX.h>      // Core graphics library
#include <Adafruit_ST7735.h>   // Hardware-specific library



// For the breakout, you can use any 2 or 3 pins
// These pins will also work for the 1.8" TFT shield
#define TFT_CS     10   // CS
#define TFT_RST    8    // RST
#define TFT_DC     9    // D/C

// Option 1 (recommended): must use the hardware SPI pins
// (for UNO thats sclk = 13 and sid = 11) and pin 10 must be
// an output. This is much faster - also required if you want
// to use the microSD card (see the image drawing example)
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS,  TFT_DC, TFT_RST);


// These define the pins to which one leg of the buttons should be attached.
// The other leg should be attached to the Arduino's ground.
int OffIntervalUpButton = 1;
int OffIntervalDownButton = 2;
int OnIntervalUpButton = 3;
int OnIntervalDownButton = 4;
int TXButton = 5;
int ResetButton = 6;

// One leg of the output relay should be atached to this pin.
// The other to the Arduino's ground.
int TXRelay = 7;

// Pin defines for mp3 sound
int Sound = 15;

// This is for a future upgrade, This will allow the Arduino to sense when the radio is receicing
// a signal and stop the timer untill the received signal goes away, This will prevent the unit 
// feom keying up on another station
const int analogInPin = A0;  // Analog input pin that the potentiometer is attached to
const int analogOutPin = 9; // Analog output pin that the LED is attached to

int sensorValue = 0;        // value read from the pot
int outputValue = 0;        // value output to the PWM (analog out)


// I don't want to lose my interval settings when the arduino loses power, so we use the Arduino's
// built in EEPROM memory cache to store settings when they are changed. These set the memory addresses
// for the off and on interval settings
int EEPROM_OFF_INTERVAL_ADDRESS = 0;
int EEPROM_ON_INTERVAL_ADDRESS = 1;

// These set how much the interval changes when you press the up and down buttons
// Values in milliseconds
long OffIntervalSteps = 60000;  
long OnIntervalSteps = 1000;

// If you hold down the up or down buttons, the interval will count up or down until you let go
// This sets the pause time between changes as you hold doun the button. Value in millisaconds.
int ButtonPressWait = 250;

// In order to make the up/down buttons count continuously up or down with a pause between jumps,
// we need a timer buffer to remember how long ago the last jump was.
long OffIntervalUpBuffer;
long OffIntervalDownBuffer;
long OnIntervalUpBuffer;
long OnIntervalDownBuffer;

// These are the interval variables
long LastTX;
long OffTime;
long OnTime;

void setup(void) {
  Serial.begin(9600);
  
  // initialize a ST7735S chip, GREEN tab
  tft.initR(INITR_144GREENTAB);   
     
  // Enable this code and push program once on the first run of a new board or to move EEPROM registers.
  // Then disable and push the program to the board again for normal operation.
  // EEPROM.write(EEPROM_OFF_INTERVAL_ADDRESS, 1);
  // EEPROM.write(EEPROM_ON_INTERVAL_ADDRESS, 10);
  
  // This grabs the stored intervals from the EEPROM cache on startup
  OffTime = EEPROM.read(EEPROM_OFF_INTERVAL_ADDRESS) * 60000; 
  OnTime = EEPROM.read(EEPROM_ON_INTERVAL_ADDRESS) * 1000;

  // Set the Pins functions 
  pinMode(OffIntervalUpButton, INPUT);
  pinMode(OffIntervalDownButton, INPUT);
  pinMode(OnIntervalUpButton, INPUT);
  pinMode(OnIntervalDownButton, INPUT);
  pinMode(ResetButton, INPUT);
  pinMode(TXButton, INPUT);
  pinMode(TXRelay, OUTPUT);
  pinMode(Sound, OUTPUT);
  
  // This enables the Arduino's built in pull up resistors on these pins to make
  // wiring pushbuttons much easier.
  digitalWrite(OffIntervalUpButton, HIGH);
  digitalWrite(OffIntervalDownButton, HIGH);
  digitalWrite(OnIntervalUpButton, HIGH);
  digitalWrite(OnIntervalDownButton, HIGH);
  digitalWrite(ResetButton, HIGH);
  digitalWrite(TXButton, HIGH);   
  digitalWrite(Sound, HIGH); 

  // Set the relay
  digitalWrite(TXRelay, LOW);
   
  // Lets clear the screen
  tft.setTextWrap(false);
  tft.fillScreen(ST7735_BLACK);

  // Set the boot message if you want one
  tft.setTextColor(ST7735_GREEN);    
  tft.setTextSize(2);               
  tft.setCursor(45, 20);             
  tft.print("CMD");                  
  tft.setTextSize(2);                
  tft.setCursor(5, 40);              
  tft.print("Technology");           
  tft.setTextSize(1);                
  tft.setCursor(40, 70);             
  tft.print("Firmware");             
  tft.setCursor(30, 90);             
  tft.print("Version 1.2");          
  delay(2000);

  //set the title screen - this appears second after the boot up screen
  tft.fillScreen(ST7735_BLACK);
  tft.setTextColor(ST7735_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(0, 0);
  tft.print("PROGRAMABLE");
  tft.setTextColor(ST7735_RED);
  tft.setTextSize(3);
  tft.setCursor(45, 30);
  tft.print("TX");
  tft.setTextColor(ST7735_GREEN);
  tft.setTextSize(3);
  tft.setCursor(45, 55);
  tft.print("RX");
  tft.setTextColor(ST7735_WHITE);
  tft.setTextSize(2);
  tft.setCursor(35, 100);
  tft.print("RELAY");
  delay(2000); 

  // Lets clear the screen  and start this baby up!!
  tft.fillScreen(ST7735_BLACK);
  
 // Initilize last TX time buffer, set it to right now.
  LastTX = millis();
}

void loop(void) {
  
  // Write the UI out to the display
  // Where time is displayed after the text, add spaces so the screen will clear out the text field
  // so things display properly. Add enough spaces to compensate for times that contain multiple digits
  tft.setTextColor(ST7735_WHITE,ST7735_BLACK);
  tft.setTextSize(1);
  tft.setCursor(37, 0);
  tft.print("Relay Time");
  tft.setTextColor(ST7735_GREEN,ST7735_BLACK);
  tft.setCursor(0, 20);
  tft.print(" RX: ");             
  tft.print(OffTime / 1000 / 60);
  tft.print("m ");
  tft.setCursor(0, 40);
  tft.print(" TX: ");
  tft.print(OnTime / 1000);
  tft.print("s ");
  tft.setCursor(0, 60);
  tft.print("Next TX: ");
  tft.print(((LastTX + OffTime) -  millis()) / 1000 / 60);
  tft.print("m ");
  tft.print((((LastTX + OffTime) -  millis()) / 1000 ) % 60);
  tft.print("s         ");
  
  // Now we loop watching for pressed buttons
  
  if (digitalRead(ResetButton) == LOW) {
    LastTX = millis();
  }
  // This section will ignor the timer and key the radio and play the audio file when the TXButton is pressed
  // This will alos reset the timer countdown 
  if (digitalRead(TXButton) == HIGH) { //SET back to low is using normaly a open switch
    digitalWrite(TXRelay, HIGH);

  //play the audio file
  digitalWrite(Sound, LOW);
  delay(100);
  digitalWrite(Sound, HIGH); 

  //  clear the screen with all RED
  tft.fillScreen(ST7735_RED);

long TXBuffer = millis() + OnTime;
  while (millis() < TXBuffer) {

    // Display text that transmitter is on and count down remaining time for action
    tft.setTextColor(ST7735_WHITE,ST7735_RED);
    tft.setTextSize(3);
    tft.setCursor(20, 50);
    tft.print("TX ON");
    tft.setCursor(50, 100);
    tft.print((TXBuffer -  millis()) / 1000 );
    tft.print("s        ");
  }

   //  clear screen  and resum normal operation
   tft.fillScreen(ST7735_BLACK);
   LastTX = millis();

  }
  else {
    digitalWrite(TXRelay, LOW);
  }
  
  if (digitalRead(OffIntervalUpButton) == LOW) {
    OffIntervalUp();
  }
  
  if (digitalRead(OffIntervalDownButton) == LOW) {
    OffIntervalDown();
  }
  
  if (digitalRead(OnIntervalUpButton) == LOW) {
    OnIntervalUp();
  }
  
  if (digitalRead(OnIntervalDownButton) == LOW) {
    OnIntervalDown();
  }
  
  if (LastTX + OffTime < millis()) {
    Transmit();
    LastTX = millis();
  }
}

void Transmit(void) {tft.fillScreen(ST7735_RED);
  digitalWrite(Sound, LOW);
  delay(100);
  digitalWrite(Sound, HIGH); 
  digitalWrite(TXRelay, HIGH);
   
   
  long TXBuffer = millis() + OnTime;
  while (millis() < TXBuffer) {
    
    tft.setTextColor(ST7735_WHITE,ST7735_RED);
    tft.setTextSize(3);
    tft.setCursor(20, 50);
    tft.print("TX ON");
    tft.setCursor(50, 100);
    tft.print((TXBuffer -  millis()) / 1000 );
    tft.print("s        ");
     
  }
   
    tft.fillScreen(ST7735_BLACK);   
    digitalWrite(TXRelay, LOW);
    
}

// Do the hold down for continuous change loop, and write new value to the EEPROM
void OffIntervalUp(void) {
  if (OffTime < 14400000) { //change back to 14400000
    if (OffIntervalUpBuffer + ButtonPressWait < millis()) {
      OffTime += OffIntervalSteps;
      EEPROM.write(EEPROM_OFF_INTERVAL_ADDRESS, OffTime / 60000);
      OffIntervalUpBuffer = millis();
    }
  }
}

// Do the hold down for continuous change loop, and write new value to the EEPROM
void OffIntervalDown(void) {
  if (OffTime > 60000) {  //change back to 60000
    if (OffIntervalDownBuffer + ButtonPressWait < millis()) {
      OffTime -= OffIntervalSteps;
      EEPROM.write(EEPROM_OFF_INTERVAL_ADDRESS, OffTime / 60000);
      OffIntervalDownBuffer = millis();
    }
  }
}

// Do the hold down for continuous change loop, and write new value to the EEPROM
void OnIntervalUp(void) {
  if (OnTime < 900000) {
    if (OnIntervalUpBuffer + ButtonPressWait < millis()) {
      OnTime += OnIntervalSteps;
      EEPROM.write(EEPROM_ON_INTERVAL_ADDRESS, OnTime / 1000);
      OnIntervalUpBuffer = millis();
    }
  }
}

// Do the hold down for continuous change loop, and write new value to the EEPROM
void OnIntervalDown(void) {
  if (OnTime > 1000) {
   if (OnIntervalDownBuffer + ButtonPressWait < millis()) {
      OnTime -= OnIntervalSteps;
      EEPROM.write(EEPROM_ON_INTERVAL_ADDRESS, OnTime / 1000);
      OnIntervalDownBuffer = millis();
    }
  }
}

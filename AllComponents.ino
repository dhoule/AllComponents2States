#include <Wire.h>
#include <LiquidCrystal.h>
#include "RTClib.h"


// Configuration values for reservoir:
#define SERIES_RESISTOR     560    // Value of the series resistor in ohms.    
#define SENSOR_PIN          A4     // Analog pin which is connected to the sensor. 
#define ZERO_VOLUME_RESISTANCE    1997.50    // Resistance value (in ohms) when no liquid is present.
#define LOW_LEVEL_RESISTANCE      1600.00 //~3.5 inches
#define TOP_LEVEL_RESISTANCE    664.10 //10 inches    // Resistance value (in ohms) when liquid is at max line.

// RTC connections:
// SDA to Arduino pin 20(Mega)/4(Uno)
// SCL to Arduino pin 21(Mega)/5(Uno)
RTC_DS1307 RTC;
// LCD Connections:
// rs (LCD pin 4) to Arduino pin 12
// rw (LCD pin 5) to Arduino pin 11
// enable (LCD pin 6) to Arduino pin 10
// LCD pin 15 to Arduino pin 13
// LCD pins d4, d5, d6, d7 to Arduino pins 9, 8, 7, 6
LiquidCrystal lcd(12, 11, 10, 9, 8, 7, 6);
// These arrays hold the inputs that will be used by the moisture senor(s)
int inputs1[] = {A0, A1}, inputs2[] = {A2, A3};
// relayPin: Pin 2 used by the Relay
// backLight: pin 13 will control the backlight of LCD
// ct: The size of 'inputs'
// soil: The mapped value of 'avg'. Range is [0, 100]
// relayOn: Value of 'soil' that will set the state of the system to ON
// relayOff: Value of 'soil' that will set the sate of the system to OFF
// systemState1: 1 = soil is wet enough. 0 = soil is too dry.
// systemState2: 1 = soil is wet enough. 0 = soil is too dry.
// buzzerPin: Pin 4 used by the piezo buzzer
int relayPin1 = 2, relayPin2 = 3, backLight = 13, ct = (sizeof(inputs1)/sizeof(*inputs1)), soil = 0, relayOn = 10, 
    relayOff = 90, systemState1, systemState2, buzzerPin = 4;
// value: Accumulator for the values of the moisture sensers
// avg: The average of the reading from the moisture sensors
double value = 0.0, avg = 0.0;
// now: Used to display the timestamp of state changes
DateTime now;

/*
  Function clears a single row on the LCD with 20 empty spaces.
*/
boolean clear_row(int row){
  lcd.setCursor(0,row);
  lcd.print("                    "); 
}

/*
  Function changes the message to match the state of the system and the timestamp.
*/
boolean change_message (char *msg, int layer) {
  if(layer == -42){
    for(int i = 0; i < 4; i++){ clear_row(i); }
    lcd.setCursor(0,1);
    lcd.print(msg);
    write_date_time(2);
  }
  else{
    clear_row(layer);
    lcd.setCursor(0,layer);
    lcd.print(msg);
    write_date_time(layer + 1);
  }
}

/*
  Function displays the current time as a timestamp on the 3rd row of the LCD.
*/
boolean write_date_time(int layer) {
  clear_row(layer);
  now = RTC.now();
  //We print the day
  lcd.setCursor(0,layer);
  lcd.print(now.day(), DEC);
  lcd.setCursor(2,layer);
  lcd.print('/');
  lcd.setCursor(3,layer);
  //We print the month
  int month = now.month();
  if(month > 9){
    lcd.print(now.month(), DEC);
  }
  else{
    lcd.print(0);
    lcd.setCursor(4,layer);
    lcd.print(now.month(), DEC);
  }
  lcd.setCursor(5,layer);
  lcd.print('/');
  lcd.setCursor(6,layer);
  //We print the year
  lcd.print(now.year(), DEC);
  lcd.setCursor(10,layer);
  lcd.print(' ');
  lcd.setCursor(11,layer);
  //We print the hour
  int hour = now.hour();
  if(hour > 9){
    lcd.print(hour, DEC);
  }
  else{
    lcd.print(0);
    lcd.setCursor(12,layer);
    lcd.print(hour, DEC);
  }  
  lcd.setCursor(13,layer);
  //We print the minutes
  lcd.print(now.minute(), DEC);
  lcd.setCursor(15,layer);
}

float readResistance(int pin, int seriesResistance) {
  // Get ADC value.
  float resistance = analogRead(pin);
  // Convert ADC reading to resistance.
  resistance = (1023.0 / resistance) - 1.0;
  resistance = seriesResistance / resistance;
  return resistance;
}

void set_off_buzzer(){
  while( !( readResistance(SENSOR_PIN, SERIES_RESISTOR) < LOW_LEVEL_RESISTANCE ) ){
    if(now.second() % 2 == 0){
      digitalWrite(buzzerPin, HIGH);
      delay(100); 
      digitalWrite(buzzerPin, LOW);
      delay(500); 
    }
  }  
}

void moisture_sensors(int test){
  // set/reset accumulator
  value = 0.0;
  // 3.3V Air = sensorValue of 674
  // 3.3V Straight, filtered water = sensorValue of 324
  // 5V Air = sensorValue of 1023
  // 5V Straight, filtered water = sensorValue of 420
  if(test == 0){
    for(int i = 0; i < ct; i++){ value += constrain(analogRead(inputs1[i]), 420, 1023); }
  }
  else{
    for(int i = 0; i < ct; i++){ value += constrain(analogRead(inputs2[i]), 420, 1023); }
  }  

  avg = value / ct;
  // map 'avg' to a range of [0,100]
  soil = 100.0 - map(avg, 420, 1023, 0, 100);

  // Determine if the system's state has changed
  if(soil < relayOn && ( test == 0 ? systemState1 == 1 : systemState2 == 1) ){
    if( test == 0 ){ systemState1 = 0; }
    else{ systemState2 = 0; }  
    digitalWrite( ( test == 0 ? relayPin1 : relayPin2 ), LOW);
    change_message("Turned ON", test);
  }
  if(soil > relayOff && ( test == 0 ? systemState1 == 0 : systemState2 == 0) ){
    if( test == 0 ){ systemState1 = 1; }
    else{ systemState2 = 1; }
    digitalWrite( ( test == 0 ? relayPin1 : relayPin2 ), HIGH);
    change_message("Turned OFF", test);
  }
}

void setup () {
  //Initialize the serial port, wire library and RTC module
  Serial.begin(9600);
  // Set the pins for outputing.
  pinMode(relayPin1, OUTPUT);
  pinMode(relayPin2, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(backLight, OUTPUT);
  digitalWrite(backLight, HIGH); // turn backlight on. Replace 'HIGH' with 'LOW' to turn it off.
  lcd.begin(16,4);              // columns, rows.  use 16,2 for a 16x2 LCD, etc.
  lcd.clear();                  // start with a blank screen
  Wire.begin();
  RTC.begin();
  // Sets up the module time and date with the computer one
  RTC.adjust(DateTime(__DATE__, __TIME__));
  systemState1 = 1;
  systemState2 = 1;
}

void loop(){
  
  float water_resistance = readResistance(SENSOR_PIN, SERIES_RESISTOR);

  if(water_resistance < LOW_LEVEL_RESISTANCE){
    // The water level in the resivior is stil high enough
    moisture_sensors(0);
  }  
  else{
    change_message("Fill reservoir", -42);
    digitalWrite(relayPin1, HIGH);
    set_off_buzzer();
  }
  // Need to check the second system
  water_resistance = readResistance(SENSOR_PIN, SERIES_RESISTOR);
  if(water_resistance < LOW_LEVEL_RESISTANCE){
    // The water level in the resivior is stil high enough
    moisture_sensors(2);
  }  
  else{
    change_message("Fill reservoir", -42);
    digitalWrite(relayPin2, HIGH);
    set_off_buzzer();
  }
  // wait 1/10 of a second before continuing
  delay(1000);  
}

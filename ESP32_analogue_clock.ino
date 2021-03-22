/*
    Another stupid clock project II, The ESP 32 sequal.
    (c) A.G.Doswell 2021

    NTP referenced ESP32 RTC clock, driving 2 analogue moving coil meters, set up to read hours & minutes in 12 hour formate

    See http://www.andydoz.blogspot.com/
    for more information and the circuit diagram.

    Uncomment sections to enable serial output/debugging.


*/
#include <WiFi.h>
#include "time.h"
const char* ssid       = "your ssid"; // change these details to your wifi details
const char* password   = "your password";
const char* ntpServer = "pool.ntp.org"; // address of NTP server
const long  gmtOffset_sec = 0; // change this to alter the time to your local
const int   daylightOffset_sec = 0;
byte extractDay;
byte extractMonth;
int extractYear;
int extractHour;
int extractMin;
int extractSec;
int displayMin;
int displayHour;
int minsAsSecs;
int hoursAsMins;
unsigned long getNTPTimer = random (720, 1440);
int oldMins;
int oldSecs;
boolean failFlag = true;
boolean PM;
boolean tick;
// use first 8 channels of 16 channels
#define LEDC_CHANNEL_0     0
#define LEDC_CHANNEL_1     1

// use 13 bit precission for LEDC timer
#define LEDC_TIMER_13_BIT  13

// use 5000 Hz as a LEDC base frequency
#define LEDC_BASE_FREQ     5000

// "LED" pins defined to use LEDC for PWM to drive meter movements
#define LED_PIN_0      23
#define LED_PIN_1      22
#define PM_pin         18   //AM/PM pin
#define TICK_PIN       15   // output to relay

void ledcAnalogWrite(uint8_t channel, uint32_t value, uint32_t valueMax = 255) {
  // calculate duty, 8191 from 2 ^ 13 - 1
  uint32_t duty = (8191 / valueMax) * min(value, valueMax);
  // write duty to LEDC
  ledcWrite(channel, duty);
}

void setup() {
  Serial.begin(115200);
  // Setup timer and attach timer to an output pin
  ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
  ledcAttachPin(LED_PIN_0, LEDC_CHANNEL_0);
  ledcSetup(LEDC_CHANNEL_1, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
  ledcAttachPin(LED_PIN_1, LEDC_CHANNEL_1);
  while (failFlag) { //if the time isn't set , set it via NTP
    getNTP ();
    extractLocalTime ();
    oldSecs = extractSec;
  }
  pinMode(PM_pin, OUTPUT);
  pinMode(TICK_PIN, OUTPUT);
  ledcAnalogWrite(0, 255); //set both channels to 255 to allow FSD calibration on meters.
  ledcAnalogWrite(1, 255);
  delay (10000);
}

void getNTP () {
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.printf(".");
  }
  Serial.println(" CONNECTED");

  //init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  extractLocalTime();
  oldMins = extractMin;

  //disconnect WiFi as it's no longer needed
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

/*void printLocalTime() //uncomment to enable serial output/debugging.
  {
  Serial.printf("Mins to next NTP update:");
  Serial.print(getNTPTimer);
  Serial.printf(" ");
  Serial.print(extractHour);
  Serial.printf(":");
  Serial.print(extractMin);
  Serial.printf(":");
  Serial.print(extractSec);
  Serial.printf(" ");
  Serial.print(extractDay);
  Serial.printf("/");
  Serial.print(extractMonth);
  Serial.printf("/");
  Serial.print(extractYear);
  Serial.printf(" Hours Pos:");
  Serial.print(hoursAsMins);
  Serial.printf(" Mins Pos:");
  Serial.print(minsAsSecs);
  Serial.printf(" PWM Hours:");
  Serial.print(displayHour);
  Serial.printf(" Mins:");
  Serial.println(displayMin);
  }*/

void extractLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) { // checks the time is set, if it isn't set a flag.
    Serial.println("Failed to obtain time");
    failFlag = true;
    return;
  }
  extractHour = timeinfo.tm_hour; // extracts the time to variables, so we can manipulate it
  extractMin  = timeinfo.tm_min;
  extractSec  = timeinfo.tm_sec;
  extractDay = timeinfo.tm_mday;
  extractMonth = timeinfo.tm_mon + 1;
  extractYear = timeinfo.tm_year + 1900;
  failFlag = false;
}

void updateClockDisplay () {
  minsAsSecs = extractSec + (extractMin * 60); // this section calulates minutes
  displayMin = map (minsAsSecs, 0, 3599, 0, 255); // and maps them to an 8-bit PWM value
  hoursAsMins = extractMin + (extractHour * 60); // this calculates the hours from the minutes value
  displayHour = map(hoursAsMins, 0, 719, 0, 255); // and maps that to an 8-bit pwm value
  ledcAnalogWrite(0, displayMin); // and writes the values as PWM to the pins
  ledcAnalogWrite(1, displayHour);
}

boolean isBST() // this bit of code blatantly plagarised from http://my-small-projects.blogspot.com/2015/05/arduino-checking-for-british-summer-time.html
{
  int imonth = extractMonth;
  int iday = extractDay;
  int hr = extractHour;
  //January, february, and november are out.
  if (imonth < 3 || imonth > 10) {
    return false;
  }
  //April to September are in
  if (imonth > 3 && imonth < 10) {
    return true;
  }
  // find last sun in mar and oct - quickest way I've found to do it
  // last sunday of march
  int lastMarSunday =  (31 - (5 * extractYear / 4 + 4) % 7);
  //last sunday of october
  int lastOctSunday = (31 - (5 * extractYear / 4 + 1) % 7);
  //In march, we are BST if is the last sunday in the month
  if (imonth == 3) {
    if ( iday > lastMarSunday)
      return true;
    if ( iday < lastMarSunday)
      return false;
    if (hr < 1)
      return false;
    return true;
  }
  //In October we must be before the last sunday to be bst.
  //That means the previous sunday must be before the 1st.
  if (imonth == 10) {
    if ( iday < lastOctSunday)
      return true;
    if ( iday > lastOctSunday)
      return false;
    if (hr >= 1)
      return false;
    return true;
  }
}
void loop() {
  extractLocalTime();
  if (!failFlag) {
    if (isBST()) {
      extractHour ++;
      if (extractHour > 12) {
        extractHour = 0;
      }
    }
    if (extractHour >= 12) {
      digitalWrite(PM_pin, HIGH);
      extractHour -= 12;
    }
    else {
      digitalWrite(PM_pin, LOW);
    }
    if (extractMin != oldMins ) {
      getNTPTimer--;
      oldMins = extractMin;
    }
    if (getNTPTimer <= 0) {
      getNTP();
      getNTPTimer = random(720, 1440);
    }
    updateClockDisplay();
    /*  printLocalTime (); //uncomment this to enable serial output & debugging
      delay (1000); */
  }
  else {
    getNTP();
  }

  if (oldSecs != extractSec) {
    tick = !tick;
    digitalWrite (TICK_PIN, tick);
    oldSecs = extractSec;
  }

}

/* Fish Feeder
   michaeldvr
   version 1.0
   ref:
   - https://randomnerdtutorials.com/esp8266-dht11dht22-temperature-and-humidity-web-server-with-arduino-ide/
   - https://randomnerdtutorials.com/esp8266-nodemcu-access-point-ap-web-server/
   - https://github.com/maniacbug/StandardCplusplus
*/
#include <Wire.h>
#include <RtcDS3231.h>
#include <ESP8266WiFi.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Servo.h>

RtcDS3231<TwoWire> Rtc(Wire);

AsyncWebServer server(80);
const char *ssid = "Fish Feeder";
const char *password = "29situbondo";

// constants won't change. They're used here to set pin numbers:
const uint8_t buttonPin = D7; // the number of the pushbutton pin
const uint8_t statusLedPin = D5;    // the number of the LED pin
const uint8_t temtPin = A0;
const uint8_t foodLedPin = D6; // food level
int switchValue;
uint8_t counter = 0;

Servo servo;
const uint8_t servoPin = D8;
const uint8_t TRAY_CLOSE = 0;
const uint8_t TRAY_OPEN = 180;
const uint8_t FEED_SIZE = 20; // ms
const int FEEDING_DELAY = 600; // ms
const float EMPTY_FOOD_THRESHOLD = 30;
uint8_t servoState = TRAY_CLOSE;
int feedingState = 0;
uint8_t statusLedState = LOW;
bool lowContent = false;

/* autofeeding schedule */
struct schedule {
  uint8_t h;
  uint8_t m;
  uint8_t s;
};
schedule feeding_schedule[] = {
  {7, 0, 0},
  {12, 0, 0},
  {17, 30, 0},
  {14, 6, 0},
  {14, 6, 30},
  {14, 7, 30}
};
const uint8_t SCHEDULE_SIZE = 6; // update when modify `feeding_schedule`
int feedingInterval = 0;

void setup()
{
  Serial.begin(9600);
  Rtc.Begin();

  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  if (!Rtc.IsDateTimeValid())
  {
    Serial.println("RTC lost confidence in the DateTime!");
    Rtc.SetDateTime(compiled);
  }


  Rtc.Enable32kHzPin(false);
  Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone);

  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  /*Serial.print("IP address: ");
    // Print ESP8266 Local IP Address
    Serial.println(WiFi.localIP());*/

  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", "This is an index page");
  });

  server.begin(); //Start the server

  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(statusLedPin, OUTPUT);
  pinMode(temtPin, INPUT);
  pinMode(foodLedPin, OUTPUT);

  // Servo
  servo.attach(servoPin);
  servo.write(servoState);
}

void print_date(RtcDateTime now, bool newline = false) {
  if (counter % 10 != 0) return;
  //Print RTC time to Serial Monitor
  Serial.print("Date: ");
  Serial.print(now.Year(), DEC);
  Serial.print('/');
  Serial.print(now.Month(), DEC);
  Serial.print('/');
  Serial.print(now.Day(), DEC);
  Serial.print(" | Time: ");
  Serial.print(now.Hour(), DEC);
  Serial.print(':');
  Serial.print(now.Minute(), DEC);
  Serial.print(':');
  Serial.print(now.Second(), DEC);
  if (newline) Serial.println();
}

void check_feeding_time(RtcDateTime now) {
  if (feedingInterval == 0) {
    bool found = false;
    for (uint8_t i = 0; i < SCHEDULE_SIZE; i++) {
      if (now.Hour() == feeding_schedule[i].h &&
          now.Minute() == feeding_schedule[i].m &&
          now.Second() == feeding_schedule[i].s) {
        feedingInterval = FEEDING_DELAY;
        feedingState = FEED_SIZE;
        Serial.println("found schedule");
      }
    }
  }
  else {
    feedingInterval--;
  }
}

void start_feeding() {
  if (feedingState > 0) {
    if (servoState == TRAY_CLOSE) {
      servoState = TRAY_OPEN;
      servo.write(servoState);
    }
    if (statusLedState == LOW) {
      statusLedState = HIGH;
    }
    else {
      statusLedState = LOW;
    }
    digitalWrite(statusLedPin, statusLedState);
    feedingState--;
  }
  else {
    if (servoState == TRAY_OPEN) {
      servoState = TRAY_CLOSE;
      servo.write(servoState);
    }
    if (statusLedState == HIGH) {
      statusLedState = LOW;
      digitalWrite(statusLedPin, statusLedState);
    }
  }
}

void check_light() {
  if (counter % 10 == 1) {
    float reading = analogRead(temtPin);
    Serial.print("intensity: ");
    Serial.println(reading);
    lowContent = (reading < EMPTY_FOOD_THRESHOLD);
    digitalWrite(foodLedPin, lowContent);
  }
}

void loop()
{
  // server.handleClient(); //Handling of incoming requests
  RtcDateTime now = Rtc.GetDateTime();
  print_date(now, true);

  delay(100); // one second
  switchValue = digitalRead(buttonPin);
  // digitalWrite(statusLedPin, switchValue);
  // Serial.print(" | Button: ");
  // Serial.println(switchValue);
  if ( switchValue == HIGH || feedingState != 0) {
    Serial.print("Button: ");
    Serial.print(switchValue);
    Serial.print(" | feedingState: ");
    Serial.print(feedingState);
    Serial.println();
  }
  if (switchValue == HIGH && feedingState == 0)
  {
    feedingState = FEED_SIZE;
  }
  check_feeding_time(now);
  start_feeding();
  check_light();
  counter++;
}

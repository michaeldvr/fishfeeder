#include <Wire.h>
#include <RtcDS3231.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Servo.h>

RtcDS3231<TwoWire> Rtc(Wire);
ESP8266WebServer server(80);

const char *ssid = "TOKO 29";
const char *password = "29situbondo";

// constants won't change. They're used here to set pin numbers:
const int buttonPin = D6; // the number of the pushbutton pin
const int ledPin = D5;    // the number of the LED pin
int switchValue;

Servo servo;
const int servoPin = D8;
const int minimumState = 0;
const int maximumState = 180;
int servoState = minimumState;

void handle_index()
{
  server.send(200, "text/plain", "This is an index page.");
}

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

  /*RtcDateTime now = Rtc.GetDateTime();*/

  Rtc.Enable32kHzPin(false);
  Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone);

  WiFi.begin(ssid, password); //Connect to the WiFi network

  while (WiFi.status() != WL_CONNECTED)
  { //Wait for connection
    delay(500);
    Serial.println("Waiting to connect...");
  }

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP()); //Print the local IP

  server.on("/", handle_index);

  server.begin(); //Start the server

  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);

  // Servo
  servo.attach(servoPin);
  servo.write(servoState);
}

void loop()
{
  server.handleClient(); //Handling of incoming requests
  RtcDateTime now = Rtc.GetDateTime();
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
  Serial.println();

  delay(1000); // one second
  switchValue = digitalRead(buttonPin);
  digitalWrite(ledPin, switchValue);
  Serial.print("Button: ");
  Serial.println(switchValue);
  if (switchValue == HIGH && servoState == minimumState)
  {
    servoState = maximumState;
    servo.write(servoState);
  }
  else if (switchValue == LOW && servoState == maximumState)
  {
    servoState = minimumState;
    servo.write(servoState);
  }
}

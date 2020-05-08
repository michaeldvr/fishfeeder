/* Fish Feeder
   michaeldvr
   version 1.0
   ref:
   - https://randomnerdtutorials.com/esp8266-dht11dht22-temperature-and-humidity-web-server-with-arduino-ide/
   - https://randomnerdtutorials.com/esp8266-nodemcu-access-point-ap-web-server/
   - https://github.com/maniacbug/StandardCplusplus

   version history:
   - 1.0 initial version
   - 1.1 [20/05/07] configurable settings via webserver
*/
#include <Wire.h>
#include <RtcDS3231.h>
#include <ESP8266WiFi.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Servo.h>
#include <EEPROM.h>


const uint8_t FFVERSION = 1;

RtcDS3231<TwoWire> Rtc(Wire);

AsyncWebServer server(80);
const char *ssid = "FishFeeder";
const char *password = "pakanikan29";

// constants won't change. They're used here to set pin numbers:
const uint8_t buttonPin = D7; // the number of the pushbutton pin
const uint8_t statusLedPin = D5;    // the number of the LED pin
int switchValue;
uint8_t counter = 0;

Servo servo;
const uint8_t servoPin = D8;

// config
uint8_t TRAY_CLOSE = 0;
uint8_t TRAY_OPEN = 80;
uint8_t FEED_SIZE = 8; // * 100 ms
int FEEDING_DELAY = 600; // *100 ms

// dynamic vars
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

const uint8_t SCHEDULE_SIZE = 3; // update when modify `feeding_schedule`
schedule feeding_schedule[SCHEDULE_SIZE] = {
  {7, 0, 0},
  {12, 0, 0},
  {17, 30, 0},
};

int feedingInterval = 0;

void initEEPROM() {
  int address = 0;
  uint8_t data;
  EEPROM.get(address, data); // get version
  if (data == FFVERSION) return;
  Serial.print("data version mismatch (");
  Serial.print(FFVERSION);
  Serial.print(") !=  ");
  Serial.println(data);
  Serial.println("initializing EEPROM data");
  address = saveEEPROM();
  Serial.println("EEPROM data initialized");
  Serial.print(address);
  Serial.print("/");
  Serial.println(EEPROM.length());
}

int saveEEPROM() {
  /*
     EEPROM data layout
     0    : version flag (0/null: unitialized, FFVERSION)
     1    : TRAY_CLOSE
     2    : TRAY_OPEN
     3    : FEED_SIZE
     4    : FEEDING_DELAY
     5    : schedule #1
     6    : schedule #2
     7    : schedule #3
  */
  int address = 0;
  EEPROM.put(address, FFVERSION);  // 0
  address = address + sizeof(FFVERSION);

  EEPROM.put(address, TRAY_CLOSE); // 1
  address = address + sizeof(TRAY_CLOSE);

  EEPROM.put(address, TRAY_OPEN); // 2
  address = address + sizeof(TRAY_OPEN);

  EEPROM.put(address, FEED_SIZE); // 3
  address = address + sizeof(FEED_SIZE);

  EEPROM.put(address, FEEDING_DELAY); // 4
  address = address + sizeof(FEEDING_DELAY);

  EEPROM.put(address, feeding_schedule[0]);
  address = address + sizeof(feeding_schedule[0]);

  EEPROM.put(address, feeding_schedule[1]);
  address = address + sizeof(feeding_schedule[1]);

  EEPROM.put(address, feeding_schedule[2]);
  address = address + sizeof(feeding_schedule[2]);

  EEPROM.commit();
  return address;
}

void checkEEPROM(bool load=true) {
  // load EEPROM to global variables
  // print data to serial
  int address = 0;
  uint8_t data;
  int feeding_delay;
  EEPROM.get(address, data);  // 0: version
  Serial.print("version: ");
  Serial.println(data);
  address += sizeof(data);

  EEPROM.get(address, data);
  Serial.print("TRAY_CLOSE: ");
  Serial.println(data);
  address += sizeof(data);
  if (load) TRAY_CLOSE = data;

  EEPROM.get(address, data);
  Serial.print("TRAY_OPEN: ");
  Serial.println(data);
  address += sizeof(data);
  if (load) TRAY_OPEN = data;

  EEPROM.get(address, data);
  Serial.print("FEED_SIZE: ");
  Serial.println(data);
  address += sizeof(data);
  if (load) FEED_SIZE = data;

  EEPROM.get(address, feeding_delay);
  Serial.print("FEEDING_DELAY: ");
  Serial.println(feeding_delay);
  address += sizeof(feeding_delay);
  if (load) FEEDING_DELAY = feeding_delay;

  schedule sched;

  EEPROM.get(address, sched);
  Serial.print("#1: ");
  printTime(sched);
  Serial.println();
  address = address + sizeof(sched);
  if (load) {
    feeding_schedule[0].s = sched.s;
    feeding_schedule[0].m = sched.m;
    feeding_schedule[0].h = sched.h;
  }

  EEPROM.get(address, sched);
  Serial.print("#2: ");
  printTime(sched);
  Serial.println();
  address = address + sizeof(sched);
  if (load) {
    feeding_schedule[1].s = sched.s;
    feeding_schedule[1].m = sched.m;
    feeding_schedule[1].h = sched.h;
  }

  EEPROM.get(address, sched);
  Serial.print("#3: ");
  printTime(sched);
  Serial.println();
  if (load) {
    feeding_schedule[2].s = sched.s;
    feeding_schedule[2].m = sched.m;
    feeding_schedule[2].h = sched.h;
  }
}

void printzero(int d) {
  if (d < 10) {
    Serial.print("0");
  }
}

void printTime(schedule s) {
  printzero(s.h);
  Serial.print(s.h);
  Serial.print(":");
  printzero(s.m);
  Serial.print(s.m);
  Serial.print(":");
  printzero(s.s);
  Serial.print(s.s);
}

String format_date_str(RtcDateTime now) {
  char res[19];
  sprintf_P(res, (PGM_P)F("%04d/%02d/%02d %02d:%02d:%02d"), now.Year(), now.Month(), now.Day(), now.Hour(), now.Minute(), now.Second());
  return String(res);
}

void format_date(RtcDateTime now, char *result) {
  char res[19];
  sprintf_P(res, (PGM_P)F("%04d/%02d/%02d %02d:%02d:%02d"), now.Year(), now.Month(), now.Day(), now.Hour(), now.Minute(), now.Second());
  for (int i = 0; i < 19; ++i) {
    result[i] = res[i];
  }
}

void print_date(RtcDateTime now, bool newline = false) {
  if (counter % 10 != 0) return;
  //Print RTC time to Serial Monitor
  Serial.print("Date: ");
  char formatted[19];
  format_date(now, formatted);
  Serial.print(formatted);
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

String format_time(schedule s) {
  char buff[8];
  sprintf(buff, "%02d:%02d:%02d", (int)s.h, (int)s.m, (int)s.s);
  String res = String(buff);
  return res;
}


// ------------------------------------------- BEGIN HTML DATA -------------------------------------------

const char index_html[] PROGMEM = "<html><head> <title>Fish Feeder</title></head><body> <h3>Fish Feeder v1.1</h3> <p>System clock: <code>%SYSDATE%</code></p>Open <a href='config'>config</a></body></html>";
const char config_html[] PROGMEM = "<form id='frm' method='POST' action=''> <p>Feeding Size <input name='fz' type='number' value='%FZ%' min='0' max='20' required> (* 100 ms)</p><p>Tray Open <input name='to' type='number' value='%TO%' min='0' max='90' required> (deg)</p><p>Tray Close <input name='tc' type='number' value='%TC%' min='0' max='90' required> (deg)</p><h3>schedule</h3> <p>#1 <input name='s1' type='time' value='%S1%' required></p><p>#2 <input name='s2' type='time' value='%S2%' required></p><p>#3 <input name='s3' type='time' value='%S3%' required></p><input type='submit' value='Save'></form><a href='/'>Return</a>";
const char success_html[] PROGMEM = "<html><head> <title>Fish Feeder</title><script src='success.js'></script></head><body> <h3>Fish Feeder v1.1</h3><p><i>Settings saved!</i></p><p>System clock: <code>%SYSDATE%</code></p>Open <a href='config'>config</a></body></html>";
const char success_js[] PROGMEM = "document.addEventListener('DOMContentLoaded',function(n){setTimeout(function(){window.location.href='/'},3e3)});";

// ------------------------------------------- END HTML DATA -------------------------------------------

String index_processor(const String& var) {
  // Serial.println(var);
  if (var == "SYSDATE") {
    RtcDateTime now = Rtc.GetDateTime();
    return format_date_str(now);
  }
  return String();
}

String config_processor(const String& var) {
  if (var == "FZ") {
    return String(FEED_SIZE);
  }
  else if (var == "TO") {
    return String(TRAY_OPEN);
  }
  else if (var == "TC") {
    return String(TRAY_CLOSE);
  }
  else if (var == "S1") {
    return format_time(feeding_schedule[0]);
  }
  else if (var == "S2") {
    return format_time(feeding_schedule[1]);
  }
  else if (var == "S3") {
    return format_time(feeding_schedule[2]);
  }
  return String();
}

int chr2int(char val) {
  return val - '0';
}

schedule parse_schedule(const String& val) {
  schedule res = {0, 0, 0};
  
  res.h = chr2int(val.charAt(0)) * 10 + chr2int(val.charAt(1)); // hour
  res.m = chr2int(val.charAt(3)) * 10 + chr2int(val.charAt(4)); // min
  // res.s = chr2int(val.charAt(5)) * 10 + chr2int(val.charAt(6)); // sec
  return res;
}

void setup()
{
  Serial.begin(9600);

  EEPROM.begin(32);

  initEEPROM();
  checkEEPROM(true);

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

  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("Password: ");
  Serial.println(password);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", index_html, index_processor);
  });

  server.on("/config", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", config_html, config_processor);
  });

  server.on("/config", HTTP_POST, [](AsyncWebServerRequest * request) {

    int params = request->params();
    bool ok = false;
    for (int i = 0; i < params; ++i) {
      AsyncWebParameter *p = request->getParam(i);
      if (p->isPost()) {
        bool valid = true;
        String name = String(p->name());
        if (name == "fz") {
          FEED_SIZE = p->value().toInt();
        }
        else if (name == "to") {
          TRAY_OPEN = p->value().toInt();
        }
        else if (name == "tc") {
          TRAY_CLOSE = p->value().toInt();
        }
        else if (name == "s1") {
          schedule s = parse_schedule(p->value());
          feeding_schedule[0].h = s.h;
          feeding_schedule[0].m = s.m;
          feeding_schedule[0].s = s.s;
        }
        else if (name == "s2") {
          schedule s = parse_schedule(p->value());
          feeding_schedule[1].h = s.h;
          feeding_schedule[1].m = s.m;
          feeding_schedule[1].s = s.s;
        }
        else if (name == "s3") {
          schedule s = parse_schedule(p->value());
          feeding_schedule[2].h = s.h;
          feeding_schedule[2].m = s.m;
          feeding_schedule[2].s = s.s;
        }
        else {
          valid = false;
        }
        if(valid) ok = true;
      }
    }
    if (ok) {
      saveEEPROM();
      checkEEPROM(false);
      request->send_P(200, "text/html", success_html, index_processor);
    }
    else {
      request->send_P(200, "text/html", config_html, config_processor);
    }
    
  });

  server.on("/success.js", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", success_js);
  });


  server.begin(); //Start the server

  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(statusLedPin, OUTPUT);

  // Servo
  servo.attach(servoPin);
  servo.write(servoState);
}

void loop()
{
  // server.handleClient(); //Handling of incoming requests
  RtcDateTime now = Rtc.GetDateTime();
  // print_date(now, true);

  delay(100); // 10 ms
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
  counter++;
}

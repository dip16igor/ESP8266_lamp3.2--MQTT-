#include <Arduino.h>
#include <EncButton.h>
#include <Adafruit_NeoPixel.h>
#include <FastBot.h>
#include <ESP8266HTTPClient.h>

#include <Wire.h>
#include <Adafruit_MCP4725.h>
#include <PubSubClient.h>

#include "secrets.h"

#define DEBUG
// LED pin definition
const int ledPin = 2;

#define DC_DC_EN D8
#define PIN 10      // Pin connected to the data input of the NeoPixel strip
#define NUM_LEDS 16 // Number of LEDs in the strip

#define MAX_DAC 2700
#define MIN_DAC 0
#define GAMMA 2.1

#define STATE_OFF 0
#define STATE_ON_MANUAL 1
#define STATE_ON_AUTO 2
#define STATE_OFF_AUTO 3

#define topic_in "Home/Light1/Action"
#define topic_out "Home/Light1/Log"
#define topic_heap "Home/Light1/FreeHeap"

Adafruit_MCP4725 dac;

FastBot bot(BOT_TOKEN);
#define TelegramDdosTimeout 5000 // timeout
// unsigned long bot_lasttime; // last time messages' scan has been done
bool Start = false;
const unsigned long BOT_MTBS = 3600; // mean time between scan messages

WiFiClient wclient;
PubSubClient client(wclient);

HTTPClient http; // Create Object of HTTPClient
int httpCode;    // server response code
String payload;  // server response string
String error;    // error code description

bool restart = 0;
unsigned long uptime;
bool telegram = true;

EncButton eb(D7, D6, D5, INPUT_PULLUP, INPUT_PULLUP);

Adafruit_NeoPixel strip(NUM_LEDS, PIN, NEO_GRB + NEO_KHZ800);

// const int pwmPin = D4;
// const int pwmPin2 = D0;
// const int pwmFrequency = 10000; // PWM frequency in Hz
// const int pwmResolution = 8;    // PWM resolution (8 bit)
volatile int encoderPos = 0;
volatile boolean encoderButtonPressed = false;
volatile boolean encoderButtonReleased = false;

int bright = 0; // TODO
int bright_temp;

// int light = 0;  // LED strip brightness, from 0 to MAX_DAC
bool PowerON = false;

int State = STATE_OFF;

long time_cur;
long time_cur2;

void setup_wifi(void);
void newMsg(FB_msg &msg);
void rainbowEffect(uint8_t rainbowLength, uint8_t brightness);
uint32_t Wheel(byte WheelPos);

int temp, temp2, temp3;

unsigned long currentMillis = 0;
unsigned long lastUpdateTime = 0;
const unsigned long updateInterval = 2000; // 1 секунда

void rainbowCycle(uint8_t wait);
void rainbowLength(uint16_t length, uint16_t brightness);

void set_DAC(int br)
{
  if (br > 4095)
    br = 4095;
  if (br < 0)
    br = 0;

  float normalizedValue = br / float(4095);                         // Normalize value from 0 to 1
  float correctedValue = pow(normalizedValue, GAMMA) * float(4095); // Apply gamma correction

  temp = 4095 - correctedValue; // invert bright

  temp2 = map(temp, 0, 4095, MIN_DAC, MAX_DAC); // scale

  temp3 = int(temp2); //

  dac.setVoltage(temp3, false); // устанавливаем значение
}

// MQTT прием сообщения от брокера
void callback(char *topic, byte *payload, unsigned int length)
{

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  String message = "";
  for (int i = 0; i < length; i++)
  {
    message += (char)payload[i];
  }

  if (strncmp(topic, topic_in, strlen(topic_in)) == 0)
  {
    if (message == "LIGHT_ON") // turn on
    {
      client.publish(topic_out, "PowerON");
      PowerON = true;

      State = STATE_ON_AUTO;

      set_DAC(0);
      delay(1);
      digitalWrite(DC_DC_EN, HIGH);

      time_cur2 = millis();
      bright_temp = bright;
    }

    if (message == "LIGHT_OFF") // turn off
    {
      client.publish(topic_out, "PowerOFF");
      PowerON = false;

      State = STATE_OFF_AUTO; // запуск автоматического выключения

      time_cur = millis();
      bright_temp = bright;
    }

    if (message == "Status") // status request
    {
      if (PowerON)
        client.publish(topic_out, "PowerON");
      else
        client.publish(topic_out, "PowerOFF");
    }
  }
}

void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection... ");
    // Create a random client ID
    String clientId = "Home_Light1";
    // clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str()))
    {
      // digitalWrite(LED2, LOW);
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(topic_out, "Reconnect");
      // ... and resubscribe
      client.subscribe(topic_in);

      // bot.sendMessage("MQTT connected", idAdmin1);
      //  delay(2000);
    }
    else
    {
      // digitalWrite(LED2, HIGH);
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");

      // bot.sendMessage("connect to MQTT ..", idAdmin1);
      //  Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup()
{
  Wire.begin();
  Serial.begin(115200);

  Serial.println("\nStart");

  // For Adafruit MCP4725A1 the address is 0x62 (default) or 0x63 (ADDR pin tied to VCC)
  // For MCP4725A0 the address is 0x60 or 0x61
  // For MCP4725A2 the address is 0x64 or 0x65
  dac.begin(0x60);

  pinMode(D5, INPUT_PULLUP);
  // if (digitalRead(D5) == 0)
  //   telegram = true;

  pinMode(DC_DC_EN, OUTPUT);
  digitalWrite(DC_DC_EN, LOW);

  // analogWrite(pwmPin, 190);
  dac.setVoltage(4000, false);
  // delay(1);
  // digitalWrite(DC_DC_EN, LOW);

  Serial.begin(115200);
  Serial.println();
  Serial.println("Reset");

  strip.begin();
  strip.setBrightness(50);
  // strip.setPixelColor(15, strip.Color(50, 0, 0));
  //  strip.setPixelColor(1, strip.Color(20, 0, 0));
  for (int i = 15; i >= 0; i--)
    strip.setPixelColor(i, strip.Color(20, 0, 0));
  strip.show();

  if (telegram)
    setup_wifi();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  reconnect();
  client.publish("Home/Light1/Log", "Restart OK");

  // delay(2000);
  // показаны значения по умолчанию
  eb.setBtnLevel(LOW);
  eb.setClickTimeout(500);
  eb.setDebTimeout(50);
  eb.setHoldTimeout(600);
  eb.setStepTimeout(200);

  eb.setEncReverse(0);
  eb.setEncType(EB_STEP1);
  eb.setFastTimeout(30);

  // сбросить счётчик энкодера
  eb.counter = 0;
}

void loop()
{
  currentMillis = millis();

  if (currentMillis - lastUpdateTime >= updateInterval)
  {
    // UpdateScreen();
    lastUpdateTime = currentMillis;
    // client.publish(topic_out, "ping..");
    if (!client.connected())
      reconnect();

    size_t freeHeap = ESP.getFreeHeap();
    // char myChar[25];
    // snprintf(myChar, sizeof(myChar), "ON Free Heap: %d", freeHeap);
    // client.publish(topic_out, myChar);
    char buffer[10]; // Буфер для хранения строки
    sprintf(buffer, "%d", freeHeap);
    client.publish(topic_heap, buffer);
    // free((void *)myChar);
  }

  client.loop(); // MQTT client

  eb.tick();

  if (eb.turn()) // крутим крутилку
  {
    if (eb.counter >= 16)
      eb.counter = 16;
    if (eb.counter <= 0)
      eb.counter = 0;
#ifdef DEBUG
    Serial.print("turn: dir ");
    Serial.print(eb.dir());
    Serial.print(", fast ");
    Serial.print(eb.fast());
    Serial.print(", hold ");
    Serial.print(eb.pressing());
    Serial.print(", counter ");
    Serial.print(eb.counter);
    Serial.print(", clicks ");
    Serial.println(eb.getClicks());
#endif

    bright = eb.counter * 256; // пересчитываем в яркость от 0 - минимальная яркость  до 4095
    if (bright <= 0)
      bright = 0;
    if (bright >= 4095)
      bright = 4095;

    Serial.println(eb.counter);
    Serial.println(bright);
    Serial.println(temp3);

    if (bright <= 250) // докрутили до минимума
    {
      State = STATE_OFF;

      digitalWrite(DC_DC_EN, 0); // ВЫКЛ DC-DC
    }
    else if (State == STATE_OFF) // крутим с минимума
    {
      State = STATE_ON_MANUAL;

      set_DAC(0);
      delay(1);
      digitalWrite(DC_DC_EN, HIGH);
    }

    if (State == STATE_OFF_AUTO || State == STATE_ON_AUTO)
    {
      State = STATE_ON_MANUAL;
    }

    if (State == STATE_ON_MANUAL && bright > 4000) // крутим от максимума вниз
    {
    }

    strip.clear();

    int br = map(bright, 0, 4095, 50, 255); // изменяем яркость индикаторов
    rainbowLength(eb.counter, br);

    // for (int i = 16; i >= 16 - eb.counter; i--)
    //   strip.setPixelColor(i, strip.Color(10, 0, 20));

    // strip.setBrightness(map(bright, 0, 4095, 50, 255)); // изменяем яркость индикаторов

    // strip.show();

    if (State == STATE_OFF)
      set_DAC(0);
    else
      set_DAC(bright);

    // Serial.println(gammaCorrectedValue);
    Serial.println(State);
  }

  // кнопка
  if (eb.press())
  {
#ifdef DEBUG
    Serial.println("press");
#endif
  }

  if (eb.click())
  {
#ifdef DEBUG
    Serial.println("click");
#endif
    if (State == STATE_ON_AUTO || State == STATE_ON_MANUAL)
    {
      State = STATE_OFF_AUTO; // запуск автоматического выключения

      time_cur = millis();
      bright_temp = bright;
    }
    else
    {
      State = STATE_ON_AUTO;

      set_DAC(0);
      delay(1);
      digitalWrite(DC_DC_EN, HIGH);

      time_cur2 = millis();
      bright_temp = bright;
    }
    Serial.println(State);
    //
  }
  if (eb.release())
  {
#ifdef DEBUG
    Serial.println("release");

    Serial.print("clicks: ");
    Serial.print(eb.getClicks());
    Serial.print(", steps: ");
    Serial.print(eb.getSteps());
    Serial.print(", press for: ");
    Serial.print(eb.pressFor());
    Serial.print(", hold for: ");
    Serial.print(eb.holdFor());
    Serial.print(", step for: ");
    Serial.println(eb.stepFor());
#endif
  }

  if (State == STATE_OFF_AUTO)
  {
    bright = bright_temp - (millis() - time_cur) / 3;

    // dac.setVoltage(bright, false);
    set_DAC(bright);

    eb.counter = bright / 256;

    for (int i = 16 - eb.counter; i >= 0; i--)
      strip.setPixelColor(i, strip.Color(20, 0, 0));
    // strip.show();

    for (int i = 16; i >= 16 - eb.counter; i--)
      strip.setPixelColor(i, strip.Color(10, 0, 20));

    strip.setBrightness(map(bright, 0, 4095, 50, 255)); // изменяем яркость индикаторов

    strip.show();
    // strip.setBrightness(map(eb.counter, 0, 240, 50, 255)); // изменяем яркость индикаторов

    if (bright <= 100)
    {
      State = STATE_OFF;
      // digitalWrite(DC_DC_EN, 0); // ВЫКЛ DC-DC
      for (int i = 15; i >= 0; i--)
        strip.setPixelColor(i, strip.Color(20, 0, 0));
      strip.show();

      digitalWrite(DC_DC_EN, LOW);

      Serial.println(State);
    }
  }

  if (State == STATE_ON_AUTO)
  {
    bright = bright_temp + (millis() - time_cur2) / 3;
    set_DAC(bright);

    eb.counter = bright / 256;

    for (int i = 16 - eb.counter; i >= 0; i--)
      strip.setPixelColor(i, strip.Color(0, 10, 5));

    for (int i = 16; i >= 16 - eb.counter; i--)
      strip.setPixelColor(i, strip.Color(0, 50, 0));

    strip.setBrightness(map(bright, 0, 4095, 50, 255)); // изменяем яркость индикаторов

    strip.show();
    //

    if (bright >= 2047) // яркость поднялась до НУЖНОГО ЗНАЧЕНИЯ
    {
      State = STATE_ON_MANUAL;

      Serial.println(State);
    }
  }

  if (restart)
  {
    Serial.println("Restart!");
    bot.tickManual();
    ESP.restart();
  }

  if (telegram)
    bot.tick(); // тикаем в луп
}

// обработчик сообщений
void newMsg(FB_msg &msg)
{
  String chat_id = msg.chatID;
  String text = msg.text;
  String firstName = msg.first_name;
  String lastName = msg.last_name;
  String userID = msg.userID;
  String userName = msg.username;

  bot.notify(false);

  // выводим всю информацию о сообщении
  Serial.println(msg.toString());
  FB_Time t(msg.unix, 5);
  Serial.print(t.timeString());
  Serial.print(' ');
  Serial.println(t.dateString());

  if (chat_id == idAdmin1)
  {
    if (text == "/restart" && msg.chatID == idAdmin1)
      restart = 1;

    // обновление прошивки по воздуху из чата Telegram
    if (msg.OTA && msg.chatID == idAdmin1)
    {
      strip.setPixelColor(0, strip.Color(100, 100, 0));
      strip.show();
      bot.update();
      strip.setPixelColor(0, strip.Color(0, 100, 100));
      strip.show();
      delay(300);
    }
    if (text == "/ping")
    {
      strip.setPixelColor(0, strip.Color(100, 0, 100));
      strip.show();
      Serial.println("/ping");
      uptime = millis(); // + 259200000;
      int min = uptime / 1000 / 60;
      int hours = min / 60;
      int days = hours / 24;

      long rssi = WiFi.RSSI();
      // int gasLevelPercent = 0; // map(gasLevel, GasLevelOffset, 1024, 0, 100);

      String time = "pong! Сообщение принято в ";
      time += t.timeString();
      time += ". Uptime: ";
      time += days;
      time += "d ";
      time += hours - days * 24;
      time += "h ";
      // time += min - days * 24 * 60 - (hours - days * 24) * 60;
      time += min - hours * 60;
      time += "m";

      time += ". RSSI: ";
      time += rssi;
      time += " dB";

      // time += ". Gas: ";
      // time += gasLevelPercent;
      // time += " %";

      bot.sendMessage(time, chat_id);
      Serial.println("Message is sended");

      strip.setPixelColor(7, strip.Color(1, 0, 2));
      strip.show();
    }
    if (text == "/start" || text == "/start@dip16_GasSensor_bot")
    {
      strip.setPixelColor(7, strip.Color(100, 100, 0));
      strip.show();
      bot.showMenuText("Команды:", "\ping \t \restart", chat_id, false);
      delay(300);
      strip.setPixelColor(7, strip.Color(0, 0, 0));
      strip.show();
    }
  }
}

void setup_wifi()
{
  int counter_WiFi = 0;
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // secured_client.setTrustAnchors(&cert); // Add root certificate for api.telegram.org

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(400);
    Serial.print(".");
    // OnLEDBLUE2();
    strip.setPixelColor(0, strip.Color(0, 0, 127));
    strip.show(); // Update the LED strip
    delay(400);
    // OffLEDBLUE2();
    strip.setPixelColor(0, strip.Color(10, 0, 0));
    strip.show(); // Update the LED strip
    counter_WiFi++;
    if (counter_WiFi >= 10)
    {
      break;
      //  Serial.print("ERROR! Unable connect to WiFi");
    }
  }

  if (counter_WiFi >= 10)
  {
    Serial.print("ERROR! Unable connect to WiFi");
    strip.setPixelColor(0, strip.Color(200, 0, 0));
    strip.show(); // Update the LED strip
    delay(1000);
  }
  else
  {
    // randomSeed(micros());

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    bot.setChatID("");  // передай "" (пустую строку) чтобы отключить проверку
    bot.skipUpdates();  // пропустить непрочитанные сообщения
    bot.attach(newMsg); // обработчик новых сообщений

    bot.sendMessage("Lamp2 Restart OK", idAdmin1);
    Serial.println("Message to telegram was sended");
    strip.setPixelColor(0, strip.Color(0, 127, 0));
    strip.setPixelColor(1, strip.Color(0, 127, 0));

    strip.show(); // Update the LED strip

    // digitalWrite(LED1, LOW); // зажигаем синий светодиод
  }
}

void OffLEDBLUE(void) // Выключить светодиод LED1 COM
{                     // turn the LED off (HIGH is the voltage level)
  digitalWrite(LED_BUILTIN, HIGH);
} // запись 1 в порт синего светодиода
void OnLEDBLUE(void) // Включить светодиод LED1
{                    // turn the LED on by making the voltage LOW
  digitalWrite(LED_BUILTIN, LOW);
} // запись 0 в порт синего светодиода

void OffLEDBLUE2(void) // Выключить светодиод LED2 WAKE
{                      // turn the LED off (HIGH is the voltage level)
  digitalWrite(LED_BUILTIN_AUX, HIGH);
} // запись 1 в порт синего светодиода
void OnLEDBLUE2(void) // Включить светодиод LED2
{                     // turn the LED on by making the voltage LOW
  digitalWrite(LED_BUILTIN_AUX, LOW);
} // запись 0 в порт синего светодиода

void rainbowEffect(uint8_t rainbowLength, uint8_t brightness)
{
  uint16_t i, j;

  for (j = 0; j < 256; j++)
  {
    for (i = 0; i < strip.numPixels(); i++)
    {
      strip.setPixelColor(i, Wheel((i + j) & 255));
    }

    strip.setBrightness(brightness);
    strip.show();
    delay(rainbowLength);
  }
}

uint32_t Wheel(byte WheelPos)
{
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85)
  {
    // return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3); // R + B
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3); // G + B
  }
  if (WheelPos < 170)
  {
    WheelPos -= 85;
    // return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3); // G + B
    return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0); // R + G
  }
  WheelPos -= 170;
  // return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0); // R + G
  return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3); // R + B
}

// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait)
{
  uint16_t i, j;

  for (j = 0; j < 256 * 5; j++)
  { // 5 cycles of all colors on wheel
    for (i = 0; i < strip.numPixels(); i++)
    {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

void rainbowLength(uint16_t length, uint16_t brightness)
{
  int i;
  if (length > strip.numPixels())
    length = strip.numPixels();

  for (i = 16; i >= 16 - length; i--)
  {
    strip.setPixelColor(i, Wheel(((i) * 256 / strip.numPixels()) & 255));
  }

  strip.setBrightness(brightness);
  strip.show();
}
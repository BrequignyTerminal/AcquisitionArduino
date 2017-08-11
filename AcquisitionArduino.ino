#include <LiquidCrystal_I2C.h>
#include <DS1302.h>
#include <Wire.h>

#include "DHT.h"

// Pins declaration
#define red_pin   10
#define green_pin 9
#define blue_pin  8
#define dht_pin   7

#define relay1_pin 6
#define relay2_pin 5

// Clock declaration
#define kSclkPin  4  // Serial Clock
#define kIoPin    3  // Input/Output
#define kCePin    2  // Chip Enable

// Temp. and Hum. sensor type
#define dht_type  DHT11

// LCD definition
#define LCD_Cols  16
#define LCD_Rows  2

// Communication definition
#define RS232_Speed 9600

// Sensor definition
#define Luminosity_Threshold 65.0

// Command definition
String g_command = "";

// Acquisition definition
bool g_temperatureAcquisition = false;
bool g_humidityAcquisition    = false;
bool g_luminosityAcquisition  = false;
bool g_timeAcquisition        = false;

DHT dht(dht_pin, dht_type);
DS1302 rtc(kCePin, kIoPin, kSclkPin);
LiquidCrystal_I2C lcd(0x27, LCD_Cols, LCD_Rows);

void setup()
{
  String l_display = "";
  
  // Coonfigure LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  l_display = "Initialisation";
  lcd.setCursor((LCD_Cols - l_display.length()) / 2, 0);
  lcd.print(l_display);
  l_display = "...";
  lcd.setCursor((LCD_Cols - l_display.length()) / 2, 1);
  lcd.print(l_display);

  // Launch DHT sensor
  dht.begin();

  // Configure rtc
  rtc.halt(false);
  rtc.writeProtect(false);
  rtc.setDOW(FRIDAY);
  rtc.setTime(21, 15, 0);
  rtc.setDate(4, 8, 2017);

  // Configure pin mode
  pinMode(red_pin,    OUTPUT);
  pinMode(green_pin,  OUTPUT);
  pinMode(blue_pin,   OUTPUT);
  pinMode(relay1_pin, OUTPUT);
  pinMode(relay2_pin, OUTPUT);

  digitalWrite(red_pin,    LOW);
  digitalWrite(green_pin,  LOW);
  digitalWrite(blue_pin,   LOW);
  digitalWrite(relay1_pin, LOW);
  digitalWrite(relay2_pin, LOW);

  // Configure Serial communication
  Serial.begin(RS232_Speed);
  Serial.println("Tapez O pour allumer l'appareil, tapez N pour l'eteindre");

  delay(1000);
  lcd.clear();
}

void loop()
{
  while (Serial.available() > 0)
  {
    delay(50);
    char l_command = Serial.read();
    g_command += (char)l_command;
  }

  if (0 < g_command.length())
  {
    launchCommand(g_command);
    g_command = "";
  }

  if (g_temperatureAcquisition)
  {
    manageTemperature();
  }
  else
  {
    setLed(false, false, false);
  }

  if (g_humidityAcquisition)
  {
    manageHumidity();
  }

  if (g_luminosityAcquisition)
  {
    manageLuminosity();
  }
  else
  {
    digitalWrite(relay1_pin, LOW);
  }

  if (g_timeAcquisition)
  {
    manageTime();
  }

  delay(2000);
}

void setLed(bool p_red, bool p_green, bool p_blue)
{
  if (p_red)
  {
    digitalWrite(red_pin, HIGH);
  }
  else
  {
    digitalWrite(red_pin, LOW);
  }

  if (p_green)
  {
    digitalWrite(green_pin, HIGH);
  }
  else
  {
    digitalWrite(green_pin, LOW);
  }

  if (p_blue)
  {
    digitalWrite(blue_pin, HIGH);
  }
  else
  {
    digitalWrite(blue_pin, LOW);
  }
}

void sendTemperature(float p_temp)
{
  if (p_temp < 18.0)
  {
    setLed(false, false, true);
  }
  else if (p_temp > 24.0)
  {
    setLed(true, false, false);
  }
  else
  {
    setLed(false, true, false);
  }
}

void sendLuminosity(float p_luminosity)
{
  if (Luminosity_Threshold > p_luminosity)
  {
    digitalWrite(relay1_pin, HIGH);
    lcd.setBacklight(255);
  }
  else
  {
    digitalWrite(relay1_pin, LOW);
    lcd.setBacklight(0);
  }
}

void launchCommand(String p_command)
{
  String l_serialCommand = p_command;
  l_serialCommand.toUpperCase();

  lcd.clear();

  if (l_serialCommand.equals("O"))
  {
    Serial.println("L'appareil est allume !");
    Serial.println("Quel capteur activer ?");
    g_timeAcquisition = true;
  }
  else if (l_serialCommand.equals("N"))
  {
    Serial.println("Tapez O pour allumer l'appreil !");
    g_temperatureAcquisition = false;
    g_humidityAcquisition = false;
    g_luminosityAcquisition = false;
    g_timeAcquisition = false;
    digitalWrite(relay1_pin, LOW);
  }
  else if (l_serialCommand.equals("HUMIDITY"))
  {
    g_humidityAcquisition = !g_humidityAcquisition;
  }
  else if (l_serialCommand.equals("TEMPERATURE"))
  {
    g_temperatureAcquisition = !g_temperatureAcquisition;
  }
  else if (l_serialCommand.equals("LUMINOSITY"))
  {
    g_luminosityAcquisition = !g_luminosityAcquisition;
  }
  else
  {
    Serial.print(l_serialCommand);
    Serial.println(" command not take in account !");

    if (LCD_Cols > l_serialCommand.length())
    {
      lcd.setCursor(0, 0);
      lcd.print(l_serialCommand);
    }
    else
    {
      lcd.setCursor(0, 0);
      lcd.print(l_serialCommand.substring(0, LCD_Cols));
      lcd.setCursor(0, 1);
      lcd.print(l_serialCommand.substring(LCD_Cols, l_serialCommand.length()));
    }

    delay(1000);
    lcd.clear();
  }
}

void manageTemperature()
{
  float l_temperature = dht.readTemperature();

  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print((int)l_temperature);
  lcd.print((char)223);
  lcd.print("C");

  sendTemperature(l_temperature);
}

void manageHumidity()
{
  float l_humidity = dht.readHumidity();

  lcd.setCursor(0, 1);
  lcd.print("H:");
  lcd.print((int)l_humidity);
  lcd.print("%");
}

void manageLuminosity()
{
  float light = analogRead(A0);
  float sensor_light = light / 1024 * 100;

  lcd.setCursor(LCD_Cols / 2, 0);
  lcd.print("L:");
  lcd.print(sensor_light);
  lcd.print("%");

  sendLuminosity(sensor_light);
}

void manageTime()
{
  Time t = rtc.getTime();
  // Nb characters for hour
  int time_String_Size = 5;

  lcd.setCursor(LCD_Cols / 2 + (LCD_Cols / 2 - time_String_Size) / 2, 1);
  lcd.print(t.hour);
  lcd.print("H");
  lcd.print(t.min);
}
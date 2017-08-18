#include <LiquidCrystal_I2C.h>
#include <DS1302.h>
#include <Wire.h>

#include "DHT.h"

struct arduino_date {
  uint8_t  dd;
  uint8_t  mm;
  uint16_t yyyy;
};

struct arduino_hour {
  uint8_t hh;
  uint8_t mm;
  uint8_t ss;
};

struct lcd_resolution {
  uint8_t cols;
  uint8_t rows;
};

enum commandArduino {
  unknown     = 0x00,
  DATE        = 0x01,
  TIME        = 0x02,
  TEMPERATURE = 0x04,
  LUMINOSITY  = 0x08,
  HUMIDITY    = 0x0A,
  STARTSTOP   = 0xFF
};

// Pins declaration
#define red_pin    10
#define green_pin  9
#define blue_pin   8
#define dht_pin    7

#define relay1_pin 6
#define relay2_pin 5

// Clock declaration
#define kSclk_pin  4  // Serial Clock
#define kIo_pin    3  // Input/Output
#define kCe_pin    2  // Chip Enable

// Temp. and Hum. sensor type
#define dht_type   DHT11

// Communication definition
#define RS232_Speed 9600

// Sensor definition
#define Luminosity_Threshold    65.0
#define HighTemperatureTreshold 24.0
#define LowTemperatureTreshold  18.0

// Acquisition definition
bool g_temperatureAcquisition = false;
bool g_humidityAcquisition    = false;
bool g_luminosityAcquisition  = false;
bool g_displayHour            = false;

// command Id
commandArduino g_commandId = unknown;

// LCD definition
String g_LCD_Ref   = "LCD1602";

// LCD resolution
lcd_resolution g_LCD_resolution = {16, 2};

// Date hour declaration
arduino_date g_date = {0, 0, 0};
arduino_hour g_hour = {0, 0, 0};

DHT dht(dht_pin, dht_type);
DS1302 rtc(kCe_pin, kIo_pin, kSclk_pin);
LiquidCrystal_I2C lcd(0x27, g_LCD_resolution.cols, g_LCD_resolution.rows);

void setup()
{
  String l_display = "";
  
  // Coonfigure LCD
  lcd.init();
  lcd.setBacklight(0);
  lcd.backlight();
  lcd.clear();
  l_display = "Initialisation";
  lcd.setCursor((g_LCD_resolution.cols - l_display.length()) / 2, 0);
  lcd.print(l_display);
  l_display = "...";
  lcd.setCursor((g_LCD_resolution.cols - l_display.length()) / 2, 1);
  lcd.print(l_display);

  // Launch DHT sensor
  dht.begin();

  // Configure rtc
  rtc.halt(false);
  rtc.writeProtect(false);

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
  // Command definition
  String l_command = "";

  while (Serial.available() > 0)
  {
    delay(50);
    char l_character = Serial.read();
    l_command += (char)l_character;
  }

  if (0 < l_command.length())
  {
    launchCommand(l_command);
    l_command = "";
  }

  if (g_temperatureAcquisition)
  {
    manageTemperature();
  }
  else
  {
    digitalWrite(relay2_pin, LOW);
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

  if (g_displayHour)
  {
    displayHour();
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

void sendTemperature(float p_temperature)
{
  if (LowTemperatureTreshold > p_temperature)
  {
    digitalWrite(relay2_pin, LOW);
    setLed(false, false, true);
  }
  else if (HighTemperatureTreshold < p_temperature)
  {
    digitalWrite(relay2_pin, HIGH);
    setLed(true, false, false);
  }
  else
  {
    digitalWrite(relay2_pin, LOW);
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
    g_commandId = STARTSTOP;
    Serial.println("L'appareil est allume !");
    Serial.println("Quel capteur activer ?");
    g_displayHour = true;
    lcd.setBacklight(255);
  }
  else if (l_serialCommand.equals("N"))
  {
    g_commandId = STARTSTOP;
    Serial.println("Tapez O pour allumer l'appreil !");
    g_temperatureAcquisition = false;
    g_humidityAcquisition    = false;
    g_luminosityAcquisition  = false;
    g_displayHour            = false;
    digitalWrite(relay1_pin, LOW);
    digitalWrite(relay2_pin, LOW);
    lcd.setBacklight(0);
  }
  else if (l_serialCommand.equals("HUMIDITY"))
  {
    g_commandId = HUMIDITY;
  }
  else if (l_serialCommand.equals("TEMPERATURE"))
  {
    g_commandId = TEMPERATURE;
  }
  else if (l_serialCommand.equals("LUMINOSITY"))
  {
    g_commandId = LUMINOSITY;
  }
  else if (l_serialCommand.equals("TIME"))
  {
    Serial.println("Quelle est l'heure ?");
    g_commandId = TIME;
  }
  else if (l_serialCommand.equals("DATE"))
  {
    Serial.println("Quelle est la date ?");
    g_commandId = DATE;
  }
  else
  {
    if(2 < g_commandId)
    {
      g_commandId = unknown;
    }
  }
  
  switch(g_commandId)
  {
    case TIME:
      if((uint8_t)2 == nbCaracterFound(l_serialCommand, char(32)))
      {
        g_hour.hh = l_serialCommand.substring(0, l_serialCommand.indexOf(" ")).toInt();
        g_hour.mm = l_serialCommand.substring(l_serialCommand.indexOf(" "), l_serialCommand.lastIndexOf(" ")).toInt();
        g_hour.ss = 0;
      }
      else if((uint8_t)2 == nbCaracterFound(l_serialCommand, char(47)))
      {
        g_hour.hh = l_serialCommand.substring(0, l_serialCommand.indexOf("/")).toInt();
        g_hour.mm = l_serialCommand.substring(l_serialCommand.indexOf("/"), l_serialCommand.lastIndexOf("/")).toInt();
        g_hour.ss = 0;
      }
      else if((uint8_t)2 == nbCaracterFound(l_serialCommand, char(58)))
      {
        g_hour.hh = l_serialCommand.substring(0, l_serialCommand.indexOf(":")).toInt();
        g_hour.mm = l_serialCommand.substring(l_serialCommand.indexOf(":"), l_serialCommand.lastIndexOf(":")).toInt();
        g_hour.ss = 0;
      }
      sendTime(g_hour.hh, g_hour.mm, g_hour.ss);
      break;
    case DATE:
      if((uint8_t)2 == nbCaracterFound(l_serialCommand, char(32)))
      {
        g_date.dd = l_serialCommand.substring(0, l_serialCommand.indexOf(" ")).toInt();
        g_date.mm = l_serialCommand.substring(l_serialCommand.indexOf(" "), l_serialCommand.lastIndexOf(" ")).toInt();
        g_date.yyyy = l_serialCommand.substring(l_serialCommand.lastIndexOf(" ")).toInt();
      }
      else if((uint8_t)2 == nbCaracterFound(l_serialCommand, char(47)))
      {
        g_date.dd = l_serialCommand.substring(0, l_serialCommand.indexOf("/")).toInt();
        g_date.mm = l_serialCommand.substring(l_serialCommand.indexOf("/"), l_serialCommand.lastIndexOf("/")).toInt();
        g_date.yyyy = l_serialCommand.substring(l_serialCommand.lastIndexOf("/")).toInt();
      }
      else if((uint8_t)2 == nbCaracterFound(l_serialCommand, char(58)))
      {
        g_date.dd = l_serialCommand.substring(0, l_serialCommand.indexOf(":")).toInt();
        g_date.mm = l_serialCommand.substring(l_serialCommand.indexOf(":"), l_serialCommand.lastIndexOf(":")).toInt();
        g_date.yyyy = l_serialCommand.substring(l_serialCommand.lastIndexOf(":")).toInt();
      }
      sendDate(g_date.dd, g_date.mm, g_date.yyyy);
      break;
    case TEMPERATURE:
      g_temperatureAcquisition = !g_temperatureAcquisition;
      break;
    case HUMIDITY:
      g_humidityAcquisition = !g_humidityAcquisition;
      break;
    case LUMINOSITY:
      g_luminosityAcquisition = !g_luminosityAcquisition;
      break;
    case STARTSTOP:
      break;
    default:
      Serial.print("g_commandId = ");
      Serial.println(g_commandId);
      Serial.print(l_serialCommand);
      Serial.println(" command not take in account !");

      if (g_LCD_resolution.cols > l_serialCommand.length())
      {
        lcd.setCursor(0, 0);
        lcd.print(l_serialCommand);
      }
      else
      {
        for(int i = 0; i < g_LCD_resolution.rows; i++)
        {
          lcd.setCursor(0, i);
          lcd.print(l_serialCommand.substring(i * g_LCD_resolution.cols, (i + 1) * g_LCD_resolution.cols));
        }
      }
      break;
  }

  delay(1000);
  lcd.clear();
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

  lcd.setCursor(0, g_LCD_resolution.rows - 1);
  lcd.print("H:");
  lcd.print((int)l_humidity);
  lcd.print("%");
}

void manageLuminosity()
{
  float light = analogRead(A0);
  float sensor_light = light / 1024 * 100;

  lcd.setCursor(g_LCD_resolution.cols / 2, 0);
  lcd.print("L:");
  lcd.print(sensor_light);
  lcd.print("%");

  sendLuminosity(sensor_light);
}

void displayHour()
{
  Time t = rtc.getTime();
  // Nb characters for hour
  uint8_t l_TimeStringSize = 5;

  lcd.setCursor(g_LCD_resolution.cols / 2 + (g_LCD_resolution.cols / 2 - l_TimeStringSize) / 2, g_LCD_resolution.rows - 1);
  lcd.print(t.hour);
  lcd.print("H");
  lcd.print(t.min);
}

void sendTime(uint8_t p_hour, uint8_t p_minutes, uint8_t p_secondes)
{
  rtc.setTime(p_hour, p_minutes, p_secondes);
}

void sendDate(uint8_t p_day, uint8_t p_month, uint16_t p_year)
{
  rtc.setDate(p_day, p_month, p_year);
}

bool isValidNumber(String p_string)
{
  bool l_returnValue = true;
  
  for(byte i = 0; i < p_string.length() && l_returnValue != false; i++)
  {
    if(isDigit(p_string.charAt(i)))
    {
      l_returnValue &= true;
    }
    else
    {
      l_returnValue &= false;
    }
  }
  
  return l_returnValue;
}

uint8_t nbCaracterFound(String p_chaine, char p_subString)
{
  uint8_t i = 0;
  uint8_t l_researchCharacter = 0;
  
  do
  {
    if(p_chaine.charAt(i) == p_subString)
    {
      l_researchCharacter++;
    }
    i++;
  } while(i < p_chaine.length());
  
  return l_researchCharacter;
}


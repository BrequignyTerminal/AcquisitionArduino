#include <DS1302.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <Wire.h>

#include "DHT.h"

enum screen
{
  noScreen    = 0x00,
  LCD1602     = 0x01,
  LCD2004     = 0x02
};

enum commandArduino
{
  noCommand      = 0x00,

  // LCD management
  DATE           = 0x01,
  TIME           = 0x02,
  TEMPERATURE    = 0x04,
  LUMINOSITY     = 0x08,
  HUMIDITY       = 0x10,
  ROOM           = 0x20,
  GET_ROOM       = 0x40,

  // HC 05/06 commands
  HC_COMMAND     = 0x80,
  GET_HC_COMMAND = 0x81,
  HC_VERIF_COMM  = 0x82,
  HC_VERSION     = 0x83,
  HC_BAUD        = 0x84,
  
  STARTSTOP      = 0xFF
};

enum displayMemory
{
  noMemory           = 0x00,
  DATE_MEMORY        = 0x01,
  TIME_MEMORY        = 0x02,
  TEMPERATURE_MEMORY = 0x04,
  LUMINOSITY_MEMORY  = 0x08,
  HUMIDITY_MEMORY    = 0x10,
  ROOM_MEMORY        = 0x20,

  ROOM_STRING_MEMORY = 0x40
};

struct arduino_date
{
  uint8_t  dd;
  uint8_t  mm;
  uint16_t yyyy;
};

struct arduino_hour
{
  uint8_t hh;
  uint8_t mm;
  uint8_t ss;
};

struct lcd_resolution
{
  screen  name;
  uint8_t cols;
  uint8_t rows;
};

// Pins declaration
#define hc06_TX    13
#define hc06_RX    12

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
#define dht_type   DHT22

// Communication definition
#define RS232_Speed 9600
#define HC06_Speed  9600

// Sensor definition
#define Luminosity_Threshold    65.0
#define HighTemperatureTreshold 24.0
#define LowTemperatureTreshold  18.0

#define RoomStringLength  15
#define AcquisitionDelay  1000

// command Id
commandArduino g_commandId = noCommand;
displayMemory  g_displayMemory = noMemory;

// LCD definition
screen g_LCD_Ref = LCD2004;

// LCD resolution
lcd_resolution g_LCD_resolution = {g_LCD_Ref, 20, 4};

// Date hour declaration
arduino_date g_date = {0, 0, 0};
arduino_hour g_hour = {0, 0, 0};

DHT dht(dht_pin, dht_type);
DS1302 rtc(kCe_pin, kIo_pin, kSclk_pin);
LiquidCrystal_I2C lcd(0x27, g_LCD_resolution.cols, g_LCD_resolution.rows);
SoftwareSerial hc06(hc06_RX, hc06_TX);

void setup()
{
  String l_display = "";
  
  // Coonfigure LCD
  lcd.init();
  lcd.setBacklight(0);
  lcd.backlight();
  lcd.clear();
  l_display = "Initialisation";
  
  if(LCD1602 == g_LCD_resolution.name)
  {
    lcd.setCursor((g_LCD_resolution.cols - l_display.length()) / 2, 0);
  }
  else if(LCD2004 == g_LCD_resolution.name)
  {
    lcd.setCursor((g_LCD_resolution.cols - l_display.length()) / 2, 1);
  }
  
  lcd.print(l_display);
  l_display = "...";
  
  if(LCD1602 == g_LCD_resolution.name)
  {
    lcd.setCursor((g_LCD_resolution.cols - l_display.length()) / 2, 1);
  }
  else if(LCD2004 == g_LCD_resolution.name)
  {
    lcd.setCursor((g_LCD_resolution.cols - l_display.length()) / 2, 2);
  }
  
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
  pinMode(hc06_RX,    INPUT); 
  pinMode(hc06_TX,    OUTPUT);  

  digitalWrite(red_pin,    LOW);
  digitalWrite(green_pin,  LOW);
  digitalWrite(blue_pin,   LOW);
  digitalWrite(relay1_pin, LOW);
  digitalWrite(relay2_pin, LOW);

  // Configure Serial communication
  Serial.begin(RS232_Speed);

  // HC06 configuration
  hc06.begin(HC06_Speed);
  
  sendMessageToComm("Tapez O pour allumer l'appareil, tapez N pour l'eteindre");
   
  delay(1000);
  lcd.clear();
}

void loop()
{
  // Command definition
  String l_command = "";
  
  if(hc06.available())
  {
    while (hc06.available() > 0)
    {
      char l_hcCharacter = hc06.read();
      l_command += (char)l_hcCharacter;
    }
  }
  
  if(Serial.available())
  {
    while (Serial.available() > 0)
    {
      delay(50);
      char l_serialCharacter = Serial.read();
      l_command += (char)l_serialCharacter;
    }
  }
  
  if (0 < l_command.length())
  {
    launchCommand(l_command);
    l_command = "";
  }

  if (1 == EEPROM.read(TEMPERATURE_MEMORY))
  {
    manageTemperature();
  }
  else
  {
    digitalWrite(relay2_pin, LOW);
    setLed(false, false, false);
  }

  if (1 == EEPROM.read(HUMIDITY_MEMORY))
  {
    manageHumidity();
  }

  if (1 == EEPROM.read(LUMINOSITY_MEMORY))
  {
    manageLuminosity();
  }
  else
  {
    digitalWrite(relay1_pin, LOW);
  }

  if (1 == EEPROM.read(ROOM_MEMORY))
  {
    manageRoom();
  }
  
  if (1 == EEPROM.read(TIME_MEMORY))
  {
    displayHour();
  }

  delay(AcquisitionDelay);
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
  String l_commSerieCommand = "";
  bool l_bytesSend = false;

  lcd.clear();

  if (l_serialCommand.equals("O"))
  {
    g_commandId = STARTSTOP;

    sendMessageToComm("L'appareil est allume !");
    sendMessageToComm("Quelle commande envoyer ?");
    EEPROM.write(TIME_MEMORY, 1);
    EEPROM.write(ROOM_MEMORY, 1);
    lcd.setBacklight(255);
  }
  else if (l_serialCommand.equals("N"))
  {
    g_commandId = STARTSTOP;
    sendMessageToComm("Tapez O pour allumer l'appareil !");
    EEPROM.write(TEMPERATURE_MEMORY, 0);
    EEPROM.write(HUMIDITY_MEMORY,    0);
    EEPROM.write(LUMINOSITY_MEMORY,  0);
    EEPROM.write(TIME_MEMORY,        0);
    EEPROM.write(ROOM_MEMORY,        0);
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
  else if (l_serialCommand.startsWith("TIME="))
  {
    g_commandId = TIME;
    l_serialCommand = l_serialCommand.substring(l_serialCommand.indexOf("=") + 1);
  }
  else if (l_serialCommand.startsWith("DATE="))
  {
    g_commandId = DATE;
    l_serialCommand = l_serialCommand.substring(l_serialCommand.indexOf("=") + 1);
  }
  else if (l_serialCommand.startsWith("ROOM="))
  {
    g_commandId = ROOM;
    l_serialCommand = l_serialCommand.substring(l_serialCommand.indexOf("=") + 1);
  }
  else if (l_serialCommand.equals("GET_ROOM"))
  {
    g_commandId = GET_ROOM;
  }
  else if (l_serialCommand.equals("AT+VERSION"))
  {
    g_commandId = HC_VERSION;
  }
  else if (l_serialCommand.startsWith("AT+BAUD"))
  {
    g_commandId = HC_BAUD;
  }
  else if (l_serialCommand.startsWith("AT+"))
  {
    g_commandId = HC_COMMAND;
  }
  else if (l_serialCommand.equals("AT"))
  {
    g_commandId = HC_VERIF_COMM;
  }
  else if (l_serialCommand.startsWith("OK"))
  {
    g_commandId = GET_HC_COMMAND;
  }
  else
  {
    g_commandId = noCommand;
  }
  
  switch(g_commandId)
  {
    case TIME:
      if((uint8_t)1 == nbCaracterFound(l_serialCommand, char(32)))
      {
        g_hour.hh = l_serialCommand.substring(0, l_serialCommand.indexOf(" ")).toInt();
        g_hour.mm = l_serialCommand.substring(l_serialCommand.indexOf(" ") + 1).toInt();
        g_hour.ss = 0;
      }
      else if((uint8_t)2 == nbCaracterFound(l_serialCommand, char(32)))
      {
        g_hour.hh = l_serialCommand.substring(0, l_serialCommand.indexOf(" ")).toInt();
        g_hour.mm = l_serialCommand.substring(l_serialCommand.indexOf(" ") + 1, l_serialCommand.lastIndexOf(" ")).toInt();
        g_hour.ss = 0;
      }
      else if((uint8_t)1 == nbCaracterFound(l_serialCommand, char(47)))
      {
        g_hour.hh = l_serialCommand.substring(0, l_serialCommand.indexOf("/")).toInt();
        g_hour.mm = l_serialCommand.substring(l_serialCommand.indexOf("/") + 1).toInt();
        g_hour.ss = 0;
      }
      else if((uint8_t)2 == nbCaracterFound(l_serialCommand, char(47)))
      {
        g_hour.hh = l_serialCommand.substring(0, l_serialCommand.indexOf("/")).toInt();
        g_hour.mm = l_serialCommand.substring(l_serialCommand.indexOf("/") + 1, l_serialCommand.lastIndexOf("/")).toInt();
        g_hour.ss = 0;
      }
      else if((uint8_t)1 == nbCaracterFound(l_serialCommand, char(58)))
      {
        g_hour.hh = l_serialCommand.substring(0, l_serialCommand.indexOf(":")).toInt();
        g_hour.mm = l_serialCommand.substring(l_serialCommand.indexOf(":") + 1).toInt();
        g_hour.ss = 0;
      }
      else if((uint8_t)2 == nbCaracterFound(l_serialCommand, char(58)))
      {
        g_hour.hh = l_serialCommand.substring(0, l_serialCommand.indexOf(":")).toInt();
        g_hour.mm = l_serialCommand.substring(l_serialCommand.indexOf(":") + 1, l_serialCommand.lastIndexOf(":")).toInt();
        g_hour.ss = 0;
      }
      sendTime(g_hour.hh, g_hour.mm, g_hour.ss);
      break;
    case DATE:
      if((uint8_t)2 == nbCaracterFound(l_serialCommand, char(32)))
      {
        g_date.dd = l_serialCommand.substring(0, l_serialCommand.indexOf(" ")).toInt();
        g_date.mm = l_serialCommand.substring(l_serialCommand.indexOf(" ") + 1, l_serialCommand.lastIndexOf(" ")).toInt();
        g_date.yyyy = l_serialCommand.substring(l_serialCommand.lastIndexOf(" ") + 1).toInt();
      }
      else if((uint8_t)2 == nbCaracterFound(l_serialCommand, char(47)))
      {
        g_date.dd = l_serialCommand.substring(0, l_serialCommand.indexOf("/")).toInt();
        g_date.mm = l_serialCommand.substring(l_serialCommand.indexOf("/") + 1, l_serialCommand.lastIndexOf("/")).toInt();
        g_date.yyyy = l_serialCommand.substring(l_serialCommand.lastIndexOf("/") + 1).toInt();
      }
      else if((uint8_t)2 == nbCaracterFound(l_serialCommand, char(58)))
      {
        g_date.dd = l_serialCommand.substring(0, l_serialCommand.indexOf(":")).toInt();
        g_date.mm = l_serialCommand.substring(l_serialCommand.indexOf(":") + 1, l_serialCommand.lastIndexOf(":")).toInt();
        g_date.yyyy = l_serialCommand.substring(l_serialCommand.lastIndexOf(":") + 1).toInt();
      }
      sendDate(g_date.dd, g_date.mm, g_date.yyyy);
      break;
    case ROOM:
      setRoomToEEPROM(l_serialCommand);
      break;
    case GET_ROOM:
      sendMessageToComm(getRoomFromEEPROM(RoomStringLength));
      break;
    case TEMPERATURE:
      EEPROM.write(TEMPERATURE_MEMORY, (EEPROM.read(TEMPERATURE_MEMORY) + 1) % 2);
      break;
    case HUMIDITY:
      EEPROM.write(HUMIDITY_MEMORY, (EEPROM.read(HUMIDITY_MEMORY) + 1) % 2);
      break;
    case LUMINOSITY:
      EEPROM.write(LUMINOSITY_MEMORY, (EEPROM.read(LUMINOSITY_MEMORY) + 1) % 2);
      break;
    case STARTSTOP:
      break;
    case HC_COMMAND:
      l_commSerieCommand = "HC_COMMAND = ";
      l_commSerieCommand.concat(l_serialCommand);
      l_bytesSend = sendToHCCommand(l_serialCommand);
      l_commSerieCommand.concat(" - l_bytesSend = ");
      l_commSerieCommand.concat((String)l_bytesSend);
      sendMessageToComm(l_commSerieCommand);
      break;
    case GET_HC_COMMAND:
      sendMessageToComm(l_serialCommand);
      break;
    case HC_VERIF_COMM:
      hc06.write("AT");
      break;
    case HC_VERSION:
      hc06.write("AT+VERSION");
      break;
    case HC_BAUD:
      l_commSerieCommand = "AT+BAUD";
      l_commSerieCommand.concat(l_serialCommand.charAt(l_serialCommand.length() - 1));
      sendToHCCommand(l_commSerieCommand);
      break;
    default:
      l_commSerieCommand = "g_commandId = ";
      l_commSerieCommand.concat((String)g_commandId);
      sendMessageToComm(l_commSerieCommand);
      l_commSerieCommand = l_serialCommand;
      l_commSerieCommand.concat(" command not take in account !");
      sendMessageToComm(l_commSerieCommand);

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
  // Nb characters for temperature
  uint8_t l_TemperatureStringSize = 6;

  if(LCD1602 == g_LCD_resolution.name)
  {
    lcd.setCursor(0, 0);
  }
  else if(LCD2004 == g_LCD_resolution.name)
  {
    lcd.setCursor((g_LCD_resolution.cols - l_TemperatureStringSize) / 4, 1);
  }

  lcd.print("T:");
  lcd.print((int)l_temperature);
  lcd.print((char)223);
  lcd.print("C");

  sendTemperature(l_temperature);
}

void manageHumidity()
{
  float l_humidity = dht.readHumidity();
  // Nb characters for humidity
  uint8_t l_HumidityStringSize = 6;

  if(LCD1602 == g_LCD_resolution.name)
  {
    lcd.setCursor(0, g_LCD_resolution.rows - 1);
  }
  else if(LCD2004 == g_LCD_resolution.name)
  {
    lcd.setCursor((g_LCD_resolution.cols - l_HumidityStringSize) / 4, 2);
  }

  lcd.print("H:");
  lcd.print((int)l_humidity);
  lcd.print("%");
}

void manageLuminosity()
{
  float light = analogRead(A0);
  float sensor_light = light / 1024 * 100;
  // Nb characters for luminosity
  uint8_t l_LuminosityStringSize = 8;

  if(LCD1602 == g_LCD_resolution.name)
  {
    lcd.setCursor(g_LCD_resolution.cols / 2, 0);
  }
  else if(LCD2004 == g_LCD_resolution.name)
  {
    lcd.setCursor(g_LCD_resolution.cols / 2 + (g_LCD_resolution.cols / 2 - l_LuminosityStringSize) / 2, 2);
  }

  lcd.print("L:");
  lcd.print(sensor_light);
  lcd.print("%");

  sendLuminosity(sensor_light);
}

void manageRoom()
{
  String l_room = getRoomFromEEPROM(RoomStringLength);
  
  if(LCD2004 == g_LCD_resolution.name)
  {
    lcd.setCursor((g_LCD_resolution.cols - l_room.length()) / 2, 0);
  }
  
  lcd.print(l_room);
}

void displayHour()
{
  Time t = rtc.getTime();
  // Nb characters for hour
  uint8_t l_TimeStringSize = 5;

  if(LCD1602 == g_LCD_resolution.name)
  {
    lcd.setCursor(g_LCD_resolution.cols / 2 + (g_LCD_resolution.cols / 2 - l_TimeStringSize) / 2, g_LCD_resolution.rows - 1);
  }
  else if(LCD2004 == g_LCD_resolution.name)
  {
    lcd.setCursor(g_LCD_resolution.cols / 2 - 2, g_LCD_resolution.rows - 1);
  }

  if(10 > t.hour)
  {
    lcd.print("0");
    lcd.print(t.hour);
  }
  else
  {
    lcd.print(t.hour);
  }
  
  lcd.print("H");

  if(10 > t.min)
  {
    lcd.print("0");
    lcd.print(t.min);
  }
  else
  {
    lcd.print(t.min);
  }
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

void setRoomToEEPROM(String p_chaine)
{
  uint8_t i = ROOM_STRING_MEMORY;
  uint8_t j = 0;

  for(; i < ROOM_STRING_MEMORY + p_chaine.length(); ++i, ++j)
  {
    EEPROM.write(i, p_chaine.charAt(j));
  }
  for(i; i < ROOM_STRING_MEMORY + RoomStringLength; ++i)
  {
    EEPROM.write(i, 0);
  }
}

String getRoomFromEEPROM(uint8_t p_chaineLength)
{
  String l_returnValue = "";

  for(uint8_t i = ROOM_STRING_MEMORY; i < ROOM_STRING_MEMORY + p_chaineLength; ++i)
  {
    if(0 < EEPROM.read(i))
    {
      l_returnValue.concat((char)EEPROM.read(i));
    }
  }

  return l_returnValue;
}

bool sendToHCCommand(String p_command)
{
  bool l_errorSendValue = false;
  bool l_returnValue = false;
  int l_bytesSent = 0;
  String l_command = "sendToHCCommand => p_command = ";
  l_command.concat(p_command);
  
  sendMessageToComm(l_command);
  
  for(uint8_t i = 0; i < p_command.length(); ++i)
  {
    l_command = "sendToHCCommand => i = ";
    l_command.concat(i);
    l_command.concat(" => ");
    l_command.concat((String)p_command.charAt(i));
    
    sendMessageToComm(l_command);
    l_bytesSent = hc06.write(p_command.charAt(i));

    if(0 < l_bytesSent)
    {
      l_returnValue |= true;
    }
    else
    {
      l_returnValue &= false;
      l_errorSendValue = true;
    }
  }

  l_returnValue &= !l_errorSendValue;

  return l_returnValue;
}

void sendMessageToComm(String p_message)
{
  Serial.println(p_message);
  hc06.println(p_message);
}


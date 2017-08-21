//introduire les bibliotheques
#include <LiquidCrystal_I2C.h>
#include <DS1302.h>
#include <Wire.h>
#include "DHT.h"

#define DHTPIN 7
#define DHTTYPE DHT11
#define RS232_Speed 9600

//definir les pins
#define red_pin  10
#define green_pin  9
#define blue_pin  8
#define relay_pin  6
#define relay_v_pin  5
#define kSclk_pin  4  // Serial Clock
#define kIo_pin    3  // Input/Output
#define kCe_pin    2  // Chip Enable

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 20, 4);
DS1302 rtc (kCe_pin, kIo_pin, kSclk_pin);

struct arduino_hour
{
  uint8_t hh;
  uint8_t mm;
};

enum commandArduino
{
  unknown     = 0x00,
  TIME        = 0x01,
  ROOM        = 0x02,
  STARTSTOP   = 0xFF
};

commandArduino g_commandID = unknown;
arduino_hour g_hour = {0, 0};

bool g_displayHour = false;
bool g_Acquisition = false;
bool g_displayRoom = false;

void setup()
{
  lcd.init();
  lcd.backlight();
  
  rtc.halt(false);
  rtc.writeProtect(false);

  dht.begin();
  pinMode(relay_pin, OUTPUT);
  pinMode(relay_v_pin, OUTPUT);
  pinMode(red_pin, OUTPUT);
  pinMode(green_pin, OUTPUT);
  pinMode(blue_pin, OUTPUT);

  digitalWrite(relay_v_pin, LOW);
  digitalWrite(relay_pin, LOW);
   
  Serial.begin(RS232_Speed);
  Serial.println("tapez O pour allumer l'appareil, tapez N pour l'eteindre ");
  
  lcd.clear();
  lcd.setCursor(3, 1);
  lcd.print("Initialisation");
  lcd.setCursor(8, 2);
  lcd.print("...");
  delay (2000);
  lcd.clear();
}

void loop()
{
  String l_command = "";
  
  while (Serial.available() > 0)
  {
    delay(50);
    char l_character = Serial.read();
    l_command += (char)l_character;
  }

  if(0 < l_command.length())
  {
    launchCommand(l_command);
    l_command = "";
  }

  if(true == g_Acquisition)
  {
    lcd.setBacklight(1);
    manageTemperature();
    manage_luminosity(); 
  }
  if(true == g_displayHour)
  {
    manage_heure();
  }
  delay(2000);
}

void setLEDs(int red, int green, int blue)
{
  if(red == 1)
  { 
    digitalWrite(red_pin, HIGH);
  }
  else
  {
    digitalWrite(red_pin, LOW);
  }
  
  if(green == 1)
  {
    digitalWrite(green_pin, HIGH);
  }
  else
  {
    digitalWrite(green_pin, LOW);
  }
  
  if(blue == 1)
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
    setLEDs(0, 0, 1); 
  }
  else if (p_temp > 24.0)
  {
    setLEDs(1, 0, 0);
  }
  else
  {
    setLEDs(0, 1, 0);
  }
  if (22.0 < p_temp)
  {
    digitalWrite(relay_v_pin, HIGH);
  }
  else
  {
    digitalWrite(relay_v_pin, LOW);
  }
}

void launchCommand(String p_command)
{
  String l_serialCommand = p_command;
  l_serialCommand.toUpperCase();

  lcd.clear();

  if(l_serialCommand.equals("O"))
  {
    Serial.println("L'appareil est allume !");
    g_Acquisition = true;
    g_commandID = STARTSTOP;
  }
  else if(l_serialCommand.equals("N"))
  {
    g_commandID = STARTSTOP;
    setLEDs(0, 0, 0);
    lcd.setBacklight(0);
    
    digitalWrite(relay_pin, LOW);
    digitalWrite(relay_v_pin, LOW);
    
    Serial.println("Tapez O pour allumer l'appareil !");
    
    g_Acquisition = false;
    g_displayHour = false;
    g_displayRoom = false;
  
  }
  else if (l_serialCommand.startsWith("TIME="))
  {
    Serial.println("Quelle heure est-il ?");
    g_commandID = TIME;
    g_displayHour = true;
    l_serialCommand = l_serialCommand.substring(l_serialCommand.indexOf("=")+1);
  }
  else if (l_serialCommand.startsWith("ROOM="))
  {
    Serial.println("Quelle est le lieu ?");
    g_commandID = ROOM;
    g_displayRoom = true;
    l_serialCommand = l_serialCommand.substring(l_serialCommand.indexOf("=")+1);
  }
  else
  {
    g_commandID = unknown;
  }
  
 
  switch(g_commandID)
  {
   
    case TIME:
    Serial.println(l_serialCommand);
      if((uint8_t)1 == nbCaracterFound(l_serialCommand, char(32)))
      {
        g_hour.hh = l_serialCommand.substring(0, l_serialCommand.indexOf(" ")).toInt();
        g_hour.mm = l_serialCommand.substring(l_serialCommand.indexOf(" ")).toInt();      
      }
      else if((uint8_t)1 == nbCaracterFound(l_serialCommand, char(47)))
      {
        g_hour.hh = l_serialCommand.substring(0, l_serialCommand.indexOf("/")).toInt();
        g_hour.mm = l_serialCommand.substring(l_serialCommand.indexOf("/")).toInt();
      }
      else if((uint8_t)1 == nbCaracterFound(l_serialCommand, char(58)))
      {
        g_hour.hh = l_serialCommand.substring(0, l_serialCommand.indexOf(":")).toInt();
        g_hour.mm = l_serialCommand.substring(l_serialCommand.indexOf(":")).toInt();
      }
      sendTime(g_hour.hh, g_hour.mm);
      break;
    case ROOM:
      Serial.println(l_serialCommand);
      lcd.setCursor((20 - l_serialCommand.length())/2,0);
      lcd.print(l_serialCommand);
      break;
    case STARTSTOP:
      break;
    default:

      Serial.print("g_commandID = ");
      Serial.println(g_commandID);
      Serial.print(l_serialCommand);
      Serial.println(" command not take in account !");

      break;   
  }
}

void manageTemperature()
{
  float l_temperature = dht.readTemperature();
  float l_humidity = dht.readHumidity();

  lcd.setCursor(2, 2);
  lcd.print("T:");
  lcd.print((int)l_temperature);
  lcd.print((char)223);
  lcd.print("C");
  lcd.print("  |");

  sendTemperature(l_temperature);

  lcd.setCursor(2, 3);
  lcd.print("H:");
  lcd.print((int)l_humidity);
  lcd.print("%");
  lcd.print("   |");
}

void manage_luminosity()
{
  float sensor_light = analogRead(A0);
  float light = sensor_light /1024 * 100;

  if (50 > light)
  {
    digitalWrite(relay_pin, HIGH);
  }
  else
  {
    digitalWrite(relay_pin, LOW);  
  }
  lcd.setCursor(13,2);
  lcd.print("L:");
  lcd.print((int)light);
  lcd.print("%");
}

void manage_heure()
{
  Time t = rtc.getTime();

  lcd.setCursor(13, 3);
  if (10 > t.hour)
  {
     lcd.print("0");
     lcd.print(t.hour);
  }
  else
  {
    lcd.print(t.hour);
  }
  
  lcd.print("H");
  if (10 > t.min)
  {
     lcd.print("0");
     lcd.print(t.min);
  }
  else
  {
    lcd.print(t.min);
  }
}

void sendTime(uint8_t p_hour, uint8_t p_minutes)
{
  rtc.setTime(p_hour, p_minutes, 0);
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
  }
  while(i < p_chaine.length());
  
  return l_researchCharacter;
}

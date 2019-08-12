//#include <printf.h>
#include <arduino.h>
#include <EEPROM.h>
#include <SPI.h>
#include <nRF24L01.h> // библиотека для nRF24L01+
#include <RF24.h>     // библиотека для радио модуля
#include <Keypad.h> //keypad lib
#include "SWTFT.h" // Hardware-specific library
#include <Adafruit_GFX.h>    // Core graphics library
//
//
//Arduino           MEGA
//10 - SD_SS        53      CS
//11 - SD-D1        51      MOSI
//12 - SD_D0        50      MISO
//13 - SD_SCK       52      CLK
//
//  RF24(uint16_t _cepin, uint16_t _cspin);

RF24 radio(49,53); 

void(* resetFunc) (void) = 0;//объявляем функцию reset с адресом 0 (123 from keyb)

// Assign human-readable names to some common 16-bit color values:
#define	BLACK   0x0000
#define	GRAY    (0x55 << 11) + (0x55 << 5) + 0x55
#define	BLUE    0x000F
#define	RED     0xF800
#define	GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

SWTFT tft;

//RADIO
const uint64_t pipe[1] = {0x0DEADF00D0LL}; // идентификатор передачи

//RF data array
unsigned int data[3] {0,0,0}; //30 sec average, last sensor reading and  send errors num will be sent by radiomodem (in mm)
byte ackdata[2] {3,0};

int SensorErrors = 0;
int MaxErrors = 15; //
bool IgnoreSensorError = false;
unsigned long wlevel = 0;
bool sensorActive = false;

///KEYBOARD
const byte ROWS = 4; // строки
const byte COLS = 4; // столбца
char keys[ROWS][COLS] = 
{
  {'D','#','0','*'},
  {'C','9','8','7'},
  {'B','6','5','4'},
  {'A','3','2','1'}
};
byte rowPins[ROWS] = {30, 32, 34, 36}; // подключить к выводам строк клавиатуры
byte colPins[COLS] = {22, 24, 26, 28}; // подключить к выводам столбцов клавиатуры
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

//timer TFT
unsigned long timenow = 0;
unsigned long showtime = 1000; 

//time radio read
unsigned long radiotimenow = 0;
unsigned long radiointerval = 200; 

//beep on error
unsigned long lcdbacktimenow = 0;
unsigned long lcdbackshowtime = 1500;
//bool lcdactive = true;

///
/// counter for turn on relay by sensor events with delay
///
int relayOnEvents = 0;
unsigned long tryToOnTime = 0;

//piezo beeper
int Beep = 62;// A8;
int BeepGround = 63;//A9;

bool automode = false;

int Relay = 64;//A10; 
bool curRelayState = false;

unsigned int leveltoonrelay = 85;// 85 cmeters 
unsigned int lelevtooffrelay = 50;//0.5 meters

unsigned int secondsToStop = 0;

#define TimerSet 600 // 10 minutes till stop per button press


bool done = false;

bool RadioDataReceived = false;

//Green  LED
//int greenLedPin = 25;
//Red LED
//int redLedPin = 23;


enum DisplayMode 
{
   ModeMain = 0,
   ModeLevelSetup,
   //ModeFlow,
};

DisplayMode currDisplay = ModeMain;

volatile byte pulseCount;  

bool key1pressed = false;
bool key2pressed = false;

//
void SetAutoMode(bool modeset)
{
    automode = modeset; 
    EEPROM.update(0, automode);
}

void SetUpLevel(int level)
{
    EEPROM.update(1, level);
}
void SetDownLevel(int level)
{
    EEPROM.update(2, level);
}
bool GetAutoMode()
{
  return EEPROM.read(0);
}

int GetUpLevel()
{
  return EEPROM.read(1);
}

int GetDownLevel()
{
  return EEPROM.read(2);
}

void SaveSettings()
{
  SetUpLevel(lelevtooffrelay); 
  SetDownLevel(leveltoonrelay);
}
void RelayOn()
{
    if(curRelayState==false)
    {
      digitalWrite(Relay, HIGH);
      //digitalWrite(LED_BUILTIN, HIGH);
      curRelayState = true;
      tone(Beep, 2000, 200);
      delay(150);
      tone(Beep, 1800, 200);
      //totalMilliLitres = 0;
    } else
    {
      //already on
    }
}
void RelayOff()
{
    if(curRelayState==true)
    {
      digitalWrite(Relay, LOW);
      //digitalWrite(LED_BUILTIN, LOW);
      curRelayState = false;
      tone(Beep, 1800, 200);
      delay(150);
      tone(Beep, 2000, 200);
    } 
    else
    {
      //already off  
    }
} 
//will only ON relay in 30 seconds of trying to on - for avoid fantom big levels
//no need to get pump hard
void TryToOnRelay()
{
  if(millis() - tryToOnTime > 1000)
  {
      if(++relayOnEvents > 30)
      {
        // do real relay on here
        RelayOn();
        relayOnEvents = 0;//reset counter
      }
      tryToOnTime = millis();
  }
}
void drawBorder () 
{
  // Draw a border
  //uint16_t width = tft.width() - 1;
  //uint16_t height = tft.height() - 1;
  //uint8_t border = 2;
  tft.fillScreen(WHITE);
  //tft.fillRect(border, border, (width - border * 2), (height - border * 2), WHITE);
}

/* Recode russian fonts from UTF-8 to Windows-1251 */
String utf8rus(String source)
{
  int i,k;
  String target;
  unsigned char n;
  char m[2] = { '0', '\0' };

  k = source.length(); i = 0;

  while (i < k) {
    n = source[i]; i++;

    if (n >= 0xC0) {
      switch (n) {
        case 0xD0: {
          n = source[i]; i++;
          if (n == 0x81) { n = 0xA8; break; }
          if (n >= 0x90 && n <= 0xBF) n = n + 0x30;
          break;
        }
        case 0xD1: {
          n = source[i]; i++;
          if (n == 0x91) { n = 0xB8; break; }
          if (n >= 0x80 && n <= 0x8F) n = n + 0x70;
          break;
        }
      }
    }
    m[0] = n; target = target + String(m);
  }
return target;
}
//float flowRate;
//unsigned long flowMilliLitres;
//unsigned long totalMilliLitres;
//bool ledState = true;
//unsigned long oldFlowTime;

void SetupPicture()
{

  switch (currDisplay)
    {
      case ModeMain:
      {
        {
          // Draw a border
          //uint16_t width = tft.width() - 1;
          //uint16_t height = tft.height() - 1;
          //uint8_t border = 2;
          tft.fillScreen(WHITE);
          //tft.fillRect(border, border, (width - border * 2), (height - border * 2), WHITE);
          tft.fillRect(70, 70, 245, 3, GRAY);
          tft.fillRect(70, 144, 245, 3, GRAY);
          //box
          tft.fillRect(4, 4, 4, 60, BLACK);
          tft.fillRect(4, 64, 60, 4, BLACK);
          tft.fillRect(60, 4, 4, 64, BLACK);
          //water
          tft.fillRect(8, 50, 52, 15, BLUE);
          tft.fillTriangle( 8,50,16,45,32,50,BLUE);
          tft.fillTriangle(32,50,40,45,48,50,BLUE);
          tft.fillTriangle(48,50,52,45,60,50,BLUE);
          
          //lineika
          tft.fillRect(26, 10, 16, 3, BLACK);
          tft.fillRect(26, 10, 3, 40, BLACK);
          tft.fillRect(40, 10, 3, 40, BLACK);

          tft.fillRect(26, 16, 8, 2, BLACK);
          tft.fillRect(26, 24, 8, 2, BLACK);
          tft.fillRect(26, 32, 10, 3, BLACK);
          tft.fillRect(26, 40, 8, 2, BLACK);
          tft.fillRect(26, 48, 8, 2, BLACK);

        
        //pump icon
          tft.drawCircle(33,110,30,BLACK);
          tft.drawCircle(33,110,29,BLACK);
          tft.drawCircle(33,110,28,BLACK);
          tft.fillCircle(33,110,14,CYAN);
          tft.drawCircle(33,110,15,BLACK);

          tft.drawPixel(33,110, RED);
          tft.drawCircle(33,110,1,RED);
          tft.drawCircle(33,110,2,RED);
          tft.drawCircle(33,110,3,RED);

          tft.fillRect(33, 80, 30, 3, BLACK);
          tft.fillRect(63, 80, 3, 10, BLACK);
          tft.fillRect(53, 90, 13, 3, BLACK);

          tft.fillRect(3, 138, 30, 3, BLACK);
          tft.fillRect(3, 128, 3, 10, BLACK);
          tft.fillRect(3, 128, 10, 3, BLACK);
          //tft.fillRect(53, 90, 13, 3, BLACK);
        }
        break;
      }
      case ModeLevelSetup:
      {
          // Draw a border
          //uint16_t width = tft.width() - 1;
          //uint16_t height = tft.height() - 1;
          //uint8_t border = 2;
          tft.fillScreen(WHITE);
        break;
      }
    }


}


void DisplayStatus()
{

  //tft.
    //drawBorder();
        //
        //  0,0 ------- 10,0
        //   |
        //   |
        //  0,10
        //
    switch (currDisplay)
    {
      case ModeMain:
      {
          tft.setCursor(80,5); // установка позиции курсора
          tft.setTextSize(8);  // установка размера шрифта
          tft.setTextColor(BLUE,WHITE);
          //tft.print("   "); 
          tft.setCursor(80,5); // установка позиции курсора
          tft.print(wlevel);
          if(wlevel<100)
            tft.print(" "); 
          //tft.setTextColor(BLACK,WHITE);
          //tft.setTextSize(6);  // установка размера шрифта
          //tft.println(utf8rus("с"));
          tft.setCursor(80,80); // установка позиции курсора
          tft.setTextSize(8);  // установка размера шрифта
        if(automode)
        {
          tft.setTextColor(BLACK,WHITE);
          tft.print(utf8rus(F("АВТО")));
          tft.setTextColor(RED,WHITE);
          curRelayState ? tft.println(F("+")):tft.println(F("-"));
        } else //manual - show current state
        {
         if(curRelayState && secondsToStop > 0)//relay is on
         {
          tft.setTextColor(GREEN,WHITE);
          tft.println(utf8rus(F("ВКЛ  ")));
           
          int minutes = secondsToStop / 60;
          int seconds = secondsToStop % 60;

          tft.setTextColor(BLACK,WHITE);

          tft.setTextSize(4);  // установка размера шрифта
          tft.setCursor(80,154); // установка позиции курсора
          if(minutes>0)
          {
            if(minutes<10)
            tft.print("0"); 
            tft.print(minutes); 
            tft.print(utf8rus(F(":")));
          } 
          if(seconds>0)
          {
            if(seconds<10)
            tft.print(F("0"));
            tft.print(seconds); 
            //tft.print(utf8rus(" "));
          } 
            if(seconds==0)
          {
            tft.print(F("00"));
            //tft.print(utf8rus(" "));
          } 

          tft.println("");
         } 
         else
          {
            tft.setTextColor(BLACK,WHITE);
            tft.println(utf8rus(F("ВЫКЛ ")));
          }
        }
       if(SensorErrors>5 && automode) 
        {
          tft.setTextColor(BLACK, WHITE);
          tft.setTextSize(3);  
          tft.setCursor(4,206);
          tft.print(utf8rus(F("Ошибок:      ")));
          tft.setCursor(4,206);
          tft.print(utf8rus(F("Ошибок: ")));
          tft.setTextColor(RED, WHITE);
          tft.println(SensorErrors);
        } else{
          //clean this line
          tft.setTextColor(BLACK, WHITE);
          tft.setTextSize(3);  
          tft.setCursor(4,206);
          tft.println(utf8rus(F("             ")));
        }
      }
      break;
      case ModeLevelSetup:
      {
        //tft.s
        tft.setCursor(3,3); // установка позиции курсора
        tft.setTextColor(BLACK, WHITE);
        tft.setTextSize(3);  // установка размера шрифта
        tft.println(utf8rus(F("Установка уровня")));
        tft.setTextSize(5);  // установка размера шрифта
        tft.setCursor(3,28); // установка позиции курсора
        tft.println(utf8rus(F("Отключения")));
        tft.setTextSize(6);  // установка размера шрифта
        tft.setTextColor(RED, WHITE);
        tft.setCursor(23,69); // установка позиции курсора
        tft.println(lelevtooffrelay);
        tft.setTextSize(5);  // установка размера шрифта
        tft.setTextColor(BLACK, WHITE);
        tft.setCursor(3,115); // установка позиции курсора
        tft.println(utf8rus(F("Включения")));
        tft.setTextSize(6);  // установка размера шрифта
        tft.setTextColor(RED, WHITE);
        tft.setCursor(23,155); // установка позиции курсора
        tft.println(leveltoonrelay);
        tft.setTextSize(1);  // установка размера шрифта
        tft.setCursor(3,205); // установка позиции курсора
        tft.setTextColor(YELLOW, RED);
        tft.println(utf8rus(F("Используйте кнопки 1,3,4,6 для изменения значений, D -сохранить настройки ")));
      }
      break;
     /*  {
      case ModeFlow:
        tft.setTextSize(2);  // установка размера шрифта
        tft.print("Lm:");
        tft.println(wflowrate);
        tft.print(" L:");
        tft.println(totalMilliLitres/1000);
      }
      break;*/
      default:
      break;
    }
    //tft.ddisplay();
}
void keypadEvent(KeypadEvent key)
{
  switch (keypad.getState())
  {
    case PRESSED:
     // tone(Beep, 1000, 30);
    break;
    case RELEASED:
    tone(Beep, 800, 30);
    //  Serial.print("depressed: ");
    //  Serial.println(key);
      switch (key)
      {
        case 'A': //auto
        currDisplay = ModeMain;
          SetupPicture();
          SetAutoMode(true); 
          RelayOff();
          key1pressed = false; 
          key2pressed = false; 
          break;
        case 'B': //on
        currDisplay = ModeMain;
          SetupPicture();
          SetAutoMode(false); 
          secondsToStop+=TimerSet;
          RelayOn();
          key1pressed = false; 
          key2pressed = false; 
          break;
        case 'C': //off
        currDisplay = ModeMain;
          SetupPicture();
          SetAutoMode(false); 
          secondsToStop = 0; 
          RelayOff();
          key1pressed = false; 
          key2pressed = false; 
          break;
        case 'D': //Setup
           if(currDisplay == ModeLevelSetup)
           { 
             currDisplay = ModeMain;
             //save settings
             SaveSettings();
             SetupPicture();
           } else
           {
             currDisplay = ModeLevelSetup; 
             SetupPicture();
           }
           break;
        /*case '#': //Flow
           if(currDisplay == ModeFlow)
           { 
             currDisplay = ModeMain;
           } else
           {
             currDisplay = ModeFlow; 
           }
        break;*/

        case '*': 
          IgnoreSensorError = !IgnoreSensorError;
          break;
        case '1': //123 to reset
          key1pressed = true; 
          if(currDisplay == ModeLevelSetup)
           { 
             if(lelevtooffrelay>30)
             lelevtooffrelay--; 
           }
          break;
        case '2':
          if(key1pressed)
          {
              key2pressed = true; 
          } else
          {
              key1pressed = false; 
              key2pressed = false; 
          }       
          break;
        case '3': //off
          if(key1pressed && key2pressed)
          {
             delay(250);  
             resetFunc(); //вызываем reset; 
          } else
          {
            key1pressed = false; 
            key2pressed = false; 
          }
          if(currDisplay == ModeLevelSetup)
          { 
            if(lelevtooffrelay < (leveltoonrelay - 15))
              lelevtooffrelay++;
          }
          break;
            case '4': //123 to reset
          if(currDisplay == ModeLevelSetup)
           { 
             if(leveltoonrelay> (lelevtooffrelay+15))
             leveltoonrelay--; 
           }
          break;
            case '6': //123 to reset
          if(currDisplay == ModeLevelSetup)
           { 
             if(leveltoonrelay<200)
             leveltoonrelay++; 
           }
          break;
        default://all other keys
        {
          key1pressed = false; 
          key2pressed = false; 
        }
        break;
     }
      break;
    case HOLD:
    break;
    case IDLE:
    break;
  }
}
void radioInit() 
{

    radio.begin();
 // printf_begin();
  delay(100);


  radio.setChannel(0x55);                 // Установка канала вещания;
  radio.setDataRate(RF24_1MBPS);          // Установка скорости;
  radio.setPALevel(RF24_PA_MAX);          // Установка максимальной мощности;

  radio.setAutoAck(true);
  radio.enableAckPayload();
  radio.enableDynamicPayloads();
  radio.openReadingPipe(1,pipe[0]);
  radio.startListening();
  radio.setRetries(15,15);

  tone(Beep, 4000, 100); 
  
}

void setup() 
{
    Serial.begin(115200);  // включаем последовательный порт

    //printf_begin();

    lelevtooffrelay = GetUpLevel(); 
    leveltoonrelay = GetDownLevel();
    automode = GetAutoMode();


    if(lelevtooffrelay < 30)
      lelevtooffrelay = 30;

    if(leveltoonrelay > 199 || leveltoonrelay < lelevtooffrelay)
      leveltoonrelay = 120;

    //relay
    pinMode(Relay, OUTPUT);
    digitalWrite(Relay, LOW);//off at start
  
    //beep on start
    pinMode(Beep, OUTPUT);
    pinMode(BeepGround, OUTPUT);
    digitalWrite(BeepGround, LOW);

    //пищим
    tone(Beep, 1000, 100);
    delay(150);
    tone(Beep, 2000, 100);
    delay(150);
    tone(Beep, 1000, 100);
    delay(150);
    
    radioInit();
       
    tft.reset();
    tft.begin(0x9341); // SDFP5408

    tft.cp437(true);

    //tft.setFont(&FreeSerif9pt7b);
    tft.setRotation(1); // Need for the Mega, please changed for your choice or rotation initial
    // Border
    drawBorder();
    // Initial screen
    SetupPicture();

    keypad.addEventListener(keypadEvent); // добавить слушателя события для данной клавиатуры

    timenow = millis(); 
    radiotimenow = millis();
    lcdbacktimenow = millis();
}

void ReadRadio()
{
  if (radio.available())             // проверяем буфер модема
  {
    //Serial.println("RA");
    radio.writeAckPayload(1, &ackdata, sizeof(ackdata) ); // prep the ack payload for next read
    radio.read(&data, sizeof(data)); // читаем данные

    if(data[0]>0)
    {
      wlevel = data[0]/10; //convert to CM from received MM
      SensorErrors = 0; 
      sensorActive = true;
      IgnoreSensorError = false;
    
      tft.fillCircle(300,20,12,GREEN);

      tft.setCursor (5, 230);

      tft.setTextSize (1);
      tft.setTextColor(BLACK);
      tft.print(utf8rus(F("УР:")));
      tft.print(data[0]);
      tft.print(utf8rus(F(" ПОСЛ:")));
      tft.print(data[1]);
      tft.print(utf8rus(F(" ОШ:")));
      tft.println(data[2]);
  
      tft.fillRect(290,230,30,10,WHITE);

    }
    else
    {
      SensorErrors++;
      tft.fillCircle(300,20,12,YELLOW);
  
      tft.fillRect(290,230,30,10,WHITE);
  
      tft.setCursor (5, 230);
      tft.setTextSize (1);
      tft.setTextColor(BLACK);
      tft.print(F("Zero data!!!        "));
    }
    radiotimenow = millis();
  } 
  else 
  {
     if(millis() - radiotimenow > 1000) //red if no data for 1 second 
    {
      //Serial.println("!RA");
      //radio.whatHappened
      SensorErrors++;
      tft.fillCircle(300,20,12,RED);
      tft.setCursor (5, 230);
      tft.setTextSize (1);
      tft.setTextColor(BLACK);
      tft.print(utf8rus(F("Нет радио сигнала ")));
      tft.fillRect(290,230,30,10,WHITE);
      tft.setTextColor(BLACK);
      tft.setCursor(290, 230);
      tft.print(SensorErrors);    
      radiotimenow =  millis();
    } 
  }
  if(SensorErrors >= MaxErrors) 
  {
    //zero data
    sensorActive = false;
    wlevel = 0;
  }
}

void loop() 
{
  // read keyboard in every loop, we need to do something
  //char key =
  keypad.getKey();//scan keyboard for events

  //relay data for remote side
  ackdata[0] = curRelayState ? 2  : 3; //2 if 0n, 3 if Off

  // always read from modem 
  ReadRadio();

  //beep if no sensor data && !ignore errors
  if(!sensorActive && !IgnoreSensorError)
  {
    if(millis() - lcdbacktimenow > lcdbackshowtime) 
    {
      if(automode)
         tone(Beep, 2000, 50);
      lcdbacktimenow =  millis();
    } 
  }

  //show on display
  if(millis() - timenow > showtime) 
  {
    if(secondsToStop>0)
      secondsToStop--;
    
    //GetFlowRate();
    DisplayStatus();

    timenow = millis();
  } 
  //
  if(automode)
  {
    //try to on relay if level is big enougth
    if(wlevel>=leveltoonrelay && sensorActive && wlevel!=0 && !curRelayState)
    {
      TryToOnRelay();
    }
    if(wlevel<=lelevtooffrelay || !sensorActive) // off rellay if sensor is gone
    {
      relayOnEvents=0;
      RelayOff();          
    }
  } else 
  { //manual mode need to check if need stop by timer
    if(secondsToStop==0 && curRelayState==true)
    { //stop relay by timer
      relayOnEvents  = 0;
      RelayOff();
    }
  }
}



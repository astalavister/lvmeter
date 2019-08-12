#include <SPI.h>      // библиотека для протокола SPI
#include <nRF24L01.h> // библиотека для nRF24L01+
#include <RF24.h>     // библиотека для радио модуля
//#include <printf.h>


//Laser sensor
#include <Wire.h>
#include <vl53l1_api.h>

void(* resetFunc) (void) = 0;//объявляем функцию reset с адресом 0 (123 from keyb)

const uint64_t pipe[2] = {0x0DEADF00D0LL, 0xF00DF2F3F4LL}; // идентификатор передачи

RF24 radio(9,10);

unsigned long radioTime;
unsigned long radioPeriod = 250; //ms
//data for send to radio
unsigned int data[3] = {0,0,0};
byte ackdata[2] {0,0};


// RANGING SENSOR
VL53L1_Dev_t sensor;
VL53L1_DEV   Sensor = &sensor;
//I2C: A4(SDA) and A5(SCL)

int status;
bool sensorError = false;
//sensor reading timer
unsigned long laserReadsPeriod = 1000; //in milliseconds, 1 second by default, sensor internal timer


//RGB led
int redPin= A0;
int greenPin = A1;
int bluePin = A2;
const byte rgbPins[3] = { A0, A1, A2 };
bool ledIsOn = false;
//led blink time
unsigned long ledTime;
unsigned long ledPeriod = 100; //ms
bool ledState = true;
//rgb led light level
int dim = 1;


const int numReadings = 30;
unsigned long readings[numReadings];      // the readings from sensor
int readIndex = 0;                        // the index of the current reading
unsigned long average = 0;                // the average
unsigned long lastRead = 0;
//loop counter
unsigned long sensorTime;
unsigned long laserReadsInterval = 1000;   // in milliseconds

unsigned int sendErrors = 0; 
//unsigned long sendOk = 0; 

byte RemoteState = 0;

int wrongAcks = 0;



void setColor(int redValue, int greenValue, int blueValue) 
{
  analogWrite(redPin, redValue);
  analogWrite(greenPin, greenValue);
  analogWrite(bluePin, blueValue);
}

void AverageArray()
{
    average = 0;
    unsigned long artotal = 0;
    for (int thisReading = 0; thisReading < numReadings; thisReading++) 
    {
      artotal+=readings[thisReading];
    }
    // calculate the average:
    average = artotal / numReadings;  
}
bool sensorInit()
{
  uint8_t byteData;
  uint16_t wordData;
  sensor.I2cDevAddr = 0x52;
  Serial.println(F("VL53L1X Resetting()"));
  if(VL53L1_software_reset(Sensor)!=VL53L1_ERROR_NONE)
  {
    sensorError = true;
    Serial.println(F("VL53L1X Reset Failed!!!"));
    return false;
  }
  Serial.println(F("VL53L1X Reset Done"));
  status = VL53L1_RdByte(Sensor, 0x010F, &byteData);
  Serial.print(F("VL53L1X Model_ID: "));
  Serial.println(byteData, HEX);
  VL53L1_RdByte(Sensor, 0x0110, &byteData);
  Serial.print(F("VL53L1X Module_Type: "));
  Serial.println(byteData, HEX);
  VL53L1_RdWord(Sensor, 0x010F, &wordData);
  Serial.print(F("VL53L1X: "));
  Serial.println(wordData, HEX);
  Serial.println(F("Ranging Measurement setup..."));
  status = VL53L1_WaitDeviceBooted(Sensor);
  if(status)
  {
    sensorError = true;
    Serial.println(F("VL53L1_WaitDeviceBooted failed"));
    return false;
  }

  status = VL53L1_DataInit(Sensor);
  if(status)
  {
    sensorError = true;
    Serial.println(F("VL53L1_DataInit failed"));
    return false;
  }
  status = VL53L1_StaticInit(Sensor);
  if(status)
  {
    sensorError = true;
    Serial.println(F("VL53L1_StaticInit failed"));
    return false;
  }
  status = VL53L1_SetDistanceMode(Sensor, VL53L1_DISTANCEMODE_MEDIUM);
  if(status)
  {
    sensorError = true;
    Serial.println(F("VL53L1_SetDistanceMode failed"));
    return false;
  }
  //VL53L1_CalibrationData_t cData;
  //status = VL53L1_GetCalibrationData(Sensor,&cData);
  //Serial.print(F("cdataX: "));
  //Serial.print((int32_t)cData.optical_centre.x_centre);
  //Serial.print(F("\tcdataY: "));
  //Serial.println((int32_t)cData.optical_centre.y_centre);
  // 0,15
  //
  //
  //
  //
  // 0,0                15,0
// n4 x5 y10
// n5 x4 y11
// n6 x3 y12
//
  VL53L1_UserRoi_t roiConfig;
  //4x4
  //roiConfig.TopLeftX = 5;
  //roiConfig.TopLeftY = 10;
  //roiConfig.BotRightX = 10;
  //roiConfig.BotRightY = 5;
  //5x5 SPAD matrix
  roiConfig.TopLeftX = 4;
  roiConfig.TopLeftY = 11;
  roiConfig.BotRightX = 11;
  roiConfig.BotRightY = 4;
  //6x6
  //roiConfig.TopLeftX = 3;
  //roiConfig.TopLeftY = 12;
  //roiConfig.BotRightX = 12;
  //roiConfig.BotRightY = 3;
  //8x8

  status = VL53L1_SetUserROI(Sensor, &roiConfig);
  status = VL53L1_SetMeasurementTimingBudgetMicroSeconds(Sensor, laserReadsPeriod * 100);
  status = VL53L1_SetInterMeasurementPeriodMilliSeconds(Sensor, laserReadsPeriod);
  //status = VL53L1X_SetLimitCheckEnable(&Sensor, VL53L1X_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE, 1);
  //status = VL53L1X_SetLimitCheckValue(&Sensor, VL53L1X_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE, 0.40*65536);
  status = VL53L1_StartMeasurement(Sensor);
  
  if(status)
  {
    sensorError = true;
    Serial.println(F("VL53L1_StartMeasurement failed"));
    return false;
  }
  Serial.println(F("VL53L1_StartMeasurement success"));
  return true;
}



//
//
//
// RADIO
//
//
//
//

void radioInit() 
{
  Serial.println( "Wireless init begin..." );
  // Setup
  radio.begin();
  delay(100);

  radio.setChannel(0x55);                 // Установка канала вещания;
  radio.setDataRate(RF24_1MBPS);          // Установка скорости;
  radio.setPALevel(RF24_PA_MAX);          // Установка максимальной мощности;

  radio.setAutoAck(true);
  radio.enableAckPayload();
  radio.enableDynamicPayloads();
  radio.stopListening();
  radio.openWritingPipe(pipe[0]);
  radio.setRetries(15,15);

  Serial.println( "Wireless initialized!" );
  //radio.printDetails();
 
}

void setup()
{
  Serial.begin(115200);  // включаем последовательный порт
  //printf_begin();
  Serial.println(F("Controller start..."));

  //for range sensor
  Wire.begin();
  Wire.setClock(400000);

  //spi.begin();      // Soft SPI for RF24
  radioInit();
  
  
    // initialize all the readings to 0:
  for (int thisReading = 0; thisReading < numReadings; thisReading++)
  {
    readings[thisReading] = 0;
  }

  // RGB led
  //Set up the status LED line as an output
  for(byte i=0; i<3; i++)
  {
    pinMode( rgbPins[i], OUTPUT );
  }
  // начальное состояние - OFF
  setColor(0,0,0);
  // printf_begin();
  //return;   
  if (!sensorInit())
  {
    ledPeriod = 2500;
  }
  Serial.println(F("Controller started...."));
}


void RadioSend()
{
  data[0] = average;
  data[1] = lastRead; 
  data[2] = sendErrors;

  //radio.stopListening();
  if(radio.write(&data, sizeof(data)))
  {
    Serial.println(F("TXOK"));
    sendErrors = 0;
    if (radio.isAckPayloadAvailable()) //got data from controller
    {
    // turn on blue led - sent data success
      radio.read(&ackdata, sizeof(ackdata));
      RemoteState = ackdata[0];
      setColor(0, 0, 255); // BLUE Color FOR BLINK
      Serial.println(ackdata[0]);
      //Serial.println(ackdata[1]);
    }
    else
    {
    // turn on red led - sent data success but bad ack
       setColor(255, 255, 0); // bLUE Color FOR BLINK
       Serial.println(F("BADACK"));
      wrongAcks++;
      if(wrongAcks > 5 )
      {
        wrongAcks = 0;
        RemoteState = 0;
      }
    }
  } 
  else
  {
    RemoteState = 0;
    setColor(200, 0, 0); // RED Color FOR BLINK
    Serial.print(F("SErr: "));
    Serial.println(sendErrors);
    if(++sendErrors > 30) //30 sec timeout, reset
    {
      Serial.println("Reset Controller...");
      delay(100);
      resetFunc();
    }
  }
  //radio.startListening();*/
}

void ReadSensor()
{

  if(sensorError)
    return;
  VL53L1_RangingMeasurementData_t RangingData;
  lastRead = 9999; //9.999 Meters
  //status = VL53L1_WaitMeasurementDataReady(Sensor);
  if(!VL53L1_WaitMeasurementDataReady(Sensor))
  {
    if(!VL53L1_GetRangingMeasurementData(Sensor, &RangingData))
    {
      //   Serial.print(F("sts:"));
      //   Serial.print(RangingData.RangeStatus);
      //   Serial.print(F(",rng:"));
      //   Serial.println(RangingData.RangeMilliMeter);
      //  Serial.print(F(",sgnl:"));
      //  Serial.print(RangingData.SignalRateRtnMegaCps/65536.0);
      //  Serial.print(F(",amb:"));
      //  Serial.println(RangingData.AmbientRateRtnMegaCps/65336.0);
      if(RangingData.RangeStatus == 0)
      {
        lastRead = RangingData.RangeMilliMeter;
        readings[readIndex] = lastRead;
        if (++readIndex >= numReadings)
        {
          readIndex = 0;
        }
      }
    }
    status = VL53L1_ClearInterruptAndStartMeasurement(Sensor);
  }
}

void loop() 
{
  //RADIO SEND
  if((millis() - radioTime) > radioPeriod)
  { 
    AverageArray();

    RadioSend();

    ledIsOn = true;
    ledTime = millis(); 
    radioTime = millis();
  }

  //LED
  if((millis() - ledTime) > ledPeriod && ledIsOn)
  { 
    switch (RemoteState)
    {
    case 0:
      setColor(0, 0, 0); // off led
      break;
    case 2:
      setColor(0, 255, 0); // green led
      break;
    case 3:
      setColor(255, 0, 0); // red led
      break;
    default:
      setColor(0, 0, 0); // off led
      break;
    }
    ledIsOn = false;
  }

  //SENSOR
  if((millis() - sensorTime) > laserReadsInterval)
  { 
    ReadSensor();
    sensorTime = millis();
  }
}


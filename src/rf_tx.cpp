#include <SPI.h>      // библиотека для протокола SPI
//Laser sensor
#include <Wire.h>
#include <VL53L1X.h>
#include "SoftwareSerial.h"



//lora serial module
SoftwareSerial mySerial(10, 9); // RX, TX

unsigned long radioTime;
unsigned long radioPeriod = 5000; //millisecs
//data for send to radio
struct senddata
{
    unsigned int level;
    unsigned int lastread;
    float ambient;
    float signal;
};
senddata data;
//unsigned int data[3] = {0,0,0};
//byte ackdata[2] {0,0};

// RANGING SENSOR
//VL53L1_Dev_t sensor;
//VL53L1_DEV   Sensor = &sensor;
//I2C: A4(SDA) and A5(SCL)

int status;
bool sensorError = false;
//sensor reading timer
unsigned long laserReadsPeriod = 2000; //in milliseconds, 2 second by default, sensor internal timer

//Mpins
//int mPin= A3;

//RGB led
int redPin= A0;
int greenPin = A1;
int bluePin = A2;
const byte rgbPins[3] = { A0, A1, A2 };
bool ledIsOn = false;
//led blink time
unsigned long ledTime;
unsigned long ledPeriod = 250; //ms
bool ledState = true;
//rgb led light level
//int dim = 1;


const int WINDOW_SIZE = 10;
static unsigned long readings[WINDOW_SIZE];      // the readings from sensor
int readIndex = 0;                        // the index of the current reading
int readSum = 0;                        // the sum of readings

//unsigned long lastRead = 0;
//loop counter
unsigned long sensorTime;
unsigned long laserReadsInterval = 2500;   // in milliseconds

unsigned int sendErrors = 0; 
//unsigned long sendOk = 0; 

byte RemoteState = 0;
int wrongAcks = 0;
VL53L1X sensor;
//SFEVL53L1X distanceSensor; 

bool sensorInit()
{

  sensor.setTimeout(1000);
  if (!sensor.init())
  {
    Serial.println("Failed to detect and initialize sensor!");
    return false;
  } 
  // Use long distance mode and allow up to 50000 us (50 ms) for a measurement.
  // You can change these settings to adjust the performance of the sensor, but
  // the minimum timing budget is 20 ms for short distance mode and 33 ms for
  // medium and long distance modes. See the VL53L1X datasheet for more
  // information on range and timing limits.
  sensor.setDistanceMode(VL53L1X::Medium);
  sensor.setMeasurementTimingBudget(laserReadsPeriod * 100);
  //sensor.

  // Start continuous readings at a rate of one measurement every 50 ms (the
  // inter-measurement period). This period should be at least as long as the
  // timing budget.
  sensor.startContinuous(laserReadsPeriod);

  /*uint8_t byteData;
  uint16_t wordData;
  sensor.I2cDevAddr = 0x52;
  Serial.println(F("VL53L1X Resetting()"));
  VL53L1_software_reset(Sensor);

  //if(VL53L1_software_reset(Sensor)!=VL53L1_ERROR_NONE)
 // {
 //   sensorError = true;
 //   Serial.println(F("VL53L1X Reset Failed!!!"));
 //   return false;
 // }
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
  VL53L1_WaitDeviceBooted(Sensor);
  
  VL53L1_DataInit(Sensor);
  VL53L1_StaticInit(Sensor);
  status = VL53L1_SetDistanceMode(Sensor, VL53L1_DISTANCEMODE_MEDIUM);
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
  Serial.println(F("VL53L1_StartMeasurement success"));
  */
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
      pinMode(11, OUTPUT);
      pinMode(13, OUTPUT);
      digitalWrite(11, LOW);
      digitalWrite(13, LOW);
  Serial.println( "Wireless initialized!" );
  //radio.printDetails();
}

void setup()
{
  Serial.begin(115200);  // включаем последовательный порт
  //printf_begin();
  Serial.println(F("Controller start..."));
  mySerial.begin(2400);


  //for range sensor
  Wire.begin();
  Wire.setClock(400000);


  //pinMode( mPin, OUTPUT);

  radioInit();
  // initialize all the readings to 0:
  for (int thisReading = 0; thisReading < WINDOW_SIZE; thisReading++)
  {
    readings[thisReading] = 0;
  }
  // RGB led
  //Set up the status LED line as an output
  for(byte i=0; i<3; i++)
  {
    pinMode( rgbPins[i], OUTPUT );
  }
  
  if(!sensorInit())
    sensorError = true;

  Serial.println(F("Controller started...."));
}
void RadioSend()
{
    mySerial.write((byte*)&data,sizeof(data));
    analogWrite(bluePin, 255);
    ledIsOn = true;
    ledTime = millis(); 
}
void ReadSensor()
{

  if(sensorError)
    return;

  sensor.read();

  /*Serial.print("range: ");
  Serial.print(sensor.ranging_data.range_mm);
  Serial.print("\tstatus: ");
  Serial.print(sensor.ranging_data.range_status);
  Serial.print("\tpeak signal: ");
  Serial.print(sensor.ranging_data.peak_signal_count_rate_MCPS);
  Serial.print("\tambient: ");
  Serial.print(sensor.ranging_data.ambient_count_rate_MCPS);
  Serial.print("\tavg: ");
  Serial.println(avg);*/
  
  //Serial.println(); 
  /*
const int WINDOW_SIZE = 10;
static unsigned long readings[WINDOW_SIZE];      // the readings from sensor
int readIndex = 0;                        // the index of the current reading
int readSum = 0;                        // the sum of readings
unsigned int avg = 0;
*/
  if(!sensor.ranging_data.range_status)
  {
    data.lastread = sensor.ranging_data.range_mm;
    readSum = readSum - readings[readIndex];// Remove the oldest entry from the sum
    readings[readIndex] = data.lastread;// Add the newest reading to the window
    readSum = readSum + data.lastread; // Add the newest reading to the sum
    readIndex = (readIndex+1) % WINDOW_SIZE;// Increment the index, and wrap to 0 if it exceeds the window size
    data.level = readSum / WINDOW_SIZE;
    data.ambient = sensor.ranging_data.ambient_count_rate_MCPS;
    data.signal = sensor.ranging_data.peak_signal_count_rate_MCPS;
  //  Serial.print("range: ");
  //Serial.print(sensor.ranging_data.range_mm);
  //Serial.print("\tstatus: ");
  //Serial.println(sensor.ranging_data.range_status);
  }

  /*VL53L1_RangingMeasurementData_t RangingData;
  lastRead = 9999; //9.999 Meters
  RangingData.RangeMilliMeter = 9999;
  status = VL53L1_WaitMeasurementDataReady(Sensor);
  if(!status)
  {
    status = VL53L1_GetRangingMeasurementData(Sensor, &RangingData);
    if(!status)
    {
        Serial.print(F("sts:"));
        Serial.print(RangingData.RangeStatus);
        Serial.print(F(",rng:"));
        Serial.println(RangingData.RangeMilliMeter);
        Serial.print(F(",sgnl:"));
        Serial.print(RangingData.SignalRateRtnMegaCps/65536.0);
        Serial.print(F(",amb:"));
        Serial.println(RangingData.AmbientRateRtnMegaCps/65336.0);
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
    else
    {
        Serial.print(F("VL53L1_GetRangingMeasurementData:"));
        Serial.println(status);
    }
    status = VL53L1_ClearInterruptAndStartMeasurement(Sensor);
  } else
  {
    
     Serial.print(F("VL53L1_WaitMeasurementDataReady:"));
        Serial.println(status);
  }*/
  
}

void loop() 
{
  //RADIO SEND
  if((millis() - radioTime) > radioPeriod)
  { 
    RadioSend();
    radioTime = millis();
  }

  //LED
  if((millis() - ledTime) > ledPeriod && ledIsOn)
  { 
    analogWrite(bluePin, 0);
    ledIsOn = false;
  }

  //SENSOR
  if((millis() - sensorTime) > laserReadsInterval)
  { 
    ReadSensor();
    sensorTime = millis();
  }

  if(mySerial.available()>0)
  {
    String a = mySerial.readString();
    Serial.println(a);
      if(a=="1")
    {
        analogWrite(greenPin, 255);
        analogWrite(redPin, 0);
    }
    else
    {
      analogWrite(greenPin, 0);
      analogWrite(redPin, 255);
    }
  }
 

}


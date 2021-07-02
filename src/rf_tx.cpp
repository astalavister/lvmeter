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

int status;
bool sensorError = false;
//sensor reading timer
unsigned long laserReadsPeriod = 2000; //in milliseconds, 2 second by default, sensor internal timer

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
  return true;
}

void radioInit() 
{
  Serial.println( "Wireless init begin..." ); 
  //set M0 and M1 pins to 0
      pinMode(11, OUTPUT);
      pinMode(13, OUTPUT);
      digitalWrite(11, LOW);
      digitalWrite(13, LOW);
  Serial.println( "Wireless initialized!" );
  delay(200);//let radiomodule time to change mode
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


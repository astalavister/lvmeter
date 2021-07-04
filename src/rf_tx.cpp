#include <SPI.h>
#include <Wire.h>
#include <VL53L1X.h>
#include "SoftwareSerial.h"
//lora serial module
SoftwareSerial mySerial(10, 9); // RX, TX

//data for send to radio
struct senddata
{
    unsigned int level;
    float ambient;
    float signal;
    unsigned int flowpulses;
};
senddata data;

//sensor reading timer
unsigned long laserReadsPeriod = 1000; //in milliseconds, 2 second by default, sensor internal timer
bool sensorError = false;

//flowmeter
int hallPin = 2;
volatile unsigned int flowPulses; //measuring the rising edges of the signal

//RGB led
int redPin= A0;
int greenPin = A1;
int bluePin = A2;
const byte rgbPins[3] = { A0, A1, A2 };
bool ledIsOn = false;

//blue led blink time
unsigned long ledTime;
unsigned long ledPeriod = 250; //ms
bool ledState = false;

//loop counter
unsigned long sensorTime;
unsigned long laserReadsInterval = 1000;   // in milliseconds

byte RemoteState = 0;
VL53L1X sensor;

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
  // Start continuous readings at a rate of one measurement every 50 ms (the
  // inter-measurement period). This period should be at least as long as the
  // timing budget.
  sensor.startContinuous(laserReadsPeriod);
  sensorError = false;
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
}

void rpm ()     //This is the function that the interupt calls 
{ 
  flowPulses++;  //This function measures the rising and 
  //falling edge of the hall effect sensors signal
} 

void setup()
{
  data.flowpulses = 0;
  data.ambient = 0;
  data.signal = 0;
  data.level = 0;

  Serial.begin(115200);  // debug port
  Serial.println(F("Controller start..."));
  mySerial.begin(2400);

  //for range sensor
  Wire.begin();
  Wire.setClock(400000);

  radioInit();

  // RGB led
  // Set up the status LED line as an output
  for(byte i=0; i < 3; i++)
  {
    pinMode(rgbPins[i], OUTPUT );
  }
  
  if(!sensorInit())
  {
      sensorError = true;
      Serial.println(F("Level sensor failed"));
  }
 
  pinMode(hallPin, INPUT); //initializes digital pin as an input
  digitalWrite(hallPin, HIGH); // Optional Internal Pull-Up
  attachInterrupt(digitalPinToInterrupt(hallPin), rpm, RISING); // Setup Interrupt
  sei(); // Enable interrupts
  Serial.println(F("Controller started...."));
}
void RadioSend()
{
    data.flowpulses = flowPulses;
    flowPulses = 0; // Reset Counter
    Serial.print("Pulse: ");
    Serial.println(data.flowpulses);
    mySerial.write((byte*)&data,sizeof(data));
    analogWrite(bluePin, 255);
    ledIsOn = true;
    ledTime = millis(); 
}

bool ReadSensor()
{

  if(sensorError)
  {
    if(!sensorInit())
    {
      data.ambient = 0;
      data.signal = 0;
      data.level = 0;
      return false;
    }
  }
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
  Serial.println(avg);
  */
  if(!sensor.ranging_data.range_status)
  {
    data.level = sensor.ranging_data.range_mm;// readSum / WINDOW_SIZE; 
    data.ambient = sensor.ranging_data.ambient_count_rate_MCPS;
    data.signal = sensor.ranging_data.peak_signal_count_rate_MCPS;
    return true;
    //Serial.print("range: ");
    //Serial.print(sensor.ranging_data.range_mm);
    //Serial.print("\tstatus: ");
    //Serial.println(sensor.ranging_data.range_status);
  } return false;
}

void loop() 
{
  //LED
  if(ledIsOn && (millis() - ledTime) > ledPeriod )
  { 
    ledIsOn = false;
    analogWrite(bluePin, 0);
  }

  //SENSOR
  if((millis() - sensorTime) > laserReadsInterval)
  { 
    if(ReadSensor())
      RadioSend();  
    sensorTime = millis();
  }

  //REMOTE DATA
  if(mySerial.available() > 0)
  {
    String a = mySerial.readString();
    Serial.print("R: ");
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


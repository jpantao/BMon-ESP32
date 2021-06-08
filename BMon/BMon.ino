
#include <WiFi.h>
#include <FirebaseESP32.h>
#include "DHT.h"

// defaults
// TODO: change values
#define TEMP_LOW = 25;
#define TEMP_HIGH = 75;
#define HUM_LOW = 25;
#define HUM_HIGH = 75;
#define LUM_LOW = 25;
#define LUM_HIGH = 75;
#define MOIST_LOW = 25;
#define MOIST_HIGH = 75;

const char* db_url = "https://bmon-c19ea-default-rtdb.europe-west1.firebasedatabase.app/";
const char* db_api_key = "AIzaSyDSzSbd3ZL81VDbo54UH7pDWqHAaN1AXM0";


// led pins
const int redLumPin = 6;
const int greenLumPin = 7;
const int blueLumPin = 8;
const int redHumPin = 9;
const int greenHumPin = 10;
const int blueHumPin = 11;
const int redTempPin = 12;
const int greenTempPin = 13;
const int blueTempPin = 14;
const int redMoistPin = 15;
const int greenMoistPin = 16;
const int blueMoistPin = 17;

// sensor pins
const int photoregistorPin = 1;
const int dht11Pin = 3;
const int moisturePin = 5;

#define DHTTYPE DHT11
DHT dht(dht11Pin, DHTTYPE); // DHT11



#if defined(ARDUINO_ARCH_AVR)
    #define debug  Serial

#elif defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_SAM)
    #define debug  SerialUSB
#else
    #define debug  Serial
#endif

float temp_hum_val[2] = {0};
int lum_val = 0;
int moist_val = 0;

void setup() {
  debug.begin(115200);
  
  // leds setup
  for(int i = 9; i <=17; i++){
    pinMode(i, OUTPUT);
  }

  // sensors setup
  dht.begin();

}


void loop() {
  debug.println("--- Loop START ---");

  
  // Measurements
  read_temp_hum();
  read_lum();
  read_moist(); // not sure if it works

    
 
  
  delay(5000); // wait before next cycle
  debug.println("---- Loop END ----");
}

void read_temp_hum(){
  // DHT11 read 
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  if (!dht.readTempAndHumidity(temp_hum_val)) {
        debug.print("Humidity: ");
        debug.print(temp_hum_val[0]);
        debug.print(" %\t");
        debug.print("Temperature: ");
        debug.print(temp_hum_val[1]);
        debug.println(" *C");
    } else {
        debug.println("Failed to get temprature and humidity value.");
    }
}


void read_lum(){
  lum_val = analogRead(photoregistorPin);
  lum_val = map(lum_val, 0, 8191, 0, 100);
  lum_val = constrain(lum_val, 0, 100);
  debug.print("Luminosity: ");
  debug.println(lum_val);
}

void read_moist(){
  moist_val = analogRead(moisturePin);
  debug.print("Soil moisture: ");
  debug.println(moist_val);
}

#include <WiFi.h>
#include <FirebaseESP32.h>
#include "DHT.h"


#include "addons/TokenHelper.h" // token generation process info.
#include "addons/RTDBHelper.h"  // RTDB payload printing info and other helper functions.


#define DEVICE_UID "ESP32"          // Device ID
#define WIFI_SSID "MEO-F6B5F="      // WiFi name
#define WIFI_PASSWORD "ec3e9334e7"  // WiFi password


#define API_KEY "AIzaSyDSzSbd3ZL81VDbo54UH7pDWqHAaN1AXM0"; // Firebase Project Web API Key
#define DATABASE_URL "https://bmon-c19ea-default-rtdb.europe-west1.firebasedatabase.app/"; // Firebase Realtime database URL

String device_location = "Living Room"; // Device Location config
FirebaseData fbdo;                      // Firebase Realtime Database Object
FirebaseAuth auth;                      // Firebase Authentication Object
FirebaseConfig config;                  // Firebase configuration Object
String databasePath = "";               // Firebase database path
String fuid = "";                       // Firebase Unique Identifier
unsigned long elapsedMillis = 0;        // Stores the elapsed time from device start up
unsigned long update_interval = 10000;  // The frequency of sensor updates to firebase, set to 10seconds
int count = 0;                          // Dummy counter to test initial firebase updates
bool isAuthenticated = false;           // Store device authentication status


// led pins
const int red_lum_pin     = 6;
const int green_lum_pin   = 7;
const int blue_lum_pin    = 8;
const int red_hum_pin     = 9;
const int green_hum_pin   = 10;
const int blue_hum_pin    = 11;
const int red_temp_pin    = 12;
const int green_temp_pin  = 13;
const int blue_temp_pin   = 14;
const int red_moist_pin   = 15;
const int green_moist_pin = 16;
const int blue_moist_pin  = 17;

// sensor pins
const int photoregistorPin  = 1;
const int dht11Pin          = 2;
const int moisturePin       = 3;

#define DHTTYPE DHT11
DHT dht(dht11Pin, DHTTYPE); // DHT11

// setup debug
#if defined(ARDUINO_ARCH_AVR)
		#define debug  Serial

#elif defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_SAM)
		#define debug  SerialUSB
#else
		#define debug  Serial
#endif

// defaults
// TODO: change values
int temp_low    = 25;
int temp_high   = 75;
int hum_low     = 25;
int hum_high    = 75;
int lum_low     = 25;
int lum_high    = 75;
int moist_low   = 25;
int moist_high  = 75;

float temp_hum_val[2] = {0};
float lum_val = 0;
float moist_val = 0;

// cycle frequency (milliseconds)
const int cycle_delay = 5*1000; // minutes
// how many normal cycles to wait before sync with firebase
const int cycle_sync = 1*60/(cycle_delay/1000); // sync each 1 minutes (depends on cycle_delay)
// cycle counter
int cycle_counter = 0;

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
	debug.print("--- ");
	debug.print(cycle_counter);
	debug.println(" Loop START ---");

	
	// Measurements
	read_temp_hum();
	read_lum();
	read_moist(); // not sure if it works

	
  //log_averages();
	if(cycle_counter != 0 && cycle_counter % cycle_sync == 0){
    // TODO: read defaults from firebase
    // TODO: upload averages to firebase
		debug.println("-------------------- NEEDS TO SYNC");
	}

  process_hum();
  process_temp();
  process_lum();
  process_temp();
  
	debug.print("---- ");
	debug.print(cycle_counter);
	debug.println(" Loop END ----");
	cycle_counter++;
	delay(5000); // wait before next cycle
}


void process_hum(){
	clear_hum_led();
	if (temp_hum_val[0] < hum_low) {
		digitalWrite(blue_hum_pin, HIGH);
	} else if (temp_hum_val[0] < hum_high) {
		digitalWrite(green_hum_pin, HIGH);
	} else{
		digitalWrite(red_hum_pin, HIGH);
	}

}

void process_temp(){
	clear_temp_led();
	if (temp_hum_val[1] < temp_low) {
		digitalWrite(blue_temp_pin, HIGH);
	} else if (temp_hum_val[1] < temp_high) {
		digitalWrite(green_temp_pin, HIGH);
	} else{
		digitalWrite(red_temp_pin, HIGH);
	}

}

void process_lum(){
	clear_lum_led();
	if (lum_val < lum_low) {
		digitalWrite(blue_lum_pin, HIGH);
	} else if (lum_val < lum_high) {
		digitalWrite(green_lum_pin, HIGH);
	} else{
		digitalWrite(red_lum_pin, HIGH);
	}

}

void process_moist(){
	clear_moist_led();
	if (moist_val < moist_low) {
		digitalWrite(blue_moist_pin, HIGH);
	} else if (moist_val < moist_high) {
		digitalWrite(green_moist_pin, HIGH);
	} else{
		digitalWrite(red_moist_pin, HIGH);
	}

}

void read_temp_hum(){
  float reading[2]  = {0};
	// DHT11 read 
	// Reading temperature or humidity takes about 250 milliseconds!
	// Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
	if (!dht.readTempAndHumidity(reading)) {
				debug.print("Humidity: ");
				debug.println(reading[0]);
				debug.print("Temperature: ");
				debug.print(reading[1]);
				debug.println(" *C");
        temp_hum_val[0]= ((temp_hum_val[0]*cycle_counter) + reading[0]) / (cycle_counter+1);
        temp_hum_val[1]=((temp_hum_val[1]*cycle_counter) + reading[1]) / (cycle_counter+1);
	} else {
				debug.println("Failed to get temprature and humidity value.");
	}
}

void read_lum(){
	int reading = analogRead(photoregistorPin);
	reading = map(reading, 0, 8191, 0, 100);
	reading = constrain(reading, 0, 100);
	debug.print("Luminosity: ");
	debug.println(reading);
  lum_val=((lum_val*cycle_counter) + reading) / (cycle_counter+1);
}

void read_moist(){
	int reading = analogRead(moisturePin);
	debug.print("Soil moisture: ");
	debug.println(reading);
  moist_val=((moist_val*cycle_counter) + reading) / (cycle_counter+1);;
}

void clear_hum_led(){
	digitalWrite(red_hum_pin, LOW);
	digitalWrite(green_hum_pin, LOW);
	digitalWrite(blue_hum_pin, LOW);
}

void clear_temp_led(){
	digitalWrite(red_temp_pin, LOW);
	digitalWrite(green_temp_pin, LOW);
	digitalWrite(blue_temp_pin, LOW);
}

void clear_lum_led(){
	digitalWrite(red_lum_pin, LOW);
	digitalWrite(green_lum_pin, LOW);
	digitalWrite(blue_lum_pin, LOW);
}

void clear_moist_led(){
	digitalWrite(red_moist_pin, LOW);
	digitalWrite(green_moist_pin, LOW);
	digitalWrite(blue_moist_pin, LOW);
}

void log_averages(){
  debug.print("Humidity: ");
  debug.println(temp_hum_val[0]);
  debug.print("Temperature: ");
  debug.println(temp_hum_val[1]);
  debug.print("Luminosity: ");
  debug.println(lum_val);
  debug.print("Moisture: ");
  debug.println(moist_val);
}

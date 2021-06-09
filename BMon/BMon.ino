#include <WiFi.h>
#include <FirebaseESP32.h>
#include "DHT.h"

#include "addons/TokenHelper.h" // token generation process info.
#include "addons/RTDBHelper.h"  // RTDB payload printing info and other helper functions.

#define DEVICE_UID "ESP32"          // Device ID
#define WIFI_SSID "MEO-F6B5F0"      // WiFi name
#define WIFI_PASSWORD "ec3e9334e7"  // WiFi password

// Firebase Project Web API Key and real time database url
#define API_KEY "AIzaSyDSzSbd3ZL81VDbo54UH7pDWqHAaN1AXM0"; 
#define DATABASE_URL "https://bmon-c19ea-default-rtdb.europe-west1.firebasedatabase.app/"; 

String device_location = "home";  // Device Location config
FirebaseData fbdo;                // Firebase Realtime Database Object
FirebaseAuth auth;                // Firebase Authentication Object
FirebaseConfig config;            // Firebase configuration Object
String databasePath = "";         // Firebase database path
String fuid = "";                 // Firebase Unique Identifier
bool isAuthenticated = false;     // Store device authentication status

// actuator pins
const int humidifier      = 5;
const int red_temp_pin    = 6;
const int green_temp_pin  = 7;
const int blue_temp_pin   = 8;
const int red_hum_pin     = 9;
const int green_hum_pin   = 10;
const int blue_hum_pin    = 11;
const int red_lum_pin     = 12;
const int green_lum_pin   = 13;
const int blue_lum_pin    = 14;
const int red_moist_pin   = 15;
const int green_moist_pin = 16;
const int blue_moist_pin  = 17;

// sensor pins
const int photoregistorPin  = 1;
const int dht11Pin          = 2;
const int moisturePin       = 3;

#define DHTTYPE DHT11
DHT dht(dht11Pin, DHTTYPE); // DHT11

// Debug log
#if defined(ARDUINO_ARCH_AVR)
		#define debug  Serial
#elif defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_SAM)
		#define debug  SerialUSB
#else
		#define debug  Serial
#endif

// defaults
// TODO: change values
int temp_low    = 10;
int temp_high   = 35;
int hum_low     = 25;
int hum_high    = 75;
int lum_low     = 25;
int lum_high    = 75;
int moist_low   = 25;
int moist_high  = 75;

float hum_temp_val[2] = {0};
float lum_val = 0;
float moist_val = 0;

FirebaseJson hum_json;
FirebaseJson temp_json;
FirebaseJson lum_json;
FirebaseJson moist_json;

// cycle frequency (milliseconds)
const int cycle_delay = 5*1000; // minutes
// how many normal cycles to wait before sync with firebase
const int cycle_sync = 1*60/(cycle_delay/1000); // sync each 1 minutes (depends on cycle_delay)
// cycle counter
int cycle_counter = 0;

void setup() {
	debug.begin(115200);
	
	for(int i = 5; i <=17; i++){
		pinMode(i, OUTPUT);
	}

	dht.begin();
	wifi_init();
	firebase_init();

  temp_json.add("deviceuid", DEVICE_UID);
  temp_json.add("name", "DHT11-Temp");
  temp_json.add("type", "Temperature");
  temp_json.add("location", device_location);
  temp_json.add("value", hum_temp_val[1]);

  hum_json.add("deviceuid", DEVICE_UID);
  hum_json.add("name", "DHT11-Hum");
  hum_json.add("type", "Humidity");
  hum_json.add("location", device_location);
  hum_json.add("value", hum_temp_val[0]);

  lum_json.add("deviceuid", DEVICE_UID);
  lum_json.add("name", "Photoregistor");
  lum_json.add("type", "Luminosity");
  lum_json.add("location", device_location);
  lum_json.add("value", lum_val);

  moist_json.add("deviceuid", DEVICE_UID);
  moist_json.add("name", "Groove Moisture Sensor");
  moist_json.add("type", "Soil moisture");
  moist_json.add("location", device_location);
  moist_json.add("value", moist_val);
}


void loop() {
	debug.print("--- ");
	debug.print(cycle_counter);
	debug.println(" Loop START ---");
  
	// read sensor data
  update_sensor_readings();

  
	
	// sync with database if in a sync cycle
	if(cycle_counter != 0 && cycle_counter % cycle_sync == 0){ 
    // TODO: read defaults from firebase
    // TODO: upload averages to firebase
    temp_json.set("value", hum_temp_val[1]);
    hum_json.set("value", hum_temp_val[0]);
    lum_json.set("value", lum_val);
    moist_json.set("value", moist_val);
    upload_data();
    log_averages();
	}

  // update leds' state
  update_leds();
  
	debug.print("---- ");
	debug.print(cycle_counter);
	debug.println(" Loop END ----");
	cycle_counter++;
	delay(5000); // wait before next cycle
}


// --- Update --- 

void update_sensor_readings(){
  float hum_temp_reading[2]  = {0};
	// DHT11 read 
	// Reading temperature or humidity takes about 250 milliseconds!
	// Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
	if (dht.readTempAndHumidity(hum_temp_reading)) {
	  debug.println("Failed to get temprature and humidity value.");
    hum_temp_reading[0] = hum_temp_val[0];
    hum_temp_reading[1] = hum_temp_val[1];
	}
 
  // Photoregistor
  int lum_reading = analogRead(photoregistorPin);
  lum_reading = map(lum_reading, 0, 8191, 0, 100);
  lum_reading = constrain(lum_reading, 0, 100);
 
  // Groove Moisture Sensor
  int moist_reading = analogRead(moisturePin);
  moist_reading = map(moist_reading, 0, 8191, 0, 100);
  moist_reading = constrain(moist_reading, 0, 100);

  // debug.print("Humidity: ");
  // debug.println(hum_temp_reading[0]);
  // debug.print("Temperature: ");
  // debug.print(hum_temp_reading[1]);
  // debug.println(" *C");
  // debug.print("Luminosity: ");
  // debug.println(reading);
  // debug.print("Soil moisture: ");
  // debug.println(moist_reading);


  // update values (weighted average)
  hum_temp_val[0]= ((hum_temp_val[0]*cycle_counter) + hum_temp_reading[0]) / (cycle_counter+1);
  hum_temp_val[1]=((hum_temp_val[1]*cycle_counter) + hum_temp_reading[1]) / (cycle_counter+1);
  lum_val=((lum_val*cycle_counter) + lum_reading) / (cycle_counter+1);
  moist_val=((moist_val*cycle_counter) + moist_reading) / (cycle_counter+1);
  
}

void update_leds(){
  clear_hum_led();
  if (hum_temp_val[0] < hum_low) {
    digitalWrite(blue_hum_pin, HIGH);
  } else if (hum_temp_val[0] < hum_high) {
    digitalWrite(green_hum_pin, HIGH);
  } else{
    digitalWrite(red_hum_pin, HIGH);
  }

  clear_temp_led();
  if (hum_temp_val[1] < temp_low) {
    digitalWrite(blue_temp_pin, HIGH);
  } else if (hum_temp_val[1] < temp_high) {
    digitalWrite(green_temp_pin, HIGH);
  } else{
    digitalWrite(red_temp_pin, HIGH);
  }

  clear_lum_led();
  if (lum_val < lum_low) {
    digitalWrite(blue_lum_pin, HIGH);
  } else if (lum_val < lum_high) {
    digitalWrite(green_lum_pin, HIGH);
  } else{
    digitalWrite(red_lum_pin, HIGH);
  }

  clear_moist_led();
  if (moist_val < moist_low) {
    digitalWrite(blue_moist_pin, HIGH);
  } else if (moist_val < moist_high) {
    digitalWrite(green_moist_pin, HIGH);
  } else{
    digitalWrite(red_moist_pin, HIGH);
  }

}


// --- Upload ---

void upload_data(){
  String temp_node = databasePath + "/temperature";
  String hum_node = databasePath + "/humidity";
  String lum_node = databasePath + "/luminosity";
  String moist_node = databasePath + "/soil_moisture";

  if(!Firebase.setJSON(fbdo, temp_node.c_str(), temp_json)){
    debug.println("Failed to upload temperature json");
    debug.println("Reason :" + fbdo.errorReason());
  }
  if(!Firebase.setJSON(fbdo, hum_node.c_str(), hum_json)){
    debug.println("Failed to upload humidity json");
    debug.println("Reason :" + fbdo.errorReason());
  }
  if(!Firebase.setJSON(fbdo, lum_node.c_str(), lum_json)){
    debug.println("Failed to upload luminosity json");
    debug.println("Reason :" + fbdo.errorReason());
  }
  if(!Firebase.setJSON(fbdo, moist_node.c_str(), moist_json)){
    debug.println("Failed to upload soil moisture json");
    debug.println("Reason :" + fbdo.errorReason());
  }
}

// --- Init ---

void wifi_init() { 
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi"); 
  while (WiFi.status() != WL_CONNECTED){ 
    Serial.print(".");  
    delay(300);  
  }
  Serial.println(); 
  Serial.print("Connected with IP: "); 
  Serial.println(WiFi.localIP()); 
  Serial.println();
}

void firebase_init() {
	
	config.api_key = API_KEY; 			    // configure firebase API Key
	config.database_url = DATABASE_URL; // configure firebase realtime database url 
	Firebase.reconnectWiFi(true); 		  // Enable WiFi reconnection

	Serial.println("------------------------------------");
	Serial.println("Sign up new user...");
  	
	// Sign in to firebase anonymously
  if (Firebase.signUp(&config, &auth, "", "")){
	  Serial.println("Success");
   	isAuthenticated = true;
 		databasePath = "/" + device_location;
	 	fuid = auth.token.uid.c_str();
	} else {
 		Serial.printf("Failed, %s\n", config.signer.signupError.message.c_str()); 
		isAuthenticated = false;
	}

	config.token_status_callback = tokenStatusCallback;	
	Firebase.begin(&config, &auth);	
}


// --- Auxiliar ---

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
  String json_str;
  temp_json.toString(json_str, true);
  debug.println(json_str);
  hum_json.toString(json_str, true);
  debug.println(json_str);
  lum_json.toString(json_str, true);
  debug.println(json_str);
  moist_json.toString(json_str, true);
  debug.println(json_str);
}

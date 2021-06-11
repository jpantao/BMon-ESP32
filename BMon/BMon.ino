#include <WiFi.h>
#include <FirebaseESP32.h>
#include "DHT.h"

#include "addons/TokenHelper.h" // token generation process info.
#include "addons/RTDBHelper.h"  // RTDB payload printing info and other helper functions.

#define DHTTYPE DHT11               // DHT type

#define DEVICE_UID "BMon-Prototype" // Device ID
#define WIFI_SSID "MEO-F6B5F0"      // WiFi name
#define WIFI_PASSWORD "ec3e9334e7"  // WiFi password

// Firebase Project Web API Key and real time database url
#define API_KEY "AIzaSyDSzSbd3ZL81VDbo54UH7pDWqHAaN1AXM0"; 
#define DATABASE_URL "https://bmon-c19ea-default-rtdb.europe-west1.firebasedatabase.app/"; 

String device_location = "Japanese Maple";  // Device Location config
FirebaseData fbdo;                          // Firebase Realtime Database Object
FirebaseAuth auth;                          // Firebase Authentication Object
FirebaseConfig config;                      // Firebase configuration Object
String database_path = "";                  // Firebase database path
String fuid = "";                           // Firebase Unique Identifier
bool is_authenticated = false;              // Store device authentication status

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
float temp_low  = 10;
float temp_high = 35;
float hum_low   = 25;
float hum_high  = 75;
int lum_low     = 25;
int lum_high    = 75;
int moist_low   = 25;
int moist_high  = 75;
bool humidifier_on = false;

float temp_val  =  0;
float hum_val   = 0;
float lum_val   = 0;
float moist_val = 0;

FirebaseJson hum_json;
FirebaseJson temp_json;
FirebaseJson lum_json;
FirebaseJson moist_json;


int cycle_delay = 5000; // cycle frequency (milliseconds)
int sync_interval = 12; // sync freqency in cycles
int cycle_counter = 0;  // cycle counter

void setup() {
	debug.begin(115200);
	setup_wifi();
	setup_firebase();
  setup_sensors();
  setup_actuators();
  upload_defaults();
}


void loop() {
	debug.print("--- ");
	debug.print(cycle_counter);
	debug.println(" Loop START ---");
  
	// read sensor data
  update_sensor_readings();
	
	// sync with database if in a sync cycle
	if(cycle_counter == 0 || cycle_counter % sync_interval == 0){ 
    debug.println("Sync cycle");
    update_config();
    temp_json.set("value", temp_val);
    hum_json.set("value", hum_val);
    lum_json.set("value", lum_val);
    moist_json.set("value", moist_val);
    upload_data();
    // log_averages();
	}
  
  // update leds' state
  update_actuators();
  
	debug.print("---- ");
	debug.print(cycle_counter);
	debug.println(" Loop END ----");
	cycle_counter++;
	delay(cycle_delay); // wait before next cycle
}


// --- Update --- 
void update_config(){
  const String cycle_delay_node = database_path + "/config/cycle_delay";
  const String sync_interval_node = database_path + "/config/sync_interval";
  const String humidifier_on_node = database_path + "/config/humidifier_on";
  const String hum_high_node = database_path + "/config/hum_high";
  const String hum_low_node = database_path + "/config/hum_low";
  const String lum_high_node = database_path + "/config/lum_high";
  const String lum_low_node = database_path + "/config/lum_low";
  const String moist_high_node = database_path + "/config/moist_high";
  const String moist_low_node = database_path + "/config/moist_low";
  const String temp_high_node = database_path + "/config/temp_high";
  const String temp_low_node = database_path + "/config/temp_low";

  Firebase.getInt(fbdo, cycle_delay_node.c_str(), cycle_delay);
  Firebase.getInt(fbdo, sync_interval_node.c_str(), sync_interval);
  Firebase.getBool(fbdo, humidifier_on_node.c_str(), humidifier_on);
  Firebase.getFloat(fbdo, hum_high_node.c_str(), hum_high);
  Firebase.getFloat(fbdo, hum_low_node.c_str(), hum_low);
  Firebase.getInt(fbdo, lum_high_node.c_str(), lum_high);
  Firebase.getInt(fbdo, lum_low_node.c_str(), lum_low);
  Firebase.getInt(fbdo, moist_high_node.c_str(), moist_high);
  Firebase.getInt(fbdo, moist_low_node.c_str(), moist_low);
  Firebase.getFloat(fbdo, temp_high_node.c_str(), temp_high);
  Firebase.getFloat(fbdo, temp_low_node.c_str(), temp_low);

}

void update_sensor_readings(){
  
  // DHT11
  float temp_reading = dht.readTemperature();
  float hum_reading  = dht.readHumidity();
  
  // Photoregistor
  int lum_reading = analogRead(photoregistorPin);
  lum_reading = map(lum_reading, 0, 8191, 0, 100);
  lum_reading = constrain(lum_reading, 0, 100);
 
  // Groove Moisture Sensor
  int moist_reading = analogRead(moisturePin);
  moist_reading = map(moist_reading, 0, 8191, 0, 100);
  moist_reading = constrain(moist_reading, 0, 100);


  // update values (weighted average)
  if ( !isnan(temp_reading) && !isnan(hum_reading)) {
    temp_val = (temp_val + temp_reading) / 2;
    hum_val = (hum_val + hum_reading) / 2;
    debug.print("Temperature: ");
    debug.print(temp_reading);
    debug.println(" *C");
    debug.print("Humidity: ");
    debug.println(hum_reading);
  } else {
    debug.println("Failed to get temprature and humidity value.");
  }

  lum_val= (lum_val + lum_reading) / 2;
  debug.print("Luminosity: ");
  debug.println(lum_reading);

  moist_val = (moist_val + moist_reading) / 2;
  debug.print("Soil moisture: ");
  debug.println(moist_reading);
}

void update_actuators(){
 
  clear_temp_led();
  if (temp_val < temp_low) {
    digitalWrite(blue_temp_pin, HIGH);
  } else if (temp_val < temp_high) {
    digitalWrite(green_temp_pin, HIGH);
  } else{
    digitalWrite(red_temp_pin, HIGH);
  }

  digitalWrite(humidifier, LOW);
  clear_hum_led();
  if (hum_val < hum_low) {
    digitalWrite(blue_hum_pin, HIGH);
    if(humidifier_on == 1){
      digitalWrite(humidifier, HIGH);
    }
  } else if (hum_val < hum_high) {
    digitalWrite(green_hum_pin, HIGH);
  } else{
    digitalWrite(red_hum_pin, HIGH);
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

void upload_defaults(){
  const String cycle_delay_node = database_path + "/config/cycle_delay";
  const String sync_interval_node = database_path + "/config/sync_interval";
  const String humidifier_on_node = database_path + "/config/humidifier_on";
  const String hum_high_node = database_path + "/config/hum_high";
  const String hum_low_node = database_path + "/config/hum_low";
  const String lum_high_node = database_path + "/config/lum_high";
  const String lum_low_node = database_path + "/config/lum_low";
  const String moist_high_node = database_path + "/config/moist_high";
  const String moist_low_node = database_path + "/config/moist_low";
  const String temp_high_node = database_path + "/config/temp_high";
  const String temp_low_node = database_path + "/config/temp_low";

  Firebase.setInt(fbdo, cycle_delay_node.c_str(), cycle_delay);
  Firebase.setInt(fbdo, sync_interval_node.c_str(), sync_interval);
  Firebase.setBool(fbdo, humidifier_on_node.c_str(), humidifier_on);
  Firebase.setFloat(fbdo, hum_high_node.c_str(), hum_high);
  Firebase.setFloat(fbdo, hum_low_node.c_str(), hum_low);
  Firebase.setInt(fbdo, lum_high_node.c_str(), lum_high);
  Firebase.setInt(fbdo, lum_low_node.c_str(), lum_low);
  Firebase.setInt(fbdo, moist_high_node.c_str(), moist_high);
  Firebase.setInt(fbdo, moist_low_node.c_str(), moist_low);
  Firebase.setFloat(fbdo, temp_high_node.c_str(), temp_high);
  Firebase.setFloat(fbdo, temp_low_node.c_str(), temp_low);
}

void upload_data(){
  String temp_node = database_path + "/temperature";
  String hum_node = database_path + "/humidity";
  String lum_node = database_path + "/luminosity";
  String moist_node = database_path + "/soil_moisture";

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

void setup_sensors(){
  dht.begin();
  temp_json.add("deviceuid", DEVICE_UID);
  temp_json.add("name", "DHT11-Temp");
  temp_json.add("type", "Temperature");
  temp_json.add("value", temp_val);

  hum_json.add("deviceuid", DEVICE_UID);
  hum_json.add("name", "DHT11-Hum");
  hum_json.add("type", "Humidity");
  hum_json.add("value", hum_val);

  lum_json.add("deviceuid", DEVICE_UID);
  lum_json.add("name", "Photoregistor");
  lum_json.add("type", "Luminosity");
  lum_json.add("value", lum_val);

  moist_json.add("deviceuid", DEVICE_UID);
  moist_json.add("name", "Groove Moisture Sensor");
  moist_json.add("type", "Soil moisture");
  moist_json.add("value", moist_val);
}

void setup_actuators(){
  pinMode(red_temp_pin, OUTPUT); 
  pinMode(green_temp_pin, OUTPUT); 
  pinMode(blue_temp_pin, OUTPUT); 
  pinMode(red_hum_pin, OUTPUT); 
  pinMode(green_hum_pin, OUTPUT); 
  pinMode(blue_hum_pin, OUTPUT); 
  pinMode(red_lum_pin, OUTPUT); 
  pinMode(green_lum_pin, OUTPUT); 
  pinMode(blue_lum_pin, OUTPUT); 
  pinMode(red_moist_pin, OUTPUT); 
  pinMode(green_moist_pin, OUTPUT); 
  pinMode(blue_moist_pin, OUTPUT); 
  
  pinMode(humidifier, OUTPUT); 
}

void setup_wifi() { 
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

void setup_firebase() {
	
	config.api_key = API_KEY; 			    // configure firebase API Key
	config.database_url = DATABASE_URL; // configure firebase realtime database url 
	Firebase.reconnectWiFi(true); 		  // Enable WiFi reconnection

	Serial.println("------------------------------------");
	Serial.println("Sign up new user...");
  	
	// Sign in to firebase anonymously
  if (Firebase.signUp(&config, &auth, "", "")){
	  Serial.println("Success");
   	is_authenticated = true;
 		database_path = "/" + device_location;
	 	fuid = auth.token.uid.c_str();
	} else {
 		Serial.printf("Failed, %s\n", config.signer.signupError.message.c_str()); 
		is_authenticated = false;
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

#include <Wire.h>
#include <WiFi.h>
#include <LiquidCrystal.h>
#include <RTClib.h>
#include <HTTPClient.h>
#include <TinyGPS++.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <MAX30100_PulseOximeter.h>

// LCD Pin Configuration (16x2 in Parallel Mode)
LiquidCrystal lcd(14, 27, 26, 25, 33, 32);

// WiFi Credentials
const char* ssid = "**********";
const char* password = "*******";

const char* botToken = "*******************************"; // Your Bot Token
const char* chatID = "*******"; // Your Chat ID

// Pin Definitions
// #define LM35_PIN A3 // Analog input (VP)
#define BUZZER 18
#define ADXL335_X A0  // GPIO36
#define ADXL335_Y 35  // GPIO39
#define ADXL335_Z A6  // GPIO34
#define REPORTING_PERIOD_MS     1000

// Create a PulseOximeter object
PulseOximeter pox;
// Time at which the last beat occurred
uint32_t tsLastReport = 0;

RTC_DS1307 rtc;
TinyGPSPlus gps;  // Create GPS object
HardwareSerial GPS_Serial(2);  // Use UART2 (GPIO16=RX, GPIO17=TX)

float calibration = 0.02;
float tempC; // Celcius
float tempF; // Fahrenheit
const byte LM35_PIN = A3;

void setup() {
    Serial.begin(115200);

    GPS_Serial.begin(9600, SERIAL_8N1, 16, 17);  // GPS on UART2
    Serial.println("GPS6MV2 Initialized...");

    Wire.begin(21,22);
    pinMode(LM35_PIN, INPUT);

    lcd.begin(16, 2);
    lcd.print("Booting..."); 

    if (!rtc.begin()) Serial.println("Couldn't find RTC");
    if(!rtc.isrunning()){
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    pinMode(BUZZER, OUTPUT);

    Serial.print("Initializing pulse oximeter..");
    Initialize sensor
    if (!pox.begin()) {
      Serial.println("FAILED");
      for(;;);
    } 
    else {Serial.println("SUCCESS");}
    // Configure sensor to use 7.6mA for LED drive
    pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);
    // Register a callback routine
    pox.setOnBeatDetectedCallback(onBeatDetected);

    // Connect to WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected!");
}

void loop() {
    //LM35 sensor temp values
    float sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += analogRead(LM35_PIN);
        delay(10);
    }
    float avgADC = sum / 10;
    tempC = avgADC * calibration; // get temp
    tempF = tempC * 1.8 + 32.0; // C to F; // Adjust for ESP32 ADC (12-bit)
    //rtc initialization
    DateTime now = rtc.now();

    //gps checking
    while (GPS_Serial.available()>0) {
      yield();
      char c = GPS_Serial.read();
      Serial.print(c);
      gps.encode(c);
      if (gps.location.isUpdated()) {
          yield();
          float latitude = gps.location.lat();
          float longitude = gps.location.lng();        
          Serial.print("Latitude: ");
          Serial.println(latitude, 6);
          Serial.print("Longitude: ");
          Serial.println(longitude, 6);
          // Generate Google Maps Link
          Serial.print("Google Maps Link: ");
          Serial.print("https://www.google.com/maps/search/?api=1&query=");
          Serial.print(latitude, 6);
          Serial.print(",");
          Serial.println(longitude, 6);
      }
    }
    //values for accelerometer
    int xValue = (((analogRead(ADXL335_X)*3.3)/4095.0)-1.65)/0.33;
    int yValue = (((analogRead(ADXL335_Y)*3.3)/4095.0)-1.65)/0.33;
    int zValue = (((analogRead(ADXL335_Z)*3.3)/4095.0)-1.65)/0.33;
    // //values for pulse-oximeter
    pox.update();
    // Grab the updated heart rate and SpO2 levels
    if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
        Serial.print("Heart rate:");
        Serial.print(pox.getHeartRate());
        Serial.print("bpm / SpO2:");
        Serial.print(pox.getSpO2());
        Serial.println("%");

        tsLastReport = millis();
    }

    Serial.print("X:"); Serial.print(xValue);
    Serial.print(" Y:"); Serial.print(yValue);
    Serial.print(" Z:"); Serial.println(zValue);

    // Fall Detection Logic
    if (zValue < -1) {  // Adjust threshold based on testing
        Serial.println("Fall Detected! Sending Telegram message...");
        tone(BUZZER,349, 1000); // tone() is the main function to use with a buzzer, it takes 2 or 3 parameteres (buzzer pin, sound frequency, duration)
        delay(1000);
        sendTelegramMessage("Fall Detected");
        delay(2000);
        tone(BUZZER,200, 2000);
        noTone(BUZZER); // You can also use noTone() to stop the sound it takes 1 parametere which is the buzzer pin
        delay(1000);
    }

    // Display Data on LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(tempC);
    lcd.print("C");
    lcd.setCursor(0, 1);
    lcd.print("Time: ");
    lcd.print(now.hour());
    lcd.print(":");
    lcd.print(now.minute());
    lcd.print(":");
    lcd.print(now.second());

    Serial.print("Temp: "); Serial.print(tempC); Serial.println(" C");Serial.print(tempF); Serial.println(" F");
    Serial.print("Time: "); Serial.print(now.hour()); Serial.print(":");
    Serial.print(now.minute()); Serial.print(":"); Serial.println(now.second());

    delay(2000);
}

// Function to send Telegram Notification
void sendTelegramMessage(String message) {
    String url = "https://api.telegram.org/bot" + String(botToken) + "/sendMessage?chat_id=" + String(chatID) + "&text=" + message;
    
    HTTPClient http;
    http.begin(url);  // Initialize the request
    int httpResponseCode = http.GET();
    
    if (httpResponseCode > 0){
        Serial.println("Telegram Message Sent Successfully.");
    } 
    else{
        Serial.print("Error sending message: ");
        Serial.println(httpResponseCode);
    }
    
    http.end(); // Close connection
}

int ReadAxis(int axisPin){
	long reading = 0;
	analogRead(axisPin);
	delay(1);
	for (int i = 0; i < 10; i++){
	  reading += analogRead(axisPin);
	}
	return reading/10;
}

void onBeatDetected() {
    Serial.println("Beat!");
}

#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <Servo.h>
#include <TimeLib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

const char* ssid = "Defeed";
const char* password = "";

#define API_KEY "AIzaSyCsOHAnm8uYtOfPgdwz-Fd6sKWXxq1glSw"
#define DATABASE_URL "deefeed-test-4af92-default-rtdb.firebaseio.com"
#define USER_EMAIL "thomas@gmail.com"
#define USER_PASSWORD "123456"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const int relayPin = D1;  
const int servoPin = D2;  
const int buzzerPin = D3;  
const int trigPin = D5;  
const int echoPin = D6;   

const int SDA_PIN = D4; 
const int SCL_PIN = D7;  

FirebaseConfig config;
FirebaseAuth auth;
FirebaseData firebaseData;

Servo myservo;
WiFiUDP ntpUDP;
const long utcOffsetInSeconds = 7 * 3600;  
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds, 60000);  

String feedingDateStart = "";
String feedingDateEnd = "";
String feedingAmount = "";
String feedingTimes[2];  
bool feedingDonePagi = false; 
bool feedingDoneSore = false; 

void setup() {
  Serial.begin(115200);
  pinMode(relayPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  digitalWrite(relayPin, LOW);  
  digitalWrite(buzzerPin, LOW);  
  myservo.attach(servoPin);
  myservo.write(100);  

  Wire.begin(SDA_PIN, SCL_PIN);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Connecting to WiFi...");
  display.display();

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Connected to WiFi");
  display.display();

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  if (Firebase.beginStream(firebaseData, "/data_jadwal/deefeed_1")) {
    Serial.println("Firebase stream started");
    display.setCursor(0, 10);
    display.print("Firebase stream started");
    display.display();
  } else {
    Serial.println("Failed to start Firebase stream");
    display.setCursor(0, 10);
    display.print("Firebase stream failed");
    display.display();
    Serial.println(firebaseData.errorReason());
  }

  timeClient.begin();
}

void loop() {
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();
  setTime(epochTime);

  Serial.print("Current time from NTP server: ");
  Serial.println(timeClient.getFormattedTime());

  if (!Firebase.readStream(firebaseData)) {
    Serial.print("Stream error: ");
    Serial.println(firebaseData.errorReason());
  }

  if (firebaseData.streamTimeout()) {
    Serial.println("Stream timeout, resuming...");
  }

  if (firebaseData.streamAvailable()) {
    Serial.println("Stream data available");
    FirebaseJson* json = firebaseData.jsonObjectPtr();
    FirebaseJsonData jsonData;

    if (json->get(jsonData, "tanggal_mulai")) {
      feedingDateStart = String(jsonData.stringValue);
      Serial.print("Feeding schedule start date: ");
      Serial.println(feedingDateStart);
    } else {
      Serial.println("Failed to get feedingDateStart");
    }

    if (json->get(jsonData, "tanggal_selesai")) {
      feedingDateEnd = String(jsonData.stringValue);
      Serial.print("Feeding schedule end date: ");
      Serial.println(feedingDateEnd);
    } else {
      Serial.println("Failed to get feedingDateEnd");
    }

    if (json->get(jsonData, "jumlah_pakan")) {
      feedingAmount = String(jsonData.stringValue);
      Serial.print("Feeding amount: ");
      Serial.println(feedingAmount);
    } else {
      Serial.println("Failed to get feedingAmount");
    }

    if (json->get(jsonData, "jamPagi")) {
      feedingTimes[0] = String(jsonData.stringValue);
      Serial.print("Feeding time pagi: ");
      Serial.println(feedingTimes[0]);
    } else {
      Serial.println("Failed to get feedingTime pagi");
    }

    if (json->get(jsonData, "jamSore")) {
      feedingTimes[1] = String(jsonData.stringValue);
      Serial.print("Feeding time sore: ");
      Serial.println(feedingTimes[1]);
    } else {
      Serial.println("Failed to get feedingTime sore");
    }
    updateOLED();
  }

  checkFeedingTime();
  checkPakanLevel();
  delay(5000);
}

void updateOLED() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.print("DEEFEED");
  display.setTextSize(1);
  display.setCursor(0, 10);
  display.print("Start: ");
  display.print(feedingDateStart);
  display.setCursor(0, 20);
  display.print("End: ");
  display.print(feedingDateEnd);
  display.setCursor(0, 30);
  display.print("Pagi: ");
  display.print(feedingTimes[0]);
  display.setCursor(0, 40);
  display.print("Sore: ");
  display.print(feedingTimes[1]);
  display.setCursor(0, 50);
  display.print("Amount: ");
  display.print(feedingAmount);
  display.display();
}

void checkFeedingTime() {
  String currentDate = String(year()) + "-" + zeroPad(month()) + "-" + zeroPad(day());
  String currentTime = zeroPad(hour()) + ":" + zeroPad(minute());

  Serial.print("Checking feeding time: ");
  Serial.print(currentDate);
  Serial.print(" ");
  Serial.println(currentTime);

  if (currentDate >= feedingDateStart && currentDate <= feedingDateEnd) {
    Serial.println("Current date is within the feeding schedule");
    if (feedingTimes[0] == currentTime && !feedingDonePagi) {
      Serial.println("It's pagi feeding time!");

      int feedingAmountInt = feedingAmount.toInt();
      Serial.print("Feeding amount: ");
      Serial.println(feedingAmountInt);

      int feedingTimeInSeconds = feedingAmountInt / 100;  
      Serial.print("Feeding time calculated: ");
      Serial.print(feedingTimeInSeconds);
      Serial.println(" seconds");

      feed(feedingAmountInt, feedingTimeInSeconds);
      feedingDonePagi = true; 
      delay(20000);
    }

    if (feedingTimes[1] == currentTime && !feedingDoneSore) {
      Serial.println("It's sore feeding time!");

      int feedingAmountInt = feedingAmount.toInt();
      Serial.print("Feeding amount: ");
      Serial.println(feedingAmountInt);

      int feedingTimeInSeconds = feedingAmountInt / 100;  
      Serial.print("Feeding time calculated: ");
      Serial.print(feedingTimeInSeconds);
      Serial.println(" seconds");

      feed(feedingAmountInt, feedingTimeInSeconds);
      feedingDoneSore = true;  
      delay(20000); 
    }
  } else {
    Serial.println("Current date is outside the feeding schedule");
  }
}

void checkPakanLevel() {
  long duration, distance;
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);
  distance = (duration / 2) / 29.1;  

  Serial.print("Pakan level distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  if (distance > 0) {
    if (Firebase.setInt(firebaseData, "/data_pakan/level_pakan", distance)) {
      Serial.println("Pakan level updated in Firebase");
    } else {
      Serial.print("Failed to update pakan level: ");
      Serial.println(firebaseData.errorReason());
    }
  } else {
    Serial.println("Invalid distance reading");
  }
}

void feed(int feedingAmount, int feedingTimeInSeconds) {
  Serial.println("Feeding function called");
  Serial.println("Activating relay, servo, and buzzer");
  digitalWrite(relayPin, HIGH); 
  digitalWrite(buzzerPin, HIGH);  
  myservo.write(0); 
  delay(feedingTimeInSeconds * 1000);  
  myservo.write(100);  
  digitalWrite(relayPin, LOW); 
  digitalWrite(buzzerPin, LOW);  
  Serial.print("Feeding done for ");
  Serial.print(feedingAmount);
  Serial.println(" grams");
}

String zeroPad(int num) {
  return (num < 10) ? "0" + String(num) : String(num);
}

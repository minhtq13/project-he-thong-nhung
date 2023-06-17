#include <ESP32Servo.h>
#include <Arduino.h>
#include <SPI.h>
#include <SPIFFS.h>
#include <SD.h>
#include <WiFi.h>
#include <ETH.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include "FirebaseESP32.h"
#include <Adafruit_I2CDevice.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define RST_PIN         4    // Reset pin for MFRC522
#define SS_PIN          5   // Slave select pin for MFRC522

#define TRIG_PIN 15
#define ECHO_PIN 32

#define PIN_IN1  12 // ESP32 pin GIOP27 connected to the IN1 pin L298N
#define PIN_IN2  13 // ESP32 pin GIOP26 connected to the IN2 pin L298N
#define PIN_ENA  14 // ESP32 pin GIOP14 connected to the EN1 pin L298N

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define WIFI_SSID       "NHA"
#define WIFI_PASSWORD   "11111111"
#define FIREBASE_HOST   "demoesp32-1e57a-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define FIREBASE_AUTH   "nzI2fjaXvgHLqfgbqexBiC7aqAXkqW3qeXadHkry"

Servo oldServo;
Servo hondaServo;
Servo yamahaServo;
Servo vinServo;

int oldServoPin = 33;
int hondaServoPin = 25;
int yamahaServoPin = 26;
int vinServoPin = 27;
int duration_us, distance_cm;
FirebaseData firebaseData;
MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create instance of MFRC522

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

bool updateMoneySub(String cardUID);
void updateMoneyAdd(String cardUID);
void rotateServo(Servo servo, int angle);
void controlServo(String vehicleType);

void setup() {
  oldServo.attach(oldServoPin);
  hondaServo.attach(hondaServoPin);
  yamahaServo.attach(yamahaServoPin);
  vinServo.attach(vinServoPin);
  Serial.begin(9600);

  SPI.begin();         // Initialize SPI bus
  mfrc522.PCD_Init();  // Initialize MFRC522

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // Khởi tạo màn hình OLED
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  randomSeed(analogRead(0)); // Khởi tạo random seed

  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_ENA, OUTPUT);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);

  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();
}

void loop() {
  while(true) {
    oldServo.write(0);
    hondaServo.write(0);
    yamahaServo.write(0);
    vinServo.write(0);
    // Check for new RFID cards

    display.clearDisplay();

    display.setCursor(0, 0);
    display.println("Chao mung ban den voi he thong ");

    display.setCursor(0, 20);
    display.println("DOi TRA PIN/ACQUY XE MAY, DAP DIEN " );

    display.setCursor(0, 40);
    display.println("Tong Cong Ty AMNQ");

    display.display();

    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      // Get the RFID card UID
      String cardUID = "";
      for (byte i = 0; i < mfrc522.uid.size; i++) {
        cardUID += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
        cardUID += String(mfrc522.uid.uidByte[i], HEX);
      }

      if(updateMoneySub(cardUID)==false) {
        break;
      }
      // oldServo.write(0);
      // delay(1000);
      oldServo.write(180);

      digitalWrite(TRIG_PIN, LOW);
      delayMicroseconds(2);
      digitalWrite(TRIG_PIN, HIGH);
      delayMicroseconds(10);
      digitalWrite(TRIG_PIN, LOW);

      duration_us = pulseIn(ECHO_PIN, HIGH);

    
      //distance_cm = random(2); // Gán giá trị ngẫu nhiên 0 hoặc 1
      distance_cm = duration_us * 0.034/2;

      Serial.print("distance: ");
      Serial.println(distance_cm); 
      delay(1000);
      if(distance_cm>=7) {
        updateMoneyAdd(cardUID);
        oldServo.write(0);
        break;
      } else {
        oldServo.write(0);
        delay(1000);
        digitalWrite(PIN_IN1, HIGH); // control the motor's direction in clockwise
        digitalWrite(PIN_IN2, LOW);  // control the motor's direction in clockwise
        analogWrite(PIN_ENA, 255); // set maximum speed
        delay(2000); // rotate at maximum speed for 2 seconds in clockwise direction
        digitalWrite(PIN_IN1, LOW);  // stop motor
        digitalWrite(PIN_IN2, LOW);  // stop motor
        analogWrite(PIN_ENA, 0); // turn off motor
      }

      if (Firebase.getString(firebaseData, "/rfid/" + cardUID + "/type")) {
        String vehicleType = firebaseData.stringData();
        Serial.print("Vehicle type: ");
        Serial.println(vehicleType);

        controlServo(vehicleType);

      } else {
        Serial.println("Card ID does not exist in Firebase");
      }

      delay(1000);  // Wait for 1 second before checking for another card
    }
}
}
bool updateMoneySub(String cardUID) {
  // Check if card UID exists in Firebase
  if (Firebase.getInt(firebaseData, "/rfid/" + cardUID + "/Name")) {
    String NameCustom = firebaseData.stringData();
    Serial.print("Name custom: ");
    Serial.println(NameCustom);

    display.clearDisplay();
    display.setCursor(0, 10);
    display.println("Xin chao khach hang: " );
    display.setCursor(0, 30);
    display.println(NameCustom);
    display.display();
    delay(1000);
  }

  if (Firebase.getInt(firebaseData, "/rfid/" + cardUID + "/money")) {
    int currentMoney = firebaseData.intData();
    Serial.print("Current money: ");
    Serial.println(currentMoney);

    // Get the vehicle type from Firebase
    if (Firebase.getString(firebaseData, "/rfid/" + cardUID + "/type")) {
      String vehicleType = firebaseData.stringData();
      Serial.print("Vehicle type: ");
      Serial.println(vehicleType);

      // Calculate newMoney based on vehicle type
      int newMoney = currentMoney;

      if (vehicleType == "honda") {
        newMoney -= 5;
        if (Firebase.getInt(firebaseData, "/report/honda")) {
          int number = firebaseData.intData();
          number++;
          Serial.print("number: ");
          Serial.println(number);
          Firebase.setInt(firebaseData, "/report/honda", number);
        }
      } else if (vehicleType == "yamaha") {
        newMoney -= 10;
        if (Firebase.getInt(firebaseData, "/report/yamaha")) {
          int number = firebaseData.intData();
          number++;
          Serial.print("number: ");
          Serial.println(number);
          Firebase.setInt(firebaseData, "/report/yamaha", number);
        }
      } else if (vehicleType == "vin") {
        newMoney -= 15;
        if (Firebase.getInt(firebaseData, "/report/vin")) {
          int number = firebaseData.intData();
          number++;
          Serial.print("number: ");
          Serial.println(number);
          Firebase.setInt(firebaseData, "/report/vin", number);
        }
      } else {
        Serial.println("Unknown vehicle type");
        return false; // Return false if the vehicle type is invalid
      }

      // Check if balance is sufficient
      if (newMoney >= 0) {
        if (Firebase.setInt(firebaseData, "/rfid/" + cardUID + "/money", newMoney)) {
          Serial.println("Money updated in Firebase");
          Serial.print("New money: ");
          Serial.println(newMoney);

          display.clearDisplay();
          display.setCursor(0, 10);
          display.println("Thanh toan thanh cong: " );
          display.setCursor(0, 30);
          display.print("So tien con lai: ");
          display.println(newMoney);
          display.display();
          delay(1000);
  
          return true; // Return true if the update is successful
        } else {
          Serial.println("Error updating money in Firebase");
          return false; // Return false if there's an error updating the money
        }
      } else {
        Serial.println("Insufficient balance");
        display.clearDisplay();
        display.setCursor(0, 10);
        display.println("Tai khoan quy khach da het: " );
        display.setCursor(0, 30);
        display.println("Xin moi gap nhan vien de nap tien" );
        display.display();
        delay(1500);
  
        return false; // Return false if the balance is insufficient
      }
    } else {
      Serial.println("Failed to retrieve vehicle type from Firebase");
      return false; // Return false if the vehicle type cannot be retrieved from Firebase
    }
  } else {
    Serial.println("Card ID does not exist in Firebase");
    return false; // Return false if the card ID does not exist in Firebase
  }
}

void updateMoneyAdd(String cardUID) {
  // Check if card UID exists in Firebase
  if (Firebase.getInt(firebaseData, "/rfid/" + cardUID + "/money")) {
    int currentMoney = firebaseData.intData();
    Serial.print("Current money: ");
    Serial.println(currentMoney);


    // Get the vehicle type from Firebase
    if (Firebase.getString(firebaseData, "/rfid/" + cardUID + "/type")) {
      String vehicleType = firebaseData.stringData();
      Serial.print("Vehicle type: ");
      Serial.println(vehicleType);

      // Calculate newMoney based on vehicle type
      int newMoney = currentMoney;
      if (vehicleType == "honda") {
        newMoney += 5;
        Firebase.setInt(firebaseData, "/rfid/" + cardUID + "/money", newMoney);
        if (Firebase.getInt(firebaseData, "/report/honda")) {
          int number = firebaseData.intData();
          number--;
          Firebase.setInt(firebaseData, "/report/honda", number);}
      } else if (vehicleType == "yamaha") {
        newMoney += 10;
        Firebase.setInt(firebaseData, "/rfid/" + cardUID + "/money", newMoney);
        if (Firebase.getInt(firebaseData, "/report/yamaha")) {
          int number = firebaseData.intData();
          number--;
          Firebase.setInt(firebaseData, "/report/yamaha", number);}
      } else if (vehicleType == "vin") {
        newMoney += 15;
        Firebase.setInt(firebaseData, "/rfid/" + cardUID + "/money", newMoney);
        if (Firebase.getInt(firebaseData, "/report/vin")) {
          int number = firebaseData.intData();
          number--;
          Firebase.setInt(firebaseData, "/report/vin", number);}
      } else {
        Serial.println("Unknown vehicle type");
      }
} 
}
}

void controlServo(String vehicleType) {
  if (vehicleType == "honda") {
    hondaServo.write(180);
    // rotateServo(hondaServo, 180); // Rotate Honda servo to 0 degrees
    delay(1000);
    hondaServo.write(0);
  } else if (vehicleType == "yamaha") {
    yamahaServo.write(180);
    // rotateServo(yamahaServo, 180); // Rotate Yamaha servo to 180 degrees
    delay(1000);
    yamahaServo.write(0);
  } else if (vehicleType == "vin") {
    vinServo.write(180);
    // rotateServo(vinServo, 180); // Rotate Vin servo to 0 degrees
    delay(1000);
    vinServo.write(0);
  } else {
    Serial.println("Unknown vehicle type");
  }
}

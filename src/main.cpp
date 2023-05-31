#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <SPIFFS.h>
#include <iostream>
#include <cstdlib>
#include <string>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Servo.h>
#include <LiquidCrystal_I2C.h>
#include <FirebaseESP32.h>
#include <addons/TokenHelper.h> // Provide the token generation process info.
#include <addons/RTDBHelper.h>  // Provide the RTDB payload printing info and other helper functions.

/**                             SO DO PIN ESP32
 *                        __________________________
 *                     EN-|                        |-D23: MOSI(MFRC522)
 *                     VP-|                        |-D22: SCL(LCD i2c)
 *                     VN-|                        |-TXD
 *                    D34-|                        |-RXD
 *  (HC-SR04_2) Echo: D35-|                        |-D21: SDA(LCD i2c)
 *  (HC-SR04_2) Trig: D32-|                        |-D19: MISO(MFRC522)  // chung ca 3 module
 *  (HC-SR04_1) Echo: D33-|                        |-D18: SCK(MFRC522)   // chung ca 3 module
 *  (HC-SR04_1) Trig: D25-|                        |-D5: SDA_0(MFRC522)
 *  (HC-SR04_0) Echo: D26-|                        |-Tx2: SDA_1(MFRC522)
 *  (HC-SR04_0) Trig: D27-|                        |-Rx2: SDA_2(MFRC522)
 *  (servo_2) PWM :   D14-|                        |-D4: RST(MFRC522)
 *  (servo_1) PWM :   D12-|                        |-D2
 *  (servo_0) PWM :   D13-|                        |-D15
 *                    GND-|                        |-GND
 *                    Vin-|__________|U|___________|-3v3
 *
 */
#define PWM_0 13
#define PWM_1 12
#define PWM_2 14

#define SCREEN_WIDTH 128 // LCD display width, in pixels
#define SCREEN_HEIGHT 64 // LCD display height, in pixels

#define RST_PIN 4 
#define SDA_0 5   
#define SDA_1 17  
#define SDA_2 16  

#define Trig_0 27
#define Echo_0 26
#define Trig_1 25
#define Echo_1 33
#define Trig_2 32
#define Echo_2 35

#define SOUND_SPEED 0.034     // van toc am thanh 0.034 cm/uS
#define NUM_CELL_PIN_IN_ATM 3 // so cell pin trong ATM   3 cell

// Firebase declare
#define FIREBASE_HOST "https://atm-pin-esp32-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "cAxqqmOjmTtXC6QfCVWthOFHd2Jq6nvicj9g1mjU"

#define FEE_PER_PERCENT 100 // 100% pin = 10k => 1% = 100 VND

// Creat Variables firebase
FirebaseData firebaseData;
FirebaseJson json;
// config wifi
const char *ssid = "Nha 185";
const char *password = "12341234";
// set the LCD number of columns and rows
int lcdColumns = 16;
int lcdRows = 2;
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);

static TimerHandle_t auto_reload_timer = NULL;
// create MFRC522
MFRC522 mfrc522[NUM_CELL_PIN_IN_ATM];
int ssMFRC522[NUM_CELL_PIN_IN_ATM] = {SDA_0, SDA_1, SDA_2};
MFRC522::MIFARE_Key key;
MFRC522::StatusCode status;
// block so 4 luu gia tri dien ap cua Pin
byte blockAddr = 4;
// buffer luu gia tri dien ap
byte bufVoltage[18];
byte sizebuffVoltage = sizeof(bufVoltage);
byte trailerBlock = 7;

bool paySucccess = false;

static SemaphoreHandle_t sem_Handle_FindUser;        // binary
static SemaphoreHandle_t sem_Handle_ReadCapacity;    // binary
static SemaphoreHandle_t sem_Handle_FindCellPinFull; // binary
static SemaphoreHandle_t sem_Handle_ReturnPin;       // binary

// event group
#define BIT_EVENT_READ_CAPACITY (1 << 0)
#define BIT_EVENT_READ_USER (1 << 1)
#define BIT_EVENT_CHECK_PIN_FULL (1 << 2)
EventGroupHandle_t xEventGroupPayMoney; // event dong bo su kien thanh toan

static QueueHandle_t xQueueListPinFull; // queue chua danh sach vi tri cac pin da day trong ATM

const char *filename = "/StatusPinInATM.json"; // file data status cell pin in ATM (save flash)
String listIDCellPin[NUM_CELL_PIN_IN_ATM];     // danh sach id cac cell pin
String listStatusCellPin[NUM_CELL_PIN_IN_ATM]; // trang thai cell pin (Empty, Full, Charging, New)

bool thereIsUser = false; // co user dang doi pin hay khong
bool returnSlotPinFullToFrontQueue = false;

// servo
Servo servo[NUM_CELL_PIN_IN_ATM];

// class User
class User
{
public:
  String IDPin;
  String IDUser;
  int Money;
  String Name;
  User() {}
};
User user;

// class new Pin
class NewPin
{
public:
  int slot;
  String IDPin;
  float voltage;
  int capacity;
  NewPin() {}
};
NewPin newPin;

// class full Pin
class FullPin
{
public:
  int slot;
  String IDPin;
  FullPin() {}
};
FullPin fullPin;

///////////////////////////////////////////////////////////////////////////////////
// write vi tri cell pin moi dua vao
void writeStatusCellToFile(const char *filename, int slot, String IDPin, String status)
{
  listStatusCellPin[slot] = status;
  listIDCellPin[slot] = IDPin;
  File outfile = SPIFFS.open(filename, "w");
  StaticJsonDocument<200> doc;
  JsonArray idPin = doc.createNestedArray("IdCellPin");
  JsonArray statusPin = doc.createNestedArray("StatusCellPin");
  for (int i = 0; i < NUM_CELL_PIN_IN_ATM; i++)
  {
    idPin.add(listIDCellPin[i]);
    statusPin.add(listStatusCellPin[i]);
  }

  serializeJson(doc, Serial);

  if (serializeJson(doc, outfile) == 0)
  {
    Serial.println("Failed to write to SPIFFS file");
  }
  else
  {
    Serial.println("Success!");
  }
  outfile.close();
}

// write data Cell Pin to flash
void writeDataToFile(const char *filename)
{
  File outfile = SPIFFS.open(filename, "w");
  StaticJsonDocument<200> doc;
  JsonArray idPin = doc.createNestedArray("IdCellPin");
  idPin.add("0");
  idPin.add("11a1d823");
  idPin.add("11e8e923");

  JsonArray statusPin = doc.createNestedArray("StatusCellPin");
  statusPin.add("Empty");
  statusPin.add("Full");
  statusPin.add("Full");

  serializeJson(doc, Serial);

  if (serializeJson(doc, outfile) == 0)
  {
    Serial.println("Failed to write to SPIFFS file");
  }
  else
  {
    Serial.println("Success!");
  }
  outfile.close();
}

// read data Cell Pin to flash
void readDataFromFile(const char *filename)
{
  // Read JSON data from a file
  File file = SPIFFS.open(filename);
  if (file)
  {
    // Deserialize the JSON data
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, file);

    // Serial.println("Read idPin: ");
    for (int i = 0; i < 3; i++)
    {
      listIDCellPin[i] = String((const char *)doc["IdCellPin"][i]);
      // Serial.println(listIDCellPin[i]);
    }

    // Serial.println("Read statusCellPin: ");
    for (int i = 0; i < 3; i++)
    {
      listStatusCellPin[i] = String((const char *)doc["StatusCellPin"][i]);
      // Serial.println(listStatusCellPin[i]);
    }
  }
  file.close();
}

// map voltage to capacity
float mapC(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// tinh dung luong pin thong qua voltage
int calCapacity(float voltage)
{
  int capacity = 0;
  if (voltage >= 12.6)
  {
    capacity = 100;
  }
  else if (voltage >= 12.5)
  {
    capacity = (int)mapC(voltage, 12.5, 12.6, 90, 100);
  }
  else if (voltage >= 12.42)
  {
    capacity = (int)mapC(voltage, 12.42, 12.5, 80, 90);
  }
  else if (voltage >= 12.32)
  {
    capacity = (int)mapC(voltage, 12.32, 12.42, 70, 80);
  }
  else if (voltage >= 12.2)
  {
    capacity = (int)mapC(voltage, 12.2, 12.32, 60, 70);
  }
  else if (voltage >= 12.06)
  {
    capacity = (int)mapC(voltage, 12.06, 12.2, 50, 60);
  }
  else if (voltage >= 11.9)
  {
    capacity = (int)mapC(voltage, 11.9, 12.06, 40, 50);
  }
  else if (voltage >= 11.75)
  {
    capacity = (int)mapC(voltage, 11.75, 11.9, 30, 40);
  }
  else if (voltage >= 11.58)
  {
    capacity = (int)mapC(voltage, 11.58, 11.75, 20, 30);
  }
  else if (voltage >= 11.31)
  {
    capacity = (int)mapC(voltage, 11.31, 11.58, 10, 20);
  }
  else if (voltage >= 10.5)
  {
    capacity = (int)mapC(voltage, 10.5, 11.31, 0, 10);
  }
  else
  {
    capacity = 0;
  }
  return capacity;
}

// convert byte ID to String
String convertByteToHex(byte *buffer, byte bufferSize)
{
  String hex = "";
  for (byte i = 0; i < bufferSize; i++)
  {
    if (buffer[i] < 0x10)
    {
      hex += '0';
    }
    hex += String(buffer[i], HEX);
  }
  return hex;
}

// tinh khoang cach HR-SC04
float distanceSonarSensor(int trigPin, int echoPin)
{
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH);
  float distanceCm = (float)duration * SOUND_SPEED / 2;
  if (distanceCm == 0)
    return 100;
  else
    return distanceCm;
}

// tinh phi theo dung luong pin
int calFee(int capacity)
{
  return (int)((100 - capacity) * 100);
}

// lock cell pin
void lockCel(int slot)
{
  servo[slot].write(90);
}

// unlock cell pin
void unlockCel(int slot)
{
  servo[slot].write(0);
}
/////////////////////////////////////////////////////////////////////////////




///////////////////////// Task function /////////////////////////////////////
/**
 * check cac o pin empty
 * danh sach o pin empty duoc luu trong flash
 */
void taskCheckPin(void *arg)
{
  while (1)
  {
    if (thereIsUser == false)
    {
      Serial.println("@task check pin");
      /////////////////////// do khoang cach ////////////
      // thuc hien check lan luot cac o xem co o nao duoc dua pin moi vao khong
      float distanceCm[NUM_CELL_PIN_IN_ATM];
      vTaskDelay(500 / portTICK_PERIOD_MS);
      distanceCm[0] = distanceSonarSensor(Trig_0, Echo_0);
      Serial.printf("distance 0:   %fcm\n", distanceCm[0]);
      vTaskDelay(500 / portTICK_PERIOD_MS);
      distanceCm[1] = distanceSonarSensor(Trig_1, Echo_1);
      Serial.printf("distance 1:   %fcm\n", distanceCm[1]);
      vTaskDelay(500 / portTICK_PERIOD_MS);
      distanceCm[2] = distanceSonarSensor(Trig_2, Echo_2);
      Serial.printf("distance 2:   %fcm\n", distanceCm[2]);
      vTaskDelay(500 / portTICK_PERIOD_MS);

      for (int i = 0; i < NUM_CELL_PIN_IN_ATM; i++)
      {
        if (distanceCm[i] < 7 && listStatusCellPin[i] == "Empty")
        {
          mfrc522[i].PCD_Init(ssMFRC522[i], RST_PIN);
          delay(1);
          newPin.slot = i;
          Serial.printf("User inserted cell %d\n", i);
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.printf("Cell %d", i);

          // Neu co pin thi kiem tra id Pin
          if (mfrc522[i].PICC_IsNewCardPresent() && mfrc522[i].PICC_ReadCardSerial())
          {
            Serial.print("ID Pin: ");
            newPin.IDPin = convertByteToHex(mfrc522[i].uid.uidByte, mfrc522[i].uid.size);
            Serial.println(newPin.IDPin);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("ID Pin");
            lcd.setCursor(0, 1);
            lcd.print(newPin.IDPin);

            // Doc duoc id Pin thi luu vi tri o moi vao flash
            writeStatusCellToFile(filename, i, newPin.IDPin, "New");
            newPin.slot = i;
            // Khoa o chua pin
            lockCel(i);
            // give sem_Handle_ReadCapacity
            xSemaphoreGive(sem_Handle_ReadCapacity);
            thereIsUser = true;
          }
          else
          {
            // Khong doc duoc id Pin thi dua ra thong bao LCD => Khong dung loai pin(co the khong phai la Pin)
            lcd.setCursor(0, 1);
            lcd.print("Pin error");
            Serial.println("Pin error!!!");
            delay(5000);
            lcd.clear();
            lcd.setCursor(4, 0);
            lcd.print("ATM PIN");
          }
        }
      }
    }

    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

/**
 * Doc dien ap pin (tu the RF)
 * > 90% => Pin van con day
 * < 90% => duoc phep doi Pin
 */
void taskReadCapacity(void *arg)
{
  while (1)
  {
    // take 1 sem_Handle_CheckPin
    xSemaphoreTake(sem_Handle_ReadCapacity, portMAX_DELAY);
    // > 80% => Pin van con day => Thong bao LCD => give 1 sem_Handle_ReturnPin
    // < 80% => duoc phep doi Pin => set bit BIT_EVENT_READ_CAPACITY
    status = (MFRC522::StatusCode)mfrc522[newPin.slot].PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522[newPin.slot].uid));
    if (status != MFRC522::STATUS_OK)
    {
      Serial.print(F("PCD_Authenticate() failed: "));
      Serial.println(mfrc522[newPin.slot].GetStatusCodeName(status));
      return;
    }
    // Read data from the block (again, should now be what we have written)
    // Serial.print(F("Reading data from block "));
    // Serial.print(blockAddr);
    // Serial.println(F(" ..."));
    status = (MFRC522::StatusCode)mfrc522[newPin.slot].MIFARE_Read(blockAddr, bufVoltage, &sizebuffVoltage);
    if (status != MFRC522::STATUS_OK)
    {
      Serial.print(F("MIFARE_Read() failed: "));
      Serial.println(mfrc522[newPin.slot].GetStatusCodeName(status));
    }
    // Serial.print(F("Data in block "));
    // Serial.print(blockAddr);
    // Serial.println(F(":"));

    // voltage duoc luu trong 2 byte dau cua block 4
    int phannguyen = 0;
    int phanthapphan = 0;
    phannguyen = bufVoltage[0];
    phanthapphan = bufVoltage[1];
    newPin.voltage = phannguyen + (float)phanthapphan / 100;
    Serial.print("Voltage: ");
    Serial.println(newPin.voltage);
    // Halt PICC
    mfrc522[newPin.slot].PICC_HaltA();
    // Stop encryption on PCD
    mfrc522[newPin.slot].PCD_StopCrypto1();

    newPin.capacity = calCapacity(newPin.voltage);
    Serial.print("Capacity: ");
    Serial.println(newPin.capacity);
    delay(1000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.printf("Capacity %d", newPin.capacity);
    lcd.print("%");

    if (newPin.capacity <= 80) // capacity <= 80 % thi doi Pin
    {
      lcd.setCursor(0, 1);
      lcd.print("Valid capacity");
      Serial.println("Valid capacity");

      // give sem_Handle_FindUser, sem_Handle_FindCellPinFull
      xSemaphoreGive(sem_Handle_FindUser);
      xSemaphoreGive(sem_Handle_FindCellPinFull);
      delay(1000);
      xEventGroupSetBits(xEventGroupPayMoney, BIT_EVENT_READ_CAPACITY);
    }
    else
    {
      returnSlotPinFullToFrontQueue = true;
      lcd.setCursor(13, 0);
      lcd.print(">80");
      lcd.setCursor(0, 1);
      lcd.print("Invalid capacity");
      Serial.println("Invalid capacity!");
      delay(1000);

      paySucccess = false;
      xSemaphoreGive(sem_Handle_ReturnPin);
    }
  }
}

/**
 * Auto sac Pin cho cac pin hien tai trong ATM
 * Neu Pin day => ngung sac => cap nhat vi tri pin day vao Queue, sau do luu flash
 */
void taskChargePin(void *arg)
{
  while (1)
  {
    vTaskDelay(60000 / portTICK_PERIOD_MS);
  }
}

/**
 * Kiem tra xem co pin day trong ATM hay khong
 */
void taskCheckPinFull(void *arg)
{
  while (1)
  {
    // take 1 sem_Handle_CheckPin
    xSemaphoreTake(sem_Handle_FindCellPinFull, portMAX_DELAY);
    // Check queue xem co Pin nao da day
    // Co Pin day => return vi tri Pin day, set BIT_EVENT_CHECK_PIN_FULL
    // Khong co Pin day => Thong bao LCD, give 1 sem_Handle_ReturnPin

    if (xQueueReceive(xQueueListPinFull, &fullPin.slot, 0) == pdPASS)
    {
      Serial.printf("Pin full = %d; ", fullPin.slot);
      fullPin.IDPin = listIDCellPin[fullPin.slot];
      Serial.printf("ID: %s\n", fullPin.IDPin);
      xEventGroupSetBits(xEventGroupPayMoney, BIT_EVENT_CHECK_PIN_FULL);
    }
    else
    {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("No Pin Full");
      Serial.println("No Pin Full!");
      paySucccess = false;
      xSemaphoreGive(sem_Handle_ReturnPin);
    }
  }
}

/**
 * Tra Pin cho User
 */
void taskReturnPin(void *arg)
{
  while (1)
  {
    // take 1 sem_Handle_ReturnPin
    xSemaphoreTake(sem_Handle_ReturnPin, portMAX_DELAY);
    // neu Thanh toan thanh cong => Thong bao LCD => Update lai flash status Pin=> Servo tra Pin (fullPin)
    // neu khong thanh toan thanh cong (hoac cac truong hop con lai) => Servo tra pin cu (newPin)
    delay(2000);
    if (paySucccess)
    {
      writeStatusCellToFile(filename, fullPin.slot, "0", "Empty");            // xoa pin full
      writeStatusCellToFile(filename, newPin.slot, newPin.IDPin, "Charging"); // them pin new
      Serial.printf("Return full pin slot %d\n", fullPin.slot);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Return full pin");
      lcd.setCursor(0, 1);
      lcd.printf("slot %d", fullPin.slot);
      unlockCel(fullPin.slot);
    }
    else
    {
      writeStatusCellToFile(filename, newPin.slot, "0", "Empty"); // xoa pin new
      Serial.printf("Return old pin slot %d\n", newPin.slot);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Return old pin");
      lcd.setCursor(0, 1);
      lcd.printf("slot %d", newPin.slot);
      unlockCel(newPin.slot);

      // return slot pin full to front queue
      if (!returnSlotPinFullToFrontQueue)
      {
        xQueueSendToFront(xQueueListPinFull, &fullPin.slot, (TickType_t)0);
      }
    }


    delay(10000);
    lcd.clear();
    lcd.setCursor(4, 0);
    lcd.print("ATM PIN");

    // Reset all semaphore and event group
    xSemaphoreTake(sem_Handle_FindUser, 0);
    xSemaphoreTake(sem_Handle_ReadCapacity, 0);
    xSemaphoreTake(sem_Handle_FindCellPinFull, 0);
    xEventGroupClearBits(xEventGroupPayMoney, BIT_EVENT_READ_USER);
    xEventGroupClearBits(xEventGroupPayMoney, BIT_EVENT_CHECK_PIN_FULL);
    xEventGroupClearBits(xEventGroupPayMoney, BIT_EVENT_READ_CAPACITY);

    // reset variable
    thereIsUser = false;
    returnSlotPinFullToFrontQueue = false;
  }
}

/**
 * Doc User tu firebase theo id Pin
 */
void taskReadUser(void *arg)
{
  while (1)
  {
    // take 1 sem_Handle_CheckPin
    xSemaphoreTake(sem_Handle_FindUser, portMAX_DELAY);

    // Ton tai User => Hien thi User LCD => set bit BIT_EVENT_READ_USER
    // Khong ton tai User => Hien thi Pin khong dung loai LCD => give 1 sem_Handle_ReturnPin

    Firebase.setQueryIndex(firebaseData, "User", "IDPin", "");

    QueryFilter query;
    query.orderBy("IDPin");
    query.equalTo(newPin.IDPin);

    bool statusJson = Firebase.getJSON(firebaseData, "/User", query);
    // Convert firebasedata to string
    String strRaw = "";
    strRaw = firebaseData.to<FirebaseJson>().raw();

    // raw = {} la khong co data (length() < 5)
    if (strRaw.length() > 5)
    {
      // Format lai chuoi theo kieu objec {"A":"value1", "B":"value2", ...}
      int index1 = strRaw.indexOf("\{", 2);
      strRaw = strRaw.substring(index1, strRaw.length() - 1);
      const char *charUser;
      charUser = strRaw.c_str(); // copy string to char array
      // Serial.println(charUser);

      // decode JSON
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, charUser);
      const char *mIDPin = doc["IDPin"];
      const char *mIDUser = doc["IDUser"];
      const char *mMoney = doc["Money"];
      const char *mName = doc["Name"];
      user.Name = mName;
      user.IDUser = mIDUser;
      user.IDPin = mIDPin;
      user.Money = atoi(mMoney);
      Serial.printf("-> Name User: %s; IDUser: %s; IDPin: %s; Money: %d\n", user.Name, user.IDUser, user.IDPin, user.Money);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.printf("User %s", mName);
      xEventGroupSetBits(xEventGroupPayMoney, BIT_EVENT_READ_USER);
    }
    else
    {
      returnSlotPinFullToFrontQueue = true;
      Serial.println("-> Khong tim thay User!");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("No User");
      lcd.setCursor(0, 1);
      lcd.print("Invalid Pin");
      paySucccess = false;
      xSemaphoreGive(sem_Handle_ReturnPin);
    }

    // Clear all query parameters
    query.clear();
  }
}

/**
 * thanh toan
 */
void taskPayMoney(void *arg)
{
  while (1)
  {
    // wait 3 bit event
    xEventGroupWaitBits(
        xEventGroupPayMoney,                                                      /* The event group being tested. */
        BIT_EVENT_CHECK_PIN_FULL | BIT_EVENT_READ_CAPACITY | BIT_EVENT_READ_USER, /* The bits within the event group to wait for. */
        pdTRUE,                                                                   /* BIT_0 & BIT_4 should be cleared before returning. */
        pdTRUE,                                                                   /*  wait for both bits, either bit will do. */
        portMAX_DELAY);                                                           /* Wait a maximum for either bit to be set. */

    // Khong du tien => Hien thi thong bao LCD => give 1 sem_Handle_ReturnPin
    // Thanh toan thanh cong(tru tien + luu id pin full vao data User) => Thong bao LCD => sem_Handle_ReturnPin
    delay(1000);
    Serial.println("Paying ...");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Paying ...");
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    // tinh tien
    int balance = user.Money - calFee(newPin.capacity);

    if (balance >= 0)
    {
      // update data to firebase
      FirebaseJson updateData;
      updateData.set("Money", (String)balance);
      updateData.set("IDPin", fullPin.IDPin);
      char linkUpdate[15];
      sprintf(linkUpdate, "/User/%s/", user.IDUser); // update node /User/IDUser/

      if (Firebase.updateNode(firebaseData, linkUpdate, updateData))
      {
        Serial.println("-> Payment success");
        paySucccess = true;
        xSemaphoreGive(sem_Handle_ReturnPin);
        lcd.setCursor(0, 1);
        lcd.print("Payment success");
      }
      else
      {
        Serial.println("-> Payment failed!");
        paySucccess = false;
        xSemaphoreGive(sem_Handle_ReturnPin);
        lcd.setCursor(0, 1);
        lcd.print("Payment failed");
      }
    }
    else
    {
      Serial.println("-> Not enough money in the account!");
      paySucccess = false;
      xSemaphoreGive(sem_Handle_ReturnPin);
      lcd.setCursor(0, 1);
      lcd.print("Not enough money");
    }
  }
}

void setup()
{
  Serial.begin(115200);
  SPI.begin(); // Init SPI bus
  // init lcd
  lcd.init();
  lcd.backlight();
  lcd.setCursor(4, 0);
  lcd.print("ATM PIN");
  delay(1000);

  // init servo
  servo[0].attach(PWM_0);
  servo[1].attach(PWM_1);
  servo[2].attach(PWM_2);

  // Initialize the SPIFFS object
  if (!SPIFFS.begin(true))
  {
    Serial.println("Error initializing SPIFFS");
    while (true)
    {
    }
  }
  Serial.println("Save data cell pin to flash");
  writeDataToFile(filename);
  delay(500);
  //  doc tu flash vi tri cell trong
  readDataFromFile(filename);
  delay(500);
  xQueueListPinFull = xQueueCreate(NUM_CELL_PIN_IN_ATM, sizeof(int));
  int idFull[] = {0, 1, 2};
  for (int i = 0; i < NUM_CELL_PIN_IN_ATM; i++)
  {
    lockCel(i);
    if (listStatusCellPin[i] == "Full")
    {
      xQueueSend(xQueueListPinFull, &idFull[i], (TickType_t)0);
    }
    if (listStatusCellPin[i] == "Empty")
    {
      unlockCel(i);
    }
  }
  delay(500);

  // init HC-SR04
  pinMode(Trig_0, OUTPUT);
  pinMode(Echo_0, INPUT);
  pinMode(Trig_1, OUTPUT);
  pinMode(Echo_1, INPUT);
  pinMode(Trig_2, OUTPUT);
  pinMode(Echo_2, INPUT);

  // init MFRC522
  for (int i = 0; i < NUM_CELL_PIN_IN_ATM; i++)
  {
    delay(1);
    mfrc522[i].PCD_Init(ssMFRC522[i], RST_PIN);
    mfrc522[i].PCD_DumpVersionToSerial();
    mfrc522[i].PICC_HaltA();
    mfrc522[i].PCD_StopCrypto1();
    Serial.println();
  }
  // Prepare the A
  // using FFFFFFFFFFFFh which is the default at chip delivery from the factory
  for (byte i = 0; i < 6; i++)
  {
    key.keyByte[i] = 0xFF;
  }

  // wifi init
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("Wifi connected");
  // connect firebase
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);

  // init semaphore
  sem_Handle_FindUser = xSemaphoreCreateBinary();        // binary
  sem_Handle_ReadCapacity = xSemaphoreCreateBinary();    // binary
  sem_Handle_FindCellPinFull = xSemaphoreCreateBinary(); // binary
  sem_Handle_ReturnPin = xSemaphoreCreateBinary();       // binary
  xEventGroupPayMoney = xEventGroupCreate();

  xTaskCreatePinnedToCore(taskCheckPin, "taskCheckPin", 5 * 1024, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(taskReadCapacity, "taskReadCapacity", 15 * 1024, NULL, 10, NULL, 1);
  xTaskCreatePinnedToCore(taskChargePin, "taskChargePin", 5 * 1024, NULL, 10, NULL, 1);
  xTaskCreatePinnedToCore(taskCheckPinFull, "taskCheckPinFull", 5 * 1024, NULL, 10, NULL, 1);
  xTaskCreatePinnedToCore(taskReturnPin, "taskReturnPin", 5 * 1024, NULL, 10, NULL, 1);
  xTaskCreatePinnedToCore(taskReadUser, "taskReadUser", 10 * 1024, NULL, 10, NULL, 1);
  xTaskCreatePinnedToCore(taskPayMoney, "taskPayMoney", 10 * 1024, NULL, 10, NULL, 1);
}

void loop()
{
}

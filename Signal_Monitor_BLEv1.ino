/**
 *
 * HX711 library for Arduino - example file
 * https://github.com/bogde/HX711
 *
 * MIT License
 * (c) 2018 Bogdan Necula
 *
**/
#include "esp_adc_cal.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
//#include <BLE2902.h>
//#include <BLE2904.h>
#include <EEPROM.h>
#include <Preferences.h>


#define RIGHT_TURN     34
#define LEFT_TURN     33
#define FILTER_LEN  15

#define SIGNAL_SERVICE_UUID  "4fafc201-1fb5-459e-8fcc-c5c9c3319142"
#define MONITOR_CHARACTERISTIC_A_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define VOLTAGE_SETTING_CHARACTERISTIC_B_UUID "beb5483e-36e1-4688-b7f6-ea07361b26b9"
#define VOLTAGE_CHARACTERISTIC_C_UUID "beb5483e-36e1-4688-b7f6-ea07361b26c7"
//#define CHARACTERISTIC_C_UUID "beb5483e-36e1-4688-b7f7-ea07361b26c8"
//#define CHARACTERISTIC_D_UUID "beb5483e-36e1-4688-b7f8-ea07361b26d9"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristicA = NULL;
BLECharacteristic* pCharacteristicB = NULL;
BLECharacteristic* pCharacteristicC = NULL;
//BLECharacteristic* pCharacteristicD = NULL;

Preferences preferences;

bool deviceConnected = false;
bool oldDeviceConnected = false;
bool voltageDetected = false;

uint32_t value = 0;

String txString;

int ADC_Setting;  //ADC setting from mobile iOS app


//These will need to be updated to the GPIO pins for each control circuit.
int POWER = 13; //13
int TIMER_SWITCH = 0; 
int BLE_CONNECTION = 2; //15
int VOLTAGE_DETECTED = 16; //16
 
uint32_t AN_Pot1_Buffer[FILTER_LEN] = {0};
int AN_Pot1_i = 0;
int AN_Pot1_Raw = 0;
int RIGHT_TURN_Raw = 0;
int LEFT_TURN_Raw = 0;
int AN_Pot1_Filtered = 0;

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      Serial.println("Characteristic Callback1");
    };
};

class ADCSettingCallback: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      Serial.println("Characteristic Voltage");
      if (value.length() > 0) {
        Serial.println("*********");
        Serial.print("ADC New value: ");
        
        char Str[] = {value[0], value[1], value[2], value[3], value[4]};
        
        Serial.println(atoi(Str));
        ADC_Setting = atoi(Str);
        preferences.putUInt("ADC_SetPoint", ADC_Setting);
        Serial.printf("ADC = ");
        Serial.println(ADC_Setting);
        EEPROM.write(1,ADC_Setting);
        EEPROM.commit();
        delay(500);
        byte value = EEPROM.read(1);
        Serial.println("ADC stored in EEPROM " + String(value));
        Serial.println();
        Serial.println("*********");
        preferences.end();
     };
  };
};

class VoltageCallback: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      Serial.println("Characteristic Voltage");
    };
};



class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Signal Monitor Device");

  preferences.begin("my−app", false);

  pinMode(POWER, OUTPUT);
  pinMode(BLE_CONNECTION, OUTPUT);
  pinMode(VOLTAGE_DETECTED, INPUT);
  
  
  
  digitalWrite(POWER, LOW);
  digitalWrite(BLE_CONNECTION, LOW);
  digitalWrite(VOLTAGE_DETECTED, LOW);
 

  // Create the BLE Device
  BLEDevice::init("SiGMo");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SIGNAL_SERVICE_UUID);


  // Create a BLE Characteristic A
  pCharacteristicA = pService->createCharacteristic(
                      MONITOR_CHARACTERISTIC_A_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE_NR |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );
                    
// Create a BLE Characteristic B
  pCharacteristicB = pService->createCharacteristic(
                      VOLTAGE_SETTING_CHARACTERISTIC_B_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE_NR  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );

// Create a BLE Characteristic C
  pCharacteristicC = pService->createCharacteristic(
                      VOLTAGE_CHARACTERISTIC_C_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE_NR  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );

  pCharacteristicA->setCallbacks(new MyCallbacks());
  pCharacteristicB->setCallbacks(new ADCSettingCallback());
  pCharacteristicC->setCallbacks(new VoltageCallback());  //THIS MAY NOT BE NEEDED

  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SIGNAL_SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");
  
  unsigned int ADC_SetPoint = preferences.getUInt("ADC_SetPoint", 0);

  Serial.printf("Current ADC Monitor at: %u\n", ADC_SetPoint);
}

void loop() {
  
    if (deviceConnected) {
        digitalWrite(BLE_CONNECTION, HIGH);
        RIGHT_TURN_Raw = analogRead(RIGHT_TURN);
        LEFT_TURN_Raw = analogRead(LEFT_TURN);
        if (RIGHT_TURN_Raw > 0) {
          AN_Pot1_Raw = RIGHT_TURN_Raw;
        }
        if (LEFT_TURN_Raw > 0) {
          AN_Pot1_Raw = LEFT_TURN_Raw;
        } 
        AN_Pot1_Filtered = readADC_Avg(RIGHT_TURN_Raw);
        //Serial.print(AN_Pot1_Raw);        // Print Raw Reading
        //Serial.print(',');                // Comma Separator
        //Serial.println(AN_Pot1_Filtered); // Print Filtered Output
        //double ADC = AN_Pot1_Filtered;
        //char txString[5];
        //dtostrf(ADC,1,2,txString);
        //pCharacteristicC->setValue(txString);
        //pCharacteristicC->notify();
        //AN_Pot1_Raw = 0;
        if ((AN_Pot1_Filtered >= 100) && (voltageDetected == false)) {
          digitalWrite(VOLTAGE_DETECTED, HIGH);
          char txString[4];
          dtostrf(AN_Pot1_Filtered, 1, 2, txString);
          pCharacteristicC->setValue(txString);
          pCharacteristicC->setValue(txString);
          pCharacteristicC->notify();
          delay(100);
          int value = 1;
          Serial.print("Active   ");
          Serial.print(AN_Pot1_Raw);        // Print Raw Reading
          Serial.print(',');                // Comma Separator
          Serial.println(AN_Pot1_Filtered); // Print Filtered Output
          pCharacteristicA->setValue((uint8_t*)&value, 4);
          pCharacteristicA->notify();
          voltageDetected = true;
          
        } else if ((AN_Pot1_Filtered <= 100) && (voltageDetected == true)) {
            digitalWrite(VOLTAGE_DETECTED, LOW);
            char txString[4];
            dtostrf(AN_Pot1_Filtered, 1, 2, txString);
            pCharacteristicC->setValue(txString);
            pCharacteristicC->setValue(txString);
            pCharacteristicC->notify();
            delay(100);                       // wait for a milli
            int value = 0;
            Serial.print("Not Active   ");
            Serial.print(AN_Pot1_Raw);        // Print Raw Reading
            Serial.print(',');                // Comma Separator
            Serial.println(AN_Pot1_Filtered); // Print Filtered Output
            pCharacteristicA->setValue((uint8_t*)&value, 4);
            pCharacteristicA->notify();
            voltageDetected = false;
        }
        //char txString[5];
        //dtostrf(AN_Pot1_Filtered,1,2,txString);
    }
    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(100); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("start advertising");
        oldDeviceConnected = deviceConnected;
       digitalWrite(BLE_CONNECTION, LOW);
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
        // do stuff here on connecting
        Serial.println(" Device is connecting");
        oldDeviceConnected = deviceConnected;
    }
}

uint32_t readADC_Avg(int ADC_Raw)
{
  int i = 0;
  uint32_t Sum = 0;
  
  AN_Pot1_Buffer[AN_Pot1_i++] = ADC_Raw;
  if(AN_Pot1_i == FILTER_LEN)
  {
    AN_Pot1_i = 0;
  }
  for(i=0; i<FILTER_LEN; i++)
  {
    Sum += AN_Pot1_Buffer[i];
  }
  return (Sum/FILTER_LEN);
}
  

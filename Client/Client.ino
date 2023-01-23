/*
  BLE_Peripheral.ino

  This program uses the ArduinoBLE library to set-up an Arduino Nano 33 BLE 
  as a peripheral device and specifies a service and a characteristic. Depending 
  of the value of the specified characteristic, an on-board LED gets on. 

  The circuit:
  - Arduino Nano 33 BLE. 

  This example code is in the public domain.
*/

//========================================Libraries===============================================
#include <ArduinoBLE.h> //Bluetooth library
#include <Arduino_LSM9DS1.h> //Accelerometer, Gyroscope sensor library


#include <TensorFlowLite.h>
#include <tensorflow/lite/micro/all_ops_resolver.h> 
#include <tensorflow/lite/micro/micro_error_reporter.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/schema/schema_generated.h>
#include <tensorflow/lite/version.h>
#include "model.h"
//================================================================================================



//========================================Global variabls=========================================
const char* deviceServiceUuid = "2AF8";  //"19b10000-e8f2-537e-4f6c-d104768a1214";
const char* deviceServiceCharacteristicUuid =  "181A"; //"19b10001-e8f2-537e-4f6c-d104768a1214";
BLEService gestureService(deviceServiceUuid); 
BLEByteCharacteristic gestureCharacteristic(deviceServiceCharacteristicUuid, BLERead | BLEWrite);
float aX, aY, aZ, gX, gY, gZ; 
const float accelerationThreshold = 2.5; // threshold of significant in G's
const int numSamples = 119;
int samplesRead = numSamples;
int gesture_index =-1;
int rightGesture_value;
float max_confidence;
int ex = 1;

// global variables used for TensorFlow Lite (Micro)
tflite::MicroErrorReporter tflErrorReporter;

// pull in all the TFLM ops, you can remove this line and
// only pull in the TFLM ops you need, if would like to reduce
// the compiled size of the sketch.
tflite::AllOpsResolver tflOpsResolver;

const tflite::Model* tflModel = nullptr;
tflite::MicroInterpreter* tflInterpreter = nullptr;
TfLiteTensor* tflInputTensor = nullptr;
TfLiteTensor* tflOutputTensor = nullptr;

// Create a static memory buffer for TFLM, the size may need to
// be adjusted based on the model you are using
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! WE HAVE TO CHANGE THE SIZE and the array GESTURES[]!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
constexpr int tensorArenaSize = 8 * 1024;
byte tensorArena[tensorArenaSize] __attribute__((aligned(16)));

// array to map gesture index to a name
const char* GESTURES[] = {
  "gesteFermer",
    "gesteArriere",
    "gesteAvantLeft",
    "gesteBas",
    "gesteHaut",
    "gesteOuvrir"
};

#define NUM_GESTURES (sizeof(GESTURES) / sizeof(GESTURES[0]))
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
enum {
  HAND_GESTURE_CLOSE = 0,
  HAND_GESTURE_BACK  = 1,
  HAND_GESTURE_FORTH  = 2,
  HAND_GESTURE_DOWN = 3,
  HAND_GESTURE_UP =  4,
  HAND_GESTURE_OPEN = 5
};
//================================================================================================



//========================================setup()=================================================
void setup() {
  Serial.begin(9600);
  
  while (!Serial);  
  
  if(!IMU.begin()){
    Serial.println("* Error initializing LSM9DS1 sensor!");
  }
  
  if (!BLE.begin()) {
    Serial.println("- Starting Bluetooth® Low Energy module failed!");
    while (1);
  }

  BLE.setLocalName("Arduino Nano 33 BLE (Peripheral)");
  BLE.setAdvertisedService(gestureService);  
  // BLE add characteristics
  gestureService.addCharacteristic(gestureCharacteristic);

  // add service
  BLE.addService(gestureService);

  //BLE.setEventHandler(BLEConnected, blePeripheralConnectHandler);
  
 // gestureCharacteristic.setEventHandler(BLEWritten, getCharacteristicWritten);

  BLE.advertise();

  Serial.println("Nano 33 BLE (Peripheral Device)");
  Serial.println(" ");

  // get the TFL representation of the model byte array
  tflModel = tflite::GetModel(model);
  if (tflModel->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println("Model schema mismatch!");
    while (1);
  }

  // Create an interpreter to run the model
  tflInterpreter = new tflite::MicroInterpreter(tflModel, tflOpsResolver, tensorArena, tensorArenaSize, &tflErrorReporter);

  // Allocate memory for the model's input and output tensors
  tflInterpreter->AllocateTensors();

  // Get pointers for the model's input and output tensors
  tflInputTensor = tflInterpreter->input(0);
  tflOutputTensor = tflInterpreter->output(0);
}
//================================================================================================

void classification(){
  ex=0;
        // wait for significant motion
   while (samplesRead == numSamples) {
      if (IMU.accelerationAvailable()) {
        // read the acceleration data
        IMU.readAcceleration(aX, aY, aZ);
  
        // sum up the absolutes
        float aSum = fabs(aX) + fabs(aY) + fabs(aZ);
  
        // check if it's above the threshold
        if (aSum >= accelerationThreshold) {
          // reset the sample read count
          samplesRead = 0;
          break;
        }
      }
    }

    // check if the all the required samples have been read since
    // the last time the significant motion was detected
    Serial.print("Insert Gesture LOOP");
  while (samplesRead < numSamples) {
      insertGesture();
  }
      Serial.print("I Inserted Gestures ");

  if (samplesRead == numSamples) {
     Serial.print("  Enter classify gestures ");
       
// Loop through the output tensor values from the model
  max_confidence = -1.0;
  for (int i = 0; i < NUM_GESTURES; i++) {
    Serial.print(GESTURES[i]);
    Serial.print(": ");
    Serial.println(tflOutputTensor->data.f[i], 6);
    if(max_confidence <= tflOutputTensor->data.f[i])
    {
    max_confidence =  tflOutputTensor->data.f[i];
    rightGesture_value = i;
    }
  } 
      Serial.println("  AFTER classify gestures LOOP");
     Serial.println("  Out classify gestures ");

    }
}


//========================================loop()=================================================
void loop() {
   // if(ex ==1){
      //==========calssification=========
  classification();
  delay(5000);
    
    //============================  Connection  ==================================
  BLEDevice central = BLE.central();
  Serial.println("- Discovering central device...");
 // delay(1000);

  if (central) {
    Serial.println("* Connected to central device!");
    Serial.print("* Device MAC address: ");
    Serial.println(central.address());
    Serial.println(" ");
    while (central.connected() && gesture_index== -1) {
      if (gestureCharacteristic.written()) {
         gesture_index = gestureCharacteristic.value();
         handGesture(gesture_index, rightGesture_value);
         central.disconnect();
         
       }
      
    }
    
    Serial.println("* Disconnected to central device!");
  }else{
    Serial.println("* central device NOT FOUND!");
    } 
 // }
}
//================================================================================================
/*void blePeripheralConnectHandler(BLEDevice central){
  Serial.println("* Connected to central device!");
    Serial.print("* Device MAC address: ");
    Serial.println(central.address());
} */
/*
void getCharacteristicWritten(BLEDevice central, BLECharacteristic characteristic){
  if (gestureCharacteristic.written()) {
         //gesture_index =(byte) characteristic.value();
         gesture_index = gestureCharacteristic.value();
         handGesture(gesture_index, rightGesture_value);
     }
     ex=1;
} */


void classifyGesture(){
    
  TfLiteStatus invokeStatus = tflInterpreter->Invoke();
  if (invokeStatus != kTfLiteOk) {
    Serial.println("Invoke failed!");
    while (1);
    return;}
// Loop through the output tensor values from the model
  max_confidence = -1;
  Serial.println("I will enter the looop");
  for (int i = 0; i < NUM_GESTURES; i++) {
    Serial.println("inside the looop");
    Serial.print(GESTURES[i]);
    Serial.print(": ");
    Serial.println(tflOutputTensor->data.f[i], 6);
    if(max_confidence <= tflOutputTensor->data.f[i])
    {
    max_confidence =  tflOutputTensor->data.f[i];
    rightGesture_value = i;
    }
  } 
  Serial.println("+++ OUTSIDE looop");

}

void insertGesture(){
  
    // check if new acceleration data is available
    if (IMU.accelerationAvailable() && IMU.gyroscopeAvailable()) {
      // read the acceleration data
      IMU.readAcceleration(aX, aY, aZ);
      IMU.readGyroscope(gX, gY, gZ);


      // normalize the IMU data between 0 to 1 and store in the model's
      // input tensor
      tflInputTensor->data.f[samplesRead * 6 + 0] = (aX + 4.0) / 8.0;
      tflInputTensor->data.f[samplesRead * 6 + 1] = (aY + 4.0) / 8.0;
      tflInputTensor->data.f[samplesRead * 6 + 2] = (aZ + 4.0) / 8.0;
      tflInputTensor->data.f[samplesRead * 6 + 3] = (gX + 2000.0) / 4000.0;
      tflInputTensor->data.f[samplesRead * 6 + 4] = (gY + 2000.0) / 4000.0;
      tflInputTensor->data.f[samplesRead * 6 + 5] = (gZ + 2000.0) / 4000.0;

      samplesRead++;
    }
}

void handGesture(int gesture_index, int rightGesture_value)
{
  
  switch (gesture_index) {
    case HAND_GESTURE_UP :
      if(rightGesture_value == HAND_GESTURE_UP){
        Serial.println("##################### HAND GESTURE => UP #####################");
      }else{
          Serial.println("##################### Unkown Gesture #####################");

      }
      break;
    case HAND_GESTURE_DOWN:
    if(rightGesture_value == HAND_GESTURE_DOWN){
      Serial.println("##################### HAND GESTURE => DOWN #####################");
      }else{
          Serial.println("##################### Unkown Gesture #####################");
           }
      break;
    case HAND_GESTURE_BACK:
      if(rightGesture_value == HAND_GESTURE_BACK){
      Serial.println("##################### HAND GESTURE => ARRIERE #####################");
        }else{
            Serial.println("##################### Unkown Gesture #####################");
           }
      break;
    case HAND_GESTURE_FORTH:
      if(rightGesture_value == HAND_GESTURE_FORTH){
        Serial.println("##################### HAND GESTURE => FORTH #####################");
      }else{
          Serial.println("##################### Unkown Gesture #####################");
           }      break;
    case HAND_GESTURE_OPEN:
      if(rightGesture_value == HAND_GESTURE_CLOSE){
        Serial.println("##################### HAND GESTURE => CLOSE  #####################");
        }else{
            Serial.println("##################### Unkown Gesture #####################");
           }
      break;
    case HAND_GESTURE_CLOSE:
     if(rightGesture_value == HAND_GESTURE_OPEN){
        Serial.println("##################### HAND GESTURE =>  OPEN #####################");
        }else{
            Serial.println("##################### Unkown Gesture #####################");
           }
      break;
  } 
  gesture_index=  -1;
}
//================================================================================================

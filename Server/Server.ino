/*
  BLE_Central_Device.ino

  This program uses the ArduinoBLE library to set-up an Arduino Nano 33 BLE Sense 
  as a central device and looks for a specified service and characteristic in a 
  peripheral device. If the specified service and characteristic is found in a 
  peripheral device, the last detected value of the on-board gesture sensor of 
  the Nano 33 BLE Sense, the APDS9960, is written in the specified characteristic. 

  The circuit:
  - Arduino Nano 33 BLE Sense. 

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
const char* deviceServiceUuid = "2AF8"; //"19b10000-e8f2-537e-4f6c-d104768a1214";
const char* deviceServiceCharacteristicUuid = "181A"; //"19b10001-e8f2-537e-4f6c-d104768a1214";
float aX, aY, aZ, gX, gY, gZ; 
const float accelerationThreshold = 2.5; // threshold of significant in G's
const int numSamples = 119;
int samplesRead = numSamples;
int gesture_index;
float max_confidence;
BLECharacteristic gestureCharacteristic;


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

//================================================================================================



//========================================setup()=================================================
void setup(){
  Serial.begin(9600);
  while(!Serial);
  
  if(!IMU.begin()){
    Serial.println("* Error initializing LSM9DS1 sensor!");
  }
  
  if(!BLE.begin()){
    Serial.println("* Starting Bluetooth® Low Energy module failed!");
    while(1);
  }
  
  BLE.setLocalName("Nano 33 BLE (Central)"); 
  BLE.advertise();
  
  Serial.println("Arduino Nano 33 BLE Sense (Central Device)");
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



//========================================loop()=================================================
void loop() {
  classifyModel();
  
  Serial.println("- Finish Classifying ...");

  connectToPeripheral();
}
//================================================================================================



//========================================connectToPeripheral()===================================
void connectToPeripheral(){
  BLEDevice peripheral;
  
  Serial.println("- Discovering peripheral device...");
  
  do
  {
    BLE.scanForUuid(deviceServiceUuid);
    peripheral = BLE.available();
  } while (!peripheral);
  
  if (peripheral) {
    Serial.println("* Peripheral device found!");
    Serial.print("* Device MAC address: ");
    Serial.println(peripheral.address());
    Serial.print("* Device name: ");
    Serial.println(peripheral.localName());
    Serial.print("* Advertised service UUID: ");
    Serial.println(peripheral.advertisedServiceUuid());
    Serial.println(" ");
    BLE.stopScan();
  }
  if (peripheral.connect()) {
    Serial.println("* Connected to peripheral device!");
    Serial.println(" ");
    }else {
    Serial.println("* Connection to peripheral device failed!");
    Serial.println(" ");
    return;
  }

    Serial.println("- Discovering peripheral device attributes...");
  
  delay(2000);

  if (peripheral.discoverAttributes()) {
    Serial.println("* Peripheral device attributes discovered!");
    Serial.println(" ");
  }else {
    Serial.println("* Peripheral device attributes discovery failed!");
    Serial.println(" ");
    peripheral.disconnect();
    return;
  } 
  gestureCharacteristic = peripheral.characteristic(deviceServiceCharacteristicUuid);

   if (!gestureCharacteristic) {
    Serial.println("* Peripheral device does not have gesture characteristic!");
    peripheral.disconnect();
    return;
  } else if (!gestureCharacteristic.canWrite()) {
    Serial.println("* Peripheral does not have a writable gesture characteristic!");
    peripheral.disconnect();
    return;
  }

  // write the value
    
 // etablir la cnx 
  if (peripheral.connected()){
    Serial.print("* Writing value to gestureCharacteristic: ");
    Serial.print("Gesture : ");
    Serial.println(GESTURES[gesture_index]);
    Serial.println(gesture_index);
    gestureCharacteristic.writeValue((byte)gesture_index);
    Serial.println("* Writing value to gestureCharacteristic done!");
    Serial.println(" ");
    Serial.println();
    peripheral.disconnect();
  }
 

}
//================================================================================================



//========================================controlPeripheral(BLEDevice peripheral)=================
void classifyModel(){
  Serial.println("- Inside classsify model ...");
  //=================Classification==================
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
    while (samplesRead < numSamples) {
        insertGesture();
    }
    
    if (samplesRead == numSamples) {
      classifyGesture();
    }
    
}

    //============================================================
  

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

 void classifyGesture(){
  // Run inferencing
  TfLiteStatus invokeStatus = tflInterpreter->Invoke();
  if (invokeStatus != kTfLiteOk) {
    Serial.println("Invoke failed!");
    while (1);
    return;}
// Loop through the output tensor values from the model
  max_confidence = -1.0;
  for (int i = 0; i < NUM_GESTURES; i++) {
    Serial.print(GESTURES[i]);
    Serial.print(": ");
    Serial.println(tflOutputTensor->data.f[i], 6);
    if(max_confidence <= tflOutputTensor->data.f[i])
    {
    max_confidence =  tflOutputTensor->data.f[i];
    gesture_index = i;
    }
  } 
 }
//================================================================================================

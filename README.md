# Aim of project 
This project was developed for learning purposes during AC classes with professor LARABI at **USTHB**.
the project idea is about creating a gesture recognition system for mute people using tiny machine learning knowledge acquired during the class.
in this approach we collected accelerometer and gyroscope data from the arduino and labelized each file to train our classification model, the trained model
was downloaded into both arduinos since our gestures are symetric, the BLE characteristics integrated into arduino nano 33 BLE helped us in sending the return
value of the predicted gesture from one arduino to another, after the value is recieved, a simple comparison is needed to output the final result.

simply used BLE characteristics integrated into the Arduino nano 33 BLE, we collected 

# Requirements 
  - 2 Arduino nano 33 BLE with wires. <br />
![image](https://user-images.githubusercontent.com/100075994/214080068-608e9838-d4f2-41f8-8d10-9010e7bdba2d.png) 
  
  - Download the tensorflow Lite library for the arduino.

# How to use it ?

  - first, you need to install the Arduino IDE
  -Clone this repository, or you can just download the code.
  -Link the arduino board to your computer's USB port using the adequate wire.
  -Select the nano arduino as type of the board in the menu bar > Tools > Boards, or install the adequate board family if necessary.
  - Open the source code downloaded.
  -Compile the project by clicking on the tick on the top left.
  -Upload the code to the arduino board by pressing on the arrow next to the compiling button.
  -Move the arduino board, and see the results in the console.
  
 #### Note : 
 Client and server sketches must be uploaded to the two arduino, and both arduinos must be moved at the same time for the bluetooth connection.
 
  

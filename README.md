# Design & Build - Group 10 - Lower Computer and Upper Computer Code Repository

Visit [FacePerks](https://service.design-build.site) (https://service.design-build.site) to view the live demo and
explore its features.

To view the full project, please visit the following repositories:

- [Server (Backend) Code](https://github.com/Design-Build-Group-10/Server) (https://github.com/Design-Build-Group-10/Server)
- [Web Client (Frontend) Code](https://github.com/Design-Build-Group-10/Web-Client) (https://github.com/Design-Build-Group-10/Web-Client)
- [Upper and Lower Computer Code](https://github.com/Design-Build-Group-10/Lower-And-Upper-Computer) (https://github.com/Design-Build-Group-10/Lower-And-Upper-Computer)
- [Face Recognition Code](https://github.com/Design-Build-Group-10/FaceRecognition) (https://github.com/Design-Build-Group-10/FaceRecognition)

## Table of Contents

1. [Introduction](#introduction)
2. [Operation](#operation)
   - [Flashing](#21-flashing)
   - [Wiring](#22-wiring)
   - [Testing](#23-testing)

## 1. Introduction

- The serial communication method is used as a backup solution to achieve full functionality (transmitting images and controlling movements) when there are not two network interfaces available.
- Two microcontrollers serve as the lower computers: CAM and WROOM.
- Communication between the microcontrollers is through serial (CAM sends simple commands to WROOM to control movements), while communication between CAM and the upper computer is through UDP (CAM sends image streams to the upper computer).

## 2. Operation

### 2.1 Flashing

- Flash `ESP32_CAM.ino` onto the microcontroller located in the spider’s head, which has the camera.
- Flash `ESP32_WROOM.ino` onto the microcontroller located on the spider's back.

### 2.2 Wiring

- Wiring: Connect the RX pin of CAM to the S pin of IO port 17 on the back of the spider, and the TX pin of CAM to the S pin of IO port 16 on the back of the spider.

### 2.3 Testing

- Connect the WROOM microcontroller to the computer using a USB cable. In the Arduino IDE’s Serial Monitor, you should see test information “HELLO” sent from CAM to WROOM.
- Connect your computer to the WiFi network: **"10组CAM"**, with the password **"87654321"**.
- Set the computer’s IPv4 settings:
  - Address: **192.168.4.2**
  - Subnet Mask: **0.0.0.0**
- Once the network settings are configured, you can run the Python program to see the image stream sent from CAM in the window.
- Since the serial port control instructions are not yet set up, spider motion control is currently handled via a mobile app. Connect the phone to the WiFi **"10组小蜘蛛"**, with the password **"87654321"**, and control the spider using the **"Quadruped Robot Spider"** app.

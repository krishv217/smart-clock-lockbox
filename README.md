# smart-clock-lockbox
A smart clock display with a built in lockbox using an ESP32 and solenoid lock. This project was made for my High School Senior Technology Capstone class. This is a guide on how to construct one for yourself, step-by-step!

Clock/Weather display page
![Clock Page](1F9A40AA-B783-4323-89D5-245EC56DFFDB_1_201_a.jpeg)

Pinpad page
![Pinpad Page](IMG_2972.jpg)

Inside layout
![Inside](image.png)

Components:
ESP32 Display: https://www.amazon.com/dp/B0D93MBWC2
Solenoid lock: https://www.amazon.com/dp/B0C793X21J
Other components needed: 5V relay, 9V battery, misc. wiring.

Directions:
1. In clockLock.ino replace your Wifi credentials and lat/longitude
2. Using the Arduino IDE, upload clockLock.ino to the ESP32

Electrical directions:
1. Connect the included harness to the I2C port on ESP32
2. Connect the 3.3V, 1025(SCL) and GND wires to the corresponding inputs on the relay
3. Connect the positive terminal of the battery to the output of the relay
4. Connect the positive wire of the solenoid lock to the output of the relay
5. Connect the negatives of the battery and solenoid together

Your wiring is now set up! You can now 3D print a box and lid or construct one out of cardboard.

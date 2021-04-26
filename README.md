# WeatherStation
Weather Station records Temperature, Humidity, Pressure and Elevation in a MariaDB hosted in a Raspberry Pi 3 Model B; 
the raw data are published via HTML by an Apache2 web server installed on the Raspberry. 
A real-Time dashboard is published as HTML directly from the ESP32 that acts as web server.
Microcontroller Parts and Sensors used:
- Development Board ESP-32 NodeMCU Dev Kit C V4 WiFi with ESP32 WROOM-32 Module (Chip is ESP32-D0WDQ6 (revision 1)
- DHT22 AM2302 Temperature and Humidity Sensor - Vendor AZDelivery
- GY-BME280 Barometric Pressure and Elevation Sensor
- Display Module Oled 128x64 Display
- Momentary PushButton Switch
- 10K Resistor
- Raspberry PI3 with MariaDB and Apache2

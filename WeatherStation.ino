// ====================================================== CONTRIBUTIONS =========================================================
// Below some links used to get suggestion and codes. Many thanks to the Owners.
// https://randomnerdtutorials.com/esp32-esp8266-raspberry-pi-lamp-server/
// https://lastminuteengineers.com/esp32-dht11-dht22-web-server-tutorial/
// https://how2electronics.com/esp32-bme280-mini-weather-station/
// https://randomnerdtutorials.com/esp32-date-time-ntp-client-server-arduino/
// https://diyprojects.io/esp8266-web-server-tutorial-create-html-interface-connected-object/

//==================================================================================================================================
// SZ Meteo Station V2.0
// This Meteo station is based on ESP32 Dev Module. 
// It is able to read from below sensors Meteo Data and to write them in a MariaDB installed on a Raspberry PI3
// Raw Data read, on pre-defined hours, from the MariaDB Table and are published via and php page from the Raspberry
// A Dashboard with the Real-Time data is published, in HTML format, by the ESP32, connected via WiFi.
// Data can be read also by an LCD 16X2 Display by pushing a button  
// Controller and Sensors:
// - ESP32 Dev Module
// - DHT22 sensor to read Temperature and Hunidity
// - BMP280 sensor to read Pressure and Station Elevation
//=======================================================================================================================================

#include <WiFi.h>
#include "time.h"
#include <Wire.h>

// WiFi Connection Settings 
const char* ssid       = "LZ_24G";
const char* password   = "*andromedA01.";

// Time Settings 
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;

// Declarations for Dashboard published directly via HTML by ESP32
#include <WebServer.h>
#define LISTEN_PORT               8080
WebServer server(LISTEN_PORT);  // publish Dashboard ESP32 (Temperature and Humidity on port 8080)

// Section HTTP request to publish data in DB hosted in a Raspi
#include <HTTPClient.h>
// Keep this API Key value to be compatible with the PHP code provided in the project page. 
// If you change the apiKeyValue value, the PHP file /post-esp-data.php also needs to have the same key 
String apiKeyValue = "tPmAT5Ab3j7F9";
String sensorName = "DHT22";
String sensorLocation = "Boot";
const char* serverName = "http://szweb.eu/post-esp-data.php"; //Raspi PI that publish Data HTML Pages

//#define DEBOUNCE_TIME 3600000  //define the DB update cycle time (1 Hour) if the time based approach isn't used (by npt server)
//long DebounceTimer=0; // used in WebSqlWrite function to start the loop for writing in DB
long currentMillis=0; // used in WebSqlWrite function
long rebootTimer=43200000;  //time to trigger SW reboot (12h in milliseconds)
int initialBoot=1; // used to control if the station did a start or reboot 1= station just started 

// Section for DTH22 Temperature and Humidity
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#define DHTPIN 19     // what digital pin we're connected to
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
DHT_Unified dht(DHTPIN, DHTTYPE);

float h_dht; //humidity 
float t_dht; // temperature in Celsius

// GY-BME 280 Barometer I2C Address 0x76 
#include <BMP280.h>
#define P0 1013.25 // pressione standard, a livello del mare in mBar/hPa
BMP280 bmp;

double A_MIN = 0;
double A_MAX = 0;
double a_bmp=130.0;  // altezza slm in via Monte Baldo
double t_bmp; 
double p_bmp;
double pCorr= 0.; //to normalize at the Level Sea P(level sea)= P(Real)+ 9*barom_H (9hpa every 100mt.) Pressure raise of 7mBar every 61 mt. in altitude
double tCorr=0.; //correction of -8 Celsius to Atm. Pressure to compensate chip calibration
double barom_H = 1.03; //Barometer Altitude divided by 100

//Section for LCD 16X2 - I2C Address 0x27
#include <LiquidCrystal_I2C.h>
int LCD_Cols = 16;
int LCD_Rows = 2;
LiquidCrystal_I2C lcd(0x27,LCD_Cols,LCD_Rows);
int pinBtn=4; // Command button for LCD - Connected to interrupt
int statusBtn; // Switch status 0=pressed 1=unpressed

//************************************************************************************************************************
void IRAM_ATTR isrLCD(void){  // this procedure is used to react to an interrupt after pressing Button for LCD display  
  statusBtn=digitalRead(pinBtn); // Switch status 0=pressed 1=unpressed 
  Serial.print ("Interrupt");
  //Serial.print (statusBtn);
}  // End of IRAM_ATTR Funtion

// ***********************************************************************************************************************
void setup(void)
{
  Serial.begin(115200);
  Serial.printf("Connecting to %s ", ssid);
  Serial.println();
  int not_Conn=0;
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
      Serial.printf("Connecting to %s ", ssid);
       delay(1000);
      not_Conn=not_Conn+1;
      Serial.println(not_Conn);
      if (not_Conn>10){  // after 10 failed connect attempts a SW reboot is performed
        Serial.println("I am restarting");
        ESP.restart();
      }
      delay(500);
  }
  
  Serial.print("Local ip ");
  Serial.println(WiFi.localIP());
  initialBoot=1; //indicates that the station just booted
  
// Init and get the time 
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  //printLocalTime();

// ESP 32 Web Dashboard
  server.on("/", handle_OnConnect); // instance server for ESP32 dashboard web page
  server.onNotFound(handle_NotFound);
  server.begin();
  Serial.println("Server WebServer Started on port 8080");  

// DHT22 Init 
  dht.begin(); //DHT22
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);

// GMP 280 Init 
  if (!bmp.begin()) // verify Barometer is OK
    {
    int A1=0;
    delay(1000);
    Serial.print("GMP280 Failure!"); // initialization fail
  }
  
  else {
    int A2=0;
    //Serial.print("inizializzazione ok"); // initialization ok
  }
  read_DHT22_U(); // Read DHT22 value
  barometer();  // Read Barometric value from BMP 280 

//LCD 16X2 Init
  lcd.begin(); // initialize the LCD
  lcd.backlight(); // Turn on the blacklight     
  PrintToLCD();
  delay(3000);
  lcd.noBacklight(); // Turn off the blacklight
  printLocalData(); // print collected data on Serial Console

//Attach Button interrupt routine for LCD
  pinMode(pinBtn, INPUT_PULLDOWN);
  attachInterrupt(pinBtn, isrLCD, RISING);
}

// ***********************************************************************************************************************
void loop(void) {
  struct tm timeinfo;  // struct to store ntp time
    
  if  (statusBtn==1){ //check statusBtn modified by interrupt if button has been pressed to display data on LCD 
    read_DHT22_U();
    barometer();
    lcd.clear();
    lcd.backlight(); // Turn on the blacklight
    PrintToLCD();
    delay(5000);
    lcd.noBacklight();
  }  // end if   

  if(WiFi.status()== WL_CONNECTED){     
    getLocalTime(&timeinfo);
    currentMillis = millis();
    
    // Test for SW reset every 12 hours
    if ((rebootTimer-currentMillis) <0 ) {
      Serial.print("elapse ");
      Serial.println(currentMillis - rebootTimer);
      ESP.restart();
     }   
     
    server.handleClient();  //  // Listen for HTTP requests from clientst
    
    getLocalTime(&timeinfo);
    // read data from sensors at defined time
    if ( (((timeinfo.tm_hour == 0) && (timeinfo.tm_min==0) && (timeinfo.tm_sec==0)) ||
       ((timeinfo.tm_hour == 3) && (timeinfo.tm_min==0) && (timeinfo.tm_sec==0)) ||
       ((timeinfo.tm_hour == 6) && (timeinfo.tm_min==0) && (timeinfo.tm_sec==0)) ||
       ((timeinfo.tm_hour == 9) && (timeinfo.tm_min==0) && (timeinfo.tm_sec==0)) ||
       ((timeinfo.tm_hour == 12) && (timeinfo.tm_min==0) && (timeinfo.tm_sec==0)) ||
       ((timeinfo.tm_hour == 15) && (timeinfo.tm_min==0) && (timeinfo.tm_sec==0)) ||
       ((timeinfo.tm_hour == 16) && (timeinfo.tm_min==0) && (timeinfo.tm_sec==0)) ||
       ((timeinfo.tm_hour == 21) && (timeinfo.tm_min==0) && (timeinfo.tm_sec==0)) || 
       (initialBoot==1)) )
    {
     
     // to be used to read from sensors at defined interval insted of prefixed time
    //if ((currentMillis - DebounceTimer) > DEBOUNCE_TIME || initialBoot==1) { 
      //Serial.println(currentMillis - DebounceTimer);     
     
      read_DHT22_U();
      barometer();
      //noInterrupts();  //exclude interrupts during the data writing on DB
      
      // Section to write to Raspi MariaDB (WebSqlWrite)
      HTTPClient http;
      http.begin (serverName);
      // Specify content-type header
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");
      
      // Prepare your HTTP POST request data
      String httpRequestData = "api_key=" + apiKeyValue + "&sensor=" + sensorName
                            + "&location=" + sensorLocation + "&t_dht=" + String(t_dht)
                            + "&h_dht=" + String(h_dht) + "&p_bmp=" + String(p_bmp) + "";
      Serial.print("httpRequestData: ");
      Serial.println(httpRequestData);
      
      int httpResponseCode = http.POST(httpRequestData);
      if (httpResponseCode>0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
      }
      else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      Serial.print("SQLWrite");
      //interrupts(); //enable interrupts
      
      //DebounceTimer=currentMillis; // to be used in case of read from sensors at defined interval
      sensorLocation="Home";
      initialBoot=0;
      delay (1000); // needed to not re-enter in the time check for multiple times
    } // end of time if clause

  } // end if wifi status
  else {
    Serial.println("WiFi Disconnected");
  }

}

// ============================================================= Start Routines' Section =======================================================================

// Routine to read from DHT22
void read_DHT22_U(void){  // read Temperature and Humidity from DTH22 Sensor

  delay(100);
  sensors_event_t event;  
  
  dht.temperature().getEvent(&event);
    if (isnan(event.temperature)) {
    Serial.println("Error reading temperature!");
  }
  t_dht=event.temperature;
   // Get humidity event and print its value.
  dht.humidity().getEvent(&event);
  if (isnan(event.relative_humidity)) {
    Serial.println("Error reading humidity!");
  }
  h_dht=event.relative_humidity;
} 

// Routine to read from BMP 280 Sensor
void barometer (void){ 
  
 //pressure in mBar; elevation in meter
 char result = bmp.startMeasurment();
 int error;
  
  pCorr=(barom_H*9);
  bmp.setOversampling(4);
  
  if (result != 0) {
      delay(result);
      result = bmp.getTemperatureAndPressure(t_bmp, p_bmp);
      if (result != 0) {
        a_bmp = bmp.altitude(p_bmp, P0);
        a_bmp=132;  //altezza in via monte Baldo
        
        if ( a_bmp > A_MAX) {
          A_MAX = a_bmp;
      }
      if ( a_bmp < A_MIN || A_MIN == 0) {
        A_MIN = a_bmp;
      }
    }
    else {
      error=1;
      //Serial.println(error);
    }
  }
  else {
    error=2;
    //Serial.println(error);
  }
}

// Routine to write data to LCD
void PrintToLCD(void){
  lcd.print("T=");
  lcd.print(t_dht,1);
  lcd.print(" H=");
  lcd.print(h_dht,1);
  
  lcd.setCursor(0,1); //move cursor to second row
  lcd.print("P=");
  lcd.print(p_bmp,1);
  lcd.print(" E=");
  lcd.print(a_bmp,1);
  statusBtn=0;
}

// Print Data on Serial Monitor
void printLocalData(void){
  int digits;
  
//  DateTime now = rtc.now();
//  Serial.println("**************************************");
//  Serial.print(now.year(), DEC);
//  Serial.print('/');
//  Serial.print(now.month(), DEC);
//  Serial.print('/');
//  Serial.print(now.day(), DEC);
//  
//  Serial.print(" (");
//  Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
//  Serial.print(") ");
//  digits=now.hour();
//  if(digits < 10)
//    Serial.print('0');
//  Serial.print(now.hour(), DEC);
//  Serial.print(':');
//  digits=now.minute();
//  if(digits < 10)
//    Serial.print('0');
//  Serial.print(now.minute(), DEC);
//  Serial.print(':');
//  digits=now.second();
//  if(digits < 10)
//    Serial.print('0');
//  Serial.println(now.second(), DEC);
  //Serial.println("-------------------------------------");
   
//  long Temp_C=Read_Temp_DS3231(); 
//  Serial.print( "T_DS3231= ");
//  Serial.print( Temp_C / 100 );
//  Serial.print( '.' );
//  Serial.println( abs(Temp_C % 100) );
//  Serial.println("-------------------------------------");
  
// Data from Barometer
  Serial.print("T_Bar = \t"); Serial.print(t_bmp, 2); Serial.println(" gr. C\t");
  Serial.print("P_Bar = \t"); Serial.print(p_bmp, 2); Serial.println(" mBar\t");
  Serial.print("A_Bar = \t"); Serial.print(a_bmp, 2); Serial.println(" m");
//  Serial.print("Alt= ");
//  Serial.println (A);
//  Serial.print("T_Barom= ");
//  Serial.println (T);
//  Serial.print("P= ");
//  Serial.println (p_bmp);
  Serial.println("**************************************");
 
 //Data from DHTH 22
  Serial.print("H_DTH22: ");
  Serial.print(h_dht);
  Serial.println(" %\t");
  Serial.print("T_DHT22: ");
  Serial.print(t_dht);
  Serial.print(" *C ");
  /*Serial.print(f_dht);
  Serial.println(" *F\t");
  Serial.print("Heat index: ");
  Serial.print(hic_dht);
  Serial.print(" *C ");
  Serial.print(hif_dht);
  Serial.println(" *F");  */

} 

//Routine on to send data to ESP32 HTNL Page
void handle_OnConnect() {
  read_DHT22_U();
  barometer();
  server.send(200, "text/html", SendHTML(t_dht,h_dht,p_bmp,a_bmp)); 
}
void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
}

//Routine on topublish data to ESP32  HTNL Page
//https://how2electronics.com/esp32-bme280-mini-weather-station/
String SendHTML(float temperature,float humidity,float pressure,float altitude){
  String ptr = "<!DOCTYPE html>";
  ptr +="<html>";
  ptr +="<head>";
  ptr +="<meta http-equiv=\"refresh\" content=\"5\" >\n";
  ptr +="<title>SZ ESP32 Weather Station</title>";
  ptr +="<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  ptr +="<link href='https://fonts.googleapis.com/css?family=Open+Sans:300,400,600' rel='stylesheet'>";
  ptr +="<style>";
  ptr +="html { font-family: 'Open Sans', sans-serif; display: block; margin: 0px auto; text-align: center;color: #444444;}";
  ptr +="body{margin: 0px;} ";
  ptr +="h1 {margin: 50px auto 30px;} ";
  ptr +=".side-by-side{display: table-cell;vertical-align: middle;position: relative;}";
  ptr +=".text{font-weight: 600;font-size: 19px;width: 200px;}";
  ptr +=".reading{font-weight: 300;font-size: 50px;padding-right: 25px;}";
  ptr +=".temperature .reading{color: #F29C1F;}";
  ptr +=".humidity .reading{color: #3B97D3;}";
  ptr +=".pressure .reading{color: #26B99A;}";
  ptr +=".altitude .reading{color: #955BA5;}";
  ptr +=".superscript{font-size: 17px;font-weight: 600;position: absolute;top: 10px;}";
  ptr +=".data{padding: 10px;}";
  ptr +=".container{display: table;margin: 0 auto;}";
  ptr +=".icon{width:65px}";
  ptr +="</style>";
  ptr +="</head>";
  ptr +="<body>";
  ptr +="<h1>SZ ESP32 Weather Station</h1>";
  ptr +="<h3>======= szweb.eu=======</h3>";
  ptr +="<div class='container'>";
  ptr +="<div class='data temperature'>";
  ptr +="<div class='side-by-side icon'>";
  ptr +="<svg enable-background='new 0 0 19.438 54.003'height=54.003px id=Layer_1 version=1.1 viewBox='0 0 19.438 54.003'width=19.438px x=0px xml:space=preserve xmlns=http://www.w3.org/2000/svg xmlns:xlink=http://www.w3.org/1999/xlink y=0px><g><path d='M11.976,8.82v-2h4.084V6.063C16.06,2.715,13.345,0,9.996,0H9.313C5.965,0,3.252,2.715,3.252,6.063v30.982";
  ptr +="C1.261,38.825,0,41.403,0,44.286c0,5.367,4.351,9.718,9.719,9.718c5.368,0,9.719-4.351,9.719-9.718";
  ptr +="c0-2.943-1.312-5.574-3.378-7.355V18.436h-3.914v-2h3.914v-2.808h-4.084v-2h4.084V8.82H11.976z M15.302,44.833";
  ptr +="c0,3.083-2.5,5.583-5.583,5.583s-5.583-2.5-5.583-5.583c0-2.279,1.368-4.236,3.326-5.104V24.257C7.462,23.01,8.472,22,9.719,22";
  ptr +="s2.257,1.01,2.257,2.257V39.73C13.934,40.597,15.302,42.554,15.302,44.833z'fill=#F29C21 /></g></svg>";
  ptr +="</div>";
  ptr +="<div class='side-by-side text'>Temperature</div>";
  ptr +="<div class='side-by-side reading'>";
  ptr +=(float)temperature;
  ptr +="<span class='superscript'>&deg;C</span></div>";
  ptr +="</div>";
  ptr +="<div class='data humidity'>";
  ptr +="<div class='side-by-side icon'>";
  ptr +="<svg enable-background='new 0 0 29.235 40.64'height=40.64px id=Layer_1 version=1.1 viewBox='0 0 29.235 40.64'width=29.235px x=0px xml:space=preserve xmlns=http://www.w3.org/2000/svg xmlns:xlink=http://www.w3.org/1999/xlink y=0px><path d='M14.618,0C14.618,0,0,17.95,0,26.022C0,34.096,6.544,40.64,14.618,40.64s14.617-6.544,14.617-14.617";
  ptr +="C29.235,17.95,14.618,0,14.618,0z M13.667,37.135c-5.604,0-10.162-4.56-10.162-10.162c0-0.787,0.638-1.426,1.426-1.426";
  ptr +="c0.787,0,1.425,0.639,1.425,1.426c0,4.031,3.28,7.312,7.311,7.312c0.787,0,1.425,0.638,1.425,1.425";
  ptr +="C15.093,36.497,14.455,37.135,13.667,37.135z'fill=#3C97D3 /></svg>";
  ptr +="</div>";
  ptr +="<div class='side-by-side text'>Humidity</div>";
  ptr +="<div class='side-by-side reading'>";
  ptr +=(float)humidity;
  ptr +="<span class='superscript'>%</span></div>";
  ptr +="</div>";
  ptr +="<div class='data pressure'>";
  ptr +="<div class='side-by-side icon'>";
  ptr +="<svg enable-background='new 0 0 40.542 40.541'height=40.541px id=Layer_1 version=1.1 viewBox='0 0 40.542 40.541'width=40.542px x=0px xml:space=preserve xmlns=http://www.w3.org/2000/svg xmlns:xlink=http://www.w3.org/1999/xlink y=0px><g><path d='M34.313,20.271c0-0.552,0.447-1,1-1h5.178c-0.236-4.841-2.163-9.228-5.214-12.593l-3.425,3.424";
  ptr +="c-0.195,0.195-0.451,0.293-0.707,0.293s-0.512-0.098-0.707-0.293c-0.391-0.391-0.391-1.023,0-1.414l3.425-3.424";
  ptr +="c-3.375-3.059-7.776-4.987-12.634-5.215c0.015,0.067,0.041,0.13,0.041,0.202v4.687c0,0.552-0.447,1-1,1s-1-0.448-1-1V0.25";
  ptr +="c0-0.071,0.026-0.134,0.041-0.202C14.39,0.279,9.936,2.256,6.544,5.385l3.576,3.577c0.391,0.391,0.391,1.024,0,1.414";
  ptr +="c-0.195,0.195-0.451,0.293-0.707,0.293s-0.512-0.098-0.707-0.293L5.142,6.812c-2.98,3.348-4.858,7.682-5.092,12.459h4.804";
  ptr +="c0.552,0,1,0.448,1,1s-0.448,1-1,1H0.05c0.525,10.728,9.362,19.271,20.22,19.271c10.857,0,19.696-8.543,20.22-19.271h-5.178";
  ptr +="C34.76,21.271,34.313,20.823,34.313,20.271z M23.084,22.037c-0.559,1.561-2.274,2.372-3.833,1.814";
  ptr +="c-1.561-0.557-2.373-2.272-1.815-3.833c0.372-1.041,1.263-1.737,2.277-1.928L25.2,7.202L22.497,19.05";
  ptr +="C23.196,19.843,23.464,20.973,23.084,22.037z'fill=#26B999 /></g></svg>";
  ptr +="</div>";
  ptr +="<div class='side-by-side text'>Pressure</div>";
  ptr +="<div class='side-by-side reading'>";
  ptr +=(int)pressure;
  ptr +="<span class='superscript'>hPa</span></div>";
  ptr +="</div>";
  ptr +="<div class='data altitude'>";
  ptr +="<div class='side-by-side icon'>";
  ptr +="<svg enable-background='new 0 0 58.422 40.639'height=40.639px id=Layer_1 version=1.1 viewBox='0 0 58.422 40.639'width=58.422px x=0px xml:space=preserve xmlns=http://www.w3.org/2000/svg xmlns:xlink=http://www.w3.org/1999/xlink y=0px><g><path d='M58.203,37.754l0.007-0.004L42.09,9.935l-0.001,0.001c-0.356-0.543-0.969-0.902-1.667-0.902";
  ptr +="c-0.655,0-1.231,0.32-1.595,0.808l-0.011-0.007l-0.039,0.067c-0.021,0.03-0.035,0.063-0.054,0.094L22.78,37.692l0.008,0.004";
  ptr +="c-0.149,0.28-0.242,0.594-0.242,0.934c0,1.102,0.894,1.995,1.994,1.995v0.015h31.888c1.101,0,1.994-0.893,1.994-1.994";
  ptr +="C58.422,38.323,58.339,38.024,58.203,37.754z'fill=#955BA5 /><path d='M19.704,38.674l-0.013-0.004l13.544-23.522L25.13,1.156l-0.002,0.001C24.671,0.459,23.885,0,22.985,0";
  ptr +="c-0.84,0-1.582,0.41-2.051,1.038l-0.016-0.01L20.87,1.114c-0.025,0.039-0.046,0.082-0.068,0.124L0.299,36.851l0.013,0.004";
  ptr +="C0.117,37.215,0,37.62,0,38.059c0,1.412,1.147,2.565,2.565,2.565v0.015h16.989c-0.091-0.256-0.149-0.526-0.149-0.813";
  ptr +="C19.405,39.407,19.518,39.019,19.704,38.674z'fill=#955BA5 /></g></svg>";
  ptr +="</div>";
  ptr +="<div class='side-by-side text'>Altitude</div>";
  ptr +="<div class='side-by-side reading'>";
  ptr +=(int)altitude;
  ptr +="<span class='superscript'>m</span></div>";
  ptr +="</div>";
  ptr +="</div>";
  ptr +="</body>";
  ptr +="</html>";
  return ptr;
}

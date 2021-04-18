// ====================================================== CONTRIBUTIONS =========================================================
// Below some links used to get suggestion and codes. Many thanks to the Owners.
// https://randomnerdtutorials.com/esp32-esp8266-raspberry-pi-lamp-server/
// https://lastminuteengineers.com/esp32-dht11-dht22-web-server-tutorial/
// https://how2electronics.com/esp32-bme280-mini-weather-station/
// https://randomnerdtutorials.com/esp32-date-time-ntp-client-server-arduino/
// https://diyprojects.io/esp8266-web-server-tutorial-create-html-interface-connected-object/

//==================================================================================================================================
// 18/04/2021 SZ Meteo Station V3.0
// Unified Outdoor and Indoor Code using 2 different Table (Outdoor_Data and Indoor_Data) in MariaDB Instance "esp_data" 
//
// 18/04/2021 SZ Meteo Station V3.0
// Unified Outdoor and Indoor Code using 2 different Table (Outdoor_Data and Indoor_Data) in MariaDB Instance "esp_data" 
// 
// This Meteo station is based on ESP32 Dev Module. 
// It is able to read from below sensors Meteo Data and to write them in a MariaDB installed on a Raspberry PI3
// Raw Data read, on pre-defined hours, from the MariaDB Table and are published via and php page from the Raspberry
// A Dashboard with the Real-Time data is published, in HTML format, by the ESP32, connected via WiFi.
// Data can be read also by an Oled 128X64 Display pixel that is refreshed every min
// A push button allows to read and record the data in the DB in asyncronous way and refresh the Oled as well 
// An interrupt routine is invoked when button is pressed.
  
// Controller and Sensors:
// - ESP32 Dev Module
// - DHT22 sensor to read Temperature and Hunidity
// - BMP280 sensor to read Pressure and Station Elevation
//==================================================================================================================================

#include <WiFi.h>
#include "time.h"
#include <Wire.h>

// ***************************** INDOOR STATION ******************************************
const char* serverName = "http:/szweb.eu/post-esp-data_out.php";  //For outdoor station 
//PLEASE CORRECT THE WEB as PORTS LISTEN_PORT   8080  LISTEN_PORT_WiFi   8081


// ***************************** OUTDOOR STATION ******************************************
//const char* serverName = "http://szweb.eu/post-esp-data_in.php";
//PLEASE CORRECT THE WEB PORTS as LISTEN_PORT   8082  LISTEN_PORT_WiFi   8083

//---------------Statement from Dashboard published directly via HTML by ESP32------------------------------------
#define LISTEN_PORT   8080 
#include <WebServer.h>
WebServer server(LISTEN_PORT); // publish Page Dashboard rom ESP32 (Temperature and Humidity on port 808x)

// -------------------------- to get data in Jason format by REST Call --------------------------------------------
#define LISTEN_PORT_WiFi          8081
#include <aREST.h>    // to get For Rest Data
aREST rest = aREST(); // Create aREST instance
WiFiServer server2(LISTEN_PORT_WiFi); 

//====================== WiFi Connection Settings =================================================================
const char* ssid       = "LZ_24G";
const char* password   = "*andromedA01.";

// ==================== Time Settings ============================================================================= 
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

//======================= Web Servers Settings ====================================================================

// -------------------- to perform HTTP request to publish data in DB hosted in a Raspi --------------------------
#include <HTTPClient.h>
// If you change the apiKeyValue value, the PHP file /post-esp-data_out.php also needs to have the same key 
String apiKeyValue = "tPmAT5Ab3j7F9";
String sensorName = "DHT22"; // free text; in that case identifies the temp and hum sensor
String sensorLocation = "In"; // free text; out=outdoor to identify that data are coming from Outside muy house 

//#define DEBOUNCE_TIME 1800000 //3600000  //define the DB update cycle time
#define REFRESH_TIME 60000 //define Oled Refresh cycle time (1 min)

long DebounceTimer=0; // used in WebSqlWrite function to start the loop for writing in DB
long currentMillis=0; // used in WebSqlWrite function
//long rebootTimer=43200000;  //time to trigger SW reboot (12h in milliseconds)
long refreshTimer=0;  // controls time interval to refresh data showed in the Oled display ; 

int initialBoot=1; // used to control if the station did a start or reboot 1= station just started 

//======================= DTH22 Temperature and Humidity =============================================================
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#define DHTPIN 32     // what digital pin we're connected to
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
DHT_Unified dht(DHTPIN, DHTTYPE);

uint32_t delayMS;
float h_dht; //humidity 
float t_dht; // temperature in Celsius

// =====================================================================================================================

// GY-BME 280 Barometer I2C Address 0x76 
#include <BMP280.h>
#define P0 1013.25 // pressione standard, a livello del mare in mBar/hPa
BMP280 bmp;

double a_bmp=130.0;  // altezza slm in via Monte Baldo
double t_bmp; 
double p_bmp;
//double pCorr= 0.; //to normalize at the Level Sea P(level sea)= P(Real)+ 9*barom_H (9hpa every 100mt.) Pressure raise of 7mBar every 61 mt. in altitude
//double tCorr=0.; //correction of -8 Celsius to Atm. Pressure to compensate chip calibration
//double barom_H = 1.03; //Barometer Altitude divided by 100

//======================= DS180 Temperature and Humidity =============================================================
#include <OneWire.h>
#include <DallasTemperature.h>
// Data wire is plugged into port 19 
#define ONE_WIRE_BUS 19
#define TEMPERATURE_PRECISION 12 // Medium resolution
float t_ds18; 

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

int numberOfDevices; // Number of temperature devices found

DeviceAddress tempDeviceAddress; // We'll use this variable to store a found device address

// ========================= SSD1306 Oled display connected to I2C SDA=21 SCL=22 ===========================================
// https://randomnerdtutorials.com/esp32-ssd1306-oled-display-arduino-ide/

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
// (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);


// ====================================Interrupt routine for display  =====================================================
int pinBtn=4; // Command button for LCD - Connected to interrupt
int statusBtn; // Switch status 0=pressed 1=unpressed
void IRAM_ATTR isrLCD(void){  // this procedure is used to react to an interrupt after pressing Button for LCD display  
  statusBtn=digitalRead(pinBtn); // Switch status 0=pressed 1=unpressed 
  //Serial.print ("Interrupt");
  //Serial.print (statusBtn);
}  // End of IRAM_ATTR Funtion
// ***********************************************************************************************************************

int ledPin=25;
void setup(void)
{
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);
  
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
      if (not_Conn>5){
        Serial.println("I am restarting");
        ESP.restart();
      }
  }
  digitalWrite(ledPin, LOW);
  Serial.print("Local ip ");
  Serial.println(WiFi.localIP());
  //initialBoot=1; //indicates that the station just booted
  
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);


// --------------------- ESP 32 Web Dashboard -------------------------------------------------------------------------------
  server.on("/", handle_OnConnect); // instance server for ESP32 dashboard web page
  server.onNotFound(handle_NotFound);
  server.begin();
  Serial.println("Server WebServer Started on port 8080");  

// ------------------------------------- Start Server instance for rest jason ------------------------------------------------
  server2.begin(); // instance server for JSON data
  Serial.println("Server WiFiServer Started on port 808x ");
  // Init variables and expose them to REST API
  rest.variable("t_dht",&t_dht);
  rest.variable("h_dht",&h_dht);
  rest.variable("t_p_bmp",&p_bmp);
  rest.variable("t_ds18",&t_ds18);
  rest.set_id("1");
  rest.set_name("Meteo Module");

 //+++++++++++++++++++++++++++++++ DHT22 Init +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  dht.begin(); //DHT22
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);

// +++++++++++++++++++++++++++++ GMP 280 Init +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ 
  if (!bmp.begin()) // verify Barometer is OK
    {
    Serial.print("BMP280 Failure!"); // initialization fail
  }
  else {  
    bmp.setOversampling(4);
    Serial.print("BMP280 OK"); // initialization ok
  }
   //Attach interrupt routine
  pinMode(pinBtn, INPUT_PULLDOWN);
  attachInterrupt(pinBtn, isrLCD, RISING);
  
display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

} // ***********  End Setup ********************

// ****************************************************************************************************************************

void loop(void) {
  
  struct tm timeinfo;  // struct to store ntp time
  if  (statusBtn==1){ //check statusBtn modified by interrupt if button has been pressed to display data on LCD 
    read_DHT22_U();
    read_BMP280();
    //read_DS18B20();
    oledWrite();
    printLocalData();
    WebSqlWrite();
    statusBtn=0;
  }  // end if 

  if(WiFi.status()== WL_CONNECTED){  
    currentMillis = millis();
    server.handleClient(); 
  
    //if ((rebootTimer-currentMillis) <0 ) 
    if ( (( (timeinfo.tm_hour==0)&& (timeinfo.tm_min==30) && (timeinfo.tm_sec==0))) || (( (timeinfo.tm_hour==8)&& (timeinfo.tm_min==30) && (timeinfo.tm_sec==0))) )
    {
     Serial.println("REBOOT");
     //Serial.println(currentMillis - rebootTimer);
      ESP.restart();
     }
     
    if (((currentMillis-refreshTimer)> REFRESH_TIME ) || (initialBoot==1) ) 
        {
      read_DHT22_U();
      read_BMP280();
      //read_DS18B20();
      delay(100);
      oledWrite();
      printLocalData();
      refreshTimer=currentMillis;
      initialBoot=0;
    }
    
    getLocalTime(&timeinfo); 
    if ((((timeinfo.tm_min==00) && (timeinfo.tm_sec==00)))) //execute the actions every hour   //|| (initialBoot==1))) needed to record the event at the boot
    {    
      //initialBoot=0;
      read_DHT22_U();
      read_BMP280();
      //read_DS18B20();
      oledWrite();
      printLocalData();
      WebSqlWrite();      
      delay (1000); // needed to not re-enter in the time check for multiple times
   
      //DebounceTimer=currentMillis;
      //sensorLocation="Out";
     }

// ---------------------------------  Handle REST calls  -------------------------------------------------
    
    WiFiClient client = server2.available();

    if (!client) {
      return;
    }
    
    if(!client.available()){
      Serial.println("!client.available");
      delay(1);
    }
    
    rest.handle(client);
    Serial.println("Rest OK");

// ---------------------------------------------------------------------------------------------------------
 } // end if wifi status
  
  else {
    Serial.println("WiFi Disconnected");
  }
} // ************** End Loop ************

// ************************************************ Begin Procedure Section ********************************
char timeWrite[7];
char timeHour[3];
int  dayWrite;
int monthWrite;
void getTimeWrite (void){
 
  struct tm timeinfo;  // struct to store ntp time  
  if(!getLocalTime(&timeinfo)){
  Serial.println("Failed to obtain time");
  return;
  }        
  strftime(timeHour,3, "%H", &timeinfo);
  strftime(timeWrite,3, "%H", &timeinfo);
  char timeMin[3];
  strftime(timeMin,3, "%M", &timeinfo);
  strcpy( timeWrite,timeHour);
  strcat( timeWrite,":");
  strcat( timeWrite,timeMin);
  
  dayWrite=timeinfo.tm_mday;
  monthWrite=timeinfo.tm_mon+1;
}

// *************************************************************************************************************
void WebSqlWrite(void)
{
  
  HTTPClient http;
  http.begin (serverName);
  // Specify content-type header
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  getTimeWrite();
//  Serial.println(dayWrite);
//  Serial.println(monthWrite);
//  Serial.println(timeHour);
  // Prepare your HTTP POST request data
String httpRequestData = "api_key=" + apiKeyValue + "&sensor=" + sensorName
                      + "&location=" + sensorLocation + "&t_dht=" + String(t_dht)
                      + "&h_dht=" + String(h_dht) + "&p_bmp=" + String(p_bmp) + "&t_bmp=" + String(t_bmp) + "&t_ds18=" + String(t_ds18) 
                      + "&Day=" + String(dayWrite) + "&Month=" + String(monthWrite) + "&Time=" + String(timeHour) + "";

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
  Serial.println("SQLWrite");
  // Free resources
  //http.end();
}

// *************************************************************************************************************
void oledWrite (void){
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  
  display.setCursor(0, 10);
  display.print("T= ");
  display.println(t_dht);
  display.setCursor(0, 30);
  display.print("H= ");
  display.println(h_dht);  
  display.setCursor(0, 50);
  display.print("P= ");
  display.println(p_bmp);  
  display.display(); 
}

// *************************************************************************************************************
void read_DHT22_U(void){  // read Temperature and Humidity from DTH22 Sensor

  delay(delayMS);
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
} // ************** read_DHT22_U ************

// *************************************************************************************************************
void read_DS18B20 (void){
  sensors.requestTemperatures(); // Send the command to get temperatures
  sensors.getAddress(tempDeviceAddress, 0);
  t_ds18=sensors.getTempC(tempDeviceAddress);
}
//*************************************************************************************************************

// **** Routine to read from BMP 280 Sensor ****
void read_BMP280 (void){ 
 char result = bmp.startMeasurment();    //pressure in mBar; elevation in meter
 int error;
  
  if (result != 0) {
      delay(result);
      result = bmp.getTemperatureAndPressure(t_bmp, p_bmp);
      if (result != 0) {
        a_bmp = bmp.altitude(p_bmp, P0);
        a_bmp=132;  //altezza in via monte Baldo  
      }
    else {
      error=1;
      Serial.println(error);
    }
  }
  else {
    error=2;
    Serial.println(error);
  }
}
//*************************************************************************************************************

void printLocalData(void){
  int digits;
  Serial.println();
  Serial.print("H_DTH22: ");
  Serial.print(h_dht);
  Serial.println(" %\t");
  Serial.print("T_DHT22: ");
  Serial.print(t_dht);
  Serial.println(" *C ");
  Serial.print("P_BMP: ");
  Serial.print(p_bmp);
  Serial.println(" mbar ");
//  Serial.print("T_DS18B20: ");
//  Serial.print(t_ds18);
//  Serial.println(" %\t");
}
// ********************************************** PrintLocalData ***********************************************


//**************************************************************************************************************
void handle_OnConnect() {
  read_DHT22_U();
  read_BMP280;
  //read_DS18B20();
  server.send(200, "text/html", SendHTML(t_dht,h_dht,p_bmp, a_bmp)); 
}

//**************************************************************************************************************
void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
}
//**************************************************************************************************************
//https://how2electronics.com/esp32-bme280-mini-weather-station/

String SendHTML(float t_DHT22,float h_DHT22, float p_bmp,float a_bmp){
  String ptr = "<!DOCTYPE html>";
  ptr +="<html>";
  ptr +="<head>";
  ptr +="<meta http-equiv=\"refresh\" content=\"5\" >\n";
  ptr +="<title>SZ ESP32 Weather Station</title>";
  ptr +="<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  ptr +="<link href='https://fonts.googleapis.com/css?family=Open+Sans:300,400,600' rel='stylesheet'>";
  ptr +="<style>";
  ptr +="html { font-family: 'Open Sans', sans-serif; display: block; margin: 0px auto; text-align: center;color: #44  4444;}";
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
  ptr +=(float)t_DHT22;
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
  ptr +=(float)h_DHT22;
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
  ptr +=(int)p_bmp;
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
  ptr +=(int)a_bmp;
  ptr +="<span class='superscript'>m</span></div>";
  ptr +="</div>";
  ptr +="</div>";
  ptr +="</body>";
  ptr +="</html>";
  return ptr;
}

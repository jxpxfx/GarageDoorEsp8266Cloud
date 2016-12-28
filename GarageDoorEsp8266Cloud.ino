/*
  To upload through terminal you can use: curl -F "image=@firmware.bin" garagedoor-webupdate.local/update
  Thanks to https://github.com/tzapu/WiFiManager
  Thanks to https://github.com/JoaoLopesF/RemoteDebug
*/
#include "configuration.h"
#include <ESP8266WiFi.h>
//#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>

// Remote debug over telnet - not recommended for production, only for development
#include "RemoteDebug.h"        //https://github.com/JoaoLopesF/RemoteDebug

// Wifi Manager
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic

//web update OTA
const char* host = "garagedoor-webupdate";
const char* ssid = "";
const char* password = "";

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

//Remote Debug
RemoteDebug Debug;
int count = 0;

//Garage Door 
const int RED = 15;  //RED
const int GREEN = 12; //GREEN
const int BLUE = 13; //BLUE

String writeAPIKey = channelId;
bool prevIsOpen = -1;
bool isOpen = 0;
bool is1stTime = true;
long prevMillisMaker = 0;
long prevMillisUpdateDweet = 0;
long updateMakerInterval = 30*60000;
long updateDweetInterval = 5000;

const int pinSwitch = 5; //Pin Reed GPIO4
int StatoSwitch = 0;

void setup(void){

  setupNetwork();
  
  pinMode(pinSwitch, INPUT);
  //YOUR SETUP CODE GOES HERE
  
  // initialize digital pin 13 as an output.
  pinMode(RED, OUTPUT);
  pinMode(GREEN, OUTPUT);
  pinMode(BLUE, OUTPUT);
  digitalWrite(RED, LOW);
  digitalWrite(GREEN, LOW);
  digitalWrite(BLUE, LOW);
  
  //YOUR SETUP CODE ENDS HERE
}

void loop(void){
  //Web Server
  httpServer.handleClient();

  //YOUR LOOP CODE GOES HERE
  
  garageDoorBusinessLogic();
  
  //YOUR LOOP CODE ENDS HERE
  
  // Remote debug over telnet
  Debug.handle();
}

void setupNetwork() {
  WiFi.begin(ssid, password);
  
  //Wifi Manager
  WiFiManager wifiManager;
  //first parameter is name of access point, second is the password
  wifiManager.autoConnect();

  MDNS.begin(host);

  httpUpdater.setup(&httpServer);
  httpServer.begin();

  MDNS.addService("http", "tcp", 80);
  Serial.printf("HTTPUpdateServer ready! Open http://%s.local/update in your browser\n", host);

  // Initialize the telnet server of RemoteDebug  
  Debug.begin("Telnet_HostName"); // Initiaze the telnet server
  // OR
  Debug.begin(host); // Initiaze the telnet server - HOST_NAME is the used in MDNS.begin
  
  Debug.setResetCmdEnabled(true); // Enable the reset command
  //Debug.showTime(true); // To show time
  // Debug.showProfiler(true); // To show profiler - time between messages of Debug  
}

void printMACAddress() {
  if (Debug.ative(Debug.DEBUG)) {    
    byte mac[6];
    WiFi.macAddress(mac);
    Debug.print("MAC: ");
    Debug.print(mac[0],HEX);
    Debug.print(":");
    Debug.print(mac[1],HEX);
    Debug.print(":");
    Debug.print(mac[2],HEX);
    Debug.print(":");
    Debug.print(mac[3],HEX);
    Debug.print(":");
    Debug.print(mac[4],HEX);
    Debug.print(":");
    Debug.println(mac[5],HEX);
  }  
}

void garageDoorBusinessLogic() {
  delay(100);
  ledsOff();
  //if ((unsigned long)(millis() - previousMillis) >= interval)
  long currMillis = millis();
  long elapsedMillisMaker = (unsigned long)(currMillis - prevMillisMaker); //currMillis - prevMillisMaker;
  long elapsedMillisDweet = (unsigned long)(currMillis - prevMillisUpdateDweet);//currMillis - prevMillisUpdateDweet;
  
  //Serial.println("elapsed:");
  //Serial.println(elapsedMillisDweet);
  //Serial.println("loop");
  
  //read switch
  StatoSwitch = digitalRead(pinSwitch);
  if (StatoSwitch == HIGH)
  {
    isOpen = false;
    digitalWrite(GREEN, HIGH);
  }
  else
  {
    isOpen = true;
    digitalWrite(RED, HIGH);
  }
  
  //tweet update logic every 5s
  if (elapsedMillisDweet > updateDweetInterval)
  {
    //Serial.println("updateDweet 5s");
    //update dweet
    updateDweet();

    prevMillisUpdateDweet = currMillis;
  }  

  //maker channel update logic whenever status change
  if (isOpen != prevIsOpen)
  {
    Serial.println("status change");
    if (Debug.ative(Debug.DEBUG)) {
      Debug.println("status change");
    }
    //update maker channel (status change notification) 
    if (is1stTime)
    {
      is1stTime = false;
    }
    else
    {
      updateMakerChannel(false);     
    }

    //update dweet
    updateDweet();

    prevIsOpen = isOpen;    
  }

  //request DHT sensor data when cycle starts and after 30min
  //update thingspeak when a new reading is made
  if ((elapsedMillisMaker > updateMakerInterval)  || (prevMillisMaker == 0))
  {
    //Serial.println("readerSensor");
    //read sensor data
    //disable-dht
    //readSensorData();
    prevMillisMaker = currMillis;

    //update thingspeak
    updateThingspeak();
  }  

  //reset after 1 hour - because this damn ESP crashes randomly 
  if (millis() > 3600000)
  {
    Serial.println("going to sleep");
    if (Debug.ative(Debug.DEBUG)) {
      Debug.println("going to sleep");
    }

    //force arduino restart!!!
    restart();
  }
}
void restart() {
  ESP.deepSleep(1000000, WAKE_RF_DEFAULT); // Sleep for 1 sec
}

void updateThingspeak(){
  if (Debug.ative(Debug.DEBUG)) {
    Debug.println("updateThingspeak method");
  }
  //Serial.print("updateThingspeak method");

  //Serial.print("connecting to ");
  //Serial.println(host);
  
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  const int httpPort = 80;
  client.connect(tshost, httpPort);
  
  // We now create a URI for the request
  String tsData = "field1=";
  tsData += isOpen;
  tsData += "&field2=";
  tsData += 0;
  tsData += "&field3=";
  tsData += 0;
  tsData += "&field4=";
  tsData += millis();
  
  Serial.print("connected TS. isOpen=");
  Serial.println(isOpen);
  client.print("POST /update HTTP/1.1\n");
  client.print("Host: api.thingspeak.com\n");
  client.print("Connection: close\n");
  client.print("X-THINGSPEAKAPIKEY: "+writeAPIKey+"\n");
  client.print("Content-Type: application/x-www-form-urlencoded\n");
  client.print("Content-Length: ");
  client.print(tsData.length());
  client.print("\n\n");

  client.print(tsData);
  delay(50);
  
  Serial.println("thingspeak updated");
  if (Debug.ative(Debug.DEBUG)) {
    Debug.println("thingspeak updated");
  }

  //blinkLed(GREEN);
  //Serial.println("closing connection");
  //delay(2000);
}

void updateMakerChannel(bool isFirstTime){
  //Serial.print("connecting to ");
  //Serial.println(hostMakerChannel);
  if (Debug.ative(Debug.DEBUG)) {
    Debug.println("updateMakerChannel method");
  }
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  const int httpPort = 80;
  client.connect(hostMakerChannel, httpPort);
  
  // We now create a URI for the request
  String url;
  if (isOpen)
  {
    url = "/trigger/garagedoor_open/with/key/";
  }
  else
  {
    url = "/trigger/garagedoor_closed/with/key/";
  }
  //String url = "/trigger/teste/with/key/";
  url+= keyMakerChannel;
  url += "?value1=";
  if (isFirstTime)
  {
    url += "-999";
  }
  else
  {
    url += isOpen;
  }  
  url += "&value2=";
  url += 0;
  url += "&value3=";
  url += millis();
  
  Serial.print("Requesting URL: ");
  Serial.println(url);
  
  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + hostMakerChannel + "\r\n" + 
               "Connection: close\r\n\r\n");
  delay(50);
  
  // Read all the lines of the reply from server and print them to Serial
  //while(client.available()){
  //  String line = client.readStringUntil('\r');
  //  Serial.print(line);
  //}
  
  Serial.println("maker channel updated");
  if (Debug.ative(Debug.DEBUG)) {
    Debug.println("maker channel updated");
  }
  //blinkLed(GREEN);
  //Serial.println("closing connection");
  //delay(2000);
  
}

void updateDweet(){
  //Serial.print("connecting to ");
  //Serial.println(dweetHost);
  if (Debug.ative(Debug.DEBUG)) {
    Debug.println("updateDweet method");
  }
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  const int httpPort = 80;
  client.connect(dweetHost, httpPort);

  int mill = millis();
  // We now create a URI for the request
  String url = "/dweet/for/";
  url+= dweetThing;
  url += "?isOpen=";
  url += isOpen;
  url += "&temperature=";
  url += 0;
  url += "&humidity=";
  url += 0;
  url += "&millis=";
  url += mill;
  
  //Serial.print("Requesting URL: ");
  //Serial.println(url);
  
  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + dweetHost + "\r\n" + 
               "Connection: close\r\n\r\n");
  delay(50);
  
  // Read all the lines of the reply from server and print them to Serial
  //while(client.available()){
  //  String line = client.readStringUntil('\r');
  //  Serial.print(line);
  //}
  
  Serial.println("dweet updated");
  if (Debug.ative(Debug.DEBUG)) {
    Debug.printf("dweet updated %d\n", mill);
  }
  //blinkLed(GREEN);
  //Serial.println("closing connection");
  //delay(2000);
  
}

void blinkLed(int color) {
  //Serial.println("will blink now...");
  for (int i = 0; i < 2; i++) {
    digitalWrite(color, HIGH);
    delay(50);
    digitalWrite(color, LOW);
    delay(10);
  }
}

void turnOff(int pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, 1);
}

void ledsOff() 
{
  digitalWrite(RED, LOW);
  digitalWrite(GREEN, LOW);
  digitalWrite(BLUE, LOW);
}


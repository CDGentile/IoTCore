



//IoT Core
//C. Gentile
//Initiated 1 Jan 2019
//Updated   2 Feb 2019
//
//
#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <TimeLib.h>
#include <WiFiUdp.h>
#include <DHT.h>
#include <DHT_U.h>


//comment out to stop serial comms
#define DEBUG

#ifdef DEBUG
 #define DEBUG_PRINT(x)  Serial.print(x)
 #define DEBUG_PRINTLN(x) Serial.println(x)
 #define DEBUG_PRINTF(x,y) Serial.printf(x,y)
#else
 #define DEBUG_PRINT(x)
 #define DEBUG_PRINTLN(x)
 #define DEBUG_PRINTF(x,y)
#endif

#define DHTPin 5
#define LEDPin  4
#define RedLED   0
#define BlueLED  2
#define DHTTYPE DHT22

const bool ledEnable = false;

static const char ntpServerName[] = "us.pool.ntp.org";
const int timeZone = -8;

DHT dht(DHTPin, DHTTYPE);

ESP8266WebServer server(80);
File fsUploadFile;              // a File object to temporarily store the received file

WiFiUDP Udp;
unsigned int localPort = 8888;
time_t getNtpTime();

void setupWifi();
void setupOTA();
void setupUdp();

String getContentType(String filename);
bool handleFileRead(String path);
void handleFileUpload();
void handleRoot();
void uploadPage(void);
void setupServer(void);

void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);

void serialTicker();


void setup() {

  //begin serial debug terminal if required
  #ifdef DEBUG
    Serial.begin(115200);
  #endif

  delay(250);

  //Initialize WiFi - todo: what to do if connection fails?
  setupWifi();

  //Function calls required for OTA updates
  //DEBUG mode required for troubleshooting
  setupOTA();

  SPIFFS.begin();                           // Start the SPI Flash Files System

  setupServer();

  setupUdp();
}

time_t prevDisplay = 0; // when the digital clock was displayed

void loop()
{

  //mandatory handlers

  server.handleClient();
  ArduinoOTA.handle();

  #ifdef DEBUG
    serialTicker();
  #endif

  delay(500);

}

void serialTicker() {
  if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) { //update the display only if time has changed
      prevDisplay = now();
      digitalClockDisplay();
    }
  }

}


//WiFi Setup routine - modify to include LED status feedback
void setupWifi() {
  WiFiManager wifiManager;

IPAddress _ip = IPAddress(192, 168, 1, 205);
IPAddress _gw = IPAddress(192, 168, 1, 1);
IPAddress _sn = IPAddress(255, 255, 255, 0);

wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn);

  delay(100);

  wifiManager.autoConnect("IoTCore");
}

void setupOTA() {
  ArduinoOTA.onStart([]() {
    DEBUG_PRINTLN("Start");
  });
  ArduinoOTA.onEnd([]() {
    DEBUG_PRINTLN("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    DEBUG_PRINTF("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    DEBUG_PRINTF("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) DEBUG_PRINTLN("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) DEBUG_PRINTLN("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) DEBUG_PRINTLN("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) DEBUG_PRINTLN("Receive Failed");
    else if (error == OTA_END_ERROR) DEBUG_PRINTLN("End Failed");
  });
  ArduinoOTA.begin();
}

//time functions
void setupUdp() {
  DEBUG_PRINTLN("Starting UDP");
  Udp.begin(localPort);
  DEBUG_PRINT("Local port: ");
  DEBUG_PRINTLN(Udp.localPort());
  DEBUG_PRINTLN("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(300);
}

void digitalClockDisplay()
{
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(".");
  Serial.print(month());
  Serial.print(".");
  Serial.print(year());
  int temp = (int) dht.readTemperature(true);
  Serial.print("  Temp: ");
  Serial.print(temp);
  Serial.print("F");
  Serial.println();
}

void printDigits(int digits)
{
  // utility for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  DEBUG_PRINTLN("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  DEBUG_PRINT(ntpServerName);
  DEBUG_PRINT(": ");
  DEBUG_PRINTLN(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      DEBUG_PRINTLN("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  DEBUG_PRINTLN("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

/// WebServer functions
String getContentType(String filename) { // convert the file extension to the MIME type
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  return "text/plain";
}

bool handleFileRead(String path) { // send the right file to the client (if it exists)
  DEBUG_PRINTLN("handleFileRead: " + path);
  if (path.endsWith("/")) path += "index.html";          // If a folder is requested, send the index file
  String contentType = getContentType(path);             // Get the MIME type
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) { // If the file exists, either as a compressed archive, or normal
    if (SPIFFS.exists(pathWithGz))                         // If there's a compressed version available
      path += ".gz";                                         // Use the compressed verion
    File file = SPIFFS.open(path, "r");                    // Open the file
    size_t sent = server.streamFile(file, contentType);    // Send it to the client
    file.close();                                          // Close the file again
    DEBUG_PRINTLN(String("\tSent file: ") + path);
    return true;
  }
  DEBUG_PRINTLN(String("\tFile Not Found: ") + path);   // If the file doesn't exist, return false
  return false;
}

void handleFileUpload(){ // upload a new file to the SPIFFS
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    DEBUG_PRINT("handleFileUpload Name: "); DEBUG_PRINTLN(filename);
    fsUploadFile = SPIFFS.open(filename, "w");            // Open the file for writing in SPIFFS (create if it doesn't exist)
    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile) {                                    // If the file was successfully created
      fsUploadFile.close();                               // Close the file again
      DEBUG_PRINT("handleFileUpload Size: "); DEBUG_PRINTLN(upload.totalSize);
      server.sendHeader("Location","/success.html");      // Redirect the client to the success page
      server.send(303);
    } else {
      server.send(500, "text/plain", "500: couldn't create file");
    }
  }
}

void handleRoot() {
  server.send(200, "text/plain", "Hello world! index.html not found - please try the /defaultupload url");   // Send HTTP status 200 (Ok) and send some text to the browser/client
}

void uploadPage(void) {
  server.send(200, "text/html", "<form method=\"post\" enctype=\"multipart/form-data\"><input type=\"file\" name=\"name\"><input class=\"button\" type=\"submit\" value=\"Upload\"></form>");
}

void setupServer(void) {
  server.on("/", handleRoot);

  server.on("/defaultupload", HTTP_GET, []() {                 // if the client requests the upload page, send it (inline for the first use)
    uploadPage();
  });

  server.on("/defaultupload", HTTP_POST,                       // if the client posts to the upload page
    [](){ server.send(200); },                          // Send status 200 (OK) to tell the client we are ready to receive
    handleFileUpload                                   // Receive and save the file
  );

  server.on("/upload.html", HTTP_POST,                       // if the client posts to the upload page
    [](){ server.send(200); },                          // Send status 200 (OK) to tell the client we are ready to receive
    handleFileUpload                                   // Receive and save the file
  );

  //server.on("/index.html", HTTP_GET, [](){ handleFileRead("/index.html"); });

  server.onNotFound([]() {                              // If the client requests any URI
    if (!handleFileRead(server.uri()))                  // send it if it exists
      server.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
  });

  server.begin();
  DEBUG_PRINTLN("HTTP Server Started.");
}
/// end WebServer functions

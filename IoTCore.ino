



//IoT Core
//C. Gentile
//Initiated 1 Jan 2019
//Updated   1 Jan 2019
//
//
#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>


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

const bool ledEnable = false;

ESP8266WebServer server(80);
File fsUploadFile;              // a File object to temporarily store the received file


//WiFi Setup routine - modify to include LED status feedback
void setup_wifi() {
  WiFiManager wifiManager;

IPAddress _ip = IPAddress(192, 168, 1, 205);
IPAddress _gw = IPAddress(192, 168, 1, 1);
IPAddress _sn = IPAddress(255, 255, 255, 0);

wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn);

  delay(100);

  wifiManager.autoConnect("IoTCore");
}

void setup_OTA() {
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
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "index.html";          // If a folder is requested, send the index file
  String contentType = getContentType(path);             // Get the MIME type
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) { // If the file exists, either as a compressed archive, or normal
    if (SPIFFS.exists(pathWithGz))                         // If there's a compressed version available
      path += ".gz";                                         // Use the compressed verion
    File file = SPIFFS.open(path, "r");                    // Open the file
    size_t sent = server.streamFile(file, contentType);    // Send it to the client
    file.close();                                          // Close the file again
    Serial.println(String("\tSent file: ") + path);
    return true;
  }
  Serial.println(String("\tFile Not Found: ") + path);   // If the file doesn't exist, return false
  return false;
}

void handleFileUpload(){ // upload a new file to the SPIFFS
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    Serial.print("handleFileUpload Name: "); Serial.println(filename);
    fsUploadFile = SPIFFS.open(filename, "w");            // Open the file for writing in SPIFFS (create if it doesn't exist)
    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile) {                                    // If the file was successfully created
      fsUploadFile.close();                               // Close the file again
      Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
      server.sendHeader("Location","/success.html");      // Redirect the client to the success page
      server.send(303);
    } else {
      server.send(500, "text/plain", "500: couldn't create file");
    }
  }
}

void handleRoot() {
  server.send(200, "text/plain", "Hello world! - try the /defaultupload url");   // Send HTTP status 200 (Ok) and send some text to the browser/client
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


void setup() {

  //begin serial debug terminal if required
  #ifdef DEBUG
    Serial.begin(9600);
  #endif

  delay(250);

  //Initialize WiFi - todo: what to do if connection fails?
  setup_wifi();

  //Function calls required for OTA updates
  //DEBUG mode required for troubleshooting
  setup_OTA();

  SPIFFS.begin();                           // Start the SPI Flash Files System

  setupServer();
}


void loop()
{

  //mandatory handlers

  server.handleClient();
  ArduinoOTA.handle();

  delay(500);

}

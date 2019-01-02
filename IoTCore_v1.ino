



//Fireplace Controller
//C. Gentile
//Initiated 30 Oct 2017
//Updated   7 Jan 2018
//
//Based on ESP8266 Blinds Controller Board
//Uses FauxMO library to respond to Alexa commands
//
//Implementation notes:
//   -assumes 2x SSR to connect buttons
//   -to do: check to see if we can sense actual state
//
//Update v3.0, 7 Jan:
//  -refactoring code per best seperation practice

#include <fauxmoESP.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <DHT.h>
#include <DHT_U.h>
#include <Adafruit_NeoPixel.h>
#include <ESP8266WebServer.h>
#include <FS.h>


//#include <PubSubClient.h>     (delay MQTT for future implementation)

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

// Pin Definitions
#define FanPin  13
#define HeaterPin  12
#define LEDPin  4
#define RedLED   0
#define BlueLED  2
#define DHTPin 5
#define DHTTYPE DHT22
#define PXL 4
#define NUMPIXELS 8

const bool ledEnable = false;

const int maxTemp = 100;
const int maxTime = 14400000;
const int deBounce = 5000;

int startTime = 0;
int currTime = 0;
bool currState = false;
bool desState = false;
String flag = "";

fauxmoESP fauxmo;
DHT dht(DHTPin, DHTTYPE);
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, LEDPin, NEO_GRB + NEO_KHZ800);
ESP8266WebServer server(80);
File fsUploadFile;              // a File object to temporarily store the received file

// WiFi Parameters
const char* ssid = "Bullpup_CA";
const char* password = "********";
const char* host = "Fireplace";
//IPAddress fixedIP(192,168,1,201);


//WiFi Setup routine - modify to include LED status feedback
void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network.
  DEBUG_PRINTLN();
  DEBUG_PRINT("Connecting to ");
  DEBUG_PRINTLN(ssid);

  WiFi.hostname(host);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    if (ledEnable) digitalWrite(BlueLED, LOW);
    delay(250);
    if (ledEnable) digitalWrite(BlueLED, HIGH);
    delay(250);
    DEBUG_PRINT(".");
  }

  randomSeed(micros());

  DEBUG_PRINTLN("");
  DEBUG_PRINTLN("WiFi connected");
  DEBUG_PRINTLN("IP address: ");
  DEBUG_PRINTLN(WiFi.localIP());

  if (ledEnable) digitalWrite(RedLED, HIGH);
  if (ledEnable) digitalWrite(BlueLED, LOW);

  delay(500);
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

void setup_FauxMo() {
  fauxmo.addDevice("Fireplace");

  //define WeMo callback routine
  fauxmo.onMessage([](unsigned char device_id, const char * device_name, bool state) {
        Serial.printf("[MAIN] Device #%d (%s) state: %s\n", device_id, device_name, state ? "ON" : "OFF");
        flag = "FAUXMO  ";
        if (state) {
          //turn fireplace on
          desState = true;
        } else {
          //turn fireplace off
          desState = false;
        }
    });
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

void fireplaceToggle() {
  desState = !desState;
}

void processButton() {
  fireplaceToggle();
  flag = "MANUAL  ";
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleRoot() {
  server.send(200, "text/plain", "Hello world! - try the /defaultupload url");   // Send HTTP status 200 (Ok) and send some text to the browser/client
}

void uploadPage(void) {
  server.send(200, "text/html", "<form method=\"post\" enctype=\"multipart/form-data\"><input type=\"file\" name=\"name\"><input class=\"button\" type=\"submit\" value=\"Upload\"></form>");
}

void setupServer(void) {
  //server.on("/", handleRoot);

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

  server.on("/index.html", HTTP_POST, processButton);

  server.onNotFound([]() {                              // If the client requests any URI
    if (!handleFileRead(server.uri()))                  // send it if it exists
      server.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
  });

  server.begin();
  DEBUG_PRINTLN("HTTP Server Started.");
}

void fireplace_on() {
  startTime = millis();
  if (ledEnable) digitalWrite(RedLED, LOW);      //set red LED on

  //start LED, fan, heater
  for(int i=1;i<NUMPIXELS;i++){
   pixels.setPixelColor(i, pixels.Color(240,240,240)); }
  pixels.show();
  digitalWrite(FanPin, HIGH);
  digitalWrite(HeaterPin, HIGH);
  currState = true;
}

void fireplace_off() {
  digitalWrite(HeaterPin, LOW);
  if (ledEnable) digitalWrite(RedLED, HIGH);
  digitalWrite(FanPin, LOW);
   for(int i=0;i<NUMPIXELS;i++){
    pixels.setPixelColor(i, pixels.Color(0,0,0)); }
  pixels.show();
  currState = false;
}



//getCurrTime should return the
//int getCurrTime() {}

//for
//void setStartTime() {}

//if checkTimeout sees that elapsed run time is > allowed, returns FALSE, else TRUE.
//bool chckTimeout() {}

void setup() {
  //initialize pin outputs
  pinMode(RedLED, OUTPUT);
  pinMode(BlueLED, OUTPUT);
  pinMode(FanPin, OUTPUT);
  pinMode(HeaterPin, OUTPUT);
  pinMode(LEDPin, OUTPUT);

  digitalWrite(BlueLED, LOW);
  digitalWrite(RedLED, LOW);

  digitalWrite(FanPin, LOW);
  digitalWrite(HeaterPin, LOW);
  digitalWrite(LEDPin, LOW);

  delay(500);

  digitalWrite(BlueLED, HIGH);
  digitalWrite(RedLED, HIGH);

  pixels.begin();
  pixels.setBrightness(240);
  pixels.show();

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

  //initialize WeMo library
  setup_FauxMo();

  SPIFFS.begin();                           // Start the SPI Flash Files System

  setupServer();
}

void writeLog(void) {
  File log = SPIFFS.open("/log.txt", "a");
  int temp = (int) dht.readTemperature(true);
  int time = millis() / 1000;
  log.print(time);
  log.print(" Curr State: ");
  log.print(currState);
  log.print("  Des State: ");
  log.print(desState);
  log.print(" Cmd Source: ");
  log.print(flag);
  log.print(" Temp: ");
  log.println(temp);
  log.close();
}

void loop()
{

  //mandatory handlers
  fauxmo.handle();      //required to process FauxMo handler
  server.handleClient();
  ArduinoOTA.handle();

  //update states if required

  //first, handle all the on->off or remain on cases
  if (currState) {
    //check max temp
    int temp = (int) dht.readTemperature(true);
    if (temp > maxTemp) {
      desState = false;
      flag = "TEMP HI ";
    }

    //check max elapsed max time
    currTime = millis();
    if ((currTime - startTime) > maxTime) {
      desState = false;
      flag = "TIME HI ";
    }

    //handle Echo bounce
    if (!desState && ((currTime-startTime) < deBounce)) {
      desState = true;
    }

    //check scheduled off time

    //check thermostat temperature
  }  else {       //handle the off->on or remain off
    //check scheduled start time
  }






  //Process FSM

  if (currState && !desState) {
    fireplace_off();
    writeLog();
  }

  if (!currState && desState) {
    fireplace_on();
    writeLog();
  }


  delay(500);

}

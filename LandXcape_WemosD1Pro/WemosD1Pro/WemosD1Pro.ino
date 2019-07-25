/*
 * Wemos D1 Pro Sketch
 * 
 * Connects to a static WLAN configuration 
 * Allows to start, stop, send to base and furthermore to switch off and on the LandXcape lawn mower incl. activation with PIN
 * Furthmore supports Updates via WLAN via Web /updateLandXcape or via terminal -> curl -f "image=@firmware.bin" LandXcapeIPAddress/updateBin
 * 
 */
 
#include <TimeLib.h>
#include <Time.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>

//variables
const char* ssid     = "Linux_2";
const char* password = "linuxrulezz";
int baudrate = 115200;
int delayToReconnectTry = 15000;
boolean debugMode = false; //only useful as long as the WEMOS is connected to the PC ;)
boolean NTPUpdateSuccessful = false;
double version = 0.45;

int buttonPressTime = 800; //in ms
int PWRButtonPressTime = 4000; // in ms

double batteryVoltage = 0;
double baseFor1V = 329.9479166;
double faktorBat = 9.322916;

int STOP = D1;
int START = D3;
int HOME = D5;
int OKAY = D7;
int BATVOLT = A0;
int PWR = D6;


ESP8266WebServer wwwserver(80);
String content = "";

//Debug messages
String connectTo = "Connection to ";
String connectionEstablished = "WiFi connected: ";
String ipAddr = "IP Address: ";

void setup() {
  if (debugMode){
    Serial.begin(baudrate);
    delay(10);   
  }
  pinMode(LED_BUILTIN,OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); //LED off for now

  //WiFi Connection
  if (debugMode){
    Serial.println();
    Serial.println(connectTo + ssid);
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if (debugMode){
      Serial.print(WiFi.status());
    }
  }
  
  if (debugMode){
      Serial.println();
      Serial.println(connectionEstablished + ssid);
      Serial.println(ipAddr + WiFi.localIP().toString());
    }

  //Activate and configure Web-Server
  wwwserver.on("/", handleRoot);
  wwwserver.on("/updateLandXcape", handleWebUpdate);
  wwwserver.on("/updateBin", HTTP_POST, handleUpdateViaBinary, handleWebUpdateHelperFunction);
  wwwserver.on("/start", handleStartMowing);
  wwwserver.on("/stop", handleStopMowing);
  wwwserver.on("/goHome", handleGoHome);
  wwwserver.on("/configure", handleAdministration);
  wwwserver.on("/PWRButton", handleSwitchOnOff);
  
  wwwserver.begin();
  if (debugMode){
    Serial.println("HTTP server started");
  }
  NTPUpdateSuccessful = syncTimeViaNTP();

  //initialize Digital Pins
  pinMode(STOP,OUTPUT); //Stop Button
  pinMode(START,OUTPUT); //Start Button
  pinMode(HOME,OUTPUT); //Home Button
  pinMode(OKAY,OUTPUT); //OK Button
  pinMode(BATVOLT,INPUT); //Battery Voltage sensing via Analog Input
  pinMode(PWR,OUTPUT);
  digitalWrite(STOP,HIGH);
  digitalWrite(START,HIGH);
  digitalWrite(HOME,HIGH);
  digitalWrite(OKAY,HIGH);
  digitalWrite(PWR,HIGH);
}

void loop() {
  
  //Webserver section
  wwwserver.handleClient(); 

  if (NTPUpdateSuccessful==false){
    NTPUpdateSuccessful = syncTimeViaNTP();
  }
}

static void handleRoot(void){

  if (debugMode){
    Serial.println((String)"Connection from outside established at UTC Time:"+hour()+":"+minute()+":"+second()+" " + year());
  }
  digitalWrite(LED_BUILTIN, LOW); //show connection via LED  

  //preparation work
  char temp[1200];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  //Battery Voltage computing
  double reading = analogRead(BATVOLT);
  reading = reading / baseFor1V;
  batteryVoltage = reading * faktorBat;
  
  snprintf(temp, 1200,

           "<html>\
            <head>\
              <title>LandXcape</title>\
              <style>\
                body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
              </style>\
            </head>\
              <body>\
                <h1>LandXcape</h1>\
                <p>Uptime: %02d:%02d:%02d</p>\
                <p>UTC Time: %02d:%02d:%02d</p>\
                <p>Version: %02lf</p>\
                <p>Battery Voltage: %02lf</p>\
                <br>\
                <form method='POST' action='/start'><button type='submit'>Start</button></form>\
                <br>\
                <form method='POST' action='/stop'><button type='submit'>Stop</button></form>\
                <br>\
                <form method='POST' action='/goHome'><button type='submit'>go Home</button></form>\
                <br>\
                <form method='POST' action='/configure'><button type='submit'>Administration</button></form>\ 
                <br>\
                <form method='POST' action='/PWRButton'><button type='submit'>Power Robi off / on</button></form>\
                <br>\
              </body>\
            </html>",

           hr, min % 60, sec % 60,
           hour(),minute(),second(),version,batteryVoltage
          );
  wwwserver.send(200, "text/html", temp);

  digitalWrite(LED_BUILTIN, HIGH); //show successful answer to request  
}
/*
 * handleStartMowing triggers the Robi to start with his job :)
 */
static void handleStartMowing(void){

   if (debugMode){
    Serial.println((String)"Start with the Start relay for " + buttonPressTime + "followed with the OK button for the same time");
  }
   
   digitalWrite(START,LOW);//Press Start Button 
   delay(buttonPressTime);
   digitalWrite(START,HIGH);//Release Start Button 
   delay(buttonPressTime);
   digitalWrite(OKAY,LOW);//Press Okay Button
   delay(buttonPressTime);
   digitalWrite(OKAY,HIGH);//Release Okay Button

  if (debugMode){
    Serial.println((String)"Mowing started at UTC Time:"+hour()+":"+minute()+":"+second()+" " + year());
  }
  char temp[700];
  snprintf(temp, 700,
           "<html>\
            <head>\
              <title>LandXcape</title>\
              <style>\
                body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
              </style>\
              <meta http-equiv='Refresh' content='4; url=\\'>\
            </head>\
              <body>\
                <h1>LandXcape</h1>\
                <p><\p>\
                <p>Mowing started at UTC Time: %02d:%02d:%02d</p>\
              </body>\
            </html>",
            hour(),minute(),second()
            );
  wwwserver.send(200, "text/html", temp);
}

/*
 * handleStopMowing triggers the Robi to stop with his job and to wait for further instructions :)
 */
static void handleStopMowing(void){
  
   if (debugMode){
    Serial.println((String)"Start with the Stop relay for " + buttonPressTime + " before releasing it again");
  }
   
   digitalWrite(STOP,LOW);//Press Stop Button 
   delay(buttonPressTime);
   digitalWrite(STOP,HIGH);//Release Stop Button 

   
  if (debugMode){
    Serial.println((String)"Mowing stoped at UTC Time:"+hour()+":"+minute()+":"+second()+" " + year());
  }
  char temp[700];
  snprintf(temp, 700,
           "<html>\
            <head>\
              <title>LandXcape</title>\
              <style>\
                body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
              </style>\
              <meta http-equiv='Refresh' content='4; url=\\'>\
            </head>\
              <body>\
                <h1>LandXcape</h1>\
                <p><\p>\
                <p>Mowing stoped at UTC Time: %02d:%02d:%02d</p>\
              </body>\
            </html>",
            hour(),minute(),second()
            );
  wwwserver.send(200, "text/html", temp);
}

/*
 * handleGoHome triggers the Robi to go back to his station and to charge :)
 */
static void handleGoHome(void){
  
  if (debugMode){
    Serial.println((String)"Start with the Home relay for " + buttonPressTime + "followed with the OK button for the same time");
  }
   
   digitalWrite(HOME,LOW);//Press Home Button 
   delay(buttonPressTime);
   digitalWrite(HOME,HIGH);//Release Home Button 
   delay(buttonPressTime);
   digitalWrite(OKAY,LOW);//Press Okay Button
   delay(buttonPressTime);
   digitalWrite(OKAY,HIGH);//Release Okay Button
   
  if (debugMode){
    Serial.println((String)"Robi sent home at UTC Time:"+hour()+":"+minute()+":"+second()+" " + year());
  }
  char temp[700];
  snprintf(temp, 700,
           "<html>\
            <head>\
              <title>LandXcape</title>\
              <style>\
                body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
              </style>\
              <meta http-equiv='Refresh' content='4; url=\\'>\
            </head>\
              <body>\
                <h1>LandXcape</h1>\
                <p><\p>\
                <p>Mowing stoped and sent back to base at UTC Time: %02d:%02d:%02d</p>\
              </body>\
            </html>",
            hour(),minute(),second()
            );
  wwwserver.send(200, "text/html", temp);
}

/*
 * handleAdministration allows to administrate your settings :)
 */
static void handleAdministration(void){

  digitalWrite(LED_BUILTIN, LOW); //show connection via LED 
  //currently dummy function
    if (debugMode){
      Serial.println((String)"Administration site requested at UTC Time:"+hour()+":"+minute()+":"+second()+" " + year());
    }
    char temp[700];
    snprintf(temp, 700,
             "<html>\
              <head>\
                <title>LandXcape</title>\
                <style>\
                  body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
                </style>\
              </head>\
                <body>\
                  <h1>LandXcape Administration Site</h1>\
                  <p><\p>\
                </body>\
              </html>"
              );
    wwwserver.send(200, "text/html", temp);
    digitalWrite(LED_BUILTIN, HIGH); //show connection via LED 
}

/*
 * handleSwitchOnOff allows to Power the robi on or off as you wish
 */
static void handleSwitchOnOff(void){

 if (debugMode){
    Serial.println((String)"handleSwitchOnOff triggered at UTC Time:"+hour()+":"+minute()+":"+second()+" " + year());
  }
   
   digitalWrite(PWR,LOW);//Press Home Button 
   delay(PWRButtonPressTime);
   digitalWrite(PWR,HIGH);//Release Home Button 

  char temp[700];
  snprintf(temp, 700,
           "<html>\
            <head>\
              <title>LandXcape</title>\
              <style>\
                body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
              </style>\
              <meta http-equiv='Refresh' content='4; url=\\'>\
            </head>\
              <body>\
                <h1>LandXcape</h1>\
                <p><\p>\
                <p>Robi switched off/on at UTC Time: %02d:%02d:%02d</p>\
              </body>\
            </html>",
            hour(),minute(),second()
            );
  wwwserver.send(200, "text/html", temp);
}

//Get the current time via NTP over the Internet
static boolean syncTimeViaNTP(void){
  
  //local one time variables
  WiFiUDP udp;
  int localPort = 2390;      // local port to listen for UDP packets
  IPAddress timeServerIP; // time.nist.gov NTP server address
  char* ntpServerName = "time.nist.gov";
  int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
  byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

    if (debugMode){
      Serial.println("Starting UDP");
    }
    udp.begin(localPort);
  
    //get a random server from the pool
    WiFi.hostByName(ntpServerName, timeServerIP);

    if (debugMode){
      Serial.println("sending NTP packet...");
    }
  
    // set all bytes in the buffer to 0
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    // Initialize values needed to form NTP request
    // (see URL above for details on the packets)
    packetBuffer[0] = 0b11100011;   // LI, Version, Mode
    packetBuffer[1] = 0;     // Stratum, or type of clock
    packetBuffer[2] = 6;     // Polling Interval
    packetBuffer[3] = 0xEC;  // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12]  = 49;
    packetBuffer[13]  = 0x4E;
    packetBuffer[14]  = 49;
    packetBuffer[15]  = 52;

    // all NTP fields have been given values, now
    // you can send a packet requesting a timestamp:
    udp.beginPacket(timeServerIP, 123); //NTP requests are to port 123
    udp.write(packetBuffer, NTP_PACKET_SIZE);
    udp.endPacket();

    // wait for a sure reply or otherwise cancel time setting process
    delay(1000);
    int cb = udp.parsePacket();
    if (!cb){
       if (debugMode){
        Serial.println((String)"Time setting failed. Received Packets="+cb);
      }
      return false;
    }

    if (debugMode){
      Serial.println((String)"packet received, length="+ cb);
    }
        
    // We've received a packet, read the data from it
    udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    
      // now convert NTP time into everyday time:
      // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
      const unsigned long seventyYears = 2208988800UL;
      // subtract seventy years:
      unsigned long epoch = secsSince1900 - seventyYears;
      //set current time on device
      setTime(epoch);
      if (debugMode){
        Serial.println("Current time updated.");

        // print the hour, minute and second:
        Serial.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
        Serial.print((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
        Serial.print(':');
        if (((epoch % 3600) / 60) < 10) {
          // In the first 10 minutes of each hour, we'll want a leading '0'
          Serial.print('0');
        }
        Serial.print((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
        Serial.print(':');
        if ((epoch % 60) < 10) {
          // In the first 10 seconds of each minute, we'll want a leading '0'
          Serial.print('0');
        }
        Serial.println(epoch % 60); // print the second
      }
    
   udp.stop();
   return true;
}

static void handleWebUpdate(void){
  digitalWrite(LED_BUILTIN, LOW); //show connection via LED 

  //preparation work
  char temp[500];
  snprintf(temp, 500,
           "<html>\
  <head>\
    <title>LandXcape WebUpdate Site</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
    <body>\
      <h1>LandXcape WebUpdate Site</h1>\
      <p>UTC Time: %02d:%02d:%02d</p>\
      <p>Version: %02lf</p>\
           <form method='POST' action='/updateBin' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>\
           </body> </html>",
           hour(),minute(),second(),version
          );
          
  //present website 
  wwwserver.send(200, "text/html", temp);

    if (debugMode){
          Serial.println("WebUpdate site requested...");
    }

  digitalWrite(LED_BUILTIN, HIGH); //show connection via LED 
}

static void handleUpdateViaBinary(void){
  digitalWrite(LED_BUILTIN, LOW); //show working process via LED 
  if (debugMode){
          Serial.println("handleUpdateViaBinary function triggered...");
  }

  wwwserver.sendHeader("Connection", "close");
  wwwserver.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
  ESP.restart();
  if (debugMode){
          Serial.println("handleUpdateViaBinary function finished...");
  }
  
  digitalWrite(LED_BUILTIN, HIGH); //show working process via LED 
}

static void handleWebUpdateHelperFunction (void){
  digitalWrite(LED_BUILTIN, LOW); //show working process via LED 

  HTTPUpload& upload = wwwserver.upload();
      if (upload.status == UPLOAD_FILE_START) {
        Serial.setDebugOutput(true);
        WiFiUDP::stopAll();
        Serial.printf("Update: %s\n", upload.filename.c_str());
        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (!Update.begin(maxSketchSpace)) { //start with max available size
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) { //true to set the size to the current progress
          Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);

      //send the user back to the main page
        char temp[700];
        snprintf(temp, 700,
               "<html>\
                <head>\
                  <title>LandXcape</title>\
                  <style>\
                    body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
                  </style>\
                  <meta http-equiv='Refresh' content='5; url=\\'>\
                </head>\
                  <body>\
                    <h1>LandXcape</h1>\
                    <p><\p>\
                    <p>Update successfull at UTC Time: %02d:%02d:%02d</p>\
                  </body>\
                </html>",
                hour(),minute(),second()
                );
      wwwserver.send(200, "text/html", temp);
          
        } else {
          Update.printError(Serial);
        }
        Serial.setDebugOutput(false);
      }
      yield();

  digitalWrite(LED_BUILTIN, HIGH); //show working process via LED 
}

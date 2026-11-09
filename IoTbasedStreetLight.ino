#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

#define LDR_PIN   35
#define SENSOR1   27
#define SENSOR2   26
#define SENSOR3   25
#define SENSOR4   33
#define LED1      18
#define LED2      19
#define LED3      21
#define LED4      22

#define DIM_BRIGHTNESS        15
#define FULL_BRIGHTNESS      255
#define VEHICLE_ON_TIME     5000
#define SPEED_SENSOR_DIST   0.5

#define PWM_FREQ  5000
#define PWM_RES      8

unsigned long offTime1 = 0;
unsigned long offTime2 = 0;
unsigned long offTime3 = 0;
unsigned long offTime4 = 0;

unsigned long sensor1TrigTime = 0;
bool sensor1Triggered = false;
float lastSpeed = 0;
int vehicleCount = 0;
bool overspeedAlert = false;

int led1Status = 0;
int led2Status = 0;
int led3Status = 0;
int led4Status = 0;

bool led1Healthy = true;
bool led2Healthy = true;
bool led3Healthy = true;
bool led4Healthy = true;

unsigned long lightsOnStart = 0;
unsigned long totalLightsOnMs = 0;
bool lightsCurrentlyOn = false;
float unitsConsumed = 0;
float moneySaved = 0;
unsigned long lastEnergyUpdate = 0;

WebServer server(80);
void setLED(int pin, int brightness);
String getHTML();
String getStatusBadge(int status, bool healthy);

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LDR_PIN, INPUT);
  pinMode(SENSOR1, INPUT);
  pinMode(SENSOR2, INPUT);
  pinMode(SENSOR3, INPUT);
  pinMode(SENSOR4, INPUT);

  ledcAttach(LED1, PWM_FREQ, PWM_RES);
  ledcAttach(LED2, PWM_FREQ, PWM_RES);
  ledcAttach(LED3, PWM_FREQ, PWM_RES);
  ledcAttach(LED4, PWM_FREQ, PWM_RES);

  setLED(LED1, 0);
  setLED(LED2, 0);
  setLED(LED3, 0);
  setLED(LED4, 0);

  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi Connected!");
  Serial.print("Open this IP in browser: http://");
  Serial.println(WiFi.localIP());

  server.on("/", []() {
    server.send(200, "text/html", getHTML());
  });
  server.begin();
  Serial.println("SYSTEM STARTED");
}

void updateEnergy(bool isNight) {
  unsigned long now = millis();

  // Track lights ON time
  if (isNight && !lightsCurrentlyOn) {
    lightsOnStart = now;
    lightsCurrentlyOn = true;
  }
  if (!isNight && lightsCurrentlyOn) {
    totalLightsOnMs += (now - lightsOnStart);
    lightsCurrentlyOn = false;
  }

  // Update energy every 5 seconds
  if (now - lastEnergyUpdate > 5000) {
    // Add current session time if lights on
    unsigned long totalMs = totalLightsOnMs;
    if (lightsCurrentlyOn) {
      totalMs += (now - lightsOnStart);
    }
    float hoursOn = totalMs / 3600000.0;

    // Smart system: 4 LEDs x 0.5W
    float smartWatts = 4 * 0.5;
    // Normal system: 4 street lights x 40W
    float normalWatts = 4 * 40;

    unitsConsumed = (smartWatts * hoursOn) / 1000.0;
    float normalUnits = (normalWatts * hoursOn) / 1000.0;

    // Rs 8 per unit saved
    moneySaved = (normalUnits - unitsConsumed) * 8;
    lastEnergyUpdate = now;
  }
}

void loop() {
  server.handleClient();
  unsigned long now = millis();

  // READ LDR
  int ldrState = digitalRead(LDR_PIN);
  bool isNight = (ldrState == 1);

  // Update energy
  updateEnergy(isNight);

  Serial.print("LDR:"); Serial.print(ldrState);
  Serial.print(" Night:"); Serial.print(isNight);
  Serial.print(" S1:"); Serial.print(digitalRead(SENSOR1));
  Serial.print(" S2:"); Serial.print(digitalRead(SENSOR2));
  Serial.print(" S3:"); Serial.print(digitalRead(SENSOR3));
  Serial.print(" S4:"); Serial.println(digitalRead(SENSOR4));

  // DAY MODE - no delay so sensors still read fast
  if (!isNight) {
    setLED(LED1, 0); led1Status = 0;
    setLED(LED2, 0); led2Status = 0;
    setLED(LED3, 0); led3Status = 0;
    setLED(LED4, 0); led4Status = 0;
  }

  // ALWAYS CHECK SENSORS regardless of day/night
  // Speed detection between sensor 1 and 2
  if (digitalRead(SENSOR1) == LOW && !sensor1Triggered) {
    sensor1TrigTime = now;
    sensor1Triggered = true;
    Serial.println("Speed timer START at S1");
  }

  if (digitalRead(SENSOR2) == LOW && sensor1Triggered) {
    unsigned long timeDiff = now - sensor1TrigTime;
    if (timeDiff > 10 && timeDiff < 10000) {
      float timeSeconds = timeDiff / 1000.0;
      lastSpeed = (SPEED_SENSOR_DIST / timeSeconds) * 3.6;
      vehicleCount++;
      overspeedAlert = (lastSpeed > 30);
      Serial.print("Speed: "); Serial.print(lastSpeed);
      Serial.print(" Count: "); Serial.println(vehicleCount);
    }
    sensor1Triggered = false;
  }

  // Reset speed trigger if too long
  if (sensor1Triggered && (now - sensor1TrigTime > 10000)) {
    sensor1Triggered = false;
    Serial.println("Speed timer RESET - took too long");
  }

  // NIGHT MODE ONLY - light control
  if (isNight) {
    if (digitalRead(SENSOR1) == LOW) {
      Serial.println("Vehicle at S1 - NIGHT");
      setLED(LED1, FULL_BRIGHTNESS); led1Status = 2;
      setLED(LED2, FULL_BRIGHTNESS); led2Status = 2;
      offTime1 = now + VEHICLE_ON_TIME;
      offTime2 = now + VEHICLE_ON_TIME;
      led1Healthy = true;
      led2Healthy = true;
    }

    if (digitalRead(SENSOR2) == LOW) {
      Serial.println("Vehicle at S2 - NIGHT");
      setLED(LED2, FULL_BRIGHTNESS); led2Status = 2;
      setLED(LED3, FULL_BRIGHTNESS); led3Status = 2;
      offTime2 = now + VEHICLE_ON_TIME;
      offTime3 = now + VEHICLE_ON_TIME;
      led2Healthy = true;
      led3Healthy = true;
    }

    if (digitalRead(SENSOR3) == LOW) {
      Serial.println("Vehicle at S3 - NIGHT");
      setLED(LED3, FULL_BRIGHTNESS); led3Status = 2;
      setLED(LED4, FULL_BRIGHTNESS); led4Status = 2;
      offTime3 = now + VEHICLE_ON_TIME;
      offTime4 = now + VEHICLE_ON_TIME;
      led3Healthy = true;
      led4Healthy = true;
    }

    if (digitalRead(SENSOR4) == LOW) {
      Serial.println("Vehicle at S4 - NIGHT");
      setLED(LED4, FULL_BRIGHTNESS); led4Status = 2;
      offTime4 = now + VEHICLE_ON_TIME;
      led4Healthy = true;
    }

    // Return to DIM after timeout
    if (now > offTime1 && offTime1 > 0) {
      setLED(LED1, DIM_BRIGHTNESS);
      led1Status = 1;
    }
    if (now > offTime2 && offTime2 > 0) {
      setLED(LED2, DIM_BRIGHTNESS);
      led2Status = 1;
    }
    if (now > offTime3 && offTime3 > 0) {
      setLED(LED3, DIM_BRIGHTNESS);
      led3Status = 1;
    }
    if (now > offTime4 && offTime4 > 0) {
      setLED(LED4, DIM_BRIGHTNESS);
      led4Status = 1;
    }

    // Set dim if never triggered yet
    if (offTime1 == 0) { setLED(LED1, DIM_BRIGHTNESS); led1Status = 1; }
    if (offTime2 == 0) { setLED(LED2, DIM_BRIGHTNESS); led2Status = 1; }
    if (offTime3 == 0) { setLED(LED3, DIM_BRIGHTNESS); led3Status = 1; }
    if (offTime4 == 0) { setLED(LED4, DIM_BRIGHTNESS); led4Status = 1; }
  }

  delay(50);
}

void setLED(int pin, int brightness) {
  ledcWrite(pin, brightness);
}

String getStatusBadge(int status, bool healthy) {
  if (!healthy) return "<span class='badge badge-fault'>FAULT</span>";
  if (status == 0) return "<span class='badge badge-off'>OFF</span>";
  if (status == 1) return "<span class='badge badge-dim'>DIM 30%</span>";
  return "<span class='badge badge-full'>FULL BRIGHT</span>";
}

String getHTML() {
  int ldrState = digitalRead(LDR_PIN);
  bool isNight = (ldrState == 1);

  unsigned long uptime = millis() / 1000;
  int h = uptime / 3600;
  int m = (uptime % 3600) / 60;
  int s = uptime % 60;

  unsigned long totalMs = totalLightsOnMs;
  if (lightsCurrentlyOn) {
    totalMs += (millis() - lightsOnStart);
  }
  float hoursOn = totalMs / 3600000.0;

  bool s1 = digitalRead(SENSOR1) == LOW;
  bool s2 = digitalRead(SENSOR2) == LOW;
  bool s3 = digitalRead(SENSOR3) == LOW;
  bool s4 = digitalRead(SENSOR4) == LOW;

  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<title>Smart City Street Light</title>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<meta http-equiv='refresh' content='2'>";
  html += "<style>";
  html += "*{margin:0;padding:0;box-sizing:border-box;}";
  html += "body{font-family:'Times New Roman',Times,serif;transition:all 0.5s;}";

  if (isNight) {
    html += "body{background:#0b0f1e;color:#e8eaf6;}";
    html += ".header{background:linear-gradient(135deg,#1a237e,#283593);border-bottom:3px solid #5c6bc0;}";
    html += ".status-bar{background:#0d1124;color:#7986cb;border-bottom:1px solid #1e2a4a;}";
    html += ".card{background:#13192e;border:1px solid #1e2a4a;color:#e8eaf6;}";
    html += ".section-title{color:#7986cb;border-left:4px solid #7986cb;}";
    html += ".led-card{background:#13192e;border:1px solid #1e2a4a;}";
    html += ".led-row{border-bottom:1px solid #1e2a4a;}";
    html += ".led-name{color:#e8eaf6;}";
    html += ".led-sub{color:#7986cb;}";
    html += ".sensor-card{background:#13192e;border:1px solid #1e2a4a;}";
    html += ".sensor-title{color:#7986cb;}";
    html += ".card-title{color:#7986cb;}";
    html += ".card-unit{color:#7986cb;}";
    html += ".mode-card{background:linear-gradient(135deg,#0d1b3e,#1a237e);border:2px solid #3949ab;}";
    html += ".mode-value{color:#90caf9;}";
    html += ".mode-desc{color:#7986cb;}";
    html += ".energy-card{background:linear-gradient(135deg,#0a2a0a,#1b5e20);border:1px solid #2e7d32;}";
    html += ".energy-val{color:#69f0ae;}";
    html += ".money-val{color:#ffd600;}";
    html += ".footer{color:#3d4a6b;border-top:1px solid #1e2a4a;}";
    html += ".uptime-val{color:#e8eaf6;}";
    html += ".idle-text{color:#444;}";
    html += ".active-text{color:#00e676;}";
    html += ".sensor-active{border:2px solid #00e676 !important;box-shadow:0 0 15px #00e67633;}";
    html += ".speed-safe{color:#00e676;}";
    html += ".speed-danger{color:#f44336;}";
  } else {
    html += "body{background:#f0f4ff;color:#1a237e;}";
    html += ".header{background:linear-gradient(135deg,#1565c0,#1976d2);border-bottom:3px solid #42a5f5;}";
    html += ".status-bar{background:#e8eaf6;color:#3949ab;border-bottom:1px solid #c5cae9;}";
    html += ".card{background:#ffffff;border:1px solid #c5cae9;color:#1a237e;box-shadow:0 2px 8px rgba(0,0,0,0.08);}";
    html += ".section-title{color:#1565c0;border-left:4px solid #1565c0;}";
    html += ".led-card{background:#ffffff;border:1px solid #c5cae9;box-shadow:0 2px 8px rgba(0,0,0,0.08);}";
    html += ".led-row{border-bottom:1px solid #e8eaf6;}";
    html += ".led-name{color:#1a237e;}";
    html += ".led-sub{color:#5c6bc0;}";
    html += ".sensor-card{background:#ffffff;border:1px solid #c5cae9;box-shadow:0 2px 8px rgba(0,0,0,0.08);}";
    html += ".sensor-title{color:#3949ab;}";
    html += ".card-title{color:#3949ab;}";
    html += ".card-unit{color:#5c6bc0;}";
    html += ".mode-card{background:linear-gradient(135deg,#fff8e1,#fff3cd);border:2px solid #ffc107;}";
    html += ".mode-value{color:#e65100;}";
    html += ".mode-desc{color:#f57f17;}";
    html += ".energy-card{background:linear-gradient(135deg,#e8f5e9,#c8e6c9);border:1px solid #81c784;}";
    html += ".energy-val{color:#1b5e20;}";
    html += ".money-val{color:#e65100;}";
    html += ".footer{color:#9fa8da;border-top:1px solid #c5cae9;}";
    html += ".uptime-val{color:#1a237e;}";
    html += ".idle-text{color:#ccc;}";
    html += ".active-text{color:#2e7d32;}";
    html += ".sensor-active{border:2px solid #2e7d32 !important;box-shadow:0 0 15px #2e7d3233;}";
    html += ".speed-safe{color:#2e7d32;}";
    html += ".speed-danger{color:#c62828;}";
  }

  html += ".header{padding:20px;text-align:center;}";
  html += ".header-sub{color:#bbdefb;font-size:13px;margin-bottom:5px;letter-spacing:1px;}";
  html += ".header h1{color:#ffffff;font-size:28px;font-weight:bold;}";
  html += ".header-desc{color:#90caf9;font-size:13px;margin-top:5px;}";
  html += ".online-dot{display:inline-block;width:8px;height:8px;background:#00e676;border-radius:50%;margin-right:5px;}";
  html += ".status-bar{padding:8px 20px;display:flex;justify-content:space-between;font-size:12px;font-weight:bold;}";
  html += ".container{padding:15px;max-width:1200px;margin:0 auto;}";
  html += ".section-title{font-size:13px;font-weight:bold;text-transform:uppercase;letter-spacing:2px;margin:18px 0 10px 0;padding-left:10px;}";
  html += ".grid2{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:12px;}";
  html += ".grid3{display:grid;grid-template-columns:1fr 1fr 1fr;gap:12px;margin-bottom:12px;}";
  html += ".grid4{display:grid;grid-template-columns:1fr 1fr 1fr 1fr;gap:10px;margin-bottom:12px;}";
  html += ".card{border-radius:12px;padding:18px;text-align:center;}";
  html += ".card-title{font-size:11px;font-weight:bold;text-transform:uppercase;letter-spacing:1px;margin-bottom:10px;}";
  html += ".card-value{font-size:30px;font-weight:bold;}";
  html += ".card-unit{font-size:12px;margin-top:5px;}";
  html += ".mode-card{border-radius:12px;padding:22px;text-align:center;}";
  html += ".mode-label{font-size:12px;font-weight:bold;text-transform:uppercase;letter-spacing:2px;margin-bottom:8px;color:#888;}";
  html += ".mode-value{font-size:34px;font-weight:bold;margin:5px 0;}";
  html += ".mode-desc{font-size:13px;margin-top:5px;}";
  html += ".led-card{border-radius:12px;padding:15px;margin-bottom:12px;}";
  html += ".led-row{display:flex;align-items:center;justify-content:space-between;padding:12px 5px;}";
  html += ".led-row:last-child{border-bottom:none !important;}";
  html += ".led-name{font-size:15px;font-weight:bold;}";
  html += ".led-sub{font-size:11px;margin-top:3px;}";
  html += ".badge{padding:6px 14px;border-radius:20px;font-size:12px;font-weight:bold;}";
  html += ".badge-off{background:#444;color:#aaa;}";
  html += ".badge-dim{background:linear-gradient(90deg,#ff8f00,#ffa000);color:#000;}";
  html += ".badge-full{background:linear-gradient(90deg,#00c853,#00e676);color:#000;}";
  html += ".badge-fault{background:linear-gradient(90deg,#c62828,#e53935);color:#fff;animation:blink 1s infinite;}";
  html += ".sensor-card{border-radius:12px;padding:14px;text-align:center;}";
  html += ".sensor-title{font-size:12px;font-weight:bold;text-transform:uppercase;margin-bottom:8px;}";
  html += ".sensor-status{font-size:18px;font-weight:bold;}";
  html += ".sensor-ref{font-size:10px;margin-top:5px;color:#888;}";
  html += ".alert-danger{background:linear-gradient(135deg,#b71c1c,#c62828);border:2px solid #ef5350;border-radius:12px;padding:16px;margin-bottom:12px;text-align:center;animation:blink 1s infinite;}";
  html += ".alert-title{color:#ffffff;font-size:20px;font-weight:bold;}";
  html += ".alert-sub{color:#ffcdd2;font-size:13px;margin-top:6px;}";
  html += ".energy-card{border-radius:12px;padding:18px;text-align:center;}";
  html += ".energy-val{font-size:28px;font-weight:bold;}";
  html += ".money-val{font-size:28px;font-weight:bold;}";
  html += ".speed-safe{font-size:30px;font-weight:bold;}";
  html += ".speed-danger{font-size:30px;font-weight:bold;}";
  html += "@keyframes blink{0%{opacity:1;}50%{opacity:0.6;}100%{opacity:1;}}";
  html += ".footer{text-align:center;padding:15px;font-size:12px;margin-top:10px;}";
  html += "</style></head><body>";

  // HEADER
  html += "<div class='header'>";
  html += "<div class='header-sub'>MUNICIPAL CORPORATION OF INDIA</div>";
  html += "<h1>Smart City Street Light Control Panel</h1>";
  html += "<div class='header-desc'><span class='online-dot'></span>Real-Time Monitoring and Intelligent Control System</div>";
  html += "</div>";

  // STATUS BAR
  char uptimeStr[20];
  sprintf(uptimeStr, "%02d:%02d:%02d", h, m, s);
  html += "<div class='status-bar'>";
  html += "<span>Uptime: <span class='uptime-val'>" + String(uptimeStr) + "</span></span>";
  html += "<span>IP: " + WiFi.localIP().toString() + "</span>";
  html += "<span>Signal: " + String(WiFi.RSSI()) + " dBm</span>";
  html += "<span>Auto Refresh: 2 sec</span>";
  html += "</div>";

  html += "<div class='container'>";

  // ALERTS
  if (overspeedAlert) {
    html += "<div class='alert-danger'>";
    html += "<div class='alert-title'>OVERSPEED ALERT</div>";
    html += "<div class='alert-sub'>Vehicle detected at " + String(lastSpeed, 1) + " km/h — Speed limit is 30 km/h</div>";
    html += "</div>";
  }
  if (!led1Healthy) html += "<div class='alert-danger'><div class='alert-title'>Street Light 1 NOT Working</div><div class='alert-sub'>Maintenance required at Zone A</div></div>";
  if (!led2Healthy) html += "<div class='alert-danger'><div class='alert-title'>Street Light 2 NOT Working</div><div class='alert-sub'>Maintenance required at Zone B</div></div>";
  if (!led3Healthy) html += "<div class='alert-danger'><div class='alert-title'>Street Light 3 NOT Working</div><div class='alert-sub'>Maintenance required at Zone C</div></div>";
  if (!led4Healthy) html += "<div class='alert-danger'><div class='alert-title'>Street Light 4 NOT Working</div><div class='alert-sub'>Maintenance required at Zone D</div></div>";

  // SYSTEM STATUS
  html += "<p class='section-title'>System Status</p>";
  html += "<div class='grid2'>";
  html += "<div class='mode-card'><div class='mode-label'>Current Mode</div>";
  if (isNight) {
    html += "<div class='mode-value'>NIGHT MODE</div>";
    html += "<div class='mode-desc'>Lights Active — Smart Dimming ON</div>";
  } else {
    html += "<div class='mode-value'>DAY MODE</div>";
    html += "<div class='mode-desc'>Lights OFF — Energy Saving Active</div>";
  }
  html += "</div>";
  html += "<div class='card'><div class='card-title'>Lights ON Duration</div>";
  html += "<div class='card-value uptime-val'>" + String(hoursOn, 3) + "</div>";
  html += "<div class='card-unit'>Total Hours Active Tonight</div></div></div>";

  // VEHICLE MONITORING
  html += "<p class='section-title'>Vehicle Monitoring</p>";
  html += "<div class='grid3'>";
  html += "<div class='card'><div class='card-title'>Total Vehicles Detected</div>";
  html += "<div class='card-value'>" + String(vehicleCount) + "</div>";
  html += "<div class='card-unit'>Vehicles Counted Tonight</div></div>";

  html += "<div class='card'><div class='card-title'>Last Vehicle Speed</div>";
  if (lastSpeed > 30) {
    html += "<div class='speed-danger'>" + String(lastSpeed, 1) + " km/h</div>";
  } else {
    html += "<div class='speed-safe'>" + String(lastSpeed, 1) + " km/h</div>";
  }
  html += "<div class='card-unit'>" + String(lastSpeed > 30 ? "OVERSPEED!" : "Normal Speed") + "</div></div>";

  html += "<div class='card'><div class='card-title'>Speed Assessment</div>";
  if (lastSpeed == 0) {
    html += "<div class='card-value' style='font-size:22px'>No Data</div>";
    html += "<div class='card-unit'>Waiting for vehicle</div>";
  } else if (lastSpeed <= 30) {
    html += "<div class='speed-safe' style='font-size:22px'>SAFE</div>";
    html += "<div class='card-unit'>Within 30 km/h limit</div>";
  } else {
    html += "<div class='speed-danger' style='font-size:22px'>DANGER</div>";
    html += "<div class='card-unit'>Exceeds 30 km/h limit</div>";
  }
  html += "</div></div>";

  // POWER MANAGEMENT
  html += "<p class='section-title'>Smart Power Management</p>";
  html += "<div class='grid3'>";
  html += "<div class='energy-card'><div class='card-title'>Units Consumed</div>";
  html += "<div class='energy-val'>" + String(unitsConsumed, 5) + " kWh</div>";
  html += "<div class='card-unit'>Smart System Usage</div></div>";

  html += "<div class='energy-card'><div class='card-title'>Money Saved</div>";
  html += "<div class='money-val'>Rs " + String(moneySaved, 2) + "</div>";
  html += "<div class='card-unit'>vs Normal Street Lights</div></div>";

  html += "<div class='energy-card'><div class='card-title'>CO2 Reduced</div>";
  html += "<div class='energy-val'>" + String(moneySaved * 0.8, 3) + " g</div>";
  html += "<div class='card-unit'>Carbon Footprint Saved</div></div>";
  html += "</div>";

  // STREET LIGHT HEALTH
  html += "<p class='section-title'>Street Light Health Monitor</p>";
  html += "<div class='led-card'>";
  html += "<div class='led-row'><div><div class='led-name'>Light 1 — Zone A</div><div class='led-sub'>Pin D18 | IR Sensor 1 (D27)</div></div>" + getStatusBadge(led1Status, led1Healthy) + "</div>";
  html += "<div class='led-row'><div><div class='led-name'>Light 2 — Zone B</div><div class='led-sub'>Pin D19 | IR Sensor 2 (D26)</div></div>" + getStatusBadge(led2Status, led2Healthy) + "</div>";
  html += "<div class='led-row'><div><div class='led-name'>Light 3 — Zone C</div><div class='led-sub'>Pin D21 | IR Sensor 3 (D25)</div></div>" + getStatusBadge(led3Status, led3Healthy) + "</div>";
  html += "<div class='led-row'><div><div class='led-name'>Light 4 — Zone D</div><div class='led-sub'>Pin D22 | IR Sensor 4 (D33)</div></div>" + getStatusBadge(led4Status, led4Healthy) + "</div>";
  html += "</div>";

  // IR SENSOR STATUS
  html += "<p class='section-title'>IR Sensor Live Status</p>";
  html += "<div class='grid4'>";

  html += "<div class='sensor-card " + String(s1 ? "sensor-active" : "") + "'>";
  html += "<div class='sensor-title'>IR Sensor 1<br>Zone A (D27)</div>";
  html += "<div class='sensor-status " + String(s1 ? "active-text" : "idle-text") + "'>" + String(s1 ? "ACTIVE" : "IDLE") + "</div>";
  html += "<div class='sensor-ref'>Speed Reference 1</div></div>";

  html += "<div class='sensor-card " + String(s2 ? "sensor-active" : "") + "'>";
  html += "<div class='sensor-title'>IR Sensor 2<br>Zone B (D26)</div>";
  html += "<div class='sensor-status " + String(s2 ? "active-text" : "idle-text") + "'>" + String(s2 ? "ACTIVE" : "IDLE") + "</div>";
  html += "<div class='sensor-ref'>Speed Reference 2</div></div>";

  html += "<div class='sensor-card " + String(s3 ? "sensor-active" : "") + "'>";
  html += "<div class='sensor-title'>IR Sensor 3<br>Zone C (D25)</div>";
  html += "<div class='sensor-status " + String(s3 ? "active-text" : "idle-text") + "'>" + String(s3 ? "ACTIVE" : "IDLE") + "</div>";
  html += "<div class='sensor-ref'>Detection Zone</div></div>";

  html += "<div class='sensor-card " + String(s4 ? "sensor-active" : "") + "'>";
  html += "<div class='sensor-title'>IR Sensor 4<br>Zone D (D33)</div>";
  html += "<div class='sensor-status " + String(s4 ? "active-text" : "idle-text") + "'>" + String(s4 ? "ACTIVE" : "IDLE") + "</div>";
  html += "<div class='sensor-ref'>Detection Zone</div></div>";
  html += "</div>";

  html += "<div class='footer'>Smart City Street Light System | Municipal Corporation of India | Auto Refresh Every 2 Seconds</div>";
  html += "</div></body></html>";

  return html;
}
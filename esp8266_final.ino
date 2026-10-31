
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ================= WIFI ACCESS POINT SETTINGS =================

// ESP8266 creates its own Wi-Fi network. No router or phone hotspot needed.
const char* AP_SSID = "arduino";
const char* AP_PASSWORD = "jomapinakapogi"; // Minimum 8 characters

// Dashboard login (change these before use)
const char* WEB_USERNAME = "admin";
const char* WEB_PASSWORD = "sir pogi";

// ================= SERVER =================
  
ESP8266WebServer server(80);

// ================= ROBOT DATA =================

String robotStatus = "READY";
String remainingSeconds = "7200";
String vacuumStatus = "OFF";
String lastDistance = "400,400,400";
String alertMessage = "System ready";

String logs[20];
int logCount = 0;

bool manualMoving = false;
bool scheduleEnabled = false;

String scheduledTime = "08:00";
String lastScheduleDate = "";

unsigned long lastBrowserHeartbeat = 0;

const unsigned long MANUAL_TIMEOUT = 1500;

// ================= ACTIVITY LOG =================

void addLog(String message) {
  String entry = String(millis() / 1000) + "s - " + message;

  if (logCount < 20) {
    logs[logCount++] = entry;
  } else {
    for (int i = 0; i < 19; i++) {
      logs[i] = logs[i + 1];
    }

    logs[19] = entry;
  }
}

// ================= AUTHENTICATION =================

bool authorized() {
  if (!server.authenticate(WEB_USERNAME, WEB_PASSWORD)) {
    server.requestAuthentication();
    return false;
  }

  return true;
}

// ================= ARDUINO COMMUNICATION =================

void sendCommand(const String& command) {
  Serial.println("CMD:" + command);
}

void readArduinoMessages() {
  static String input = "";

  while (Serial.available()) {
    char incoming = Serial.read();

    if (incoming == '\n' || incoming == '\r') {
      input.trim();

      if (input.length() == 0) {
        input = "";
        continue;
      }

      if (input.startsWith("STATUS:")) {
        robotStatus = input.substring(7);
      }
      else if (input.startsWith("REMAINING:")) {
        remainingSeconds = input.substring(10);
      }
      else if (input.startsWith("VACUUM:")) {
        vacuumStatus = input.substring(7);
      }
      else if (input.startsWith("SENSOR:")) {
        lastDistance = input.substring(7);
      }
      else if (input.startsWith("ALERT:")) {
        alertMessage = input.substring(6);
        addLog("Alert: " + alertMessage);
      }

      input = "";
    }
    else if (input.length() < 100) {
      input += incoming;
    }
    else {
      input = "";
    }
  }
}

// ================= JSON HELPERS =================

String jsonEscape(String value) {
  value.replace("\\", "\\\\");
  value.replace("\"", "\\\"");
  value.replace("\n", "\\n");
  value.replace("\r", "");
  return value;
}

String buildStatusJSON() {
  String json = "{";

  json += "\"status\":\"" + jsonEscape(robotStatus) + "\",";
  json += "\"vacuum\":\"" + jsonEscape(vacuumStatus) + "\",";
  json += "\"remaining\":" + String(remainingSeconds.toInt()) + ",";
  json += "\"distance\":\"" + jsonEscape(lastDistance) + "\",";
  json += "\"alert\":\"" + jsonEscape(alertMessage) + "\",";
  json += "\"wifi\":\"HOTSPOT ACTIVE\",";

  json += "\"scheduleEnabled\":";
  json += scheduleEnabled ? "true" : "false";
  json += ",";

  json += "\"scheduleTime\":\"" + scheduledTime + "\"";

  json += "}";

  return json;
}

String buildLogsJSON() {
  String json = "[";

  for (int i = 0; i < logCount; i++) {
    if (i > 0) {
      json += ",";
    }

    json += "\"" + jsonEscape(logs[i]) + "\"";
  }

  json += "]";

  return json;
}

// ================= COMMAND ROUTES =================

void handleRobotCommand(String command, String description) {
  if (!authorized()) {
    return;
  }

  sendCommand(command);

  if (command == "FORWARD" ||
      command == "BACKWARD" ||
      command == "LEFT" ||
      command == "RIGHT") {
    manualMoving = true;
    lastBrowserHeartbeat = millis();
  }
  else {
    manualMoving = false;
  }

  addLog(description);

  server.send(200, "application/json", "{\"success\":true}");
}

void setupRoutes() {

  // Dashboard
 
server.on("/", HTTP_GET, []() {
  if (!authorized()) return;

  server.send_P(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>CleanBot Dashboard</title>

<style>
:root {
  --bg: #0b1220;
  --panel: #151f30;
  --panel2: #1c2a3d;
  --cyan: #36dce5;
  --blue: #3978ed;
  --green: #24c58b;
  --red: #ef5b69;
  --text: #f2f7fc;
  --muted: #92a5ba;
  --border: #29394e;
}

* {
  box-sizing: border-box;
}

body {
  margin: 0;
  background: var(--bg);
  color: var(--text);
  font-family: Arial, sans-serif;
}

.layout {
  display: flex;
  min-height: 100vh;
}

.sidebar {
  width: 230px;
  background: #111b2a;
  border-right: 1px solid var(--border);
  padding: 25px 18px;
  flex-shrink: 0;
}

.brand {
  display: flex;
  align-items: center;
  gap: 12px;
  margin-bottom: 40px;
}

.brand-icon {
  width: 44px;
  height: 44px;
  border-radius: 13px;
  background: #153b4c;
  color: var(--cyan);
  display: grid;
  place-items: center;
  font-size: 23px;
}

.brand h2 {
  margin: 0;
  font-size: 18px;
}

.brand small {
  color: var(--muted);
  font-size: 10px;
  letter-spacing: 1px;
}

.nav-label {
  color: var(--muted);
  font-size: 11px;
  margin: 25px 10px 10px;
  letter-spacing: 1px;
}

.nav-item {
  padding: 13px;
  border-radius: 9px;
  margin-bottom: 7px;
  color: var(--muted);
  font-size: 14px;
}

.nav-item.active {
  background: #20364d;
  color: var(--cyan);
}

.main {
  flex: 1;
  padding: 30px;
  min-width: 0;
}

.topbar {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 15px;
  margin-bottom: 25px;
}

.topbar h1 {
  margin: 0;
  font-size: 25px;
}

.subtitle {
  color: var(--muted);
  font-size: 13px;
  margin-top: 7px;
}

.online {
  background: #123d34;
  color: #53e2b1;
  padding: 9px 14px;
  border-radius: 30px;
  font-size: 12px;
  white-space: nowrap;
}

.grid {
  display: grid;
  grid-template-columns: repeat(4, minmax(0, 1fr));
  gap: 16px;
  margin-bottom: 18px;
}

.card {
  background: var(--panel);
  border: 1px solid var(--border);
  border-radius: 15px;
  padding: 20px;
  min-width: 0;
}

.metric-label {
  color: var(--muted);
  font-size: 12px;
}

.metric-value {
  font-size: 25px;
  font-weight: bold;
  margin-top: 12px;
}

.metric-icon {
  font-size: 22px;
  margin-bottom: 15px;
}

.cyan { color: var(--cyan); }
.green { color: var(--green); }
.red { color: var(--red); }

.section-title {
  font-size: 16px;
  font-weight: bold;
  margin: 0 0 18px;
}

.content-grid {
  display: grid;
  grid-template-columns: 1.2fr 1fr;
  gap: 18px;
  margin-bottom: 18px;
}

.control-pad {
  max-width: 290px;
  margin: 10px auto;
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 9px;
}

.control-btn {
  border: 0;
  min-height: 65px;
  border-radius: 12px;
  background: #253b55;
  color: white;
  font-size: 22px;
  cursor: pointer;
  touch-action: none;
  user-select: none;
}

.control-btn:active {
  background: var(--blue);
}

.stop-btn {
  background: #7f2936;
  font-size: 13px;
  font-weight: bold;
}

.control-note {
  text-align: center;
  color: var(--muted);
  font-size: 11px;
  margin-top: 15px;
}

.action-btn {
  width: 100%;
  border: 0;
  padding: 13px;
  border-radius: 9px;
  margin-top: 9px;
  font-weight: bold;
  cursor: pointer;
  color: white;
}

.blue-btn { background: var(--blue); }
.green-btn { background: #128363; }
.red-btn { background: #a83143; }
.purple-btn { background: #6545b8; }

.sensor-grid {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 10px;
}

.sensor-box {
  background: var(--panel2);
  border-radius: 11px;
  padding: 16px 5px;
  text-align: center;
}

.sensor-box small {
  color: var(--muted);
  font-size: 10px;
}

.sensor-number {
  font-size: 23px;
  color: var(--cyan);
  font-weight: bold;
  margin: 12px 0 4px;
}

.schedule-row {
  display: flex;
  gap: 9px;
  flex-wrap: wrap;
}

input[type=time] {
  flex: 1;
  min-width: 130px;
  padding: 12px;
  background: var(--panel2);
  color: white;
  border: 1px solid var(--border);
  border-radius: 8px;
}

.logs {
  max-height: 230px;
  overflow-y: auto;
}

.log-item {
  padding: 11px 0;
  border-bottom: 1px solid var(--border);
  color: #bdcbd9;
  font-size: 12px;
  overflow-wrap: anywhere;
}

.alert-box {
  background: #2b2830;
  border: 1px solid #66513a;
  border-radius: 10px;
  padding: 13px;
  color: #f4c77e;
  font-size: 13px;
}

.footer {
  text-align: center;
  color: #64768b;
  font-size: 11px;
  padding: 20px;
}

@media(max-width: 1000px) {
  .grid {
    grid-template-columns: repeat(2, 1fr);
  }

  .content-grid {
    grid-template-columns: 1fr;
  }
}

@media(max-width: 650px) {
  .layout {
    display: block;
  }

  .sidebar {
    width: 100%;
    padding: 14px 18px;
    border-right: 0;
    border-bottom: 1px solid var(--border);
  }

  .brand {
    margin-bottom: 0;
  }

  .nav-label, .nav-item {
    display: none;
  }

  .main {
    padding: 18px 13px;
  }

  .topbar h1 {
    font-size: 21px;
  }

  .grid {
    gap: 9px;
  }

  .card {
    padding: 15px;
  }

  .metric-value {
    font-size: 20px;
  }

  .online {
    font-size: 10px;
    padding: 8px;
  }
}
</style>
</head>

<body>

<div class="layout">

<aside class="sidebar">

  <div class="brand">
    <div class="brand-icon">🤖</div>
    <div>
      <h2>CLEANBOT</h2>
      <small>SMART ROBOT SYSTEM</small>
    </div>
  </div>

  <div class="nav-label">MAIN MENU</div>
  <div class="nav-item active">▦ Dashboard</div>
  <div class="nav-item">◉ Robot Control</div>
  <div class="nav-item">◷ Cleaning Schedule</div>
  <div class="nav-item">▤ Activity Logs</div>

  <div class="nav-label">SYSTEM</div>
  <div class="nav-item">⚙ Device Status</div>

</aside>

<main class="main">

  <div class="topbar">
    <div>
      <h1>Dashboard Overview</h1>
      <div class="subtitle">
        Monitor and control your floor cleaning robot.
      </div>
    </div>

    <div class="online" id="wifi">● CONNECTING</div>
  </div>

  <!-- METRIC CARDS -->

  <div class="grid">

    <div class="card">
      <div class="metric-icon cyan">◉</div>
      <div class="metric-label">ROBOT STATUS</div>
      <div class="metric-value cyan" id="status">READY</div>
    </div>

    <div class="card">
      <div class="metric-icon green">◈</div>
      <div class="metric-label">VACUUM MOTOR</div>
      <div class="metric-value" id="vacuum">OFF</div>
    </div>

    <div class="card">
      <div class="metric-icon cyan">◷</div>
      <div class="metric-label">RUNTIME REMAINING</div>
      <div class="metric-value" id="remaining">2h 00m</div>
    </div>

    <div class="card">
      <div class="metric-icon cyan">⌁</div>
      <div class="metric-label">CLEANING MODE</div>
      <div class="metric-value" id="mode">STANDBY</div>
    </div>

  </div>

  <!-- CONTROL SECTION -->

  <div class="content-grid">

    <div class="card">

      <h3 class="section-title">Manual Robot Control</h3>

      <div class="control-pad">

        <div></div>

        <button class="control-btn"
          onpointerdown="startMove(event,'/forward')"
          onpointerup="stopMove()"
          onpointerleave="stopMove()"
          onpointercancel="stopMove()">▲</button>

        <div></div>

        <button class="control-btn"
          onpointerdown="startMove(event,'/left')"
          onpointerup="stopMove()"
          onpointerleave="stopMove()"
          onpointercancel="stopMove()">◀</button>

        <button class="control-btn stop-btn"
          onclick="stopMove()">STOP</button>

        <button class="control-btn"
          onpointerdown="startMove(event,'/right')"
          onpointerup="stopMove()"
          onpointerleave="stopMove()"
          onpointercancel="stopMove()">▶</button>

        <div></div>

        <button class="control-btn"
          onpointerdown="startMove(event,'/backward')"
          onpointerup="stopMove()"
          onpointerleave="stopMove()"
          onpointercancel="stopMove()">▼</button>

        <div></div>

      </div>

      <div class="control-note">
        Hold a direction to move. Release to stop.
      </div>

    </div>

    <div class="card">

      <h3 class="section-title">Cleaning Operations</h3>

      <div class="metric-label">AUTOMATIC NAVIGATION</div>

      <button class="action-btn purple-btn"
        onclick="sendAction('/auto_on')">
        ▶ START AUTOMATIC CLEANING
      </button>

      <button class="action-btn red-btn"
        onclick="sendAction('/auto_off')">
        ■ STOP AUTOMATIC CLEANING
      </button>

      <br><br>

      <div class="metric-label">VACUUM MOTOR CONTROL</div>

      <button class="action-btn green-btn"
        onclick="sendAction('/vacuum_on')">
        VACUUM ON
      </button>

      <button class="action-btn red-btn"
        onclick="sendAction('/vacuum_off')">
        VACUUM OFF
      </button>

    </div>

  </div>

  <!-- SENSOR SECTION -->

  <div class="card" style="margin-bottom:18px">

    <h3 class="section-title">Ultrasonic Sensor Monitoring</h3>

    <div class="sensor-grid">

      <div class="sensor-box">
        <small>LEFT SENSOR</small>
        <div class="sensor-number" id="leftSensor">--</div>
        <small>CM</small>
      </div>

      <div class="sensor-box">
        <small>FRONT SENSOR</small>
        <div class="sensor-number" id="frontSensor">--</div>
        <small>CM</small>
      </div>

      <div class="sensor-box">
        <small>RIGHT SENSOR</small>
        <div class="sensor-number" id="rightSensor">--</div>
        <small>CM</small>
      </div>

    </div>

  </div>

  <!-- SCHEDULE -->

  <div class="content-grid">

    <div class="card">

      <h3 class="section-title">Daily Cleaning Schedule</h3>

      <div class="metric-label">SET AUTOMATIC START TIME</div>

      <div class="schedule-row" style="margin-top:12px">

        <input type="time" id="scheduleTime" value="08:00">

        <button class="action-btn blue-btn"
          style="width:auto;margin:0"
          onclick="saveSchedule()">
          SAVE
        </button>

      </div>

      <button class="action-btn red-btn"
        onclick="disableSchedule()">
        DISABLE SCHEDULE
      </button>

      <p class="subtitle" id="scheduleStatus">
        Schedule disabled
      </p>

    </div>

    <div class="card">

      <h3 class="section-title">System Alert</h3>

      <div class="alert-box" id="alert">
        System ready. No current alerts.
      </div>

      <br>

      <div class="metric-label">
        Wi-Fi connection and robot notifications appear here.
      </div>

    </div>

  </div>

  <!-- ACTIVITY LOG -->

  <div class="card">

    <h3 class="section-title">Recent Activity</h3>

    <div class="logs" id="logs">
      Loading activity...
    </div>

  </div>

  <div class="footer">
    CLEANBOT SMART FLOOR CLEANING ROBOT · ESP8266 CONTROL SYSTEM
  </div>

</main>
</div>

<script>

let activeMove = false;
let heartbeatTimer = null;

async function request(url) {
  try {
    return await fetch(url, {cache: 'no-store'});
  } catch (error) {
    console.error(error);
    return null;
  }
}

async function sendAction(url) {
  await request(url);
  updateStatus();
}

async function startMove(event, url) {

  event.preventDefault();

  if (activeMove) return;

  activeMove = true;

  try {
    event.currentTarget.setPointerCapture(event.pointerId);
  } catch (e) {}

  await request(url);

  heartbeatTimer = setInterval(() => {
    if (activeMove) {
      request('/heartbeat');
    }
  }, 500);
}

async function stopMove() {

  if (!activeMove) return;

  activeMove = false;

  if (heartbeatTimer) {
    clearInterval(heartbeatTimer);
    heartbeatTimer = null;
  }

  await request('/stop');
}

async function updateStatus() {

  const response = await request('/status');

  if (!response || !response.ok) return;

  try {

    const data = await response.json();

    document.getElementById('status').innerText =
      data.status;

    document.getElementById('vacuum').innerText =
      data.vacuum;

    document.getElementById('wifi').innerText =
      data.wifi === 'HOTSPOT ACTIVE'
        ? '● HOTSPOT ACTIVE'
        : '● OFFLINE';

    document.getElementById('mode').innerText =
      data.status === 'AUTO' ? 'AUTO' :
      data.status === 'MANUAL' ? 'MANUAL' :
      data.status === 'TIME_LIMIT' ? 'LIMIT REACHED' :
      'STANDBY';

    const seconds = Number(data.remaining) || 0;

    const hours = Math.floor(seconds / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);

    document.getElementById('remaining').innerText =
      hours + 'h ' + minutes + 'm';

    const distances =
      (data.distance || '400,400,400').split(',');

    document.getElementById('leftSensor').innerText =
      (distances[0] || '--').trim();

    document.getElementById('frontSensor').innerText =
      (distances[1] || '--').trim();

    document.getElementById('rightSensor').innerText =
      (distances[2] || '--').trim();

    document.getElementById('alert').innerText =
      data.alert || 'System ready';

    document.getElementById('scheduleStatus').innerText =
      data.scheduleEnabled
        ? 'Daily schedule enabled: ' + data.scheduleTime
        : 'Schedule disabled';

    if (document.activeElement !==
        document.getElementById('scheduleTime')) {
      document.getElementById('scheduleTime').value =
        data.scheduleTime;
    }

  } catch (error) {
    console.error(error);
  }
}

async function updateLogs() {

  const response = await request('/logs');

  if (!response || !response.ok) return;

  try {

    const entries = await response.json();

    const container = document.getElementById('logs');

    container.innerHTML = '';

    if (!entries.length) {
      container.textContent = 'No activity recorded yet.';
      return;
    }

    entries.slice().reverse().forEach(entry => {

      const item = document.createElement('div');

      item.className = 'log-item';
      item.textContent = '• ' + entry;

      container.appendChild(item);

    });

  } catch (error) {
    console.error(error);
  }
}

async function saveSchedule() {

  const time =
    document.getElementById('scheduleTime').value;

  if (!time) {
    alert('Please select a schedule time.');
    return;
  }

  const response = await request('/schedule?time=' +
    encodeURIComponent(time));

  if (response && response.status === 503) {
    alert('Daily scheduling in hotspot-only mode requires a DS3231 RTC module.');
  }

  updateStatus();
}

async function disableSchedule() {

  await request('/schedule_off');

  updateStatus();
}

document.addEventListener('visibilitychange', () => {
  if (document.hidden) stopMove();
});

window.addEventListener('blur', stopMove);

window.addEventListener('pointerup', stopMove);

window.addEventListener('pointercancel', stopMove);

setInterval(updateStatus, 1500);
setInterval(updateLogs, 3000);

updateStatus();
updateLogs();

</script>

</body>
</html>
)rawliteral");
});


  // Movement
  server.on("/forward", HTTP_GET, []() {
    handleRobotCommand("FORWARD", "Moving forward");
  });

  server.on("/backward", HTTP_GET, []() {
    handleRobotCommand("BACKWARD", "Moving backward");
  });

  server.on("/left", HTTP_GET, []() {
    handleRobotCommand("LEFT", "Turning left");
  });

  server.on("/right", HTTP_GET, []() {
    handleRobotCommand("RIGHT", "Turning right");
  });

  server.on("/stop", HTTP_GET, []() {
    handleRobotCommand("STOP", "Robot stopped");
  });

  // Vacuum
  server.on("/vacuum_on", HTTP_GET, []() {
    handleRobotCommand("VACUUM_ON", "Vacuum turned ON");
  });

  server.on("/vacuum_off", HTTP_GET, []() {
    handleRobotCommand("VACUUM_OFF", "Vacuum turned OFF");
  });

  // Automatic cleaning
  server.on("/auto_on", HTTP_GET, []() {
    handleRobotCommand("AUTO_ON", "Automatic cleaning started");
  });

  server.on("/auto_off", HTTP_GET, []() {
    handleRobotCommand("AUTO_OFF", "Automatic cleaning stopped");
  });

  // Manual movement heartbeat
  server.on("/heartbeat", HTTP_GET, []() {
    if (!authorized()) return;

    lastBrowserHeartbeat = millis();

    if (manualMoving) {
      sendCommand("HB");
    }

    server.send(200, "application/json", "{\"success\":true}");
  });

  // Status
  server.on("/status", HTTP_GET, []() {
    if (!authorized()) return;

    server.send(200, "application/json", buildStatusJSON());
  });

  // Logs
  server.on("/logs", HTTP_GET, []() {
    if (!authorized()) return;

    server.send(200, "application/json", buildLogsJSON());
  });

  // Daily scheduling is unavailable in standalone hotspot mode.
  // Add a DS3231 RTC module to support schedules without internet.
  server.on("/schedule", HTTP_GET, []() {
    if (!authorized()) return;

    scheduleEnabled = false;
    server.send(503, "application/json",
      "{\"error\":\"Daily scheduling requires a DS3231 RTC module in offline mode.\"}");
  });

  server.on("/schedule_off", HTTP_GET, []() {
    if (!authorized()) return;

    scheduleEnabled = false;
    addLog("Schedule disabled");
    server.send(200, "application/json", "{\"success\":true}");
  });

  server.onNotFound([]() {
    if (!authorized()) return;

    server.send(404, "text/plain", "Page not found");
  });
}

// ================= ESP8266 HOTSPOT =================

void startHotspot() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  IPAddress localIP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);

  WiFi.softAPConfig(localIP, gateway, subnet);

  // Channel 1, visible network, maximum ONE connected Wi-Fi station.
  bool started = WiFi.softAP(AP_SSID, AP_PASSWORD, 1, false, 1);

  if (started) {
    alertMessage = "CLEANBOT hotspot active";
    addLog("Hotspot started: CLEANBOT_ROBOT");
    addLog("Dashboard address: http://192.168.4.1");
  } else {
    alertMessage = "Hotspot startup failed";
    addLog("ERROR: hotspot startup failed");
  }
}

// ================= SETUP =================

void setup() {
  // UART is shared with the Arduino Uno. Do not print debug text here.
  Serial.begin(9600);
  Serial.setTimeout(20);
  delay(500);

  setupRoutes();
  startHotspot();
  server.begin();

  addLog("Robot web server initialized");
}

// ================= MAIN LOOP =================

void loop() {
  // Handle dashboard requests from the one connected phone/device.
  server.handleClient();

  // Keep reading telemetry from the Arduino Uno.
  readArduinoMessages();

  // Safety timeout: stop manual movement if browser heartbeat is lost.
  if (manualMoving &&
      millis() - lastBrowserHeartbeat > MANUAL_TIMEOUT) {

    manualMoving = false;
    sendCommand("STOP");
    addLog("Manual movement stopped: heartbeat timeout");
  }

  yield();
}

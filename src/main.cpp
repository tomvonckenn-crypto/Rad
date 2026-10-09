#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include "motor_control.h"
#include "control_page.h"

#if __has_include("wifi_secrets.h")
#include "wifi_secrets.h"
#else
#define RAD_WIFI_SSID ""
#define RAD_WIFI_PASSWORD ""
#endif

constexpr uint8_t RPWM_PIN = 4; // D2
constexpr uint8_t LPWM_PIN = 5; // D1
constexpr char HOSTNAME[] = "rad-wemos";
constexpr char VERSION[] = "0.2.0-handmatig";
ESP8266WebServer server(80);
rad::MotorControl motor;
bool uploadBusy = false;
bool uploadFailed = false;
uint32_t sessionToken = 0;
uint32_t safetyEpoch = 0;
int lastRpwm = -1;
int lastLpwm = -1;

void applyOutputs() {
  const int rpwm = motor.direction() == rad::Direction::Right ? motor.current() : 0;
  const int lpwm = motor.direction() == rad::Direction::Left ? motor.current() : 0;
  if (rpwm == lastRpwm && lpwm == lastLpwm) return;
  // Clear the inactive side before setting PWM on the active side.
  if (rpwm > 0) { analogWrite(LPWM_PIN, 0); analogWrite(RPWM_PIN, rpwm); }
  else if (lpwm > 0) { analogWrite(RPWM_PIN, 0); analogWrite(LPWM_PIN, lpwm); }
  else { analogWrite(RPWM_PIN, 0); analogWrite(LPWM_PIN, 0); }
  lastRpwm = rpwm;
  lastLpwm = lpwm;
}

void updateMotor() {
  motor.tick(millis());
  if (sessionToken != 0 && !motor.leaseActive()) { sessionToken = 0; ++safetyEpoch; }
  applyOutputs();
}

void tripMotor(rad::Reason reason, bool permanent = false) {
  motor.trip(reason, permanent);
  sessionToken = 0;
  ++safetyEpoch; // Delayed arm requests from before a stop are invalid now.
  applyOutputs();
}

const char* stateName() {
  switch (motor.state()) {
    case rad::State::Locked: return "locked";
    case rad::State::Ready: return "ready";
    case rad::State::Running: return "running";
    case rad::State::Stopping: return "stopping";
    case rad::State::AwaitStandstill: return "await_standstill";
    case rad::State::Fault: return "fault";
  }
  return "fault";
}
const char* reasonName() {
  switch (motor.reason()) {
    case rad::Reason::Boot: return "boot";
    case rad::Reason::None: return "none";
    case rad::Reason::Emergency: return "emergency";
    case rad::Reason::ConnectionLost: return "connection_lost";
    case rad::Reason::Stopped: return "stopped";
    case rad::Reason::Ota: return "ota";
    case rad::Reason::OtaError: return "ota_error";
  }
  return "ota_error";
}
String statusJson(bool includeSession = false) {
  const bool connected = WiFi.status() == WL_CONNECTED;
  String json;
  json.reserve(620);
  json = F("{\"device\":\"rad-wemos\",\"version\":\""); json += VERSION;
  json += F("\",\"build\":\""); json += __DATE__ " " __TIME__;
  json += F("\",\"station_connected\":"); json += connected ? "true" : "false";
  json += F(",\"station_ip\":\""); json += connected ? WiFi.localIP().toString() : String("");
  json += F("\",\"ap_ip\":\""); json += WiFi.softAPIP().toString();
  json += F("\",\"ota_busy\":"); json += uploadBusy ? "true" : "false";
  json += F(",\"ota_failed\":"); json += uploadFailed ? "true" : "false";
  json += F(",\"state\":\""); json += stateName();
  json += F("\",\"stop_reason\":\""); json += reasonName();
  json += F("\",\"direction\":\"");
  json += motor.direction() == rad::Direction::Left ? "left" : motor.direction() == rad::Direction::Right ? "right" : "none";
  json += F("\",\"control_armed\":"); json += motor.leaseActive() ? "true" : "false";
  json += F(",\"motor_enabled\":");
  json += motor.state() == rad::State::Running || motor.state() == rad::State::Stopping ? "true" : "false";
  json += F(",\"target_pwm\":"); json += motor.target();
  json += F(",\"actual_pwm\":"); json += motor.current();
  json += F(",\"rpwm\":"); json += lastRpwm;
  json += F(",\"lpwm\":"); json += lastLpwm;
  json += F(",\"max_pwm\":"); json += rad::MotorControl::MaxPwm;
  json += F(",\"safety_epoch\":"); json += safetyEpoch;
  if (includeSession) { json += F(",\"session_token\":"); json += sessionToken; }
  json += F(",\"uptime_ms\":"); json += millis(); json += '}';
  return json;
}
void sendStatus(bool includeSession = false) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", statusJson(includeSession));
}
void sendError(const char* message, int code = 409) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", String(F("{\"error\":\"")) + message + "\"}");
}
bool unsignedArg(const char* name, uint32_t& value) {
  if (!server.hasArg(name)) return false;
  const String input = server.arg(name);
  if (input.length() == 0 || input.length() > 10) return false;
  uint64_t parsed = 0;
  for (size_t i = 0; i < input.length(); ++i) {
    if (input[i] < '0' || input[i] > '9') return false;
    parsed = parsed * 10 + static_cast<unsigned>(input[i] - '0');
    if (parsed > UINT32_MAX) return false;
  }
  value = static_cast<uint32_t>(parsed);
  return true;
}
bool validSession() {
  updateMotor();
  uint32_t supplied = 0;
  if (!motor.leaseActive() || sessionToken == 0 || !unsignedArg("token", supplied) || supplied != sessionToken) {
    sendError("Bediening vergrendeld of sessie verlopen. Bevestig fysieke stilstand en geef opnieuw vrij.", 403);
    return false;
  }
  return true;
}
void controlRequest() {
  updateMotor();
  const String action = server.arg("action");
  if (action == "arm") {
    uint32_t epoch = 0;
    if (server.arg("confirm") != "stil" || !unsignedArg("epoch", epoch) || epoch != safetyEpoch) {
      sendError("Nieuwe bevestiging van fysieke stilstand nodig."); return;
    }
    if (!motor.arm(millis())) {
      sendError("Eerst stoppen en wachten op fysieke stilstand. Bij een uploadfout is herstart nodig."); return;
    }
    sessionToken = ESP.random();
    if (sessionToken == 0) sessionToken = 1;
    applyOutputs(); sendStatus(true); return;
  }
  if (!validSession()) return;
  bool accepted = false;
  if (action == "start") {
    const String direction = server.arg("direction");
    const auto selected = direction == "left" ? rad::Direction::Left : direction == "right" ? rad::Direction::Right : rad::Direction::None;
    accepted = motor.start(selected, millis());
  } else if (action == "step") {
    const String delta = server.arg("delta");
    accepted = motor.step(delta == "1" ? 1 : delta == "-1" ? -1 : 0);
  } else if (action == "set_pwm") {
    uint32_t value = 0;
    accepted = unsignedArg("value", value) && value <= rad::MotorControl::MaxPwm && motor.setTarget(static_cast<uint16_t>(value));
  } else if (action == "slow_stop") {
    accepted = motor.slowStop(millis());
  }
  updateMotor();
  if (!accepted) { sendError("Commando geweigerd. Controleer de toestand, richting en PWM-grens."); return; }
  sendStatus();
}

void setup() {
  digitalWrite(RPWM_PIN, LOW); digitalWrite(LPWM_PIN, LOW);
  pinMode(RPWM_PIN, OUTPUT); pinMode(LPWM_PIN, OUTPUT);
  analogWriteRange(1023); analogWriteFreq(1000);
  applyOutputs();
  Serial.begin(115200);
  safetyEpoch = ESP.random();
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.hostname(HOSTNAME);
  WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
  WiFi.softAP("Rad-Wemos");
  WiFi.setAutoReconnect(true);
  if (RAD_WIFI_SSID[0] != '\0') WiFi.begin(RAD_WIFI_SSID, RAD_WIFI_PASSWORD);
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.onStart([]() { uploadBusy = true; tripMotor(rad::Reason::Ota); });
  ArduinoOTA.onEnd([]() { tripMotor(rad::Reason::Ota); });
  ArduinoOTA.onError([](ota_error_t error) {
    uploadFailed = true; uploadBusy = false;
    tripMotor(rad::Reason::OtaError, true);
    Serial.printf("OTA error: %u\n", static_cast<unsigned>(error));
  });
  ArduinoOTA.begin();
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html; charset=utf-8", CONTROL_PAGE);
  });
  server.on("/api/status", HTTP_GET, []() { updateMotor(); sendStatus(); });
  server.on("/api/stop", HTTP_POST, []() { tripMotor(rad::Reason::Emergency); sendStatus(); });
  server.on("/api/control", HTTP_POST, controlRequest);
  server.on("/api/heartbeat", HTTP_POST, []() {
    if (!validSession()) return;
    if (!motor.heartbeat(millis())) { updateMotor(); sendError("Bedieningssessie verlopen.", 403); return; }
    sendStatus();
  });
  server.onNotFound([]() { sendError("Onbekend adres of onjuiste HTTP-methode.", 404); });
  server.begin();
  Serial.printf("%s; motor locked; AP: %s\n", VERSION, WiFi.softAPIP().toString().c_str());
}

void loop() {
  updateMotor();
  ArduinoOTA.handle();
  if (!uploadBusy) { updateMotor(); server.handleClient(); }
  updateMotor();
  MDNS.update();
  yield();
}

#include <Settings.h>
#include <FileSystems.h>
#include <SD.h>
#include <Functions.h>

void SettingsClass::load() {
  File json = ConfigFS.open("/settings.json");
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, json);

  json.close();

  if (error == DeserializationError::Ok) {
    version = doc["version"] | version;

    // Version in `settings.json` is older so check for upgrade paths and save any update changes
    if (version < T3_VERSION) {
      upgrade(doc);
      version = T3_VERSION;
      save();
    }

    rotation = doc["rotation"] | rotation;
    storageMode = doc["storageMode"] | storageMode;
    theme = doc["theme"] | theme;
    brightness = doc["brightness"] | brightness;
    autoDim = doc["autoDim"] | autoDim;
    emergencyStopDelay = doc["emergencyStopDelay"] | emergencyStopDelay;
    AP.load(doc["ap"]);
    CS.load(doc["cs"]);
    LocoUI.load(doc["locoui"]);

    if (doc.containsKey("touchCal")) {
      JsonObject cal = doc["touchCal"];
      TouchCal.xMin = cal["xMin"] | TouchCal.xMin;
      TouchCal.xMax = cal["xMax"] | TouchCal.xMax;
      TouchCal.yMin = cal["yMin"] | TouchCal.yMin;
      TouchCal.yMax = cal["yMax"] | TouchCal.yMax;
    }

    recentLocoCount = 0;
    if (doc.containsKey("recentLocos")) {
      for (JsonVariant v : doc["recentLocos"].as<JsonArray>()) {
        if (recentLocoCount >= RECENT_LOCOS_MAX) break;
        uint16_t a = v.as<uint16_t>();
        if (a != 0) recentLocos[recentLocoCount++] = a;
      }
    }
  } else {
    init();
  }
}

void SettingsClass::save() {
  StaticJsonDocument<1024> doc;

  doc["version"] = version;
  doc["rotation"] = rotation;
  doc["storageMode"] = storageMode;
  doc["theme"] = theme;
  doc["brightness"] = brightness;
  doc["autoDim"] = autoDim;
  doc["emergencyStopDelay"] = emergencyStopDelay;
  AP.save(doc["ap"] | doc.createNestedObject("ap"));
  CS.save(doc["cs"] | doc.createNestedObject("cs"));
  LocoUI.save(doc["locoui"] | doc.createNestedObject("locoui"));

  JsonObject cal = doc.createNestedObject("touchCal");
  cal["xMin"] = TouchCal.xMin;
  cal["xMax"] = TouchCal.xMax;
  cal["yMin"] = TouchCal.yMin;
  cal["yMax"] = TouchCal.yMax;

  JsonArray recents = doc.createNestedArray("recentLocos");
  for (uint8_t i = 0; i < recentLocoCount; i++) recents.add(recentLocos[i]);

  File json = ConfigFS.open("/settings.json", FILE_WRITE);
  serializeJson(doc, json);
  json.close();
  Serial.printf("[Settings] Saved\n");
}

void SettingsClass::init() {
  version = T3_VERSION;
  
  if (SD.cardType() != CARD_NONE) {
    storageMode = StorageMode::SD_CARD;
  } else {
    storageMode = StorageMode::LITTLEFS;
  }

  // Generate ap ssid and password
  char buf[9] = { 0 };
  randomChars(buf, 4);
  AP.SSID = "DCC-EX-CYD-";
  AP.SSID += buf;

  randomChars(buf, 8);
  AP.password = buf;

  save();
}

void SettingsClass::upgrade(JsonDocument& doc) {
  (void)doc;
}

void SettingsClass::AP::load(const JsonObject& obj) {
  SSID = obj["ssid"].as<const char*>();
  password = obj["password"].as<const char*>();
  enabled = obj["enabled"] | false;
}

void SettingsClass::AP::save(const JsonObject& obj) {
  obj["ssid"] = SSID;
  obj["password"] = password;
  obj["enabled"] = enabled;
}

bool SettingsClass::CS::valid() {
  return !_ssid.isEmpty() && !_server.isEmpty() && _port != 0;
}

void SettingsClass::CS::load(const JsonObject& obj) {
  SSID(obj["ssid"]);
  password(obj["password"]);
  server(obj["server"]);
  port(obj["port"]);
}

void SettingsClass::CS::save(const JsonObject & obj) {
  obj["ssid"] = SSID();
  obj["password"] = password();
  obj["server"] = server();
  obj["port"] = port();
}

String SettingsClass::CS::SSID() const {
  return _ssid;
}

bool SettingsClass::CS::SSID(const String& value) {
  if (value.length() > 32) {
    return false; 
  }
  _ssid = value;
  return true;
}

String SettingsClass::CS::password() const {
  return _password;
}

bool SettingsClass::CS::password(const String& value) {
  if (value.length() > 63) {
    return false;
  }
  _password = value;
  return true;
}

String SettingsClass::CS::server() const {
  return _server;
}

bool SettingsClass::CS::server(const String& value) {
  if (value.length() > 32) {
    return false; 
  }
  _server = value;
  return true;
}

uint16_t SettingsClass::CS::port() const {
  return _port;
}

void SettingsClass::CS::port(uint16_t value) {
  _port = value;
}

void SettingsClass::LocoUI::load(const JsonObject& obj) {
  speedStep    = obj["step"]  | speedStep;
  acceleration = obj["accel"] | acceleration;
}

void SettingsClass::LocoUI::save(const JsonObject& obj) {
  obj["step"]  = speedStep;
  obj["accel"] = acceleration;
}

void SettingsClass::pushRecentLoco(uint16_t addr) {
  if (addr == 0) return;

  // Already at the front — nothing to do (avoids a needless flash write).
  if (recentLocoCount > 0 && recentLocos[0] == addr) return;

  // Drop any existing occurrence, preserving order of the rest.
  uint8_t w = 0;
  for (uint8_t i = 0; i < recentLocoCount; i++) {
    if (recentLocos[i] != addr) recentLocos[w++] = recentLocos[i];
  }
  recentLocoCount = w;

  // Shift down (capped) and insert at the front.
  uint8_t last = (recentLocoCount < RECENT_LOCOS_MAX) ? recentLocoCount : (RECENT_LOCOS_MAX - 1);
  for (uint8_t i = last; i > 0; i--) recentLocos[i] = recentLocos[i - 1];
  recentLocos[0] = addr;
  if (recentLocoCount < RECENT_LOCOS_MAX) recentLocoCount++;

  save();
}

SettingsClass Settings;

fs::FS& SettingsClass::getFS() const {
  if (storageMode == StorageMode::SD_CARD && SD.cardType() != CARD_NONE) {
    return (fs::FS&)SD;
  }
  return (fs::FS&)ConfigFS;
}

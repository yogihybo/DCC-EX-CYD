#include <ThrottleServer.h>
#include <AsyncTCP.h>
#include <AsyncJson.h>
#include <FileSystems.h>
#include <SD.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <StreamUtils.h>
#include <Settings.h>
#include <Version.h>
#include "ui/LVGL_Layouts.h"
#include "ui/SettingsUI.h"

extern volatile bool csIsConnected;

class WebLoggerHandler : public AsyncWebHandler {
public:
  WebLoggerHandler() {}
  virtual ~WebLoggerHandler() {}

  bool canHandle(AsyncWebServerRequest *request) const override {
    Serial.printf("[Web] Incoming: %s %s (Free Heap: %u)\n", request->methodToString(), request->url().c_str(), ESP.getFreeHeap());
    return false;
  }

  void handleRequest(AsyncWebServerRequest *request) override {}
};

ThrottleServer::ThrottleServer() : AsyncWebServer(80) { }

class ThrottleAPIHandler : public AsyncWebHandler {
public:
  ThrottleAPIHandler() {}
  virtual ~ThrottleAPIHandler() {}

  bool canHandle(AsyncWebServerRequest *request) const override {
    const String& raw = request->url();
    String stripped;
    const String& url = raw.startsWith("/$/") ? (stripped = raw.substring(2), stripped) : raw;

    if (request->method() == HTTP_GET) {
      if (url == "/locos" || url == "/fns" || url == "/icons" || url == "/consists") return true;
      if (url.endsWith(".json") || url.endsWith(".bmp")) {
        if (url.startsWith("/locos/") || url.startsWith("/fns/") || url.startsWith("/icons/") || url.startsWith("/consists/") || url == "/groups.json" || url == "/groups.bmp") return true;
      }
    } else if (request->method() == HTTP_HEAD) {
      if (url.startsWith("/locos/") && url.endsWith(".json")) return true;
      if (url.startsWith("/consists/") && url.endsWith(".json")) return true;
    } else if (request->method() == HTTP_DELETE) {
      if (url.endsWith(".json") || url.endsWith(".bmp")) {
        if (url.startsWith("/locos/") || url.startsWith("/fns/") || url.startsWith("/icons/") || url.startsWith("/consists/")) return true;
      }
    } else if (request->method() == HTTP_PUT) {
      if (url.endsWith(".json") || url.endsWith(".bmp")) {
        if (url.startsWith("/locos/") || url.startsWith("/fns/") || url.startsWith("/icons/") || url.startsWith("/consists/") || url == "/groups.json" || url == "/groups.bmp") return true;
      }
    }
    return false;
  }

  void handleRequest(AsyncWebServerRequest *request) override {
    const String& raw = request->url();
    Serial.printf("[Web] Request: %s %s\n", request->methodToString(), raw.c_str());
    String stripped;
    const String& url = raw.startsWith("/$/") ? (stripped = raw.substring(2), stripped) : raw;

    if (request->method() == HTTP_GET) {
      if (url == "/locos" || url == "/fns" || url == "/icons" || url == "/consists") {
        AsyncJsonResponse* response = new AsyncJsonResponse(true, 1024);
        JsonVariant& list = response->getRoot();

        auto listDir = [](File dir, auto cb) {
          while (File file = dir.openNextFile()) {
            if (!file.isDirectory()) {
              cb(file);
            }
            file.close();
            yield();
          }
          dir.close();
        };

        if (url == "/icons") {
          if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
            auto addPrefixed = [&list](File file) {
              char buf[64];
              snprintf(buf, sizeof(buf), "/$%s", file.path());
              list.add(buf);
            };
            listDir(ConfigFS.open(url), addPrefixed);
            listDir(WebsiteFS.open(url), addPrefixed);
            if (SD.cardType() != CARD_NONE) {
              listDir(SD.open(url), [&list](File file) {
                list.add(file.path());
              });
            }
            xSemaphoreGive(lvgl_mutex);
          }
        } else if (url == "/consists") {
          StaticJsonDocument<32> filterDoc;
          filterDoc["name"] = true;
          filterDoc["members"] = true;
          StaticJsonDocument<512> doc;

          auto addConsistsFromFS = [&listDir, &filterDoc, &doc, &list](fs::FS& fs) {
            File dir = fs.open("/consists");
            if (dir) {
              listDir(dir, [&filterDoc, &doc, &list](File file) {
                doc.clear();
                ReadBufferingStream bufferedFile(file, doc.capacity());
                deserializeJson(doc, bufferedFile, DeserializationOption::Filter(filterDoc));
                JsonObject item = list.createNestedObject();
                item["file"] = String(file.path());
                item["name"] = String(doc["name"].as<const char*>());
                item["memberCount"] = doc["members"].size();
                yield();
              });
            }
          };

          if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
            addConsistsFromFS(Settings.getFS());
            xSemaphoreGive(lvgl_mutex);
          }
        } else {
          StaticJsonDocument<16> filterDoc;
          filterDoc["name"] = true;
          StaticJsonDocument<48> doc;

          auto addItemsFromFS = [&listDir, &filterDoc, &doc, &list](fs::FS& fs, String path) {
            File dir = fs.open(path);
            if (dir) {
              listDir(dir, [&filterDoc, &doc, &list](File file) {
                doc.clear();
                ReadBufferingStream bufferedFile(file, doc.capacity());
                deserializeJson(doc, bufferedFile, DeserializationOption::Filter(filterDoc));
                JsonObject item = list.createNestedObject();
                item["file"] = String(file.path());
                item["name"] = String(doc["name"].as<const char*>());
                yield();
              });
            }
          };

          if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
            addItemsFromFS(Settings.getFS(), url);
            xSemaphoreGive(lvgl_mutex);
          }
        }

        response->setLength();
        request->send(response);
      } else {
        fs::FS* fsPtr = &Settings.getFS();
        String fsName = (fsPtr == &SD) ? "SD" : "ConfigFS";

        bool exists = false;
        if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
          exists = fsPtr->exists(url);
          if (!exists && WebsiteFS.exists(url)) {
            fsPtr = &WebsiteFS;
            fsName = "WebsiteFS";
            exists = true;
          }
          xSemaphoreGive(lvgl_mutex);
        }
        
        Serial.printf("[Web] Serving %s from %s\n", url.c_str(), fsName.c_str());

        fs::FS& fs = *fsPtr;

        if (exists) {
          File file;
          if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
            file = fs.open(url, "r");
            xSemaphoreGive(lvgl_mutex);
          }

          if (file) {
            String contentType = url.endsWith(".bmp") ? "image/bmp" : "application/json";
            size_t fileSize = file.size();
            
            AsyncWebServerResponse *response = request->beginResponse(contentType, fileSize, [file](uint8_t *buffer, size_t maxLen, size_t index) mutable -> size_t {
              if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
                size_t bytesRead = file.read(buffer, maxLen);
                xSemaphoreGive(lvgl_mutex);
                return bytesRead;
              }
              return 0;
            });
            
            if (url.endsWith(".bmp")) {
              response->addHeader("Cache-Control", "max-age=604800");
            }
            request->send(response);
            return;
          }
        }
        request->send(404);
      }
    } else if (request->method() == HTTP_HEAD) {
      bool exists = false;
      if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
        exists = Settings.getFS().exists(url);
        xSemaphoreGive(lvgl_mutex);
      }
      request->send(exists ? 204 : 404);
    } else if (request->method() == HTTP_DELETE) {
      if (url.startsWith("/fns/builtin-")) { request->send(403); return; }
      bool success = false;
      if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
        fs::FS& fs = Settings.getFS();
        if (fs.exists(url)) {
          success = fs.remove(url);
        }
        xSemaphoreGive(lvgl_mutex);
      }
      request->send(success ? 204 : 404);
    } else if (request->method() == HTTP_PUT) {
      if (url.startsWith("/fns/builtin-")) { request->send(403); return; }
      request->send(204);
    }
  }

  void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) override {
    if (request->method() == HTTP_PUT) {
      if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
        if (!request->_tempFile) {
          const String& raw = request->url();
          String stripped;
          const String& url = raw.startsWith("/$/") ? (stripped = raw.substring(2), stripped) : raw;
          int slash = url.lastIndexOf('/');
          if (slash > 0) Settings.getFS().mkdir(url.substring(0, slash));
          request->_tempFile = Settings.getFS().open(url, "w", true);
        }
        if (request->_tempFile) {
          request->_tempFile.write(data, len);
          if (len + index == total) {
            request->_tempFile.close();
          }
        }
        xSemaphoreGive(lvgl_mutex);
      }
    }
  }
};

void ThrottleServer::begin() {
  // Force browser to use sequential connections instead of parallel ones.
  // Without this, 4+ simultaneous large file downloads exhaust the lwIP
  // TCP buffer heap (especially when the SD card is mounted), causing a deadlock.
  DefaultHeaders::Instance().addHeader("Connection", "close");

  on("/cs", HTTP_HEAD, [](AsyncWebServerRequest* request) {
    request->send(csIsConnected ? 200 : 404);
  });

  // Polling endpoint: 200 = programming mode active, 404 = not active.
  // The web UI polls this periodically and redirects to / when it returns 404.
  on("/throttle-programming", HTTP_HEAD, [](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response = request->beginResponse(
      SettingsUI::throttleProgrammingActive ? 200 : 404);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
  });

  on("/cs", HTTP_GET, [](AsyncWebServerRequest* request) {
    AsyncJsonResponse* response = new AsyncJsonResponse(false, 1024);
    JsonVariant& cs = response->getRoot();

    cs["ssid"] = Settings.CS.SSID();
    // Never send the stored WiFi password back over HTTP. Expose only whether one
    // is set; the UI leaves the field blank and a blank on save keeps the existing.
    cs["has_password"] = Settings.CS.password().length() > 0;
    cs["server"] = Settings.CS.server();
    cs["port"] = Settings.CS.port();
    cs["storageMode"] = Settings.storageMode;
    cs["has_sd"] = SD.cardType() != CARD_NONE;
    
    String version("v");
    version += T3_VERSION_MAJOR; version += "."; version += T3_VERSION_MINOR; version += "."; version += T3_VERSION_PATCH;
    cs["version"] = version;
    cs["platform"] = String("ESP32 (") + ESP.getChipModel() + ")";
    cs["free_ram"] = ESP.getFreeHeap() / 1024;
    cs["ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString();

    response->setLength();
    request->send(response);
  });

  addHandler(new AsyncCallbackJsonWebHandler("/cs", [](AsyncWebServerRequest* request, JsonVariant &json) {
    Settings.CS.SSID(json["ssid"]);
    // Blank/absent password = keep the currently stored one (the UI never
    // receives it, so it can't echo it back). Only overwrite on a real value.
    if (json.containsKey("password")) {
      String pw = json["password"].as<String>();
      if (pw.length() > 0) Settings.CS.password(pw);
    }
    Settings.CS.server(json["server"]);
    Settings.CS.port(json["port"]);
    
    if (json.containsKey("storageMode")) {
      Settings.storageMode = json["storageMode"].as<uint8_t>();
    }

    if (Settings.CS.valid()) { // Save new settings as we're valid
      request->send(204);
      // Defer save — Settings.save() needs more stack than async_tcp provides
      if (xTaskCreate([](void*) {
        if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
          Settings.save();
          xSemaphoreGive(lvgl_mutex);
        }
        Settings.dispatchEvent(SettingsClass::Event::CS_CHANGE);
        vTaskDelete(NULL);
      }, "cs_save", 8192, nullptr, 1, nullptr) != pdPASS) {
        request->send(503);
        return;
      }
    } else { // New settings are invalid so reload
      Settings.load();
      request->send(404);
    }
  }));

  on("/backup", HTTP_GET, [](AsyncWebServerRequest* request) {
    fs::FS& fs = Settings.getFS();
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    response->addHeader("Content-Disposition", "attachment; filename=\"dcc-ex-cyd-backup.json\"");

    auto streamDir = [&](const char* key, const char* dirPath) {
      response->printf("\"%s\":[", key);
      bool first = true;

      // Collect paths under mutex (fast directory scan only)
      std::vector<String> paths;
      if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
        File dir = fs.open(dirPath);
        if (dir && dir.isDirectory()) {
          while (File f = dir.openNextFile()) {
            if (!f.isDirectory()) paths.push_back(String(f.path()));
            f.close();
          }
          dir.close();
        }
        xSemaphoreGive(lvgl_mutex);
      }

      // Stream each file under its own brief mutex hold
      for (const String& path : paths) {
        if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
          File file = fs.open(path);
          if (file && !file.isDirectory()) {
            if (!first) response->print(",");
            first = false;
            response->printf("{\"path\":\"%s\",\"data\":", path.c_str());
            uint8_t buf[256];
            while (file.available()) {
              size_t n = file.read(buf, sizeof(buf));
              if (n > 0) response->write((const char*)buf, n);
            }
            response->print("}");
            file.close();
          }
          xSemaphoreGive(lvgl_mutex);
        }
        yield();
      }

      response->print("]");
    };

    response->print("{");
    streamDir("locos", "/locos");
    response->print(",");
    streamDir("fns", "/fns");
    response->print(",\"groups\":");

    if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
      File groups = fs.open("/groups.json");
      if (groups && groups.size() > 0) {
        uint8_t buf[256];
        while (groups.available()) {
          size_t n = groups.read(buf, sizeof(buf));
          if (n > 0) response->write((const char*)buf, n);
        }
        groups.close();
      } else {
        response->print("[]");
      }
      xSemaphoreGive(lvgl_mutex);
    }

    response->print("}");
    request->send(response);
  });

  addHandler(new AsyncCallbackJsonWebHandler("/migrate", [](AsyncWebServerRequest* request, JsonVariant &json) {
    uint8_t toMode = json["to"] | 0;
    
    fs::FS* sourceFSPtr = nullptr;
    fs::FS* targetFSPtr = nullptr;
    
    if (toMode == 1) { // 1 = SD_CARD
      if (SD.cardType() == CARD_NONE) {
        request->send(400, "text/plain", "SD Card not mounted");
        return;
      }
      targetFSPtr = &SD;
      sourceFSPtr = &ConfigFS;
    } else {
      if (SD.cardType() == CARD_NONE) {
        request->send(400, "text/plain", "SD Card not mounted");
        return;
      }
      targetFSPtr = &ConfigFS;
      sourceFSPtr = &SD;
    }
    
    fs::FS& targetFS = *targetFSPtr;
    fs::FS& sourceFS = *sourceFSPtr;

    // copyAndBackupFile: takes/releases mutex internally around each chunk
    auto copyAndBackupFile = [](fs::FS& sFS, fs::FS& tFS, const char* path, const char* backupPath) {
      if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) != pdTRUE) return;
      if (!sFS.exists(path)) { xSemaphoreGive(lvgl_mutex); return; }
      File source = sFS.open(path, "r");
      if (!source) { xSemaphoreGive(lvgl_mutex); return; }

      String targetDir = String(path);
      targetDir = targetDir.substring(0, targetDir.lastIndexOf('/'));
      if (targetDir.length() > 0 && !tFS.exists(targetDir.c_str())) tFS.mkdir(targetDir.c_str());

      File target = tFS.open(path, "w", true);
      if (target) {
        uint8_t buf[512];
        size_t len = source.size();
        while (len) {
          size_t toRead = len > 512 ? 512 : len;
          source.read(buf, toRead);
          target.write(buf, toRead);
          len -= toRead;
        }
        target.close();
      }
      source.close();

      String backupDir = String(backupPath);
      backupDir = backupDir.substring(0, backupDir.lastIndexOf('/'));
      if (backupDir.length() > 0 && !sFS.exists(backupDir.c_str())) sFS.mkdir(backupDir.c_str());
      sFS.rename(path, backupPath);
      xSemaphoreGive(lvgl_mutex);
      yield();
    };

    auto migrateDir = [&](fs::FS& sFS, fs::FS& tFS, const char* dirName) {
      // Collect paths under a brief mutex hold
      std::vector<String> filesToMigrate;
      if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
        File dir = sFS.open(dirName);
        if (dir && dir.isDirectory()) {
          while (File file = dir.openNextFile()) {
            if (!file.isDirectory()) filesToMigrate.push_back(String(file.path()));
            file.close();
          }
          dir.close();
        }
        xSemaphoreGive(lvgl_mutex);
      }
      // Copy each file with its own mutex hold — LVGL can run between files
      for (const String& path : filesToMigrate) {
        char backupPath[64];
        snprintf(backupPath, sizeof(backupPath), "/backup%s", path.c_str());
        copyAndBackupFile(sFS, tFS, path.c_str(), backupPath);
      }
    };

    if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
      sourceFS.mkdir("/backup");
      xSemaphoreGive(lvgl_mutex);
    }

    migrateDir(sourceFS, targetFS, "/locos");
    migrateDir(sourceFS, targetFS, "/fns");
    migrateDir(sourceFS, targetFS, "/icons");

    if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
      bool hasGroups = sourceFS.exists("/groups.json");
      xSemaphoreGive(lvgl_mutex);
      if (hasGroups) copyAndBackupFile(sourceFS, targetFS, "/groups.json", "/backup/groups.json");
    }

    if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
      Settings.storageMode = toMode;
      Settings.save();
      xSemaphoreGive(lvgl_mutex);
    }

    request->send(200);
  }));

  addHandler(new WebLoggerHandler());
  addHandler(new ThrottleAPIHandler());

  // Root route: serve the minimal "not started" page unless Throttle
  // Programming mode is active, in which case serve the full Vue app.
  // This prevents the heavy Vue bundle from being loaded (and crashing
  // the device) unless the user has explicitly enabled programming mode.
  on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (SettingsUI::throttleProgrammingActive) {
      request->redirect("/index.html");
    } else {
      request->redirect("/not_programming.html");
    }
  });

  serveStatic("/", WebsiteFS, "/www/")
    .setCacheControl("max-age=604800");

  AsyncWebServer::begin();
}

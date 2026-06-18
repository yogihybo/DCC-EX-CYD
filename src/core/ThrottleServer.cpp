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
    String url = request->url();
    if (url.startsWith("/$/")) url = url.substring(2);

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
    String url = request->url();
    Serial.printf("[Web] Request: %s %s\n", request->methodToString(), url.c_str());
    if (url.startsWith("/$/")) url = url.substring(2);

    if (request->method() == HTTP_GET) {
      if (url == "/locos" || url == "/fns" || url == "/icons" || url == "/consists") {
        AsyncJsonResponse* response = new AsyncJsonResponse(true, 2048);
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
            listDir(ConfigFS.open(url), [&list](File file) {
              list.add("/$" + String(file.path()));
            });
            listDir(WebsiteFS.open(url), [&list](File file) {
              list.add("/$" + String(file.path()));
            });

            if (SD.cardType() != CARD_NONE) {
              listDir(SD.open(url), [&list](File file) {
                list.add(String(file.path()));
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
      request->send(204);
    }
  }

  void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) override {
    if (request->method() == HTTP_PUT) {
      if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
        if (!request->_tempFile) {
          String url = request->url();
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
    cs["password"] = Settings.CS.password();
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
    Settings.CS.password(json["password"]);
    Settings.CS.server(json["server"]);
    Settings.CS.port(json["port"]);
    
    if (json.containsKey("storageMode")) {
      Settings.storageMode = json["storageMode"].as<uint8_t>();
    }

    if (Settings.CS.valid()) { // Save new settings as we're valid
      request->send(204);
      // Defer save — Settings.save() needs more stack than async_tcp provides
      xTaskCreate([](void*) {
        if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
          Settings.save();
          xSemaphoreGive(lvgl_mutex);
        }
        Settings.dispatchEvent(SettingsClass::Event::CS_CHANGE);
        vTaskDelete(NULL);
      }, "cs_save", 8192, nullptr, 1, nullptr);
    } else { // New settings are invalid so reload
      Settings.load();
      request->send(404);
    }
  }));

  on("/backup", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) != pdTRUE) {
      request->send(500);
      return;
    }

    fs::FS& fs = Settings.getFS();
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    response->addHeader("Content-Disposition", "attachment; filename=\"dcc-ex-cyd-backup.json\"");

    auto streamDir = [&](const char* key, const char* dirPath) {
      response->printf("\"%s\":[", key);
      bool first = true;
      File dir = fs.open(dirPath);
      if (dir && dir.isDirectory()) {
        while (File file = dir.openNextFile()) {
          if (!file.isDirectory()) {
            if (!first) response->print(",");
            first = false;
            response->printf("{\"path\":\"%s\",\"data\":", file.path());
            uint8_t buf[256];
            while (file.available()) {
              size_t n = file.read(buf, sizeof(buf));
              if (n > 0) response->write((const char*)buf, n);
              yield();
            }
            response->print("}");
          }
          file.close();
          yield();
        }
        dir.close();
      }
      response->print("]");
    };

    response->print("{");
    streamDir("locos", "/locos");
    response->print(",");
    streamDir("fns", "/fns");
    response->print(",\"groups\":");

    File groups = fs.open("/groups.json");
    if (groups && groups.size() > 0) {
      uint8_t buf[256];
      while (groups.available()) {
        size_t n = groups.read(buf, sizeof(buf));
        if (n > 0) response->write((const char*)buf, n);
        yield();
      }
      groups.close();
    } else {
      response->print("[]");
    }

    response->print("}");
    xSemaphoreGive(lvgl_mutex);
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

    if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
      auto copyAndBackupFile = [](fs::FS& sFS, fs::FS& tFS, String path, String backupPath) {
        if (!sFS.exists(path)) return;
        File source = sFS.open(path, "r");
        if (!source) return;
        
        String targetDir = path.substring(0, path.lastIndexOf('/'));
        if (targetDir.length() > 0 && !tFS.exists(targetDir)) tFS.mkdir(targetDir);
        
        File target = tFS.open(path, "w", true);
        if (target) {
          size_t len = source.size();
          uint8_t buf[512];
          while (len) {
            size_t toRead = len > 512 ? 512 : len;
            source.read(buf, toRead);
            target.write(buf, toRead);
            len -= toRead;
            yield();
          }
          target.close();
        }
        source.close();
        
        String backupDir = backupPath.substring(0, backupPath.lastIndexOf('/'));
        if (backupDir.length() > 0 && !sFS.exists(backupDir)) sFS.mkdir(backupDir);
        sFS.rename(path, backupPath);
      };

      auto migrateDir = [&](fs::FS& sFS, fs::FS& tFS, String dirName) {
        File dir = sFS.open(dirName);
        if (!dir || !dir.isDirectory()) return;
        
        std::vector<String> filesToMigrate;
        while (File file = dir.openNextFile()) {
          if (!file.isDirectory()) {
            filesToMigrate.push_back(String(file.path()));
          }
          file.close();
          yield();
        }
        dir.close();

        for (const String& path : filesToMigrate) {
          String backupPath = "/backup" + path;
          copyAndBackupFile(sFS, tFS, path, backupPath);
        }
      };
      
      sourceFS.mkdir("/backup");
      migrateDir(sourceFS, targetFS, "/locos");
      migrateDir(sourceFS, targetFS, "/fns");
      migrateDir(sourceFS, targetFS, "/icons");
      
      if (sourceFS.exists("/groups.json")) {
        copyAndBackupFile(sourceFS, targetFS, "/groups.json", "/backup/groups.json");
      }
      
      Settings.storageMode = toMode;
      Settings.save();
      
      xSemaphoreGive(lvgl_mutex);
      request->send(200);
    } else {
      request->send(500, "text/plain", "Mutex timeout");
    }
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

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

ThrottleServer::ThrottleServer() : AsyncWebServer(80) { }

class ThrottleAPIHandler : public AsyncWebHandler {
public:
  ThrottleAPIHandler() {}
  virtual ~ThrottleAPIHandler() {}

  bool canHandle(AsyncWebServerRequest *request) const override {
    String url = request->url();
    if (url.startsWith("/$/")) url = url.substring(2);

    if (request->method() == HTTP_GET) {
      if (url == "/locos" || url == "/fns" || url == "/icons") return true;
      if (url.endsWith(".json") || url.endsWith(".bmp")) {
        if (url.startsWith("/locos/") || url.startsWith("/fns/") || url.startsWith("/icons/") || url == "/groups.json" || url == "/groups.bmp") return true;
      }
    } else if (request->method() == HTTP_HEAD) {
      if (url.startsWith("/locos/") && url.endsWith(".json")) return true;
    } else if (request->method() == HTTP_DELETE) {
      if (url.endsWith(".json") || url.endsWith(".bmp")) {
        if (url.startsWith("/locos/") || url.startsWith("/fns/") || url.startsWith("/icons/")) return true;
      }
    } else if (request->method() == HTTP_PUT) {
      if (url.endsWith(".json") || url.endsWith(".bmp")) {
        if (url.startsWith("/locos/") || url.startsWith("/fns/") || url.startsWith("/icons/") || url == "/groups.json" || url == "/groups.bmp") return true;
      }
    }
    return false;
  }

  void handleRequest(AsyncWebServerRequest *request) override {
    String url = request->url();
    if (url.startsWith("/$/")) url = url.substring(2);

    if (request->method() == HTTP_GET) {
      if (url == "/locos" || url == "/fns" || url == "/icons") {
        AsyncJsonResponse* response = new AsyncJsonResponse(true, 4096);
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
        if (!fsPtr->exists(url) && WebsiteFS.exists(url)) {
          fsPtr = &WebsiteFS;
        }
        fs::FS& fs = *fsPtr;

        if (url.endsWith(".bmp")) {
          if (fs.exists(url)) {
            AsyncWebServerResponse *response = request->beginResponse(fs, url, "image/bmp");
            response->addHeader("Cache-Control", "max-age=604800");
            request->send(response);
          } else {
            request->send(404);
          }
        } else {
          if (fs.exists(url)) {
            request->send(fs, url);
          } else {
            request->send(404);
          }
        }
      }
    } else if (request->method() == HTTP_HEAD) {
      bool exists = Settings.getFS().exists(url);
      request->send(exists ? 204 : 404);
    } else if (request->method() == HTTP_DELETE) {
      fs::FS& fs = Settings.getFS();
      bool success = false;
      if (fs.exists(url)) {
        success = fs.remove(url);
      }
      request->send(success ? 204 : 404);
    } else if (request->method() == HTTP_PUT) {
      request->send(204);
    }
  }

  void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) override {
    if (request->method() == HTTP_PUT) {
      if (!request->_tempFile) {
        request->_tempFile = Settings.getFS().open(request->url(), "w", true);
      }
      request->_tempFile.write(data, len);
      if (len + index == total) {
        request->_tempFile.close();
      }
    }
  }
};

void ThrottleServer::begin() {

  on("/cs", HTTP_HEAD, [](AsyncWebServerRequest* request) {
    request->send(Settings.CS.valid() ? 200 : 404);
  });

  on("/cs", HTTP_GET, [](AsyncWebServerRequest* request) {
    AsyncJsonResponse* response = new AsyncJsonResponse(false, 1024);
    JsonVariant& cs = response->getRoot();

    cs["ssid"] = Settings.CS.SSID();
    cs["password"] = Settings.CS.password();
    cs["server"] = Settings.CS.server();
    cs["port"] = Settings.CS.port();
    
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

    if (Settings.CS.valid()) { // Save new settings as we're valid
      Settings.save();
      request->send(204);
      Settings.dispatchEvent(SettingsClass::Event::CS_CHANGE);
    } else { // New settings are invalid so reload
      Settings.load();
      request->send(404);
    }
  }));

  addHandler(new ThrottleAPIHandler());

  serveStatic("/", WebsiteFS, "/www/")
    .setCacheControl("max-age=604800")
    .setDefaultFile("index.html");

  AsyncWebServer::begin();
}

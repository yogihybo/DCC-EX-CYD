#include <ThrottleServer.h>
#include <AsyncTCP.h>
#include <AsyncJson.h>
#include <SPIFFS.h>
#include <SD.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <StreamUtils.h>
#include <Settings.h>

ThrottleServer::ThrottleServer() : AsyncWebServer(80) { }

void ThrottleServer::begin() {
  serveStatic("/", SPIFFS, "/www/")
    .setCacheControl("max-age=604800")
    .setDefaultFile("index.html");

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

  on("^(\\/(?:locos|fns|icons))$", HTTP_GET, [](AsyncWebServerRequest* request) {
    AsyncJsonResponse* response = new AsyncJsonResponse(true, 4096);
    JsonVariant& list = response->getRoot();
    String path = request->pathArg(0);

    auto listDir = [](File dir, auto cb) {
      while (File file = dir.openNextFile()) {
        if (!file.isDirectory()) {
          cb(file);
        }
        file.close();
      }
      dir.close();
    };

    if (path.startsWith("/icons")) {
      listDir(SPIFFS.open(path), [&list](File file) {
        list.add("/$" + String(file.path()));
      });

      if (SD.cardType() != CARD_NONE) {
        listDir(SD.open(path), [&list](File file) {
          list.add(String(file.path()));
        });
      }
    } else {
      StaticJsonDocument<16> filterDoc;
      filterDoc["name"] = true;
      StaticJsonDocument<48> doc;

      auto addItemsFromFS = [&listDir, &filterDoc, &doc, &list](fs::FS& fs, String path) {
        if (fs.exists(path)) {
          listDir(fs.open(path), [&filterDoc, &doc, &list](File file) {
            ReadBufferingStream bufferedFile(file, doc.capacity());
            deserializeJson(doc, bufferedFile, DeserializationOption::Filter(filterDoc));
            JsonObject item = list.createNestedObject();
            item["file"] = String(file.path());
            item["name"] = String(doc["name"].as<const char*>());
          });
        }
      };

      addItemsFromFS(SPIFFS, path);
      if (SD.cardType() != CARD_NONE) {
        addItemsFromFS(SD, path);
      }
    }

    response->setLength();
    request->send(response);
  });

  on("^\\/locos\\/.+\\.json$", HTTP_HEAD, [](AsyncWebServerRequest* request) {
    bool exists = SPIFFS.exists(request->url()) || (SD.cardType() != CARD_NONE && SD.exists(request->url()));
    request->send(exists ? 204 : 404);
  });

  on("^(?:\\/\\$)?\\/(?:(?:locos|fns|icons)\\/.+|groups)\\.(?:json|bmp)$", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (request->url().endsWith(".bmp")) { // Bit hacky but allows us to use the built in ETag and Cache-Control code
      AsyncStaticWebHandler* handler;
      if (request->url().startsWith("/$/")) {
        handler = new AsyncStaticWebHandler(request->url().c_str(), SPIFFS, request->url().c_str() + 2, "max-age=604800");
      } else {
        if (SPIFFS.exists(request->url())) {
          handler = new AsyncStaticWebHandler("", SPIFFS, "", "max-age=604800");
        } else {
          handler = new AsyncStaticWebHandler("", SD, "", "max-age=604800");
        }
      }
      handler->canHandle(request);
      handler->handleRequest(request);
    } else {
      if (SPIFFS.exists(request->url())) {
        request->send(SPIFFS, request->url());
      } else {
        request->send(SD, request->url());
      }
    }
  });

  on("^\\/(?:locos|fns|icons)\\/.+\\.(?:json|bmp)$", HTTP_DELETE, [](AsyncWebServerRequest* request) {
    bool success = false;
    if (SPIFFS.exists(request->url())) {
      success = SPIFFS.remove(request->url());
    } else if (SD.cardType() != CARD_NONE && SD.exists(request->url())) {
      success = SD.remove(request->url());
    }
    request->send(success ? 204 : 404);
  });

  on("^\\/(?:(?:locos|fns|icons)\\/.+|groups)\\.(?:json|bmp)$", HTTP_PUT, [](AsyncWebServerRequest* request) {

  }, NULL, [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    if (!request->_tempFile) {
      if (SD.cardType() != CARD_NONE) {
        request->_tempFile = SD.open(request->url(), "w", true);
      } else {
        request->_tempFile = SPIFFS.open(request->url(), "w", true);
      }
    }

    request->_tempFile.write(data, len);

    if (len + index == total) {
      request->_tempFile.close();
      request->send(204);
    }
  });

  AsyncWebServer::begin();
}

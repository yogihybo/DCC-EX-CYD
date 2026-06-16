#ifndef SETTINGS_H
#define SETTINGS_H

#include <Version.h>
#include <Events.h>
#include <IPAddress.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <FileSystems.h>

class SettingsClass : public Events {
  public:
    struct Event {
      static constexpr uint8_t CS_CHANGE = 0;
      static constexpr uint8_t ROTATION_CHANGE = 1;
      static constexpr uint8_t BRIGHTNESS_CHANGE = 2;
      static constexpr uint8_t THEME_CHANGE = 3;
      static constexpr uint8_t THROTTLE_PROGRAM_ENTER = 4;
      static constexpr uint8_t THROTTLE_PROGRAM_EXIT = 5;
    };
    struct Rotation {
      static constexpr uint8_t STANDARD = 0;
      static constexpr uint8_t INVERTED = 1;
      static constexpr uint8_t ACCELEROMETER = 2;
    };
    struct StorageMode {
      static constexpr uint8_t LITTLEFS = 0;
      static constexpr uint8_t SD_CARD = 1;
    };
    struct Theme {
      static constexpr uint8_t LIGHT = 0;
      static constexpr uint8_t DARK = 1;
    };

    uint32_t version = 0;
    uint8_t rotation = Rotation::STANDARD;
    uint8_t storageMode = StorageMode::LITTLEFS;
    uint8_t theme = Theme::DARK;
    uint8_t brightness = 255;
    uint8_t emergencyStopDelay = 5; // seconds of encoder button hold to trigger e-stop

    struct TouchCal {
      int xMin = 200;
      int xMax = 3750;
      int yMin = 200;
      int yMax = 3700;
    } TouchCal;

    fs::FS& getFS() const;

    void load();
    void save();
    void init();
    void upgrade(JsonDocument& doc);

    struct AP {
      friend class SettingsClass;

      String SSID;
      String password;
      bool enabled = false;

      private:
        void load(const JsonObject& obj);
        void save(const JsonObject& obj); 
    } AP;

    struct CS {
      friend class SettingsClass;
        
      bool valid();

      String SSID() const;
      bool SSID(const String& value);

      String password() const;
      bool password(const String& value);

      String server() const;
      bool server(const String& value);

      uint16_t port() const;
      void port(uint16_t value);
    
      CS() {
        _ssid.reserve(32);
        _password.reserve(63);
      }

      private:
        String _ssid;
        String _password;
        String _server = "dccex";
        uint16_t _port = 2560;

        void load(const JsonObject& obj);
        void save(const JsonObject& obj);
    } CS;

    struct LocoUI {
      friend class SettingsClass;

      struct SpeedStep {
        static constexpr uint8_t STEP_1 = 0;
        static constexpr uint8_t STEP_2 = 1;
        static constexpr uint8_t STEP_4 = 2;
      };

      uint8_t speedStep = SpeedStep::STEP_1;

      private:
        void load(const JsonObject& obj);
        void save(const JsonObject& obj);
    } LocoUI;
};

extern SettingsClass Settings;

#endif

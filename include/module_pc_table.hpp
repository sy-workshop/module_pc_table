// ############################################
// #    MODULE-PC-TABLE - SMARTSHOP MODULE    #
// ############################################
//
// > Version 0.4.0
// 

# pragma once

/// @brief Software Version string of the PC-Table
# define PC_TABLE_VERSION "0.4.0/2024/12/10"

// Libraries
# include <WiFi.h>
# include <PubSubClient.h>
# include <DHTesp.h>

# include <sylo/logging.hpp>
# include <sylo/components/rotary_encoder.hpp>

// PINS
  /// @brief The pin of the DHT Sensor to measure temperature and humidity from
  # define PIN_DHT_SENSOR 26
  /// @brief The pin the buzzer is attached to
  # define PIN_BUZZER 25

  // Upper rotary encoder pins
  # define PIN_RE_UPPER_SW 23
  # define PIN_RE_UPPER_DT 19
  # define PIN_RE_UPPER_CL 18

  // Lower rotary encoder pins
  # define PIN_RE_LOWER_SW 16
  # define PIN_RE_LOWER_DT 27
  # define PIN_RE_LOWER_CL 14
// 

// Other
  /// @brief A correction value for the temperature measurement, usually the sensor is off by two degrees
  # define TEMP_CORRECTION -2.0

  /// @brief The hostname of the module, used in WiFi and MQTT connections
  # define HOSTNAME "module-pc-table"
// 

namespace module_pc_table {
  namespace device {
    /// @brief Upper encoder of the module
    static RotaryEncoder upper_encoder = RotaryEncoder(PIN_RE_UPPER_SW, PIN_RE_UPPER_DT, PIN_RE_UPPER_CL);
    /// @brief Lower encoder of the module
    static RotaryEncoder lower_encoder = RotaryEncoder(PIN_RE_UPPER_SW, PIN_RE_UPPER_DT, PIN_RE_UPPER_CL);

    /// @brief DHT11 Temperature & Humidity sensor
    static DHTesp dht;

    void setup() {
      log_info("| > Setting up DHT-Temp sensor ... ");
      dht.setup(PIN_DHT_SENSOR, DHTesp::DHT11);
      log_infoln("done!");
    }
  }

  float get_temp() {
    return device::dht.getTemperature() + TEMP_CORRECTION;
  }

  float get_humid() {
    return device::dht.getHumidity();
  }

  namespace remote {
    static WiFiClient wifi_client;
    static PubSubClient mqtt_client(wifi_client);

    // MQTT Functions
    /// @brief Callback function for incomming mqtt messages
    void mqtt_callback(char* _topic, byte* message, unsigned int length);

    /// @brief Reconnection function for the MQTT client, skips if connection is still up
    void mqtt_reconnect();

    /// @brief Publishes the current active status to the MQTT server
    void mqtt_active_loop();

    /// @brief Publishes the current temperature and humidity to the MQTT server
    void mqtt_temp_loop();

    void loop() {
      // Secure connection
      mqtt_reconnect();
      
      // Send data
      mqtt_active_loop();
      mqtt_temp_loop();
    }
  }
}
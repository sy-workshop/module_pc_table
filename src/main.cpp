// #####################################
// #    PC-TABLE - SMARTSHOP MODULE    #
// #####################################
//
// > Version 0.4.0
// 

// SyLo - Library
# include <sylo/logging.hpp>
# include <sylo/components/rotary_encoder.hpp>
# include <sylo/timing/toff.hpp>
# include <sylo/timing/timer.hpp>
# include <sylo/triggers/rtrig.hpp>

# include <sylo/esp32/wifi.hpp>

// Cosy - Library
# include <cosy/mqtt.hpp>
# include <cosy/network.hpp>

// General module
# include "module_pc_table.hpp"


// Static variables
static RTrig ut, lt;
static TOff up(50), lp(50);

// Static helper variables
static Timer timer_mqtt_reconn (5000), timer_mqtt_active (10000), timer_mqtt_temp (10000);

static char payload_buffer[128];

// Helper functions
  /// @brief Prints out configuration data used for the WiFi connection
  void print_wifi_info() { 
    log_info("| > SSID: '");
    log_info(PRIVATE_WIFI_SSID);
    log_infoln("'");

    log_info("| > PASSWORD: '");
    log_info(PRIVATE_WIFI_PWD);
    log_infoln("'");

    log_info("| > HOSTNAME: '");
    log_info(HOSTNAME);
    log_infoln("'");
  }

  /// @brief Prints out configuration data used for the MQTT connection
  void print_mqtt_info() {
    log_info("| > ADDR: 'mqtt://");
    log_info(COSY_HUB_HOSTNAME); 
    log_info(":");
    log_info(COSY_HUB_MQTT_PORT);
    log_infoln("'");
  }
// 

namespace module_pc_table {
  namespace remote {
    /// @brief Callback function for incomming mqtt messages
    void mqtt_callback(char* _topic, byte* message, unsigned int length) {
      // Parse into strings
      String topic (_topic);
      String messageTemp;
      for (int i = 0; i < length; i++) {
        messageTemp += (char)message[i];
      }

      // React to messages
      if (topic == TOPIC_LIGHT_CHAIN_MAIN) {
        if (messageTemp == "true") {
          log_infoln("| | > Main light chain is activated!");
          circlelab::light::chain_main = true;
        } else if (messageTemp == "false") {
          log_infoln("| | > Main light chain is deactivated!");
          circlelab::light::chain_main = false;
        } else {
          log_info("| | > Bad payload: ");
          log_infoln(messageTemp);
        }
      }
    }

    /// @brief Reconnection function for the MQTT client, skips if connection is still up
    void mqtt_reconnect() {
      if (!WiFi.isConnected()) {
        return;
      }

      while (!mqtt_client.connected() && (timer_mqtt_reconn.has_elapsed())) {
        log_info("| > Connecting to MQTT-Server ... ");

        if (mqtt_client.connect(HOSTNAME)) {
          log_infoln("done!");
          
          // Subscribe to topics
          mqtt_client.subscribe(TOPIC_LIGHT_CHAIN_MAIN);
          mqtt_client.subscribe(TOPIC_LIGHT_CHAIN_LIVING);
        } else {
          log_info("failed! Error-Code (");
          log_info(mqtt_client.state());
          log_infoln(") trying again soon ... ");
        }

        timer_mqtt_reconn.set();
      }
    }

    /// @brief Publishes the current active status to the MQTT server
    void mqtt_active_loop() {
      if (timer_mqtt_active.has_elapsed()) {
        log_info("> Sending active status ... ");

        sprintf(payload_buffer, "{\"uptime\":%u,\"version\":\"" PC_TABLE_VERSION "\"}", millis() / 1000);
        mqtt_client.publish(JOIN_TOPICS(TOPIC_MODULES, HOSTNAME), payload_buffer);
        timer_mqtt_active.set();

        log_infoln("done!");
      }
    }

    /// @brief Publishes the current temperature and humidity to the MQTT server
    void mqtt_temp_loop() {
      if (timer_mqtt_temp.has_elapsed()) {
        log_infoln("> Sending tempature status ... ");

        circlelab::humid = module_pc_table::get_humid();
        circlelab::temp = module_pc_table::get_temp();

        if (module_pc_table::device::dht.getStatus()) {
          log_info("| > Measurement failed! Error: ");
          log_infoln(module_pc_table::device::dht.getStatusString());
        } else {
          log_info("| > Temperature: ");
          log_infoln(circlelab::temp);
          log_info("| > Humidity: ");
          log_infoln(circlelab::humid);

          sprintf(payload_buffer, "%.2f", circlelab::temp);
          mqtt_client.publish(TOPIC_TEMP, payload_buffer, (bool)true);

          sprintf(payload_buffer, "%.2f", circlelab::humid);
          mqtt_client.publish(TOPIC_HUMID, payload_buffer, (bool)true);
        }

        timer_mqtt_temp.set();
        
        log_infoln("| > Done!");
      }
    }
  }
}

void setup() {
  init_logging(115200);

  log_infoln("# PC-Table Module");
  log_infoln("> Initializing devices ... ");

  module_pc_table::device::setup();

  log_infoln("| > Done!");

  log_infoln("> Setting up Wifi ... ");
  print_wifi_info();

  // Connect to smartshop wifi with hostname
  cosy::remote::connect(HOSTNAME);
  
  // MQTT
  log_infoln("> Setting up MQTT ... ");
  print_mqtt_info();

  cosy::remote::connect_mqtt(
    &module_pc_table::remote::mqtt_client,
    module_pc_table::remote::mqtt_callback
  );

  module_pc_table::remote::mqtt_reconnect();
  
  log_infoln("| > Done!");

  delay(100);   // Wait before starting to recv msgs

  log_infoln("> Initialization complete!");
}


void loop() {
  // Rotary-Encoders
  RotaryMove upper_val = module_pc_table::device::upper_encoder.check_rotary();
  RotaryMove lower_val = module_pc_table::device::lower_encoder.check_rotary();

  // Maintain Connections
  sylo::wifi_reconnect();
  module_pc_table::remote::loop();
}
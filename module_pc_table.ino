// #####################################
// #    PC-TABLE - SMARTSHOP MODULE    #
// #####################################
//
// > Version 0.4.0
// 

// Libraries
# include <WiFi.h>
# include <PubSubClient.h>

// SyLo - Library
# include <sylo/logging.hpp>
# include <sylo/components/rotary_encoder.hpp>
# include <sylo/timing/toff.hpp>
# include <sylo/triggers/rtrig.hpp>

# include <sylo/esp32/wifi.hpp>

// General module
# include "pc_table.hpp"
# include "private/wifi_creds.hpp"   // Hidden file
# include "smartshop/mqtt_topics.hpp"
# include "smartshop/network.hpp"

// Connection
/// @brief The hostname of the module, used in WiFi and MQTT connections
# define HOSTNAME "module-pc-table"
/// @brief The amount of milliseconds between reconnection tries when disconnected to the MQTT server
# define MQTT_RECONN_INTERVAL 5000
/// @brief The amout of milliseconds between sending an active status information to the MQTT broker
# define MQTT_ACTIVE_INTERVAL 3000
/// @brief The amount of millisends between sending a measurement of the temperature and humidity
# define TEMP_READ_INTERVAL 5000

// Static variables
static RTrig ut, lt;
static TOff up(50), lp(50);

static WiFiClient esp_client;
static PubSubClient mqtt_client(esp_client);

// Static helper variables
static unsigned long mqtt_reconn_helper = 0;
static unsigned long mqtt_active_helper = 0;
static unsigned long mqtt_temp_helper = 0;

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
    log_info(SMARTSHOP_HUB_HOSTNAME); 
    log_info(":");
    log_info(SMARTSHOP_HUB_MQTT_PORT);
    log_infoln("'");
  }
// 

// WiFi functions
  IPAddress wifi_resolve_hub() {
    IPAddress hub_ip;

    log_info("> Resolving hub hostname ... ");
    WiFi.hostByName(SMARTSHOP_HUB_HOSTNAME, hub_ip);
    log_infoln("done!");

    log_info("| > IP: '");
    log_info(hub_ip);
    log_infoln("'");

    return hub_ip;
  }
//

// MQTT Functions
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
        smartshop::light::chain_main = true;
      } else if (messageTemp == "false") {
        log_infoln("| | > Main light chain is deactivated!");
        smartshop::light::chain_main = false;
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

    while (!mqtt_client.connected() && ((mqtt_active_helper - millis()) > MQTT_RECONN_INTERVAL)) {
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

      mqtt_active_helper = millis();
    }
  }

  // Data
  void mqtt_send_active() {
    if ((millis() - mqtt_active_helper) > MQTT_ACTIVE_INTERVAL) {
      log_info("> Sending active status ... ");

      sprintf(payload_buffer, "{\"uptime\":%u,\"version\":\"" PC_TABLE_VERSION "\"}", millis());
      mqtt_client.publish(JOIN_TOPICS(TOPIC_MODULES, HOSTNAME), payload_buffer);
      mqtt_active_helper = millis();

      log_infoln("done!");
    }
  }

  void mqtt_send_temp() {
    if ((millis() - mqtt_temp_helper) > TEMP_READ_INTERVAL) {
      log_infoln("> Sending tempature status ... ");

      smartshop::humid = pc_table::get_humid();
      smartshop::temp = pc_table::get_temp();

      if (pc_table::dht.getStatus()) {
        log_info("| > Measurement failed! Error: ");
        log_infoln(pc_table::dht.getStatusString());
      } else {
        log_info("| > Temperature: ");
        log_infoln(smartshop::temp);
        log_info("| > Humidity: ");
        log_infoln(smartshop::humid);

        sprintf(payload_buffer, "%.2f", smartshop::temp);
        mqtt_client.publish(TOPIC_TEMP, payload_buffer, (bool)true);

        sprintf(payload_buffer, "%.2f", smartshop::humid);
        mqtt_client.publish(TOPIC_HUMID, payload_buffer, (bool)true);
      }

      mqtt_temp_helper = millis();
      
      log_infoln("| > Done!");
    }
  }
// 

void setup() {
  init_logging(115200);

  log_infoln("# PC-Table Module");
  log_infoln("> Initializing devices ... ");

  pc_table::setup();

  log_infoln("| > Done!");

  log_infoln("> Setting up Wifi ... ");
  print_wifi_info();

  // Setup WiFi
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.setHostname(HOSTNAME);
  WiFi.mode(WIFI_MODE_STA);
  WiFi.begin(PRIVATE_WIFI_SSID, PRIVATE_WIFI_PWD);

  log_info("| > Connecting ");
  while (WiFi.status() != WL_CONNECTED) {
    log_info(".");
    delay(500);
  }
  log_infoln(" done!");
  
  log_info("| | > IP: ");
  log_infoln(WiFi.localIP());

  log_info("| | > RRSI: ");
  log_infoln(WiFi.RSSI());

  IPAddress hub_ip = wifi_resolve_hub();
  
  // MQTT
  log_infoln("> Setting up MQTT ... ");
  print_mqtt_info();
  
  mqtt_client.setServer(hub_ip, SMARTSHOP_HUB_MQTT_PORT);
  mqtt_client.setCallback(mqtt_callback);

  mqtt_reconnect();
  
  log_infoln("| > Done!");

  delay(100);   // Wait before starting to recv msgs

  log_infoln("> Initialization complete!");
}


void loop() {
  // Rotary-Encoders
  RotaryMove upper_val = pc_table::upper_encoder.check_rotary();
  RotaryMove lower_val = pc_table::lower_encoder.check_rotary();

  if (ut(up(pc_table::upper_encoder.check_switch()))) {
    log_infoln("| > Upper switch pressed!");
    mqtt_client.publish(TOPIC_LIGHT_CHAIN_MAIN, "true", true);
  }

  if (lt(lp(pc_table::lower_encoder.check_switch()))) {
    log_infoln("| > Lower switch pressed!");
    mqtt_client.publish(TOPIC_LIGHT_CHAIN_MAIN, "false", true);
  }

  if (is_movement(upper_val)) {
    log_info("| > Upper rotary encoder moved to: ");
    log_infoln(pc_table::upper_encoder.counter); 
  }

  if (is_movement(lower_val)) {
    log_info("| > Lower rotary encoder moved to: ");
    log_infoln(pc_table::lower_encoder.counter); 
  }

  // Maintain Connections
  wifi_reconnect();
  mqtt_reconnect();

  mqtt_send_active();
  mqtt_send_temp();

  mqtt_client.loop();
}

// ############################################
// #    MODULE-PC-TABLE - SMARTSHOP MODULE    #
// ############################################
//
// > Version 0.4.0
// 

# pragma once

# define PC_TABLE_VERSION "0.4.0"

# include <DHTesp.h>

# include <sylo/logging.hpp>
# include <sylo/components/rotary_encoder.hpp>

// PINS
    /// @brief The pin of the DHT Sensor to measure temperature and humidity from
    # define PIN_DHT_SENSOR 26
    /// @brief The pin the buzzer is attached to
    # define PIN_BUZZER 25
// 

// Other
    /// @brief A correction value for the temperature measurement, usually the sensor is off by two degrees
    # define TEMP_CORRECTION -2.0
// 

namespace pc_table {
    static RotaryEncoder upper_encoder = RotaryEncoder(23, 19, 18);
    static RotaryEncoder lower_encoder = RotaryEncoder(16, 27, 14);

    static DHTesp dht;

    void setup() {
        log_info("| > Setting up DHT-Temp sensor ... ");
        dht.setup(PIN_DHT_SENSOR, DHTesp::DHT11);
        log_infoln("done!");
    }

    float get_temp() {
        return dht.getTemperature() + TEMP_CORRECTION;
    }

    float get_humid() {
        return dht.getHumidity();
    }
}
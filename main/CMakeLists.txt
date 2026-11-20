idf_component_register(SRCS "ZigbeeCoordinator.c" "SensorManager.cpp" "MqttManager.cpp" "WiFiManager.cpp" "main.cpp" "Gardener_Sensors.cpp" "OtaManager.cpp" 
                    INCLUDE_DIRS "."
                    REQUIRES
                        arduino-esp32
                        esp-zigbee-lib
                        mbedtls
                        nvs_flash
                        app_update
                        DHT
                        PubSubClient)
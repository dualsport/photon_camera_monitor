/* 
 * Project myProject
 * Author: Your Name
 * Date: 
 * For comprehensive documentation and examples, please visit:
 * https://docs.particle.io/firmware/best-practices/firmware-template/
 */

// Include Particle Device OS APIs
#include "Particle.h"
#include "MQTT.h"

// Let Device OS manage the connection to the Particle Cloud
//SYSTEM_MODE(AUTOMATIC);
// Run the application and system concurrently in separate threads
SYSTEM_THREAD(ENABLED);

char program_name[] = "particle-mqtt-camera-monitor";
String device_id = System.deviceID();

int led1 = D0;
int mqtt_led = D7;
int camera_state = 0;
int mqtt_connected = 0;

// Buffersizes for storing credentials in EEPROM
// Null byte at end reduces allowed string size by one
const int eeprom_start_addr = 0;
const int mqtt_server_buff_size = 64;
const int mqtt_username_buff_size = 32;
const int mqtt_password_buff_size = 32;

char mqtt_server[mqtt_server_buff_size];
char mqtt_username[mqtt_username_buff_size];
char mqtt_password[mqtt_password_buff_size];

const int mqtt_server_offset = eeprom_start_addr;
const int mqtt_username_offset = mqtt_server_offset + mqtt_server_buff_size;
const int mqtt_password_offset = mqtt_username_offset + mqtt_username_buff_size;

// MQTT
void callback(char* topic, byte* payload, unsigned int length);
// mqtt_server is null at this point, setBroker in setup() after load_mqtt_config()
MQTT client(mqtt_server, 1883, callback);

// Below callback handles received messages
void callback(char* topic, byte* payload, unsigned int length) {
    char p[length + 1];
    memcpy(p, payload, length);
    p[length] = NULL;

    if(!strcmp(p, "start")) {
        camera_state = 1;
    }
    else {
        camera_state = 0;
    }
}

void load_mqtt_config() {
    {
        char stringBuf[mqtt_server_buff_size];
        EEPROM.get(mqtt_server_offset, stringBuf);
        stringBuf[sizeof(stringBuf) - 1] = 0; // make sure it's null terminated
        for (unsigned int i = 0; i < sizeof(stringBuf); i++) mqtt_server[i] = stringBuf[i];
        client.setBroker(mqtt_server, 1883);
    }
    {
        char stringBuf[mqtt_username_buff_size];
        EEPROM.get(mqtt_username_offset, stringBuf);
        stringBuf[sizeof(stringBuf) - 1] = 0;
        for (unsigned int i = 0; i < sizeof(stringBuf); i++) mqtt_username[i] = stringBuf[i];
    }
    {
        char stringBuf[mqtt_password_buff_size];
        EEPROM.get(mqtt_password_offset, stringBuf);
        stringBuf[sizeof(stringBuf) - 1] = 0;
        for (unsigned int i = 0; i < sizeof(stringBuf); i++) mqtt_password[i] = stringBuf[i];
    }
}

int set_mqtt_server(String new_mqtt_server) {
    int addr = mqtt_server_offset;
    char stringBuf[mqtt_server_buff_size];
    // getBytes handles truncating the string if it's longer than the buffer.
    new_mqtt_server.getBytes((unsigned char *)stringBuf, sizeof(stringBuf));
    EEPROM.put(addr, stringBuf);
    load_mqtt_config();
    return 1;
}

int set_mqtt_useranme(String new_user_name) {
    int addr = mqtt_username_offset;
    char stringBuf[mqtt_username_buff_size];
    new_user_name.getBytes((unsigned char *)stringBuf, sizeof(stringBuf));
    EEPROM.put(addr, stringBuf);
    load_mqtt_config();
    return 1;
}

int set_mqtt_password(String new_password) {
    int addr = mqtt_password_offset;
    char stringBuf[mqtt_password_buff_size];
    new_password.getBytes((unsigned char *)stringBuf, sizeof(stringBuf));
    EEPROM.put(addr, stringBuf);
    load_mqtt_config();
    return 1;
}

void setup() {
    load_mqtt_config();
    client.setBroker(mqtt_server, 1883);

    Particle.publish("status", "start", PRIVATE);
    Particle.publish("program_name", program_name, PRIVATE);
    Particle.publish("device_id", device_id.c_str(), PRIVATE);

    Particle.function("set_mqtt_server", set_mqtt_server);
    Particle.function("set_mqtt_useranme", set_mqtt_useranme);
    Particle.function("set_mqtt_password", set_mqtt_password);

    Particle.variable("Program name", program_name);
    Particle.variable("MQTT Server", mqtt_server);

    // Wait until WiFi is ready
    do {
        delay(50);
    } while ((!WiFi.ready()));
    // MQTT connect
    client.connect(device_id.c_str(), mqtt_username, mqtt_password);
    // MQTT publish
    if (client.isConnected()) {
        client.publish(String::format("%s/message", device_id.c_str()),"MQTT Connected");
        client.subscribe("security/camera/backyard/recordstatus");
        String message = String::format("Succes for server %s", mqtt_server);
        Particle.publish("MQTT Connection Status", message, PRIVATE);
    }
    else {
        String message = String::format("Failed for server %s", mqtt_server);
        Particle.publish("MQTT Connection Status", message, PRIVATE);
    }

    pinMode(led1, OUTPUT);
    pinMode(mqtt_led, OUTPUT);
    digitalWrite(led1, LOW);
    delay(2000);
}


void loop() {
    if (client.isConnected()) {
        client.loop();
        mqtt_connected = 1;
    }
    else {
        mqtt_connected = 0;
        client.connect(device_id.c_str(), mqtt_username, mqtt_password);
        if (client.isConnected()) {
            client.publish(String::format("%s/message", device_id.c_str()),"MQTT Reonnected");
            client.subscribe("security/camera/backyard/recordstatus");
        }
    }

    // Camera recording indicator LED
    if (camera_state == 1) {
        digitalWrite(led1, HIGH);
        delay(2000);
        digitalWrite(led1, LOW);
        delay(500);
    }

    // MQTT connection status LED
    if (mqtt_connected == 1) {
        digitalWrite(mqtt_led, HIGH);
    }
    else {
        digitalWrite(mqtt_led, LOW);
    }
}


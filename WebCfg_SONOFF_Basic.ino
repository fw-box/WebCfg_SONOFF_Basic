//
// Copyright (c) 2022 Fw-Box (https://fw-box.com)
// Author: Hartman Hsieh
//
// Description :
//   None
//
// Connections :
//
// Required Library :
//   https://github.com/fw-box/FwBox_Preferences
//   https://github.com/fw-box/FwBox_WebCfg
//   https://github.com/knolleary/pubsubclient
//   https://github.com/plapointe6/HAMqttDevice
//

#include <Wire.h>
#include "FwBox_WebCfg.h"
#include <PubSubClient.h>
#include "HAMqttDevice.h"

#define DEVICE_TYPE 36
#define FIRMWARE_VERSION "1.0.3"
#define DEV_TYPE_NAME "SWITCH"
#define PIN_SWITCH 12
#define PIN_LED 13

//
// WebCfg instance
//
FwBox_WebCfg WebCfg;

WiFiClient espClient;
PubSubClient MqttClient(espClient);
String MqttBrokerIp = "";
String MqttBrokerUsername = "";
String MqttBrokerPassword = "";
String DevName = "";

HAMqttDevice* HaDev = 0;
bool HAEnable = false;

unsigned long ReadingTime = 0;
unsigned long AttemptingMqttConnTime = 0;

void setup()
{
  Wire.begin();
  Serial.begin(115200);
  pinMode(PIN_SWITCH, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  AttemptingMqttConnTime = 0;
  
  WebCfg.earlyBegin();

  //
  // Create 5 inputs in web page.
  //
  WebCfg.setItem(0, "MQTT Broker IP", "MQTT_IP"); // string input
  WebCfg.setItem(1, "MQTT Broker Username", "MQTT_USER"); // string input
  WebCfg.setItem(2, "MQTT Broker Password", "MQTT_PASS"); // string input
  WebCfg.setItem(3, "Home Assistant", "HA_EN_DIS", ITEM_TYPE_EN_DIS); // enable/disable select input
  WebCfg.setItem(4, "Device Name", "DEV_NAME"); // string input
  WebCfg.setWiFiApMiddleName(DEV_TYPE_NAME);
  WebCfg.begin();

  //
  // Get the value of "MQTT_IP" from web input.
  //
  MqttBrokerIp = WebCfg.getItemValueString("MQTT_IP");
  Serial.printf("MQTT Broker IP = %s\n", MqttBrokerIp.c_str());

  MqttBrokerUsername = WebCfg.getItemValueString("MQTT_USER");
  Serial.printf("MQTT Broker Username = %s\n", MqttBrokerUsername.c_str());

  MqttBrokerPassword = WebCfg.getItemValueString("MQTT_PASS");
  Serial.printf("MQTT Broker Password = %s\n", MqttBrokerPassword.c_str());

  DevName = WebCfg.getItemValueString("DEV_NAME");
  Serial.printf("Device Name = %s\n", DevName.c_str());
  if (DevName.length() <= 0) {
    //DevName = "fwbox_????_";
    DevName = "fwbox_";
    DevName += DEV_TYPE_NAME;
    DevName += "_";
    String str_mac = WiFi.macAddress();
    str_mac.replace(":", "");
    if (str_mac.length() >= 12) {
      DevName = DevName + str_mac.substring(8);
      DevName.toLowerCase(); // Default device name
      Serial.printf("Auto generated Device Name = %s\n", DevName.c_str());
    }
  }

  //
  // Check the user input value of 'Home Assistant', 'Enable' or 'Disable'.
  //
  if (WebCfg.getItemValueInt("HA_EN_DIS", 0) == 1) {
    HAEnable = true;
    HaDev = new HAMqttDevice(DevName, HAMqttDevice::SWITCH, "homeassistant");

    //HaDev->enableCommandTopic();
    //HaDev->enableAttributesTopic();

    //HaDev->addConfigVar("device_class", "door")
    //HaDev->addConfigVar("retain", "false");

    Serial.println("HA Config topic : " + HaDev->getConfigTopic());
    Serial.println("HA Config payload : " + HaDev->getConfigPayload());
    Serial.println("HA State topic : " + HaDev->getStateTopic());
    Serial.println("HA Command topic : " + HaDev->getCommandTopic());
    Serial.println("HA Attributes topic : " + HaDev->getAttributesTopic());

    if (WiFi.status() == WL_CONNECTED) {
      //
      // Connect to Home Assistant MQTT broker.
      //
      int re_code = HaMqttConnect(
        MqttBrokerIp,
        MqttBrokerUsername,
        MqttBrokerPassword,
        HaDev->getConfigTopic(),
        HaDev->getConfigPayload(),
        HaDev->getCommandTopic(),
        &AttemptingMqttConnTime
      );
      Serial.printf("re_code (A) = %d\n", re_code);
      if (re_code == 0) { // Success
        //
        // Publish the switch status to home assistant server.
        //
        if (digitalRead(PIN_SWITCH) == HIGH) {
          digitalWrite(PIN_LED, HIGH);
          bool result_publish = MqttClient.publish(HaDev->getStateTopic().c_str(), "ON");
          Serial.printf("result_publish(ON) = %d\n", result_publish);
        }
        else {
          digitalWrite(PIN_LED, LOW);
          bool result_publish = MqttClient.publish(HaDev->getStateTopic().c_str(), "OFF");
          Serial.printf("result_publish(OFF) = %d\n", result_publish);
        }
      }
    }
  }
  Serial.printf("Home Assistant = %d\n", HAEnable);

} // void setup()

void loop()
{
  WebCfg.handle();
  MqttClient.loop();

  //
  // If user enable the home assistant function.
  //
  if (HAEnable == true) {
    if (WiFi.status() == WL_CONNECTED) {
      //Serial.printf("MqttClient.connected() = %d\n", MqttClient.connected());
      if (MqttClient.connected()) {
      } // END OF "if (MqttClient.connected())"
      else {
        Serial.printf("MqttClient.connected() = %d\n", MqttClient.connected());
        //Serial.print("Try to connect MQTT broker - ");
        //Serial.printf("%s, %s, %s\n", MqttBrokerIp.c_str(), MqttBrokerUsername.c_str(), MqttBrokerPassword.c_str());
        int re_code = HaMqttConnect(
          MqttBrokerIp,
          MqttBrokerUsername,
          MqttBrokerPassword,
          HaDev->getConfigTopic(),
          HaDev->getConfigPayload(),
          HaDev->getCommandTopic(),
          &AttemptingMqttConnTime
        );
        //Serial.println("Done");
        Serial.printf("re_code (B) = %d\n", re_code);
        if (re_code == 0) { // Success
          //
          // Publish the switch status to home assistant server.
          //
          if (digitalRead(PIN_SWITCH) == HIGH) {
            digitalWrite(PIN_LED, HIGH);
            bool result_publish = MqttClient.publish(HaDev->getStateTopic().c_str(), "ON");
            Serial.printf("result_publish(ON) = %d\n", result_publish);
          }
          else {
            digitalWrite(PIN_LED, LOW);
            bool result_publish = MqttClient.publish(HaDev->getStateTopic().c_str(), "OFF");
            Serial.printf("result_publish(OFF) = %d\n", result_publish);
          }
        }
      }
    } // END OF "if (WiFi.status() == WL_CONNECTED)"
  } // END OF "if (HAEnable == true)"

} // END OF "void loop()"

int HaMqttConnect(
      const String& brokerIp,
      const String& brokerUsername,
      const String& brokerPassword,
      const String& configTopic,
      const String& configPayload,
      const String& commandTopic,
      unsigned long* attemptingTime)
{
  //Serial.printf("HaMqttConnect : 00 millis()=%d\n", millis());
  //Serial.printf("HaMqttConnect : 00 *attemptingTime=%d\n", (*attemptingTime));
  
  if ((millis() - (*attemptingTime)) < (10 * 1000)) {
    return 5; // attemptingTime is too short
  }

  //
  // Attempt to connect
  //
  *attemptingTime = millis();

  Serial.println("HaMqttConnect : 01");
  
  if (brokerIp.length() > 0) {
    MqttClient.setServer(brokerIp.c_str(), 1883);
    MqttClient.setCallback(MqttCallback);

    if (!MqttClient.connected()) {
      Serial.print("Attempting MQTT connection...\n");
      Serial.print("MQTT Broker Ip : ");
      Serial.println(brokerIp);
      // Create a random client ID
      String str_mac = WiFi.macAddress();
      str_mac.replace(":", "");
      str_mac.toUpperCase();
      String client_id = "Fw-Box-";
      client_id += str_mac;
      Serial.println("client_id :" + client_id);

      if (MqttClient.connect(client_id.c_str(), brokerUsername.c_str(), brokerPassword.c_str())) {
        Serial.println("connected");

        Serial.printf("configTopic.c_str()=%s\n", configTopic.c_str());
        Serial.printf("configPayload.c_str()=%s\n", configPayload.c_str());
        Serial.printf("commandTopic.c_str()=%s\n", commandTopic.c_str());
        
        bool result_publish = MqttClient.publish(configTopic.c_str(), configPayload.c_str());
        Serial.printf("result_publish=%d\n", result_publish);
      
        bool result_subscribe = MqttClient.subscribe(commandTopic.c_str());
        Serial.printf("result_subscribe=%d\n", result_subscribe);
        return 0; // Success
      }
      else {
        Serial.print("failed, rc=");
        Serial.print(MqttClient.state());
        Serial.println(" try again in 5 seconds");
        return 1; // Failed
      }
    } // END OF "if (!MqttClient.connected())"

  } // END OF "if (brokerIp.length() > 0)"
  else {
    return 2; // Failed, broker IP is empty.
  }

  return 0; // Success
}

//
// Callback function for MQTT subscribe.
//
void MqttCallback(char* topic, byte* payload, unsigned int length) {
  String str_from_ha = "";
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    str_from_ha += (char)payload[i];
  }
  Serial.println(str_from_ha);
  if (str_from_ha.equals("ON")) {
    digitalWrite(PIN_SWITCH, HIGH);
    digitalWrite(PIN_LED, HIGH);
    bool result_publish = MqttClient.publish(HaDev->getStateTopic().c_str(), "ON");
    Serial.printf("result_publish(ON) = %d\n", result_publish);
  }
  else {
    digitalWrite(PIN_SWITCH, LOW);
    digitalWrite(PIN_LED, LOW);
    bool result_publish = MqttClient.publish(HaDev->getStateTopic().c_str(), "OFF");
    Serial.printf("result_publish(OFF) = %d\n", result_publish);
  }

}

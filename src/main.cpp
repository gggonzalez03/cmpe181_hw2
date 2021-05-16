#include <Arduino.h>
#include <ArduinoJson.h>

#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <MQTTClient.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "secrets/secrets.h"

#define AWS_IOT_PUB_TOPIC "thing/esp32/hw2_pub"
#define AWS_IOT_SUB_TOPIC "thing/esp32/hw2_sub"

WiFiClientSecure net  = WiFiClientSecure();
MQTTClient client = MQTTClient(256);

QueueHandle_t touch_data_q;

void connect_to_wifi(void);
void connect_to_aws(void);
void publish_touch_to_topic(void* parameter);
void message_received(String &topic, String &payload);

/**
 * FreeRTOS Tasks
 **/ 
void main_task(void* parameter);
void send_touch_sensor_data_to_aws(void* parameter);

void setup()
{
  Serial.begin(115200);

  connect_to_wifi();
  connect_to_aws();

  touch_data_q = xQueueCreate(1, sizeof(uint16_t));

  xTaskCreate(main_task, "main_task", 4000, NULL, 1, NULL);
  xTaskCreate(send_touch_sensor_data_to_aws, "touch_aws", 10000, NULL, 1, NULL);

  // This is already called by esp-idf, calling this again crashes the program
  // vTaskStartScheduler();
}

void loop()
{
  vTaskDelay(portMAX_DELAY);
}

void connect_to_wifi(void)
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWiFi connected");
}

void connect_to_aws(void)
{
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  client.begin(AWS_IOT_ENDPOINT, 8883, net);
  client.onMessage(message_received);

  Serial.println("Connecting to AWS IoT...");

  while (!client.connect(THINGNAME))
  {
    Serial.print(".");
    delay(500);
  }

  if (!client.connected())
  {
    Serial.println("AWS IoT timeout");
  }

  client.subscribe(AWS_IOT_SUB_TOPIC);

  Serial.println("AWS IoT connected");
}

void publish_touch_to_topic(void* parameter)
{
  uint16_t* touch = (uint16_t*)parameter;

  StaticJsonDocument<200> json_doc;
  json_doc["timestamp"] = millis();
  json_doc["data"] = *touch;

  char buffer[512];
  serializeJson(json_doc, buffer);

  client.publish(AWS_IOT_PUB_TOPIC, buffer);
}

void message_received(String &topic, String &payload) {
  Serial.println("Message received from AWS");
}

void main_task(void* parameter)
{
  uint16_t touch = 0;

  while (1)
  {
    touch = touchRead(4);

    if (touch < 20)
    {
      xQueueSend(touch_data_q, &touch, portMAX_DELAY);
      vTaskDelay(500);
    }
  }
}

void send_touch_sensor_data_to_aws(void* parameter)
{
  uint16_t touch = 0;

  while (1)
  {
    if (xQueueReceive(touch_data_q, &touch, portMAX_DELAY))
    {
      printf("Sending to AWS: %d\n", touch);
      publish_touch_to_topic((void *)&touch);
    }
  }
}
/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    a7670e.h
  * @brief   Minimal A7670E (SIMCom) AT + MQTT helper.
  ******************************************************************************
  * @attention
  *
  * This module drives an A7670E cellular modem over UART using AT commands and
  * publishes JSON payloads to an MQTT broker.
  *
  * Default broker settings match the provided MQTTX screenshot:
  *   - Host: broker.emqx.io
  *   - Port: 1883 (no TLS)
  *   - Topic: pv/device_001/data
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef A7670E_H
#define A7670E_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void A7670E_Init(void);
void A7670E_Process(void);

bool A7670E_IsNetworkReady(void);
bool A7670E_IsMqttConnected(void);

/* Queue a publish (keeps the latest payload if called repeatedly). */
bool A7670E_RequestPublish(const char *topic, const char *payload, size_t payload_len, uint8_t qos);

/* Convenience: publish to default topic with QoS0. */
bool A7670E_RequestPublishDefault(const char *payload, size_t payload_len);

/* Subscribe to a topic (called once after MQTT connect). */
void A7670E_SetSubscribeTopic(const char *topic);

/* Register callback for incoming MQTT messages. */
typedef void (*A7670E_MsgCallback_t)(const char *topic, const char *payload, uint16_t payload_len);
void A7670E_RegisterMsgCallback(A7670E_MsgCallback_t cb);

#ifdef __cplusplus
}
#endif

#endif /* A7670E_H */


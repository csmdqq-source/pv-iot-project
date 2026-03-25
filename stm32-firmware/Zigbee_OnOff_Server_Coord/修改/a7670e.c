/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    a7670e.c
  * @brief   A7670E (SIMCom) AT + MQTT state machine (non-blocking).
  ******************************************************************************
  */
/* USER CODE END Header */

#include "a7670e.h"

#include "app_common.h"
#include "app_conf.h"
#include "dbg_trace.h"
#include "hw_if.h"
#include "main.h"
#include "stm_logging.h"
#include "stm32_seq.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ============================= User Config ============================== */

/* UART used to talk to A7670E. */
#ifndef A7670E_UART_ID
#define A7670E_UART_ID hw_lpuart1
#endif

/* Cellular APN (MUST be set to your SIM's APN if default doesn't work). */
#ifndef A7670E_APN
#define A7670E_APN "cytamobile"
#endif

/* PDP context CID used for data session. */
/* A7670E MQTT typically uses CID 1 (default) */
#ifndef A7670E_PDP_CID
#define A7670E_PDP_CID 1U
#endif

/* MQTT broker settings (from MQTTX screenshot). */
#ifndef A7670E_MQTT_HOST
#define A7670E_MQTT_HOST "broker.emqx.io"
#endif
#ifndef A7670E_MQTT_PORT
#define A7670E_MQTT_PORT 1883
#endif
#ifndef A7670E_MQTT_CLIENT_ID
#define A7670E_MQTT_CLIENT_ID "pv_device_001_stm32"
#endif
#ifndef A7670E_MQTT_KEEPALIVE_SEC
#define A7670E_MQTT_KEEPALIVE_SEC 60
#endif

/* Optional MQTT username/password (empty = no auth). */
#ifndef A7670E_MQTT_USERNAME
#define A7670E_MQTT_USERNAME ""
#endif
#ifndef A7670E_MQTT_PASSWORD
#define A7670E_MQTT_PASSWORD ""
#endif

/* Publishing defaults */
#ifndef A7670E_DEFAULT_TOPIC
#define A7670E_DEFAULT_TOPIC "pv/device_001/data"
#endif

/* Modem process tick and timeouts */
#define A7670E_PROCESS_TICK_MS        100U
#define A7670E_AT_TIMEOUT_MS          2000U   /* Increased for stability */
#define A7670E_NET_TIMEOUT_MS         3000U   /* Network registration check */
#define A7670E_PDP_TIMEOUT_MS         20000U  /* PDP activation can be slow */
#define A7670E_MQTT_START_TIMEOUT_MS  10000U  /* MQTT service start */
#define A7670E_MQTT_CONN_TIMEOUT_MS   25000U  /* MQTT connect (increased for mobile network) */
#define A7670E_PUB_RETRY_MAX          2U      /* Retry publishes twice */

/* Internal buffers */
#define A7670E_RX_RING_SIZE     1024U /* power-of-two */
#define A7670E_RX_WINDOW_SIZE   512U
#define A7670E_TOPIC_MAX        128U
#define A7670E_PAYLOAD_MAX      256U
#define A7670E_SUB_TOPIC_MAX    128U
#define A7670E_SUB_PAYLOAD_MAX  256U

#if ((A7670E_RX_RING_SIZE & (A7670E_RX_RING_SIZE - 1U)) != 0U)
#error "A7670E_RX_RING_SIZE must be power-of-two"
#endif

/* ============================= Internals =============================== */

typedef enum {
  A7670E_STATE_IDLE = 0,
  A7670E_STATE_PROBE_AT,
  A7670E_STATE_ATE0,
  A7670E_STATE_CPIN,
  A7670E_STATE_CEREG,
  A7670E_STATE_CGATT,
  A7670E_STATE_CGATT_SET,
  A7670E_STATE_CGDCONT,
  A7670E_STATE_CGACT,
  A7670E_STATE_NETCLOSE,      /* Close network first to ensure clean state */
  A7670E_STATE_NETOPEN,
  A7670E_STATE_MQTT_START,
  A7670E_STATE_MQTT_CFG_PDP,    /* Configure MQTT to use correct PDP context */
  A7670E_STATE_MQTT_REL_PRE,    /* Release old client before ACCQ (best-effort) */
  A7670E_STATE_MQTT_ACCQ,
  A7670E_STATE_MQTT_CONNECT,
  A7670E_STATE_MQTT_SUB_TOPIC_CMD,
  A7670E_STATE_MQTT_SUB_TOPIC_DATA,
  A7670E_STATE_READY,
  A7670E_STATE_PUB_TOPIC_CMD,
  A7670E_STATE_PUB_TOPIC_DATA,
  A7670E_STATE_PUB_PAYLOAD_CMD,
  A7670E_STATE_PUB_PAYLOAD_DATA,
  A7670E_STATE_PUB_SEND,
  A7670E_STATE_CLEANUP_DISC,
  A7670E_STATE_CLEANUP_REL,
  A7670E_STATE_CLEANUP_STOP,
  A7670E_STATE_BACKOFF,
} a7670e_state_t;

static volatile uint16_t rx_head = 0U;
static volatile uint16_t rx_tail = 0U;
static uint8_t rx_ring[A7670E_RX_RING_SIZE];
static uint8_t rx_byte = 0U;

static char rx_window[A7670E_RX_WINDOW_SIZE];
static uint16_t rx_window_len = 0U;

static a7670e_state_t state = A7670E_STATE_IDLE;
static uint32_t deadline_ms = 0U;
static uint32_t next_action_ms = 0U;
static uint32_t state_timeout_ms = 0U;
static bool cmd_sent = false;
static uint8_t retry = 0U;

static bool network_ready = false;
static bool mqtt_connected = false;

static bool pub_pending = false;
static char pub_topic[A7670E_TOPIC_MAX];
static uint16_t pub_topic_len = 0U;
static char pub_payload[A7670E_PAYLOAD_MAX];
static uint16_t pub_payload_len = 0U;
static uint8_t pub_qos = 0U;
static uint8_t pub_retry = 0U;

/* Subscription */
static char sub_topic[A7670E_SUB_TOPIC_MAX] = "";
static uint16_t sub_topic_len = 0U;
static bool sub_requested = false;
static bool sub_done = false;

/* Incoming message buffer */
static char rx_msg_payload[A7670E_SUB_PAYLOAD_MAX];
static uint16_t rx_msg_payload_len = 0U;

/* User callback for incoming messages */
static A7670E_MsgCallback_t msg_callback = NULL;

static uint8_t mqtt_conn_retry = 0U;
#define A7670E_MQTT_CONN_RETRY_MAX 3U

static uint8_t process_timer_id = 0U;
static bool process_timer_created = false;

/* Dynamic client ID buffer (base + unique suffix) */
static char mqtt_client_id[64];
static bool client_id_generated = false;

static void a7670e_uart_rx_cb(void);
static void a7670e_rx_window_clear(void);
static void a7670e_rx_drain_to_window(void);
static bool a7670e_rx_has(const char *token);
static bool a7670e_rx_has_any_error(void);
static void a7670e_send_str(const char *s);
static void a7670e_send_fmt(const char *fmt, ...);
static void a7670e_send_raw(const void *data, uint16_t len);
static void a7670e_state_enter(a7670e_state_t st, uint32_t timeout_ms);
static bool a7670e_parse_cereg_registered(void);
static bool a7670e_parse_cgatt_attached(bool *attached);
static bool a7670e_parse_cmqttstart_ok(void);
static bool a7670e_parse_cmqttconnect_ok(void);
static bool a7670e_parse_cmqttpub_ok(void);
static bool a7670e_is_publish_state(a7670e_state_t st);

static bool a7670e_state_allows_error_token(a7670e_state_t st);
static bool a7670e_state_uses_polling_timeout(a7670e_state_t st);

static void a7670e_tick_cb(void);
static void a7670e_generate_client_id(void);

/* ============================= Client ID generation ===================== */

static void a7670e_generate_client_id(void)
{
  if (client_id_generated) {
    return;
  }

  /* Use STM32 unique device ID to generate suffix */
  uint32_t uid0 = HAL_GetUIDw0();
  uint32_t uid1 = HAL_GetUIDw1();
  uint32_t uid2 = HAL_GetUIDw2();
  uint32_t hash = uid0 ^ uid1 ^ uid2;

  /* Format: base_id + _ + 8-char hex suffix */
  snprintf(mqtt_client_id, sizeof(mqtt_client_id), "%s_%08lX",
           A7670E_MQTT_CLIENT_ID, (unsigned long)hash);

  client_id_generated = true;
}

/* ============================= UART RX ================================== */

static inline void rb_push(uint8_t b)
{
  uint16_t next = (uint16_t)((rx_head + 1U) & (A7670E_RX_RING_SIZE - 1U));
  if (next == rx_tail) {
    /* overflow: drop byte */
    return;
  }
  rx_ring[rx_head] = b;
  rx_head = next;
}

static inline bool rb_pop(uint8_t *out)
{
  if (rx_tail == rx_head) {
    return false;
  }
  *out = rx_ring[rx_tail];
  rx_tail = (uint16_t)((rx_tail + 1U) & (A7670E_RX_RING_SIZE - 1U));
  return true;
}

static void a7670e_uart_rx_cb(void)
{
  rb_push(rx_byte);
  HW_UART_Receive_IT(A7670E_UART_ID, &rx_byte, 1U, a7670e_uart_rx_cb);
}

/* ============================= RX window ================================ */

static void a7670e_rx_window_clear(void)
{
  rx_window_len = 0U;
  rx_window[0] = '\0';
}

static void a7670e_rx_window_append(char c)
{
  if (rx_window_len + 1U >= (uint16_t)sizeof(rx_window)) {
    /* keep the last half to avoid O(N) shifts on every char */
    uint16_t keep = (uint16_t)(sizeof(rx_window) / 2U);
    memmove(rx_window, &rx_window[rx_window_len - keep], keep);
    rx_window_len = keep;
    rx_window[rx_window_len] = '\0';
  }

  rx_window[rx_window_len++] = c;
  rx_window[rx_window_len] = '\0';
}

static void a7670e_rx_drain_to_window(void)
{
  uint8_t b;
  while (rb_pop(&b)) {
    /* ignore NULs */
    if (b == 0U) {
      continue;
    }
    a7670e_rx_window_append((char)b);
  }
}

static bool a7670e_rx_has(const char *token)
{
  if (token == NULL || token[0] == '\0') {
    return false;
  }
  return (strstr(rx_window, token) != NULL);
}

static bool a7670e_rx_has_any_error(void)
{
  return a7670e_rx_has("ERROR") || a7670e_rx_has("+CME ERROR") || a7670e_rx_has("+CMS ERROR");
}

/* ============================= UART TX ================================== */

static void a7670e_send_raw(const void *data, uint16_t len)
{
  if (data == NULL || len == 0U) {
    return;
  }
  (void)HW_UART_Transmit(A7670E_UART_ID, (uint8_t *)data, len, 2000U);
}

static void a7670e_send_str(const char *s)
{
  if (s == NULL) {
    return;
  }
  a7670e_send_raw(s, (uint16_t)strlen(s));
}

static void a7670e_send_fmt(const char *fmt, ...)
{
  char buf[192];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (n <= 0) {
    return;
  }
  if (n >= (int)sizeof(buf)) {
    /* truncated; still send what we have */
    n = (int)sizeof(buf) - 1;
  }
  a7670e_send_raw(buf, (uint16_t)n);
}

/* ============================= Parsing ================================== */

static bool a7670e_parse_cereg_registered(void)
{
  /* Accept +CEREG: <n>,<stat> where stat 1=home, 5=roaming */
  const char *p = strstr(rx_window, "+CEREG:");
  if (p == NULL) {
    p = strstr(rx_window, "+CREG:");
  }
  if (p == NULL) {
    return false;
  }

  /* move to ':' */
  p = strchr(p, ':');
  if (p == NULL) {
    return false;
  }
  p++;
  while (*p == ' ' || *p == '\t') {
    p++;
  }

  /* skip <n> */
  while (*p && isdigit((unsigned char)*p)) {
    p++;
  }
  if (*p != ',') {
    return false;
  }
  p++;
  while (*p == ' ' || *p == '\t') {
    p++;
  }

  int stat = -1;
  stat = (int)strtol(p, NULL, 10);
  return (stat == 1) || (stat == 5);
}

static bool a7670e_parse_cgatt_attached(bool *attached)
{
  const char *p = strstr(rx_window, "+CGATT:");
  if (p == NULL) {
    return false;
  }
  p = strchr(p, ':');
  if (p == NULL) {
    return false;
  }
  p++;
  while (*p == ' ' || *p == '\t') {
    p++;
  }
  int v = (int)strtol(p, NULL, 10);
  if (attached != NULL) {
    *attached = (v == 1);
  }
  return true;
}

static bool a7670e_parse_cmqttstart_ok(void)
{
  /* Success: +CMQTTSTART: 0; already started: 23 */
  const char *p = strstr(rx_window, "+CMQTTSTART:");
  if (p == NULL) {
    return false;
  }
  p = strchr(p, ':');
  if (p == NULL) {
    return false;
  }
  p++;
  while (*p == ' ' || *p == '\t') {
    p++;
  }
  int code = (int)strtol(p, NULL, 10);
  return (code == 0) || (code == 23);
}

static bool a7670e_parse_cmqttconnect_ok(void)
{
  const char *p = strstr(rx_window, "+CMQTTCONNECT:");
  if (p == NULL) {
    return false;
  }
  /* +CMQTTCONNECT: <client>,<result> */
  p = strchr(p, ':');
  if (p == NULL) {
    return false;
  }
  p++;
  while (*p == ' ' || *p == '\t') {
    p++;
  }
  /* skip client id */
  while (*p && isdigit((unsigned char)*p)) {
    p++;
  }
  if (*p != ',') {
    return false;
  }
  p++;
  while (*p == ' ' || *p == '\t') {
    p++;
  }
  int result = (int)strtol(p, NULL, 10);
  return result == 0;
}

static bool a7670e_parse_cmqttpub_ok(void)
{
  const char *p = strstr(rx_window, "+CMQTTPUB:");
  if (p == NULL) {
    return false;
  }
  /* +CMQTTPUB: <client>,<result> */
  p = strchr(p, ':');
  if (p == NULL) {
    return false;
  }
  p++;
  while (*p == ' ' || *p == '\t') {
    p++;
  }
  while (*p && isdigit((unsigned char)*p)) {
    p++;
  }
  if (*p != ',') {
    return false;
  }
  p++;
  while (*p == ' ' || *p == '\t') {
    p++;
  }
  int result = (int)strtol(p, NULL, 10);
  return result == 0;
}

/* Parse CMQTTCONNECT error code: +CMQTTCONNECT: <client>,<result>
 * Returns: -1 if not found, otherwise the result code
 * Result codes: 0=success, 1=other error, 2=invalid param, 3=network fail, 4=network not open, 5=not MQTT conn
 */
static int a7670e_parse_cmqttconnect_result(void)
{
  const char *p = strstr(rx_window, "+CMQTTCONNECT:");
  if (p == NULL) {
    return -1;
  }
  p = strchr(p, ':');
  if (p == NULL) {
    return -1;
  }
  p++;
  while (*p == ' ' || *p == '\t') {
    p++;
  }
  /* skip client id */
  while (*p && isdigit((unsigned char)*p)) {
    p++;
  }
  if (*p != ',') {
    return -1;
  }
  p++;
  while (*p == ' ' || *p == '\t') {
    p++;
  }
  return (int)strtol(p, NULL, 10);
}

/* ============================= State helpers ============================ */

static void a7670e_state_enter(a7670e_state_t st, uint32_t timeout_ms)
{
  state = st;
  state_timeout_ms = timeout_ms;
  deadline_ms = 0U;
  cmd_sent = false;
  a7670e_rx_window_clear();
}

static void a7670e_schedule_backoff(uint32_t ms)
{
  next_action_ms = HAL_GetTick() + ms;
  a7670e_state_enter(A7670E_STATE_BACKOFF, 0U);
}

static bool a7670e_state_allows_error_token(a7670e_state_t st)
{
  /* Some commands may legitimately return ERROR on certain firmware. */
  switch (st) {
    case A7670E_STATE_NETCLOSE:      /* Close may fail if not open */
    case A7670E_STATE_NETOPEN:
    case A7670E_STATE_MQTT_START:    /* +CMQTTSTART: 23 may come with ERROR */
    case A7670E_STATE_MQTT_CFG_PDP:  /* Config may fail if already set */
    case A7670E_STATE_MQTT_REL_PRE:  /* Release may fail if no client exists */
    case A7670E_STATE_MQTT_ACCQ:     /* ACCQ may fail if client already exists */
    case A7670E_STATE_MQTT_CONNECT:  /* Connect may fail, handle with retry logic */
      return true;
    default:
      return false;
  }
}

static bool a7670e_state_uses_polling_timeout(a7670e_state_t st)
{
  /* These states intentionally retry on timeout instead of forcing cleanup. */
  switch (st) {
    case A7670E_STATE_PROBE_AT:
    case A7670E_STATE_CEREG:
      return true;
    default:
      return false;
  }
}

static bool a7670e_is_publish_state(a7670e_state_t st)
{
  return (st >= A7670E_STATE_PUB_TOPIC_CMD) && (st <= A7670E_STATE_PUB_SEND);
}

static void a7670e_tick_cb(void)
{
  UTIL_SEQ_SetTask(1U << CFG_TASK_MODEM_PROCESS, CFG_SCH_PRIO_0);
}

/* ============================= Public API =============================== */

void A7670E_Init(void)
{
  /* Ensure LPUART1 is initialized (PB5/PC0) */
  MX_LPUART1_UART_Init();

  rx_head = rx_tail = 0U;
  a7670e_rx_window_clear();

  network_ready = false;
  mqtt_connected = false;
  retry = 0U;
  pub_retry = 0U;

  /* Generate unique client ID based on MCU UID */
  a7670e_generate_client_id();

  /* start RX */
  HW_UART_Receive_IT(A7670E_UART_ID, &rx_byte, 1U, a7670e_uart_rx_cb);

  /* register periodic process task + timer */
  UTIL_SEQ_RegTask(1U << CFG_TASK_MODEM_PROCESS, UTIL_SEQ_RFU, A7670E_Process);
  if (!process_timer_created) {
    if (HW_TS_Create(CFG_TIM_PROC_ID_ISR, &process_timer_id, hw_ts_Repeated, a7670e_tick_cb) == hw_ts_Successful) {
      process_timer_created = true;
    } else {
      APP_DBG("[A7670E] ERROR: HW_TS_Create failed");
    }
  }
  if (process_timer_created) {
    uint32_t timer_ticks = (A7670E_PROCESS_TICK_MS * 1000U) / CFG_TS_TICK_VAL;
    if (timer_ticks == 0U) {
      timer_ticks = 1U;
    }
    HW_TS_Start(process_timer_id, timer_ticks);
  }

  APP_DBG("[A7670E] init broker=%s:%u id=%s apn=%s cid=%u",
          A7670E_MQTT_HOST,
          (unsigned int)A7670E_MQTT_PORT,
          mqtt_client_id,  /* Use unique generated client ID */
          A7670E_APN,
          (unsigned int)A7670E_PDP_CID);

  a7670e_state_enter(A7670E_STATE_PROBE_AT, A7670E_AT_TIMEOUT_MS);
}

bool A7670E_IsNetworkReady(void)
{
  return network_ready;
}

bool A7670E_IsMqttConnected(void)
{
  return mqtt_connected;
}

bool A7670E_RequestPublish(const char *topic, const char *payload, size_t payload_len, uint8_t qos)
{
  if (topic == NULL || payload == NULL) {
    return false;
  }
  /* Prevent payload/length overwrite while a previous publish is in progress. */
  if (pub_pending || a7670e_is_publish_state(state)) {
    return false;
  }
  size_t tlen = strlen(topic);
  if (tlen == 0U || tlen >= A7670E_TOPIC_MAX) {
    return false;
  }
  if (payload_len == 0U || payload_len >= A7670E_PAYLOAD_MAX) {
    return false;
  }

  memcpy(pub_topic, topic, tlen);
  pub_topic[tlen] = '\0';
  pub_topic_len = (uint16_t)tlen;

  memcpy(pub_payload, payload, payload_len);
  pub_payload[payload_len] = '\0';
  pub_payload_len = (uint16_t)payload_len;

  pub_qos = qos;
  pub_retry = 0U;
  pub_pending = true;
  return true;
}

bool A7670E_RequestPublishDefault(const char *payload, size_t payload_len)
{
  return A7670E_RequestPublish(A7670E_DEFAULT_TOPIC, payload, payload_len, 0U);
}

void A7670E_Process(void)
{
  uint32_t now = HAL_GetTick();

  a7670e_rx_drain_to_window();

  /* Backoff state: do nothing until next_action_ms */
  if (state == A7670E_STATE_BACKOFF) {
    if (now >= next_action_ms) {
      retry = 0U;
      a7670e_state_enter(A7670E_STATE_PROBE_AT, A7670E_AT_TIMEOUT_MS);
    }
    return;
  }

  /* If a strict state times out, go to cleanup/backoff */
  if ((deadline_ms != 0U) && (now > deadline_ms) && !a7670e_state_uses_polling_timeout(state) &&
      (state != A7670E_STATE_CLEANUP_DISC) && (state != A7670E_STATE_CLEANUP_REL) &&
      (state != A7670E_STATE_CLEANUP_STOP)) {
    APP_DBG("[A7670E] timeout st=%d", (int)state);
    mqtt_connected = false;
    network_ready = false;
    pub_retry = 0U;
    retry = 0U;
    a7670e_state_enter(A7670E_STATE_CLEANUP_DISC, 2000U);
  }

  /* Fast error detection */
  if (a7670e_rx_has_any_error() && !a7670e_state_allows_error_token(state) &&
      (state != A7670E_STATE_CLEANUP_DISC) &&
      (state != A7670E_STATE_CLEANUP_REL) &&
      (state != A7670E_STATE_CLEANUP_STOP)) {
    if (a7670e_is_publish_state(state) && mqtt_connected && pub_pending &&
        (pub_retry < A7670E_PUB_RETRY_MAX)) {
      pub_retry++;
      APP_DBG("[A7670E] pub retry=%u st=%d", (unsigned int)pub_retry, (int)state);
      a7670e_state_enter(A7670E_STATE_PUB_TOPIC_CMD, A7670E_AT_TIMEOUT_MS);
      return;
    }
    APP_DBG("[A7670E] ERROR st=%d", (int)state);
    mqtt_connected = false;
    network_ready = false;
    pub_retry = 0U;
    retry = 0U;
    a7670e_state_enter(A7670E_STATE_CLEANUP_DISC, 2000U);
  }

  switch (state)
  {
    case A7670E_STATE_IDLE:
      return;

    case A7670E_STATE_PROBE_AT:
      if (!cmd_sent) {
        a7670e_send_str("AT\r\n");
        cmd_sent = true;
        deadline_ms = now + state_timeout_ms;
      }
      if (a7670e_rx_has("OK")) {
        retry = 0U;
        a7670e_state_enter(A7670E_STATE_ATE0, A7670E_AT_TIMEOUT_MS);
      } else if (now > deadline_ms) {
        retry++;
        if (retry >= 10U) {
          APP_DBG("[A7670E] no AT response, backoff");
          a7670e_schedule_backoff(2000U);
        } else {
          a7670e_rx_window_clear();
          cmd_sent = false;
          deadline_ms = 0U;
        }
      }
      break;

    case A7670E_STATE_ATE0:
      if (!cmd_sent) {
        a7670e_send_str("ATE0\r\n");
        cmd_sent = true;
        deadline_ms = now + state_timeout_ms;
      }
      if (a7670e_rx_has("OK")) {
        a7670e_state_enter(A7670E_STATE_CPIN, A7670E_AT_TIMEOUT_MS);
      }
      break;

    case A7670E_STATE_CPIN:
      if (!cmd_sent) {
        a7670e_send_str("AT+CPIN?\r\n");
        cmd_sent = true;
        deadline_ms = now + state_timeout_ms;
      }
      if (a7670e_rx_has("READY") && a7670e_rx_has("OK")) {
        a7670e_state_enter(A7670E_STATE_CEREG, A7670E_NET_TIMEOUT_MS);
      }
      break;

    case A7670E_STATE_CEREG:
      if (!cmd_sent) {
        a7670e_send_str("AT+CEREG?\r\n");
        cmd_sent = true;
        deadline_ms = now + state_timeout_ms;
      }
      if (a7670e_parse_cereg_registered()) {
        network_ready = true;
        a7670e_state_enter(A7670E_STATE_CGATT, A7670E_NET_TIMEOUT_MS);
      } else if (now > deadline_ms) {
        a7670e_rx_window_clear();
        cmd_sent = false;
        deadline_ms = 0U;
        /* poll again */
      }
      break;

    case A7670E_STATE_CGATT: {
      bool attached = false;
      if (!cmd_sent) {
        a7670e_send_str("AT+CGATT?\r\n");
        cmd_sent = true;
        deadline_ms = now + state_timeout_ms;
      }
      if (a7670e_parse_cgatt_attached(&attached) && a7670e_rx_has("OK")) {
        if (attached) {
          a7670e_state_enter(A7670E_STATE_CGDCONT, A7670E_NET_TIMEOUT_MS);
        } else {
          a7670e_state_enter(A7670E_STATE_CGATT_SET, A7670E_PDP_TIMEOUT_MS);
        }
      } else if (a7670e_rx_has("OK") && a7670e_rx_has("+CGATT: 1")) {
        a7670e_state_enter(A7670E_STATE_CGDCONT, A7670E_NET_TIMEOUT_MS);
      }
      break;
    }

    case A7670E_STATE_CGATT_SET:
      if (!cmd_sent) {
        a7670e_send_str("AT+CGATT=1\r\n");
        cmd_sent = true;
        deadline_ms = now + state_timeout_ms;
      }
      if (a7670e_rx_has("OK")) {
        a7670e_state_enter(A7670E_STATE_CGDCONT, A7670E_NET_TIMEOUT_MS);
      }
      break;

    case A7670E_STATE_CGDCONT:
      if (!cmd_sent) {
        a7670e_send_fmt("AT+CGDCONT=%u,\"IP\",\"%s\"\r\n",
                        (unsigned int)A7670E_PDP_CID,
                        A7670E_APN);
        cmd_sent = true;
        deadline_ms = now + state_timeout_ms;
      }
      if (a7670e_rx_has("OK")) {
        a7670e_state_enter(A7670E_STATE_CGACT, A7670E_PDP_TIMEOUT_MS);
      }
      break;

    case A7670E_STATE_CGACT:
      if (!cmd_sent) {
        APP_DBG("[A7670E] Activating PDP context %u", (unsigned int)A7670E_PDP_CID);
        a7670e_send_fmt("AT+CGACT=1,%u\r\n", (unsigned int)A7670E_PDP_CID);
        cmd_sent = true;
        deadline_ms = now + state_timeout_ms;
      }
      if (a7670e_rx_has("OK")) {
        APP_DBG("[A7670E] PDP activated OK");
        a7670e_state_enter(A7670E_STATE_NETCLOSE, 3000U);
      } else if (a7670e_rx_has("ERROR")) {
        APP_DBG("[A7670E] PDP activation failed, rx: %.60s", rx_window);
        /* Try NETCLOSE/NETOPEN anyway */
        a7670e_state_enter(A7670E_STATE_NETCLOSE, 3000U);
      }
      break;

    case A7670E_STATE_NETCLOSE:
      /* Close network to ensure clean state before opening */
      if (!cmd_sent) {
        APP_DBG("[A7670E] Closing network...");
        a7670e_send_str("AT+NETCLOSE\r\n");
        cmd_sent = true;
        deadline_ms = now + state_timeout_ms;
      }
      /* Proceed regardless of result */
      if (a7670e_rx_has("+NETCLOSE") || a7670e_rx_has("OK") || a7670e_rx_has("ERROR") ||
          ((deadline_ms != 0U) && (now > deadline_ms))) {
        APP_DBG("[A7670E] NETCLOSE result: %.40s", rx_window);
        a7670e_state_enter(A7670E_STATE_NETOPEN, 8000U);
      }
      break;

    case A7670E_STATE_NETOPEN:
      /* Some firmware requires NETOPEN before CMQTT. If unsupported, ignore error and continue. */
      if (!cmd_sent) {
        APP_DBG("[A7670E] Opening network...");
        a7670e_send_str("AT+NETOPEN\r\n");
        cmd_sent = true;
        deadline_ms = now + state_timeout_ms;
      }
      if (a7670e_rx_has("+NETOPEN: 0") || a7670e_rx_has("OK") || a7670e_rx_has("ERROR")) {
        APP_DBG("[A7670E] NETOPEN result: %.40s", rx_window);
        a7670e_state_enter(A7670E_STATE_MQTT_START, A7670E_MQTT_START_TIMEOUT_MS);
      }
      break;

    case A7670E_STATE_MQTT_START:
      if (!cmd_sent) {
        a7670e_send_str("AT+CMQTTSTART\r\n");
        cmd_sent = true;
        deadline_ms = now + state_timeout_ms;
      }
      if (a7670e_parse_cmqttstart_ok() || (a7670e_rx_has("OK") && a7670e_rx_has("+CMQTTSTART"))) {
        APP_DBG("[A7670E] MQTT service started");
        /* Configure MQTT to use the correct PDP context */
        a7670e_state_enter(A7670E_STATE_MQTT_CFG_PDP, A7670E_AT_TIMEOUT_MS);
      } else if (a7670e_rx_has("OK") && rx_window_len > 0U) {
        /* Some firmware replies only OK; proceed */
        a7670e_state_enter(A7670E_STATE_MQTT_CFG_PDP, A7670E_AT_TIMEOUT_MS);
      }
      break;

    case A7670E_STATE_MQTT_CFG_PDP:
      /* Configure MQTT client 0 to use PDP context CID */
      if (!cmd_sent) {
        APP_DBG("[A7670E] Configuring MQTT PDP context to CID %u", (unsigned int)A7670E_PDP_CID);
        a7670e_send_fmt("AT+CMQTTCFG=\"pdpcid\",0,%u\r\n", (unsigned int)A7670E_PDP_CID);
        cmd_sent = true;
        deadline_ms = now + state_timeout_ms;
      }
      if (a7670e_rx_has("OK") || a7670e_rx_has("ERROR")) {
        APP_DBG("[A7670E] MQTT CFG result: %.40s", rx_window);
        /* Proceed regardless of result (may already be configured) */
        a7670e_state_enter(A7670E_STATE_MQTT_REL_PRE, 2000U);
      }
      break;

    case A7670E_STATE_MQTT_REL_PRE:
      /* Best-effort release of stale client 0 before creating new one */
      if (!cmd_sent) {
        a7670e_send_str("AT+CMQTTREL=0\r\n");
        cmd_sent = true;
        deadline_ms = now + state_timeout_ms;
      }
      /* Proceed regardless of OK or ERROR (client may not exist) */
      if (a7670e_rx_has("OK") || a7670e_rx_has("ERROR") ||
          ((deadline_ms != 0U) && (now > deadline_ms))) {
        APP_DBG("[A7670E] REL_PRE done, rx: %.60s", rx_window);
        a7670e_state_enter(A7670E_STATE_MQTT_ACCQ, A7670E_AT_TIMEOUT_MS);
      }
      break;

    case A7670E_STATE_MQTT_ACCQ:
      if (!cmd_sent) {
        /* Use unique client ID to avoid conflicts on public brokers */
        APP_DBG("[A7670E] ACCQ client_id=%s", mqtt_client_id);
        a7670e_send_fmt("AT+CMQTTACCQ=0,\"%s\"\r\n", mqtt_client_id);
        cmd_sent = true;
        deadline_ms = now + state_timeout_ms;
      }
      if (a7670e_rx_has("OK")) {
        APP_DBG("[A7670E] ACCQ OK");
        a7670e_state_enter(A7670E_STATE_MQTT_CONNECT, A7670E_MQTT_CONN_TIMEOUT_MS);
      } else if (a7670e_rx_has("ERROR")) {
        /* Client may already exist; proceed to CONNECT anyway */
        APP_DBG("[A7670E] ACCQ error, rx: %.80s", rx_window);
        a7670e_state_enter(A7670E_STATE_MQTT_CONNECT, A7670E_MQTT_CONN_TIMEOUT_MS);
      }
      break;

    case A7670E_STATE_MQTT_CONNECT:
      if (!cmd_sent) {
        APP_DBG("[A7670E] MQTT CONNECT attempt %u/%u",
                (unsigned int)(mqtt_conn_retry + 1U),
                (unsigned int)A7670E_MQTT_CONN_RETRY_MAX);
        if (strlen(A7670E_MQTT_USERNAME) > 0U) {
          a7670e_send_fmt("AT+CMQTTCONNECT=0,\"tcp://%s:%u\",%u,1,\"%s\",\"%s\"\r\n",
                          A7670E_MQTT_HOST, (unsigned int)A7670E_MQTT_PORT,
                          (unsigned int)A7670E_MQTT_KEEPALIVE_SEC,
                          A7670E_MQTT_USERNAME, A7670E_MQTT_PASSWORD);
        } else {
          a7670e_send_fmt("AT+CMQTTCONNECT=0,\"tcp://%s:%u\",%u,1\r\n",
                          A7670E_MQTT_HOST, (unsigned int)A7670E_MQTT_PORT,
                          (unsigned int)A7670E_MQTT_KEEPALIVE_SEC);
        }
        cmd_sent = true;
        deadline_ms = now + state_timeout_ms;
      }
      if (a7670e_parse_cmqttconnect_ok()) {
        mqtt_connected = true;
        mqtt_conn_retry = 0U;
        APP_DBG("[A7670E] MQTT connected to %s:%u",
                A7670E_MQTT_HOST, (unsigned int)A7670E_MQTT_PORT);
        a7670e_state_enter(A7670E_STATE_READY, 0U);
      } else if (a7670e_rx_has("ERROR") || a7670e_rx_has("+CMQTTCONNECT:")) {
        /* Check for CMQTTCONNECT failure result */
        int conn_result = a7670e_parse_cmqttconnect_result();
        if (conn_result != 0) {
          mqtt_conn_retry++;
          APP_DBG("[A7670E] MQTT CONNECT failed, code=%d, retry=%u",
                  conn_result, (unsigned int)mqtt_conn_retry);

          if (conn_result == 3 || conn_result == 4) {
            /* Error 3: Network open failed, Error 4: Network not open
             * Need to re-establish network connection from PDP activation */
            APP_DBG("[A7670E] Network error, restarting from CGACT");
            mqtt_conn_retry = 0U;
            a7670e_state_enter(A7670E_STATE_CGACT, A7670E_PDP_TIMEOUT_MS);
          } else if (mqtt_conn_retry < A7670E_MQTT_CONN_RETRY_MAX) {
            /* Other errors: Release and re-acquire client, then retry connect */
            a7670e_state_enter(A7670E_STATE_MQTT_REL_PRE, 2000U);
          } else {
            /* Max retries reached, full cleanup and backoff */
            mqtt_conn_retry = 0U;
            a7670e_state_enter(A7670E_STATE_CLEANUP_DISC, 2000U);
          }
        }
      }
      break;

    /* ====== MQTT Subscribe states ====== */
    case A7670E_STATE_MQTT_SUB_TOPIC_CMD:
      if (!cmd_sent) {
        a7670e_send_fmt("AT+CMQTTSUB=0,%u,1\r\n", (unsigned int)sub_topic_len);
        cmd_sent = true;
        deadline_ms = now + A7670E_AT_TIMEOUT_MS;
      }
      if (a7670e_rx_has(">")) {
        a7670e_state_enter(A7670E_STATE_MQTT_SUB_TOPIC_DATA, 8000U);
      }
      break;

    case A7670E_STATE_MQTT_SUB_TOPIC_DATA:
      if (!cmd_sent) {
        a7670e_send_raw(sub_topic, sub_topic_len);
        cmd_sent = true;
        deadline_ms = now + 8000U;
      }
      if (a7670e_rx_has("+CMQTTSUB: 0,0") || a7670e_rx_has("OK")) {
        APP_DBG("[A7670E] Subscribed to: %s", sub_topic);
        sub_done = true;
        a7670e_state_enter(A7670E_STATE_READY, 0U);
      } else if (a7670e_rx_has("ERROR")) {
        APP_DBG("[A7670E] Subscribe failed, will retry");
        sub_done = false;
        a7670e_state_enter(A7670E_STATE_READY, 0U);
      }
      break;

    case A7670E_STATE_READY:
      if (!mqtt_connected) {
        a7670e_state_enter(A7670E_STATE_MQTT_CONNECT, A7670E_MQTT_CONN_TIMEOUT_MS);
        break;
      }

      /* Subscribe if not done yet */
      if (sub_requested && !sub_done) {
        a7670e_state_enter(A7670E_STATE_MQTT_SUB_TOPIC_CMD, A7670E_AT_TIMEOUT_MS);
        break;
      }

      /* Check for incoming MQTT messages (URC: +CMQTTRXPAYLOAD) */
      if (a7670e_rx_has("+CMQTTRXPAYLOAD:")) {
        const char *p = strstr(rx_window, "+CMQTTRXPAYLOAD:");
        if (p) {
          const char *data_start = strchr(p, '\n');
          if (data_start) {
            data_start++;
            rx_msg_payload_len = 0U;
            while (*data_start && *data_start != '\r' && *data_start != '\n'
                   && rx_msg_payload_len < A7670E_SUB_PAYLOAD_MAX - 1U) {
              rx_msg_payload[rx_msg_payload_len++] = *data_start++;
            }
            rx_msg_payload[rx_msg_payload_len] = '\0';

            if (rx_msg_payload_len > 0U && msg_callback != NULL) {
              APP_DBG("[A7670E] RX msg len=%u: %s",
                      (unsigned int)rx_msg_payload_len, rx_msg_payload);
              msg_callback(sub_topic, rx_msg_payload, rx_msg_payload_len);
            }
          }
        }
        a7670e_rx_window_clear();
      }

      if (pub_pending) {
        a7670e_state_enter(A7670E_STATE_PUB_TOPIC_CMD, A7670E_AT_TIMEOUT_MS);
      }
      break;

    case A7670E_STATE_PUB_TOPIC_CMD:
      if (!pub_pending) {
        a7670e_state_enter(A7670E_STATE_READY, 0U);
        break;
      }
      if (!cmd_sent) {
        a7670e_send_fmt("AT+CMQTTTOPIC=0,%u\r\n", (unsigned int)pub_topic_len);
        cmd_sent = true;
        deadline_ms = now + state_timeout_ms;
      }
      if (a7670e_rx_has(">")) {
        a7670e_state_enter(A7670E_STATE_PUB_TOPIC_DATA, A7670E_AT_TIMEOUT_MS);
      }
      break;

    case A7670E_STATE_PUB_TOPIC_DATA:
      if (!cmd_sent) {
        a7670e_send_raw(pub_topic, pub_topic_len);
        cmd_sent = true;
        deadline_ms = now + state_timeout_ms;
      }
      if (a7670e_rx_has("OK")) {
        a7670e_state_enter(A7670E_STATE_PUB_PAYLOAD_CMD, A7670E_AT_TIMEOUT_MS);
      }
      break;

    case A7670E_STATE_PUB_PAYLOAD_CMD:
      if (!cmd_sent) {
        a7670e_send_fmt("AT+CMQTTPAYLOAD=0,%u\r\n", (unsigned int)pub_payload_len);
        cmd_sent = true;
        deadline_ms = now + state_timeout_ms;
      }
      if (a7670e_rx_has(">")) {
        a7670e_state_enter(A7670E_STATE_PUB_PAYLOAD_DATA, 8000U);
      }
      break;

    case A7670E_STATE_PUB_PAYLOAD_DATA:
      if (!cmd_sent) {
        a7670e_send_raw(pub_payload, pub_payload_len);
        cmd_sent = true;
        deadline_ms = now + state_timeout_ms;
      }
      if (a7670e_rx_has("OK")) {
        a7670e_state_enter(A7670E_STATE_PUB_SEND, 12000U);
      }
      break;

    case A7670E_STATE_PUB_SEND:
      if (!cmd_sent) {
        /* AT+CMQTTPUB=0,<qos>,<timeout> */
        uint8_t qos = (pub_qos > 2U) ? 0U : pub_qos;
        a7670e_send_fmt("AT+CMQTTPUB=0,%u,60\r\n", (unsigned int)qos);
        cmd_sent = true;
        deadline_ms = now + state_timeout_ms;
      }
      if (a7670e_parse_cmqttpub_ok() || a7670e_rx_has("+CMQTTPUB: 0,0")) {
        APP_DBG("[A7670E] pub ok len=%u", (unsigned int)pub_payload_len);
        pub_retry = 0U;
        pub_pending = false;
        a7670e_state_enter(A7670E_STATE_READY, 0U);
      }
      break;

    /* cleanup: best-effort */
    case A7670E_STATE_CLEANUP_DISC:
      if (!cmd_sent) {
        a7670e_send_str("AT+CMQTTDISC=0,60\r\n");
        cmd_sent = true;
        deadline_ms = now + state_timeout_ms;
      }
      if (a7670e_rx_has("OK") || a7670e_rx_has("ERROR") || ((deadline_ms != 0U) && (now > deadline_ms))) {
        a7670e_state_enter(A7670E_STATE_CLEANUP_REL, 2000U);
      }
      break;

    case A7670E_STATE_CLEANUP_REL:
      if (!cmd_sent) {
        a7670e_send_str("AT+CMQTTREL=0\r\n");
        cmd_sent = true;
        deadline_ms = now + state_timeout_ms;
      }
      if (a7670e_rx_has("OK") || a7670e_rx_has("ERROR") || ((deadline_ms != 0U) && (now > deadline_ms))) {
        a7670e_state_enter(A7670E_STATE_CLEANUP_STOP, 2000U);
      }
      break;

    case A7670E_STATE_CLEANUP_STOP:
      if (!cmd_sent) {
        a7670e_send_str("AT+CMQTTSTOP\r\n");
        cmd_sent = true;
        deadline_ms = now + state_timeout_ms;
      }
      if (a7670e_rx_has("OK") || a7670e_rx_has("+CMQTTSTOP") || a7670e_rx_has("ERROR") ||
          ((deadline_ms != 0U) && (now > deadline_ms))) {
        mqtt_connected = false;
        network_ready = false;
        mqtt_conn_retry = 0U;  /* Reset retry counter after full cleanup */
        sub_done = false;      /* Re-subscribe after reconnect */
        a7670e_schedule_backoff(1500U);
      }
      break;

    default:
      break;
  }
}

/* ============================= Subscribe API ============================= */

void A7670E_SetSubscribeTopic(const char *topic)
{
  if (topic == NULL) {
    return;
  }
  uint16_t len = (uint16_t)strlen(topic);
  if (len == 0U || len >= A7670E_SUB_TOPIC_MAX) {
    return;
  }
  memcpy(sub_topic, topic, len);
  sub_topic[len] = '\0';
  sub_topic_len = len;
  sub_requested = true;
  sub_done = false;
  APP_DBG("[A7670E] Subscribe topic set: %s", sub_topic);
}

void A7670E_RegisterMsgCallback(A7670E_MsgCallback_t cb)
{
  msg_callback = cb;
}

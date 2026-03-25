/* USER CODE BEGIN Header */
/**
******************************************************************************
* File Name : App/app_zigbee.c
* Description : Zigbee Application.
******************************************************************************
* @attention
*
* Copyright (c) 2019-2021 STMicroelectronics.
* All rights reserved.
*
* This software is licensed under terms that can be found in the LICENSE file
* in the root directory of this software component.
* If no LICENSE file comes with this software, it is provided AS-IS.
*
******************************************************************************
*/
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app_common.h"
#include "app_entry.h"
#include "dbg_trace.h"
#include "app_zigbee.h"
#include "zigbee_interface.h"
#include "shci.h"
#include "stm_logging.h"
#include "app_conf.h"
#include "stm32wbxx_core_interface_def.h"
#include "zigbee_types.h"
#include "stm32_seq.h"
#include "zigbee.aps.h"
#include "zigbee.nwk.h"
#include "zigbee.zdo.h"

/* Private includes -----------------------------------------------------------*/
#include <assert.h>
#include "zcl/zcl.h"
#include "zcl/zcl.enum.h"
#include "zcl/general/zcl.onoff.h"
#include "zcl/general/zcl.basic.h"

/* USER CODE BEGIN Includes */
#include "stm32wb5mm_dk.h"
#include "stm32wb5mm_dk_lcd.h"
#include "stm32_lcd.h"
#include "hw_if.h"
#include "a7670e.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private defines -----------------------------------------------------------*/
#define APP_ZIGBEE_STARTUP_FAIL_DELAY 500U
#define CHANNEL 15

#define SW1_ENDPOINT 17
#define SNIFFER_ENDPOINT 1  // 鏂板锛氱敤浜庢姄鍙栨墍鏈夊彂寰€ EP1 鐨勬姤鏂?

/* USER CODE BEGIN PD */
#define SW1_GROUP_ADDR 0x0001
#define PERMIT_JOIN_DELAY 60 /* 10s */
#define TUYA_CLUSTER_ID 0xEF00
#define TUYA_TARGET_ENDPOINT 1U
#define TUYA_QUERY_PERIOD_MS 10000U  /* 10绉掓煡璇竴娆″姛鐜囨暟鎹?*/

#define MQTT_TOPIC          "pv/device_001/data"
#define MQTT_DEVICE_ID      "device_001"
#define MQTT_JSON_BUF_SIZE  256U
#define MQTT_DEFAULT_EFFICIENCY_X10   5U    /* 0.5 */
#define MQTT_DEFAULT_TEMPERATURE_X10 353U   /* 35.3 (备用默认值) */

/* ============================================================================
 * Tuya DP ID 配置区 - 在此修改不同的DP点进行测试
 * ============================================================================
 * 常见 TOWSMR1-40 DP 点参考:
 *   DP 1   - 开关状态 (Boolean)
 *   DP 6   - 电量数据 (8字节: 电压+电流+功率)
 *   DP 16  - 断路器控制 (Boolean)
 *   DP 17  - 电流告警阈值
 *   DP 18  - 温度 (可能, Value类型, 单位0.1°C)
 *   DP 19  - 电流值 (mA)
 *   DP 20  - 功率值 (W)
 *   DP 101 - 温度 (部分固件)
 *   DP 133 - 温度 (部分固件)
 *   DP 135 - 温度告警阈值
 * ============================================================================ */

/* 电量数据 DP (包含电压、电流、功率) */
#define TUYA_DP_POWER_DATA          6U      /* 电量组合数据, 8字节 */
#define TUYA_DP_POWER_DATA_LEN      8U      /* 电量数据长度 */

/* 断路器控制 DP */
#define TUYA_DP_BREAKER_SWITCH      16U     /* 断路器开关, Boolean */

/* 温度 DP - 修改此值尝试不同的温度DP点 */
#define TUYA_DP_TEMPERATURE         18U     /* 温度DP ID, 可尝试: 18, 101, 133 */
#define TUYA_DP_TEMPERATURE_SCALE   10U     /* 温度缩放因子: 10=0.1°C, 1=1°C */

/* 备用温度 DP (如果主DP无数据，可尝试这些) */
#define TUYA_DP_TEMPERATURE_ALT1    101U    /* 备用温度DP 1 */
#define TUYA_DP_TEMPERATURE_ALT2    133U    /* 备用温度DP 2 */

/* 是否启用备用温度DP自动探测 */
#define TUYA_TEMPERATURE_AUTO_DETECT  1U    /* 1=启用, 0=仅使用主DP */

#ifndef TUYA_VERBOSE_LOG
#define TUYA_VERBOSE_LOG 0
#endif

#if TUYA_VERBOSE_LOG
#define TUYA_LOG(...) APP_DBG(__VA_ARGS__)
#else
#define TUYA_LOG(...) do {} while (0)
#endif

/* USER CODE END PD */

/* Private macros ------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* External definition -------------------------------------------------------*/
enum ZbStatusCodeT ZbStartupWait(struct ZigBeeT *zb, struct ZbStartupT *config);
extern uint32_t LCD_Inst;

/* USER CODE BEGIN ED */
/* USER CODE END ED */

/* Private function prototypes -----------------------------------------------*/
static void APP_ZIGBEE_StackLayersInit(void);
static void APP_ZIGBEE_ConfigEndpoints(void);
static void APP_ZIGBEE_NwkForm(void);
static void APP_ZIGBEE_PermitJoin(void);
static void APP_ZIGBEE_SW2_Process(void);
static void APP_ZIGBEE_ZbZdoPermitJoinReq_cb(struct ZbZdoPermitJoinRspT* rsp, void* arg);
static void APP_ZIGBEE_ConfigGroupAddr(void);
static void APP_ZIGBEE_TraceError(const char *pMess, uint32_t ErrCode);
static void APP_ZIGBEE_CheckWirelessFirmwareInfo(void);
static int APP_ZIGBEE_DeviceAnnce_cb(struct ZigBeeT *zb, struct ZbZdoDeviceAnnceT *annce,
uint8_t seqno, void *arg);
static void APP_ZIGBEE_ActiveEpRsp(struct ZbZdoActiveEpRspT *rsp, void *arg);
static void APP_ZIGBEE_SimpleDescRsp(struct ZbZdoSimpleDescRspT *rsp, void *arg);
static void APP_ZIGBEE_OnOffClientCallback(struct ZbZclCommandRspT *rsp, void *arg);
static void APP_ZIGBEE_ReadBasicInfo(void);
static void APP_ZIGBEE_BasicReadCallback(const struct ZbZclReadRspT *readRsp, void *arg);
static enum ZclStatusCodeT APP_ZIGBEE_TuyaCommand(struct ZbZclClusterT *cluster,
struct ZbZclHeaderT *zclHdrPtr,
struct ZbApsdeDataIndT *dataIndPtr);
static void APP_ZIGBEE_TuyaDumpPayload(const uint8_t *payload, uint16_t len);
static void APP_ZIGBEE_TuyaDumpPayloadOneLine(const uint8_t *payload, uint16_t len);
static uint32_t APP_ZIGBEE_TuyaReadUintBE(const uint8_t *data, uint16_t len);
static void ParseTuya_PowerData(const uint8_t *data, uint8_t len);
static unsigned int APP_ZIGBEE_TuyaParseDpList(const uint8_t *payload, uint16_t len, uint16_t start_offset);
static void APP_ZIGBEE_HandleTuyaCurrent(uint8_t *payload, uint16_t length);
static void APP_ZIGBEE_TuyaQueryPowerData(void);
static void APP_ZIGBEE_TuyaBindReq(uint8_t endpoint);
static void APP_ZIGBEE_TuyaBindRsp(struct ZbZdoBindRspT *rsp, void *arg);
static void APP_ZIGBEE_TuyaIeeeAddrReq(uint16_t nwk_addr);
static void APP_ZIGBEE_TuyaIeeeAddrRsp(struct ZbZdoIeeeAddrRspT *rsp, void *arg);
static void APP_ZIGBEE_TuyaSendDpCommand(uint8_t dp_id, uint8_t dp_type, const uint8_t *dp_data, uint16_t dp_len);
static void APP_ZIGBEE_TuyaControlBreaker(bool on);
static void APP_ZIGBEE_UpdateBreakerStatus(bool on);
static void APP_ZIGBEE_UpdatePowerDisplay(void);
static void APP_ZIGBEE_TuyaQueryPowerTask(void);
static void APP_ZIGBEE_TuyaQueryTimerCallback(void);
static void APP_ZIGBEE_MqttControlCallback(const char *topic, const char *payload, uint16_t len);
static int APP_ZIGBEE_ApsSniffer(struct ZbApsdeDataIndT *dataInd, void *arg);
static uint32_t APP_ZIGBEE_GetTimestampSeconds(void);
static void APP_ZIGBEE_MqttUploadJson(void);

static void Wait_Getting_Ack_From_M0(void);
static void Receive_Ack_From_M0(void);
static void Receive_Notification_From_M0(void);
static void APP_ZIGBEE_ProcessNotifyM0ToM4(void);
static void APP_ZIGBEE_ProcessRequestM0ToM4(void);

/* Private variables -----------------------------------------------*/
static TL_CmdPacket_t *p_ZIGBEE_otcmdbuffer;
static TL_EvtPacket_t *p_ZIGBEE_notif_M0_to_M4;
static TL_EvtPacket_t *p_ZIGBEE_request_M0_to_M4;
static __IO uint32_t CptReceiveNotifyFromM0 = 0;
static __IO uint32_t CptReceiveRequestFromM0 = 0;

/* USER CODE BEGIN PV */
static uint32_t g_unix_time_base = 0U;
static uint32_t g_unix_time_tick = 0U;
static bool g_unix_time_valid = false;
/* USER CODE END PV */

PLACE_IN_SECTION("MB_MEM1") ALIGN(4) static TL_ZIGBEE_Config_t ZigbeeConfigBuffer;
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static TL_CmdPacket_t ZigbeeOtCmdBuffer;
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static uint8_t ZigbeeNotifRspEvtBuffer[sizeof(TL_PacketHeader_t) + TL_EVT_HDR_SIZE + 255U];
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static uint8_t ZigbeeNotifRequestBuffer[sizeof(TL_PacketHeader_t) + TL_EVT_HDR_SIZE + 255U];
uint8_t g_ot_notification_allowed = 0U;

struct zigbee_app_info
{
struct ZigBeeT *zb;
enum ZbStartType startupControl;
enum ZbStatusCodeT join_status;
uint32_t join_delay;
bool init_after_join;
bool has_init;

struct ZbZclClusterT *onOff_server;
struct ZbZclClusterT *onOff_client;
struct ZbZclClusterT *basic_client;
struct ZbZclClusterT *tuya_client;
struct ZbZdoFilterT *device_annce_filter;
struct ZbApsFilterT *aps_sniffer_filter;
uint16_t last_joined_nwkAddr;
uint64_t last_joined_extAddr;
uint8_t last_joined_endpoint;
bool tuya_bound;
uint8_t tuya_endpoint_pending;
bool breaker_on; // 鏂矾鍣ㄥ綋鍓嶇姸鎬侊細true=闂悎(ON), false=鏂紑(OFF)

/* USER CODE BEGIN - Tuya Power Data */
uint16_t tuya_voltage_dv;    // 电压，单位：0.1V (decivolt)
uint32_t tuya_current_ma;    // 电流，单位：mA
uint32_t tuya_power_w;       // 功率，单位：W
uint8_t tuya_query_timer_id; // 定时器ID，用于周期性查询功率数据
/* USER CODE END - Tuya Power Data */

/* USER CODE BEGIN - Tuya Temperature Data */
uint16_t tuya_temperature_x10;  // 温度，单位：0.1°C (例如 353 = 35.3°C)
bool tuya_temperature_valid;    // 温度数据是否有效 (已收到真实数据)
uint8_t tuya_temperature_dp_id; // 实际使用的温度DP ID (用于调试)
/* USER CODE END - Tuya Temperature Data */
};
static struct zigbee_app_info zigbee_app_info;


/* Clusters definition ---------------------------------------------------------*/
/* OnOff server 1 custom callbacks */
static enum ZclStatusCodeT onOff_server_off_cb(struct ZbZclClusterT *cluster, struct ZbZclAddrInfoT *srcInfo, void *arg);
static enum ZclStatusCodeT onOff_server_on_cb(struct ZbZclClusterT *cluster, struct ZbZclAddrInfoT *srcInfo, void *arg);
static enum ZclStatusCodeT onOff_server_toggle_cb(struct ZbZclClusterT *cluster, struct ZbZclAddrInfoT *srcInfo, void *arg);

static struct ZbZclOnOffServerCallbacksT OnOffServerCallbacks = {
.off = onOff_server_off_cb,
.on = onOff_server_on_cb,
.toggle = onOff_server_toggle_cb,
};

/* Functions Definition ------------------------------------------------------*/

/**
* @brief OnOff server off command callback
* @param clusterPtr pointer to cluster
* @param srcInfo source addr of the device that send the command
* @param arg extra arg
* @retval stack status code
*/
static enum ZclStatusCodeT onOff_server_off_cb(struct ZbZclClusterT *clusterPtr, struct ZbZclAddrInfoT *srcInfo, void *arg)
{
uint8_t endpoint;

endpoint = ZbZclClusterGetEndpoint(clusterPtr);
if (endpoint == SW1_ENDPOINT)
{
APP_DBG("ZCL CB - LED OFF");
HAL_Delay(10);
LED_Off();
(void)ZbZclAttrIntegerWrite(clusterPtr, ZCL_ONOFF_ATTR_ONOFF, 0);
}
else
{
APP_DBG("Unknown endpoint");
return ZCL_STATUS_FAILURE;
}
return ZCL_STATUS_SUCCESS;
} /* onOff_server_off_cb */

/**
* @brief OnOff server on command callback
* @param clusterPtr pointer to cluster
* @param srcInfo source addr
* @param arg extra arg
* @retval stack status code
*/
static enum ZclStatusCodeT onOff_server_on_cb(struct ZbZclClusterT *clusterPtr, struct ZbZclAddrInfoT *srcInfo, void *arg)
{
uint8_t endpoint;

endpoint = ZbZclClusterGetEndpoint(clusterPtr);
if (endpoint == SW1_ENDPOINT)
{
APP_DBG("ZCL CB - LED ON");
HAL_Delay(10);
LED_On();
(void)ZbZclAttrIntegerWrite(clusterPtr, ZCL_ONOFF_ATTR_ONOFF, 1);
}
else
{
APP_DBG("Unknown endpoint");
return ZCL_STATUS_FAILURE;
}
return ZCL_STATUS_SUCCESS;
} /* onOff_server_on_cb */

/**
* @brief OnOff server toggle 1 command callback
* @param clusterPtr pointer to cluster
* @param srcInfo source addr
* @param arg extra arg
* @retval stack status code
*/
static enum ZclStatusCodeT onOff_server_toggle_cb(struct ZbZclClusterT *clusterPtr, struct ZbZclAddrInfoT *srcInfo, void *arg)
{
uint8_t attrVal;

if (ZbZclAttrRead(clusterPtr, ZCL_ONOFF_ATTR_ONOFF, NULL,
&attrVal, sizeof(attrVal), false) != ZCL_STATUS_SUCCESS)
{
return ZCL_STATUS_FAILURE;
}

if (attrVal != 0)
{
return onOff_server_off_cb(clusterPtr, srcInfo, arg);
}
else
{
return onOff_server_on_cb(clusterPtr, srcInfo, arg);
}
} /* onOff_server_toggle_cb */


/**
* @brief Zigbee application initialization
* @param None
* @retval None
*/
void APP_ZIGBEE_Init(void)
{
SHCI_CmdStatus_t ZigbeeInitStatus;

APP_DBG("APP_ZIGBEE_Init");

/* Check the compatibility with the Coprocessor Wireless Firmware loaded */
APP_ZIGBEE_CheckWirelessFirmwareInfo();

/* Register cmdbuffer */
APP_ZIGBEE_RegisterCmdBuffer(&ZigbeeOtCmdBuffer);

/* Init config buffer and call TL_ZIGBEE_Init */
APP_ZIGBEE_TL_INIT();

/* Register task */
/* Create the different tasks */

UTIL_SEQ_RegTask(1U << (uint32_t)CFG_TASK_NOTIFY_FROM_M0_TO_M4, UTIL_SEQ_RFU, APP_ZIGBEE_ProcessNotifyM0ToM4);
UTIL_SEQ_RegTask(1U << (uint32_t)CFG_TASK_REQUEST_FROM_M0_TO_M4, UTIL_SEQ_RFU, APP_ZIGBEE_ProcessRequestM0ToM4);

/* Task associated with network creation process */
UTIL_SEQ_RegTask(1U << CFG_TASK_ZIGBEE_NETWORK_FORM, UTIL_SEQ_RFU, APP_ZIGBEE_NwkForm);

/* Task associated with button action */
UTIL_SEQ_RegTask(1U << CFG_TASK_BUTTON_SW1, UTIL_SEQ_RFU, APP_ZIGBEE_PermitJoin);
UTIL_SEQ_RegTask(1U << CFG_TASK_BUTTON_SW2, UTIL_SEQ_RFU, APP_ZIGBEE_SW2_Process);
/* USER CODE BEGIN APP_ZIGBEE_INIT */
/* Task for periodic Tuya power query */
UTIL_SEQ_RegTask(1U << CFG_TASK_TUYA_QUERY_POWER, UTIL_SEQ_RFU, APP_ZIGBEE_TuyaQueryPowerTask);
/* A7670E cellular modem (AT + MQTT) */
A7670E_Init();
/* Subscribe to control topic for remote breaker commands */
A7670E_SetSubscribeTopic("pv/device_001/control");
A7670E_RegisterMsgCallback(APP_ZIGBEE_MqttControlCallback);
/* USER CODE END APP_ZIGBEE_INIT */

/* Start the Zigbee on the CPU2 side */
ZigbeeInitStatus = SHCI_C2_ZIGBEE_Init();
/* Prevent unused argument(s) compilation warning */
UNUSED(ZigbeeInitStatus);

/* Initialize Zigbee stack layers */
APP_ZIGBEE_StackLayersInit();

} /* APP_ZIGBEE_Init */

/**
* @brief Initialize Zigbee stack layers
* @param None
* @retval None
*/
static void APP_ZIGBEE_StackLayersInit(void)
{
APP_DBG("APP_ZIGBEE_StackLayersInit");

zigbee_app_info.zb = ZbInit(0U, NULL, NULL);
assert(zigbee_app_info.zb != NULL);

// 鍒濆鍖?Tuya 鐩稿叧鍙橀噺
zigbee_app_info.tuya_bound = false;
zigbee_app_info.tuya_endpoint_pending = 0;
zigbee_app_info.last_joined_nwkAddr = 0xFFFF;
zigbee_app_info.last_joined_extAddr = 0;
zigbee_app_info.last_joined_endpoint = 0;
zigbee_app_info.breaker_on = false; // 鍒濆鐘舵€侊細鏂紑

/* USER CODE BEGIN - Initialize Power Data */
zigbee_app_info.tuya_voltage_dv = 0U;
zigbee_app_info.tuya_current_ma = 0U;
zigbee_app_info.tuya_power_w = 0U;
zigbee_app_info.tuya_query_timer_id = 0U;

/* USER CODE BEGIN - Initialize Temperature Data */
zigbee_app_info.tuya_temperature_x10 = 0U;
zigbee_app_info.tuya_temperature_valid = false;
zigbee_app_info.tuya_temperature_dp_id = 0U;
/* USER CODE END - Initialize Temperature Data */

/* 鍒涘缓鍛ㄦ湡鎬ф煡璇㈠畾鏃跺櫒 */
HW_TS_Create(CFG_TIM_PROC_ID_ISR, &zigbee_app_info.tuya_query_timer_id,
             hw_ts_Repeated, APP_ZIGBEE_TuyaQueryTimerCallback);
/* USER CODE END - Initialize Power Data */

APP_DBG("Tuya variables initialized");

// 鏄剧ず鍒濆鐘舵€?
BSP_LCD_Clear(LCD_Inst, SSD1315_COLOR_BLACK);
UTIL_LCD_DisplayStringAt(0, 0, (uint8_t*)"Zigbee Breaker", CENTER_MODE);
UTIL_LCD_DisplayStringAt(0, 16, (uint8_t*)"Initializing...", CENTER_MODE);
BSP_LCD_Refresh(LCD_Inst);

// 璁剧疆鍒濆 LED 鐘舵€侊紙钃濊壊=鏂紑锛?
LED_Set_rgb(PWM_LED_GSDATA_OFF, PWM_LED_GSDATA_OFF, PWM_LED_GSDATA_47_0);

zigbee_app_info.device_annce_filter = ZbZdoDeviceAnnceFilterRegister(
zigbee_app_info.zb, APP_ZIGBEE_DeviceAnnce_cb, NULL);
if (zigbee_app_info.device_annce_filter == NULL) {
APP_DBG("Device Announce filter registration failed");
}

/* Create the endpoint and cluster(s) */
APP_ZIGBEE_ConfigEndpoints();

/* 娉ㄥ唽 APS 灞傝繃婊ゅ櫒锛堟姄鍙栨墍鏈夊彂寰€ SNIFFER_ENDPOINT 鐨勬姤鏂囷級 */
zigbee_app_info.aps_sniffer_filter = ZbApsFilterEndpointAdd(
zigbee_app_info.zb,
SNIFFER_ENDPOINT,
ZCL_PROFILE_HOME_AUTOMATION,  // Profile ID (0x0104)
APP_ZIGBEE_ApsSniffer,
NULL
);
if (zigbee_app_info.aps_sniffer_filter == NULL) {
APP_DBG("ERROR: APS Sniffer filter registration failed!");
} else {
APP_DBG("APS Sniffer filter registered (EP=%u)", SNIFFER_ENDPOINT);
}

/* USER CODE BEGIN APP_ZIGBEE_StackLayersInit */
/* USER CODE END APP_ZIGBEE_StackLayersInit */

/* Configure the joining parameters */
zigbee_app_info.join_status = (enum ZbStatusCodeT) 0x01; /* init to error status */
zigbee_app_info.join_delay = HAL_GetTick(); /* now */
zigbee_app_info.startupControl = ZbStartTypeForm;

/* Text Feature */
BSP_LCD_Clear(LCD_Inst, SSD1315_COLOR_BLACK);
BSP_LCD_Refresh(LCD_Inst);
UTIL_LCD_DisplayStringAt(0, LINE(0), (uint8_t *)"OnOff Server Coord", CENTER_MODE);
BSP_LCD_Refresh(LCD_Inst);

/* Initialization Complete */
zigbee_app_info.has_init = true;

/* run the task */
UTIL_SEQ_SetTask(1U << CFG_TASK_ZIGBEE_NETWORK_FORM, CFG_SCH_PRIO_0);
} /* APP_ZIGBEE_StackLayersInit */

/**
* @brief Configure and register Zigbee application endpoints, onoff callbacks
* @param None
* @retval None
*/
static void APP_ZIGBEE_ConfigEndpoints(void)
{
struct ZbApsmeAddEndpointReqT req;
struct ZbApsmeAddEndpointConfT conf;

/* Endpoint: SNIFFER_ENDPOINT (鐢ㄤ簬鎺ユ敹 Tuya 鎶ユ枃) */
memset(&req, 0, sizeof(req));
req.profileId = ZCL_PROFILE_HOME_AUTOMATION;
req.deviceId = ZCL_DEVICE_ONOFF_SWITCH;
req.endpoint = SNIFFER_ENDPOINT;
ZbZclAddEndpoint(zigbee_app_info.zb, &req, &conf);
assert(conf.status == ZB_STATUS_SUCCESS);
APP_DBG("Registered SNIFFER_ENDPOINT=%u", SNIFFER_ENDPOINT);

/* Endpoint: SW1_ENDPOINT */
memset(&req, 0, sizeof(req));
req.profileId = ZCL_PROFILE_HOME_AUTOMATION;
req.deviceId = ZCL_DEVICE_ONOFF_SWITCH;
req.endpoint = SW1_ENDPOINT;
ZbZclAddEndpoint(zigbee_app_info.zb, &req, &conf);
assert(conf.status == ZB_STATUS_SUCCESS);

/* OnOff server */
zigbee_app_info.onOff_server = ZbZclOnOffServerAlloc(zigbee_app_info.zb, SW1_ENDPOINT, &OnOffServerCallbacks, NULL);
assert(zigbee_app_info.onOff_server != NULL);
ZbZclClusterEndpointRegister(zigbee_app_info.onOff_server);

/* OnOff client */
zigbee_app_info.onOff_client = ZbZclOnOffClientAlloc(zigbee_app_info.zb, SW1_ENDPOINT);
assert(zigbee_app_info.onOff_client != NULL);
ZbZclClusterEndpointRegister(zigbee_app_info.onOff_client);

/* Basic client (for reading manufacturer/model info) */
zigbee_app_info.basic_client = ZbZclBasicClientAlloc(zigbee_app_info.zb, SW1_ENDPOINT);
assert(zigbee_app_info.basic_client != NULL);
ZbZclClusterEndpointRegister(zigbee_app_info.basic_client);

/* Tuya (EF00) client for receiving DP reports */
zigbee_app_info.tuya_client = ZbZclClusterAlloc(zigbee_app_info.zb, sizeof(struct ZbZclClusterT),
(enum ZbZclClusterIdT)TUYA_CLUSTER_ID,
SW1_ENDPOINT, ZCL_DIRECTION_TO_CLIENT);
assert(zigbee_app_info.tuya_client != NULL);
zigbee_app_info.tuya_client->command = APP_ZIGBEE_TuyaCommand;
ZbZclClusterSetProfileId(zigbee_app_info.tuya_client, ZCL_PROFILE_HOME_AUTOMATION);
if (ZbZclClusterAttach(zigbee_app_info.tuya_client) != ZCL_STATUS_SUCCESS) {
APP_DBG("Tuya cluster attach failed");
} else {
ZbZclClusterEndpointRegister(zigbee_app_info.tuya_client);
}

/* USER CODE BEGIN CONFIG_ENDPOINT */
/* USER CODE END CONFIG_ENDPOINT */
} /* APP_ZIGBEE_ConfigEndpoints */

/**
* @brief Handle Zigbee network forming and joining
* @param None
* @retval None
*/
static void APP_ZIGBEE_NwkForm(void)
{
if ((zigbee_app_info.join_status != ZB_STATUS_SUCCESS) && (HAL_GetTick() >= zigbee_app_info.join_delay))
{
struct ZbStartupT config;
enum ZbStatusCodeT status;

/* Configure Zigbee Logging */
ZbSetLogging(zigbee_app_info.zb, ZB_LOG_MASK_LEVEL_5, NULL);

/* Attempt to join a zigbee network */
ZbStartupConfigGetProDefaults(&config);

/* Set the centralized network */
APP_DBG("Network config : APP_STARTUP_CENTRALIZED_COORDINATOR");
config.startupControl = zigbee_app_info.startupControl;

/* Using the default HA preconfigured Link Key */
memcpy(config.security.preconfiguredLinkKey, sec_key_ha, ZB_SEC_KEYSIZE);

config.channelList.count = 1;
config.channelList.list[0].page = 0;
config.channelList.list[0].channelMask = 1 << CHANNEL; /*Channel in use */

/* Using ZbStartupWait (blocking) */
status = ZbStartupWait(zigbee_app_info.zb, &config);

APP_DBG("ZbStartup Callback (status = 0x%02x)", status);
zigbee_app_info.join_status = status;

if (status == ZB_STATUS_SUCCESS)
{
/* USER CODE BEGIN 0 */
uint16_t short_addr = 0xffff;
uint64_t ext_addr = 0;

(void)ZbNwkGet(zigbee_app_info.zb, ZB_NWK_NIB_ID_NetworkAddress,
&short_addr, sizeof(short_addr));
ext_addr = ZbExtendedAddress(zigbee_app_info.zb);
APP_DBG("Ext Addr: 0x%016llx", (unsigned long long)ext_addr);
APP_DBG("Short Addr: 0x%04x", short_addr);

zigbee_app_info.join_delay = 0U;
zigbee_app_info.init_after_join = true;
/* flash x3 Green LED to inform the joining connection*/
LED_Set_rgb(PWM_LED_GSDATA_OFF, PWM_LED_GSDATA_47_0, PWM_LED_GSDATA_OFF);
HAL_Delay(500);
LED_Off();
HAL_Delay(500);
LED_Set_rgb(PWM_LED_GSDATA_OFF, PWM_LED_GSDATA_47_0, PWM_LED_GSDATA_OFF);
HAL_Delay(500);
LED_Off();
HAL_Delay(500);
LED_Set_rgb(PWM_LED_GSDATA_OFF, PWM_LED_GSDATA_47_0, PWM_LED_GSDATA_OFF);
HAL_Delay(500);
LED_Off();
HAL_Delay(500);
LED_Off();

// 鏄剧ず缃戠粶灏辩华鐘舵€?
BSP_LCD_Clear(LCD_Inst, SSD1315_COLOR_BLACK);
UTIL_LCD_DisplayStringAt(0, 0, (uint8_t*)"Zigbee Breaker", CENTER_MODE);
UTIL_LCD_DisplayStringAt(0, 16, (uint8_t*)"Network Ready", CENTER_MODE);
UTIL_LCD_DisplayStringAt(0, 32, (uint8_t*)"Waiting device", CENTER_MODE);
BSP_LCD_Refresh(LCD_Inst);
}
else
{
/* USER CODE END 0 */
APP_DBG("Startup failed, attempting again after a short delay (%d ms)", APP_ZIGBEE_STARTUP_FAIL_DELAY);
zigbee_app_info.join_delay = HAL_GetTick() + APP_ZIGBEE_STARTUP_FAIL_DELAY;
}
}

/* If Network forming/joining was not successful reschedule the current task to retry the process */
if (zigbee_app_info.join_status != ZB_STATUS_SUCCESS)
{
UTIL_SEQ_SetTask(1U << CFG_TASK_ZIGBEE_NETWORK_FORM, CFG_SCH_PRIO_0);
}

/* USER CODE BEGIN NW_FORM */
else
{
zigbee_app_info.init_after_join = false;

/* Assign ourselves to the group addresses */
APP_ZIGBEE_ConfigGroupAddr();

/* Since we're using group addressing (broadcast), shorten the broadcast timeout */
uint32_t bcast_timeout = 3;
ZbNwkSet(zigbee_app_info.zb, ZB_NWK_NIB_ID_NetworkBroadcastDeliveryTime, &bcast_timeout, sizeof(bcast_timeout));
}
} /* APP_ZIGBEE_NwkForm */

static int APP_ZIGBEE_DeviceAnnce_cb(struct ZigBeeT *zb, struct ZbZdoDeviceAnnceT *annce,
uint8_t seqno, void *arg)
{
struct ZbZdoActiveEpReqT req;
enum ZbStatusCodeT status;

UNUSED(zb);
UNUSED(seqno);
UNUSED(arg);

// 淇濆瓨鍦板潃
zigbee_app_info.last_joined_nwkAddr = annce->nwkAddr;
zigbee_app_info.last_joined_extAddr = annce->extAddr;

APP_DBG("Device joined: 0x%04x", annce->nwkAddr);

memset(&req, 0, sizeof(req));
req.dstNwkAddr = annce->nwkAddr;
req.nwkAddrOfInterest = annce->nwkAddr;
ZbZdoActiveEpReq(zigbee_app_info.zb, &req, APP_ZIGBEE_ActiveEpRsp, NULL);

return ZB_APS_FILTER_CONTINUE;
}

static void APP_ZIGBEE_ActiveEpRsp(struct ZbZdoActiveEpRspT *rsp, void *arg)
{
UNUSED(arg);

if (rsp->status != ZB_STATUS_SUCCESS) {
return;
}

for (uint8_t i = 0; i < rsp->activeEpCount; i++) {
struct ZbZdoSimpleDescReqT req;

if (i == 0U) {
zigbee_app_info.last_joined_endpoint = rsp->activeEpList[i];
APP_DBG(">>> Saved endpoint: %u", rsp->activeEpList[i]);
}

memset(&req, 0, sizeof(req));
req.dstNwkAddr = rsp->nwkAddr;
req.nwkAddrOfInterest = rsp->nwkAddr;
req.endpt = rsp->activeEpList[i];
ZbZdoSimpleDescReq(zigbee_app_info.zb, &req, APP_ZIGBEE_SimpleDescRsp, NULL);
}
}

static void APP_ZIGBEE_SimpleDescRsp(struct ZbZdoSimpleDescRspT *rsp, void *arg)
{
UNUSED(arg);

if (rsp->status != ZB_STATUS_SUCCESS) {
APP_DBG("Simple Descriptor Response error: 0x%02x", rsp->status);
return;
}

bool has_tuya_cluster = false;

// 妫€鏌ユ槸鍚︽湁 Tuya 闆嗙兢
for (uint8_t i = 0; i < rsp->simpleDesc.inputClusterCount; i++) {
uint16_t cluster_id = rsp->simpleDesc.inputClusterList[i];

if (cluster_id == 0x0000U) {
APP_ZIGBEE_ReadBasicInfo();
}

if (cluster_id == TUYA_CLUSTER_ID) {
has_tuya_cluster = true;
APP_DBG("Tuya device detected (0xEF00)");
}
}

// 妫€娴嬪埌 Tuya 闆嗙兢鍚庯紝鑷姩缁戝畾
if (has_tuya_cluster && !zigbee_app_info.tuya_bound) {
zigbee_app_info.tuya_endpoint_pending = rsp->simpleDesc.endpoint;

if (zigbee_app_info.last_joined_extAddr != 0U) {
APP_ZIGBEE_TuyaBindReq(rsp->simpleDesc.endpoint);
} else {
APP_ZIGBEE_TuyaIeeeAddrReq(rsp->nwkAddr);
}
}
}

/*************************************************************
* ZbStartupWait Blocking Call
*************************************************************/
struct ZbStartupWaitInfo {
bool active;
enum ZbStatusCodeT status;
};

static void ZbStartupWaitCb(enum ZbStatusCodeT status, void *cb_arg)
{
struct ZbStartupWaitInfo *info = cb_arg;

info->status = status;
info->active = false;
UTIL_SEQ_SetEvt(EVENT_ZIGBEE_STARTUP_ENDED);
} /* ZbStartupWaitCb */

/**
* @brief startup wait function
* @param zb :Zigbee device object pointer, config: startup config pointer
* @param ErrCode
* @retval zigbee status stack code
*/
enum ZbStatusCodeT ZbStartupWait(struct ZigBeeT *zb, struct ZbStartupT *config)
{
struct ZbStartupWaitInfo *info;
enum ZbStatusCodeT status;

info = malloc(sizeof(struct ZbStartupWaitInfo));
if (info == NULL) {
return ZB_STATUS_ALLOC_FAIL;
}
memset(info, 0, sizeof(struct ZbStartupWaitInfo));

info->active = true;
status = ZbStartup(zb, config, ZbStartupWaitCb, info);
if (status != ZB_STATUS_SUCCESS)
{
free(info);
return status;
}
UTIL_SEQ_WaitEvt(EVENT_ZIGBEE_STARTUP_ENDED);
status = info->status;
free(info);
return status;
} /* ZbStartupWait */

/**
* @brief Trace the error or the warning reported.
* @param ErrId :
* @param ErrCode
* @retval None
*/
void APP_ZIGBEE_Error(uint32_t ErrId, uint32_t ErrCode)
{
switch (ErrId) {
default:
APP_ZIGBEE_TraceError("ERROR Unknown ", 0);
break;
}
} /* APP_ZIGBEE_Error */

/*************************************************************
*
* LOCAL FUNCTIONS
*
*************************************************************/

/**
* @brief Warn the user that an error has occurred.
*
* @param pMess : Message associated to the error.
* @param ErrCode: Error code associated to the module (Zigbee or other module if any)
* @retval None
*/
static void APP_ZIGBEE_TraceError(const char *pMess, uint32_t ErrCode)
{
APP_DBG("**** Fatal error = %s (Err = %d)", pMess, ErrCode);
/* USER CODE BEGIN TRACE_ERROR */
while (1U == 1U)
{
UTIL_LCD_ClearStringLine(4);
UTIL_LCD_DisplayStringAt(0, LINE(4), (uint8_t *)"FATAL_ERROR", CENTER_MODE);
BSP_LCD_Refresh(0);
}
/* USER CODE END TRACE_ERROR */

} /* APP_ZIGBEE_TraceError */

/**
* @brief Check if the Coprocessor Wireless Firmware loaded supports Zigbee
* and display associated information
* @param None
* @retval None
*/
static void APP_ZIGBEE_CheckWirelessFirmwareInfo(void)
{
WirelessFwInfo_t wireless_info_instance;
WirelessFwInfo_t *p_wireless_info = &wireless_info_instance;

if (SHCI_GetWirelessFwInfo(p_wireless_info) != SHCI_Success) {
APP_ZIGBEE_Error((uint32_t)ERR_ZIGBEE_CHECK_WIRELESS, (uint32_t)ERR_INTERFACE_FATAL);
}
else {
APP_DBG("**********************************************************");
APP_DBG("WIRELESS COPROCESSOR FW:");
/* Print version */
APP_DBG("VERSION ID = %d.%d.%d", p_wireless_info->VersionMajor, p_wireless_info->VersionMinor, p_wireless_info->VersionSub);

switch (p_wireless_info->StackType) {
case INFO_STACK_TYPE_ZIGBEE_FFD:
APP_DBG("FW Type : FFD Zigbee stack");
break;
case INFO_STACK_TYPE_ZIGBEE_RFD:
APP_DBG("FW Type : RFD Zigbee stack");
break;
default:
/* No Zigbee device supported ! */
APP_ZIGBEE_Error((uint32_t)ERR_ZIGBEE_CHECK_WIRELESS, (uint32_t)ERR_INTERFACE_FATAL);
break;
}
// print the application name
char* __PathProject__ =(strstr(__FILE__, "Zigbee") ? strstr(__FILE__, "Zigbee") + 7 : __FILE__);
char *del;
if ( (strchr(__FILE__, '/')) == NULL)
{del = strchr(__PathProject__, '\\');}
else
{del = strchr(__PathProject__, '/');}
int index = (int) (del - __PathProject__);
APP_DBG("Application flashed: %*.*s",index,index,__PathProject__);
//print channel
APP_DBG("Channel used: %d", CHANNEL);
//print Link Key
APP_DBG("Link Key: %.16s", sec_key_ha);
//print Link Key value hex
char Z09_LL_string[ZB_SEC_KEYSIZE*3+1];
Z09_LL_string[0]=0;
for(int str_index=0; str_index < ZB_SEC_KEYSIZE; str_index++)
{
sprintf(&Z09_LL_string[str_index*3],"%02x ",sec_key_ha[str_index]);
}
APP_DBG("Link Key value: %s",Z09_LL_string);
//print clusters allocated
APP_DBG("Clusters allocated are:");
APP_DBG("OnOff Server on Endpoint %d",SW1_ENDPOINT);
APP_DBG("**********************************************************");
}
} /* APP_ZIGBEE_CheckWirelessFirmwareInfo */

/*************************************************************
*
* WRAP FUNCTIONS
*
*************************************************************/

void APP_ZIGBEE_RegisterCmdBuffer(TL_CmdPacket_t *p_buffer)
{
p_ZIGBEE_otcmdbuffer = p_buffer;
} /* APP_ZIGBEE_RegisterCmdBuffer */

Zigbee_Cmd_Request_t * ZIGBEE_Get_OTCmdPayloadBuffer(void)
{
return (Zigbee_Cmd_Request_t *)p_ZIGBEE_otcmdbuffer->cmdserial.cmd.payload;
} /* ZIGBEE_Get_OTCmdPayloadBuffer */

Zigbee_Cmd_Request_t * ZIGBEE_Get_OTCmdRspPayloadBuffer(void)
{
return (Zigbee_Cmd_Request_t *)((TL_EvtPacket_t *)p_ZIGBEE_otcmdbuffer)->evtserial.evt.payload;
} /* ZIGBEE_Get_OTCmdRspPayloadBuffer */

Zigbee_Cmd_Request_t * ZIGBEE_Get_NotificationPayloadBuffer(void)
{
return (Zigbee_Cmd_Request_t *)(p_ZIGBEE_notif_M0_to_M4)->evtserial.evt.payload;
} /* ZIGBEE_Get_NotificationPayloadBuffer */

Zigbee_Cmd_Request_t * ZIGBEE_Get_M0RequestPayloadBuffer(void)
{
return (Zigbee_Cmd_Request_t *)(p_ZIGBEE_request_M0_to_M4)->evtserial.evt.payload;
}

/**
* @brief This function is used to transfer the commands from the M4 to the M0.
* @param None
* @return None
*/
void ZIGBEE_CmdTransfer(void)
{
Zigbee_Cmd_Request_t *cmd_req = (Zigbee_Cmd_Request_t *)p_ZIGBEE_otcmdbuffer->cmdserial.cmd.payload;

/* Zigbee OT command cmdcode range 0x280 .. 0x3DF = 352 */
p_ZIGBEE_otcmdbuffer->cmdserial.cmd.cmdcode = 0x280U;
/* Size = otCmdBuffer->Size (Number of OT cmd arguments : 1 arg = 32bits so multiply by 4 to get size in bytes)
* + ID (4 bytes) + Size (4 bytes) */
p_ZIGBEE_otcmdbuffer->cmdserial.cmd.plen = 8U + (cmd_req->Size * 4U);

TL_ZIGBEE_SendM4RequestToM0();

/* Wait completion of cmd */
Wait_Getting_Ack_From_M0();
} /* ZIGBEE_CmdTransfer */

/**
* @brief This function is used to transfer the commands from the M4 to the M0 with notification
*
* @param None
* @return None
*/
void ZIGBEE_CmdTransferWithNotif(void)
{
g_ot_notification_allowed = 1;
ZIGBEE_CmdTransfer();
}

/**
* @brief This function is called when the M0+ acknowledge the fact that it has received a Cmd
* @param Otbuffer : a pointer to TL_EvtPacket_t
* @return None
*/
void TL_ZIGBEE_CmdEvtReceived(TL_EvtPacket_t *Otbuffer)
{
/* Prevent unused argument(s) compilation warning */
UNUSED(Otbuffer);

Receive_Ack_From_M0();
} /* TL_ZIGBEE_CmdEvtReceived */

/**
* @brief This function is called when notification from M0+ is received.
* @param Notbuffer : a pointer to TL_EvtPacket_t
* @return None
*/
void TL_ZIGBEE_NotReceived(TL_EvtPacket_t *Notbuffer)
{
p_ZIGBEE_notif_M0_to_M4 = Notbuffer;

Receive_Notification_From_M0();
} /* TL_ZIGBEE_NotReceived */

/**
* @brief This function is called before sending any ot command to the M0
* core. The purpose of this function is to be able to check if
* there are no notifications coming from the M0 core which are
* pending before sending a new ot command.
* @param None
* @retval None
*/
void Pre_ZigbeeCmdProcessing(void)
{
UTIL_SEQ_WaitEvt(EVENT_SYNCHRO_BYPASS_IDLE);
} /* Pre_ZigbeeCmdProcessing */

/**
* @brief This function waits for getting an acknowledgment from the M0.
* @param None
* @retval None
*/
static void Wait_Getting_Ack_From_M0(void)
{
UTIL_SEQ_WaitEvt(EVENT_ACK_FROM_M0_EVT);
} /* Wait_Getting_Ack_From_M0 */

/**
* @brief Receive an acknowledgment from the M0+ core.
* Each command send by the M4 to the M0 are acknowledged.
* This function is called under interrupt.
* @param None
* @retval None
*/
static void Receive_Ack_From_M0(void)
{
UTIL_SEQ_SetEvt(EVENT_ACK_FROM_M0_EVT);
} /* Receive_Ack_From_M0 */

/**
* @brief Receive a notification from the M0+ through the IPCC.
* This function is called under interrupt.
* @param None
* @retval None
*/
static void Receive_Notification_From_M0(void)
{
CptReceiveNotifyFromM0++;
UTIL_SEQ_SetTask(1U << (uint32_t)CFG_TASK_NOTIFY_FROM_M0_TO_M4, CFG_SCH_PRIO_0);
}

/**
* @brief This function is called when a request from M0+ is received.
* @param Notbuffer : a pointer to TL_EvtPacket_t
* @return None
*/
void TL_ZIGBEE_M0RequestReceived(TL_EvtPacket_t *Reqbuffer)
{
p_ZIGBEE_request_M0_to_M4 = Reqbuffer;

CptReceiveRequestFromM0++;
UTIL_SEQ_SetTask(1U << (uint32_t)CFG_TASK_REQUEST_FROM_M0_TO_M4, CFG_SCH_PRIO_0);
}

/**
* @brief Perform initialization of TL for Zigbee.
* @param None
* @retval None
*/
void APP_ZIGBEE_TL_INIT(void)
{
ZigbeeConfigBuffer.p_ZigbeeOtCmdRspBuffer = (uint8_t *)&ZigbeeOtCmdBuffer;
ZigbeeConfigBuffer.p_ZigbeeNotAckBuffer = (uint8_t *)ZigbeeNotifRspEvtBuffer;
ZigbeeConfigBuffer.p_ZigbeeNotifRequestBuffer = (uint8_t *)ZigbeeNotifRequestBuffer;
TL_ZIGBEE_Init(&ZigbeeConfigBuffer);
}

/**
* @brief Process the messages coming from the M0.
* @param None
* @retval None
*/
static void APP_ZIGBEE_ProcessNotifyM0ToM4(void)
{
if (CptReceiveNotifyFromM0 != 0)
{
/* Reset counter */
CptReceiveNotifyFromM0 = 0;
Zigbee_CallBackProcessing();
}
}

/**
* @brief Process the requests coming from the M0.
* @param None
* @return None
*/
static void APP_ZIGBEE_ProcessRequestM0ToM4(void)
{
if (CptReceiveRequestFromM0 != 0) {
CptReceiveRequestFromM0 = 0;
Zigbee_M0RequestProcessing();
}
}
/* USER CODE BEGIN FD_LOCAL_FUNCTIONS */

/**
* @brief Set group addressing mode
* @param None
* @retval None
*/
static void APP_ZIGBEE_ConfigGroupAddr(void)
{
struct ZbApsmeAddGroupReqT req;
struct ZbApsmeAddGroupConfT conf;

memset(&req, 0, sizeof(req));
req.endpt = SW1_ENDPOINT;
req.groupAddr = SW1_GROUP_ADDR;
ZbApsmeAddGroupReq(zigbee_app_info.zb, &req, &conf);

} /* APP_ZIGBEE_ConfigGroupAddr */

/**
* @brief Send the request to NWK layer a Permit to Join the network
* @param None
* @retval None
*/
static void APP_ZIGBEE_PermitJoin(void)
{
struct ZbZdoPermitJoinReqT req;
enum ZbStatusCodeT status;

memset(&req, 0, sizeof(req));
req.destAddr = ZB_NWK_ADDR_BCAST_ROUTERS;
req.tcSignificance = true;
req.duration = PERMIT_JOIN_DELAY;

APP_DBG("Send command Permit join during %ds", PERMIT_JOIN_DELAY);
status = ZbZdoPermitJoinReq(zigbee_app_info.zb, &req, APP_ZIGBEE_ZbZdoPermitJoinReq_cb, NULL);
UNUSED(status);

UTIL_SEQ_WaitEvt(EVENT_ZIGBEE_PERMIT_JOIN_REQ_RSP);
}

static void APP_ZIGBEE_ZbZdoPermitJoinReq_cb(struct ZbZdoPermitJoinRspT *rsp, void *arg)
{
UNUSED(arg);
if(rsp->status != ZB_STATUS_SUCCESS){
APP_DBG("Error, cannot set permit join duration");
} else {
APP_DBG("Permit join duration successfully changed.");
}
/* Unlock the waiting on this event */
UTIL_SEQ_SetEvt(EVENT_ZIGBEE_PERMIT_JOIN_REQ_RSP);
}

static void APP_ZIGBEE_SW2_Process(void)
{
struct ZbApsAddrT dst;
enum ZclStatusCodeT status;
static bool tuya_state = false; // 鐢ㄤ簬 Tuya 璁惧鐨勭姸鎬佸垏鎹?

if (zigbee_app_info.last_joined_nwkAddr == 0x0000 ||
zigbee_app_info.last_joined_nwkAddr == 0xffff ||
zigbee_app_info.last_joined_endpoint == 0U) {
APP_DBG("Error: Breaker not joined yet!");
return;
}

// 鏂瑰紡1: 鍏堝皾璇?Tuya 鍗忚鎺у埗
if (zigbee_app_info.tuya_bound) {
APP_DBG("");
APP_DBG("========================================");
APP_DBG("SW2 Pressed - Tuya Device Control");
APP_DBG("========================================");
APP_DBG("Binding Status: BOUND");
APP_DBG("Device Addr: 0x%04x", zigbee_app_info.last_joined_nwkAddr);
APP_DBG("Endpoint: %u", zigbee_app_info.last_joined_endpoint);
APP_DBG("========================================");
APP_DBG("");

// 鍒囨崲鏂矾鍣ㄧ姸鎬?
tuya_state = !tuya_state;
APP_DBG(">>> Toggling breaker to: %s", tuya_state ? "ON" : "OFF");
APP_ZIGBEE_TuyaControlBreaker(tuya_state);
APP_ZIGBEE_TuyaQueryPowerData();

return;
} else {
APP_DBG("");
APP_DBG("========================================");
APP_DBG("WARNING: Tuya device NOT bound yet!");
APP_DBG("========================================");
APP_DBG("Cannot send Tuya commands");
APP_DBG("Please wait for device to join...");
APP_DBG("========================================");
APP_DBG("");
}

// 鏂瑰紡2: 濡傛灉涓嶆槸 Tuya 璁惧,浣跨敤鏍囧噯 OnOff 鍛戒护
memset(&dst, 0, sizeof(dst));
dst.mode = ZB_APSDE_ADDRMODE_SHORT;
dst.endpoint = zigbee_app_info.last_joined_endpoint;
dst.nwkAddr = zigbee_app_info.last_joined_nwkAddr;

APP_DBG("Sending OnOff Toggle to breaker 0x%04x:EP%d",
dst.nwkAddr, dst.endpoint);
status = ZbZclOnOffClientToggleReq(zigbee_app_info.onOff_client, &dst,
APP_ZIGBEE_OnOffClientCallback, NULL);

if (status != ZCL_STATUS_SUCCESS) {
APP_DBG("Error sending Toggle command: 0x%02x", status);
}
}

void APP_ZIGBEE_OnOffSend(uint8_t on)
{
struct ZbApsAddrT dst;
enum ZclStatusCodeT status;

if (zigbee_app_info.tuya_bound) {
APP_DBG("Using Tuya DP control for On/Off");
APP_ZIGBEE_TuyaControlBreaker(on != 0U);
return;
}

if (zigbee_app_info.last_joined_nwkAddr == 0x0000 ||
zigbee_app_info.last_joined_nwkAddr == 0xffff ||
zigbee_app_info.last_joined_endpoint == 0U) {
APP_DBG("Error: Breaker not joined yet!");
return;
}

memset(&dst, 0, sizeof(dst));
dst.mode = ZB_APSDE_ADDRMODE_SHORT;
dst.endpoint = zigbee_app_info.last_joined_endpoint;
dst.nwkAddr = zigbee_app_info.last_joined_nwkAddr;

if (on != 0U) {
APP_DBG("Sending OnOff ON to breaker 0x%04x:EP%d",
dst.nwkAddr, dst.endpoint);
status = ZbZclOnOffClientOnReq(zigbee_app_info.onOff_client, &dst,
APP_ZIGBEE_OnOffClientCallback, NULL);
} else {
APP_DBG("Sending OnOff OFF to breaker 0x%04x:EP%d",
dst.nwkAddr, dst.endpoint);
status = ZbZclOnOffClientOffReq(zigbee_app_info.onOff_client, &dst,
APP_ZIGBEE_OnOffClientCallback, NULL);
}

if (status != ZCL_STATUS_SUCCESS) {
APP_DBG("Error sending OnOff command: 0x%02x", status);
}
}

/* USER CODE BEGIN EF */
void APP_ZIGBEE_SetUnixTime(uint32_t unix_seconds)
{
  g_unix_time_base = unix_seconds;
  g_unix_time_tick = HAL_GetTick();
  g_unix_time_valid = true;
  APP_DBG("Unix time synced: %lu", (unsigned long)unix_seconds);
}
/* USER CODE END EF */

static void APP_ZIGBEE_OnOffClientCallback(struct ZbZclCommandRspT *rsp, void *arg)
{
UNUSED(arg);

APP_DBG("OnOff Client Response received:");
APP_DBG(" Status: 0x%02x", rsp->status);
APP_DBG(" From: 0x%04x", rsp->src.nwkAddr);
}

static void APP_ZIGBEE_ReadBasicInfo(void)
{
struct ZbZclReadReqT req;
enum ZclStatusCodeT status;

if (zigbee_app_info.last_joined_nwkAddr == 0x0000 ||
zigbee_app_info.last_joined_nwkAddr == 0xffff ||
zigbee_app_info.last_joined_endpoint == 0U) {
return;
}

memset(&req, 0, sizeof(req));
req.dst.mode = ZB_APSDE_ADDRMODE_SHORT;
req.dst.endpoint = zigbee_app_info.last_joined_endpoint;
req.dst.nwkAddr = zigbee_app_info.last_joined_nwkAddr;
req.count = 2;
req.attr[0] = ZCL_BASIC_ATTR_MFR_NAME;
req.attr[1] = ZCL_BASIC_ATTR_MODEL_NAME;

status = ZbZclReadReq(zigbee_app_info.basic_client, &req,
APP_ZIGBEE_BasicReadCallback, NULL);
if (status != ZCL_STATUS_SUCCESS) {
APP_DBG("Basic read request failed: 0x%02x", status);
}
}

static void APP_ZIGBEE_BasicReadCallback(const struct ZbZclReadRspT *readRsp, void *arg)
{
UNUSED(arg);

if (readRsp->status != ZCL_STATUS_SUCCESS) {
APP_DBG("Basic read response error: 0x%02x", readRsp->status);
return;
}

for (unsigned int i = 0; i < readRsp->count; i++) {
const struct ZbZclReadRspDataT *attr = &readRsp->attr[i];
if (attr->status != ZCL_STATUS_SUCCESS || attr->value == NULL) {
continue;
}
if ((attr->type == ZCL_DATATYPE_STRING_CHARACTER ||
attr->type == ZCL_DATATYPE_STRING_LONG_CHARACTER) &&
attr->length > 0U) {
uint8_t str_len = attr->value[0];
if (str_len > (attr->length - 1U)) {
str_len = (uint8_t)(attr->length - 1U);
}
if (attr->attrId == ZCL_BASIC_ATTR_MFR_NAME) {
APP_DBG("Manufacturer: %.*s", str_len, (const char *)&attr->value[1]);
} else if (attr->attrId == ZCL_BASIC_ATTR_MODEL_NAME) {
APP_DBG("Model: %.*s", str_len, (const char *)&attr->value[1]);
}
}
}
}

static enum ZclStatusCodeT APP_ZIGBEE_TuyaCommand(struct ZbZclClusterT *cluster,
struct ZbZclHeaderT *zclHdrPtr,
struct ZbApsdeDataIndT *dataIndPtr)
{
struct ZbZclHeaderT hdr;
int hdr_len;
const uint8_t *payload = NULL;
uint16_t payload_len = 0U;
unsigned int dp_count;

// 绔嬪嵆鎵撳嵃锛岀‘璁ゅ洖璋冭瑙﹀彂
TUYA_LOG("");
TUYA_LOG("****************************************");
TUYA_LOG("*** TUYA CALLBACK TRIGGERED! (0xEF00) ***");
TUYA_LOG("****************************************");

hdr_len = ZbZclParseHeader(&hdr, dataIndPtr->asdu, dataIndPtr->asduLength);
if (hdr_len < 0) {
TUYA_LOG("Tuya EF00: invalid ZCL header");
return ZCL_STATUS_MALFORMED_COMMAND;
}

payload = &dataIndPtr->asdu[hdr_len];
if (dataIndPtr->asduLength > (uint16_t)hdr_len) {
payload_len = (uint16_t)(dataIndPtr->asduLength - (uint16_t)hdr_len);
}

TUYA_LOG("========================================");
TUYA_LOG(">>> RECEIVED Tuya 0xEF00 Cluster Message!");
TUYA_LOG(">>> Source: NwkAddr=0x%04x, EP=%u",
dataIndPtr->src.nwkAddr, dataIndPtr->src.endpoint);
TUYA_LOG(">>> ZCL Command: 0x%02x", zclHdrPtr->cmdId);
TUYA_LOG(">>> Tuya CmdId (EF00): 0x%02x", zclHdrPtr->cmdId);
TUYA_LOG(">>> Total ASDU Length: %u bytes", dataIndPtr->asduLength);
TUYA_LOG(">>> ZCL Header Length: %d bytes", hdr_len);
TUYA_LOG(">>> Tuya Payload Length: %u bytes", payload_len);
TUYA_LOG("========================================");

// 鎵撳嵃瀹屾暣鐨勫師濮?ASDU锛堝寘鎷?ZCL Header + Payload锛?
#if TUYA_VERBOSE_LOG
APP_DBG("");
APP_DBG(">>> COMPLETE RAW ASDU (HEX):");
APP_DBG(">>> [Single Line - Easy to Copy]:");
APP_ZIGBEE_TuyaDumpPayloadOneLine(dataIndPtr->asdu, dataIndPtr->asduLength);
APP_DBG(">>> [Multi Line - Detailed View]:");
APP_ZIGBEE_TuyaDumpPayload(dataIndPtr->asdu, dataIndPtr->asduLength);

// 鍗曠嫭鎵撳嵃 ZCL Header 閮ㄥ垎
APP_DBG("");
APP_DBG(">>> ZCL HEADER (HEX):");
APP_ZIGBEE_TuyaDumpPayloadOneLine(dataIndPtr->asdu, (uint16_t)hdr_len);

// 鍗曠嫭鎵撳嵃 Tuya Payload 閮ㄥ垎
APP_DBG("");
APP_DBG(">>> TUYA PAYLOAD (HEX):");
APP_ZIGBEE_TuyaDumpPayloadOneLine(payload, payload_len);
APP_DBG(">>> [Detailed View]:");
APP_ZIGBEE_TuyaDumpPayload(payload, payload_len);
#endif

APP_ZIGBEE_HandleTuyaCurrent((uint8_t *)payload, payload_len);

dp_count = APP_ZIGBEE_TuyaParseDpList(payload, payload_len, 2U);
if (dp_count == 0U) {
dp_count = APP_ZIGBEE_TuyaParseDpList(payload, payload_len, 1U);
}
if (dp_count == 0U) {
(void)APP_ZIGBEE_TuyaParseDpList(payload, payload_len, 0U);
}

return ZCL_STATUS_SUCCESS;
}

static void APP_ZIGBEE_TuyaBindReq(uint8_t endpoint)
{
struct ZbZdoBindReqT req;
enum ZbStatusCodeT status;
uint64_t coord_ext_addr;

if (zigbee_app_info.last_joined_nwkAddr == 0x0000 ||
zigbee_app_info.last_joined_nwkAddr == 0xffff ||
zigbee_app_info.last_joined_extAddr == 0U) {
APP_DBG("Bind request failed: invalid address");
return;
}

coord_ext_addr = ZbExtendedAddress(zigbee_app_info.zb);

memset(&req, 0, sizeof(req));
req.target = zigbee_app_info.last_joined_nwkAddr;
req.srcExtAddr = zigbee_app_info.last_joined_extAddr;
req.srcEndpt = endpoint;
req.clusterId = TUYA_CLUSTER_ID;
req.dst.mode = ZB_APSDE_ADDRMODE_EXT;
req.dst.extAddr = coord_ext_addr;
req.dst.endpoint = SW1_ENDPOINT;

status = ZbZdoBindReq(zigbee_app_info.zb, &req, APP_ZIGBEE_TuyaBindRsp, NULL);
if (status != ZB_STATUS_SUCCESS) {
APP_DBG("Bind request failed: 0x%02x", status);
}
}

static void APP_ZIGBEE_TuyaBindRsp(struct ZbZdoBindRspT *rsp, void *arg)
{
UNUSED(arg);

if (rsp->status == ZB_STATUS_SUCCESS) {
zigbee_app_info.tuya_bound = true;
APP_DBG("========================================");
APP_DBG("Tuya binding SUCCESS!");
APP_DBG("  Device: 0x%04x", zigbee_app_info.last_joined_nwkAddr);
APP_DBG("  Endpoint: %u", zigbee_app_info.tuya_endpoint_pending);
APP_DBG("  Cluster: 0xEF00");
APP_DBG("  Coordinator can now receive DP reports");
APP_DBG("========================================");

/* 绔嬪嵆鏌ヨ涓€娆″姛鐜囨暟鎹?*/
APP_ZIGBEE_TuyaQueryPowerData();

/* 鍚姩鍛ㄦ湡鎬ф煡璇㈠畾鏃跺櫒锛?绉掑懆鏈燂級 */
uint32_t timer_ticks = TUYA_QUERY_PERIOD_MS * 1000 / CFG_TS_TICK_VAL;
HW_TS_Start(zigbee_app_info.tuya_query_timer_id, timer_ticks);
APP_DBG("Tuya query timer started (period: %u ms)", TUYA_QUERY_PERIOD_MS);
} else {
APP_DBG("========================================");
APP_DBG("Tuya binding FAILED!");
APP_DBG("  Status: 0x%02x", rsp->status);
APP_DBG("  Cannot receive DP reports");
APP_DBG("========================================");
}
}

static void APP_ZIGBEE_TuyaIeeeAddrReq(uint16_t nwk_addr)
{
struct ZbZdoIeeeAddrReqT req;
enum ZbStatusCodeT status;

memset(&req, 0, sizeof(req));
req.dstNwkAddr = nwk_addr;
req.nwkAddrOfInterest = nwk_addr;
req.reqType = ZB_ZDO_ADDR_REQ_TYPE_SINGLE;
req.startIndex = 0U;

status = ZbZdoIeeeAddrReq(zigbee_app_info.zb, &req, APP_ZIGBEE_TuyaIeeeAddrRsp, NULL);
if (status != ZB_STATUS_SUCCESS) {
APP_DBG("IEEE addr request failed: 0x%02x", status);
}
}

static void APP_ZIGBEE_TuyaIeeeAddrRsp(struct ZbZdoIeeeAddrRspT *rsp, void *arg)
{
UNUSED(arg);

if (rsp->status != ZB_STATUS_SUCCESS) {
APP_DBG("IEEE addr response failed: 0x%02x", rsp->status);
return;
}

zigbee_app_info.last_joined_extAddr = rsp->extAddr;
zigbee_app_info.last_joined_nwkAddr = rsp->nwkAddr;

if (!zigbee_app_info.tuya_bound && zigbee_app_info.tuya_endpoint_pending != 0U) {
APP_ZIGBEE_TuyaBindReq(zigbee_app_info.tuya_endpoint_pending);
}
}

static void APP_ZIGBEE_TuyaDumpPayload(const uint8_t *payload, uint16_t len)
{
static const char hex[] = "0123456789ABCDEF";
char line[(3U * 16U) + 1U];
uint16_t offset = 0U;

if (payload == NULL || len == 0U) {
TUYA_LOG(" Payload: <empty>");
return;
}

while (offset < len) {
uint16_t chunk = (len - offset > 16U) ? 16U : (len - offset);
uint16_t pos = 0U;
for (uint16_t i = 0U; i < chunk; i++) {
uint8_t b = payload[offset + i];
line[pos++] = hex[b >> 4];
line[pos++] = hex[b & 0x0F];
if (i + 1U < chunk) {
line[pos++] = ' ';
}
}
line[pos] = '\0';
TUYA_LOG(" Payload[%u]: %s", offset, line);
offset = (uint16_t)(offset + chunk);
}
}

/**
* @brief 鍗曡杩炵画鎵撳嵃瀹屾暣鐨勫崄鍏繘鍒舵暟鎹紙鏂逛究澶嶅埗鍒嗘瀽锛?
* @param payload: 鏁版嵁鎸囬拡
* @param len: 鏁版嵁闀垮害
* @retval None
*/
static void APP_ZIGBEE_TuyaDumpPayloadOneLine(const uint8_t *payload, uint16_t len)
{
static const char hex[] = "0123456789ABCDEF";
char line[256]; // 鏈€澶氭樉绀虹害85瀛楄妭鏁版嵁
uint16_t pos = 0U;

if (payload == NULL || len == 0U) {
TUYA_LOG(" <empty>");
return;
}

// 闄愬埗鏈€澶ф樉绀洪暱搴︼紙閬垮厤缂撳啿鍖烘孩鍑猴級
uint16_t display_len = (len > 80U) ? 80U : len;

for (uint16_t i = 0U; i < display_len; i++) {
uint8_t b = payload[i];
line[pos++] = hex[b >> 4];
line[pos++] = hex[b & 0x0F];
if (i + 1U < display_len) {
line[pos++] = ' ';
}
}
line[pos] = '\0';

TUYA_LOG(" %s%s", line, (len > 80U) ? " ..." : "");
}

static uint32_t APP_ZIGBEE_TuyaReadUintBE(const uint8_t *data, uint16_t len)
{
uint32_t value = 0U;
if (data == NULL || len == 0U || len > 4U) {
return 0U;
}
for (uint16_t i = 0U; i < len; i++) {
value = (value << 8) | data[i];
}
return value;
}

/**
 * @brief 瑙ｆ瀽娑傞甫 DP 6 鐢甸噺鏁版嵁锛堢數鍘嬨€佺數娴併€佸姛鐜囷級
 * @param data: 鎸囧悜8瀛楄妭鏁版嵁鐨勬寚閽?
 * @param len: 鏁版嵁闀垮害锛屽繀椤荤瓑浜?
 * @note 鏁版嵁鏍煎紡锛堝ぇ绔簭锛夛細
 *       Byte 0-1: 鐢靛帇 (uint16, 鍗曚綅 0.1V)
 *       Byte 2-4: 鐢垫祦 (uint24, 鍗曚綅 mA)
 *       Byte 5-7: 鍔熺巼 (uint24, 鍗曚綅 W)
 * @retval None
 */
static void ParseTuya_PowerData(const uint8_t *data, uint8_t len)
{
  /* parameter check */
  if (data == NULL) {
    APP_DBG("ERROR: ParseTuya_PowerData - NULL pointer");
    return;
  }

  if (len != 8U) {
    APP_DBG("ERROR: ParseTuya_PowerData - Invalid length %u (expected 8)", len);
    return;
  }

  /* Byte 0-1: voltage (uint16 BE, 0.1V) */
  uint16_t voltage_raw = ((uint16_t)data[0] << 8) | data[1];

  /* Byte 2-4: current (uint24 BE, mA) */
  uint32_t current_ma = ((uint32_t)data[2] << 16) |
                        ((uint32_t)data[3] << 8) |
                        data[4];

  /* Byte 5-7: power (uint24 BE, W) */
  uint32_t power_w = ((uint32_t)data[5] << 16) |
                     ((uint32_t)data[6] << 8) |
                     data[7];

  /* save to global struct */
  zigbee_app_info.tuya_voltage_dv = voltage_raw;
  zigbee_app_info.tuya_current_ma = current_ma;
  zigbee_app_info.tuya_power_w = power_w;

  APP_DBG("[POWER] V=%u.%uV I=%lumA P=%luW",
          voltage_raw / 10U, voltage_raw % 10U,
          (unsigned long)current_ma, (unsigned long)power_w);

  /* 鏇存柊 LCD 鏄剧ず */
  APP_ZIGBEE_UpdatePowerDisplay();
}

/**
 * @brief  解析 Tuya 温度数据
 * @param  dp_id: 实际收到的 DP ID
 * @param  data: 原始数据指针
 * @param  len: 数据长度
 * @note   温度值支持多种格式:
 *         - 1字节: 直接温度值 (0-255)
 *         - 2字节: uint16 大端序
 *         - 4字节: uint32 大端序
 *         根据 TUYA_DP_TEMPERATURE_SCALE 进行缩放
 * @retval None
 */
static void ParseTuya_TemperatureData(uint8_t dp_id, const uint8_t *data, uint8_t len)
{
  uint32_t temp_raw = 0U;

  if (data == NULL || len == 0U || len > 4U) {
    APP_DBG("ERROR: ParseTuya_TemperatureData - Invalid params (len=%u)", len);
    return;
  }

  /* 根据长度解析大端序数值 */
  for (uint8_t i = 0U; i < len; i++) {
    temp_raw = (temp_raw << 8) | data[i];
  }

  /* 根据缩放因子转换为 0.1°C 单位 */
  uint16_t temp_x10;
  if (TUYA_DP_TEMPERATURE_SCALE == 10U) {
    /* 原始值已经是 0.1°C 单位 */
    temp_x10 = (uint16_t)temp_raw;
  } else if (TUYA_DP_TEMPERATURE_SCALE == 1U) {
    /* 原始值是 1°C 单位，转换为 0.1°C */
    temp_x10 = (uint16_t)(temp_raw * 10U);
  } else {
    /* 其他缩放因子 */
    temp_x10 = (uint16_t)((temp_raw * 10U) / TUYA_DP_TEMPERATURE_SCALE);
  }

  /* 保存到全局结构体 */
  zigbee_app_info.tuya_temperature_x10 = temp_x10;
  zigbee_app_info.tuya_temperature_valid = true;
  zigbee_app_info.tuya_temperature_dp_id = dp_id;

  APP_DBG("[TEMP] DP=%u Raw=%lu -> %u.%u°C",
          dp_id, (unsigned long)temp_raw,
          temp_x10 / 10U, temp_x10 % 10U);
}

/**
 * @brief  检查是否为温度 DP ID
 * @param  dp_id: 待检查的 DP ID
 * @retval true=是温度DP, false=不是
 */
static bool IsTuya_TemperatureDP(uint8_t dp_id)
{
  /* 检查主温度 DP */
  if (dp_id == TUYA_DP_TEMPERATURE) {
    return true;
  }

#if TUYA_TEMPERATURE_AUTO_DETECT
  /* 检查备用温度 DP */
  if (dp_id == TUYA_DP_TEMPERATURE_ALT1 || dp_id == TUYA_DP_TEMPERATURE_ALT2) {
    /* 仅在主 DP 无数据时使用备用 */
    if (!zigbee_app_info.tuya_temperature_valid) {
      return true;
    }
  }
#endif

  return false;
}

static void APP_ZIGBEE_HandleTuyaCurrent(uint8_t *payload, uint16_t length)
{
uint16_t idx = 0U;

if (payload == NULL || length < 8U) {
return;
}

while ((uint16_t)(idx + 7U) < length) {
if (payload[idx] == 0x7D && payload[idx + 1U] == 0x02) {
uint16_t dp_len = ((uint16_t)payload[idx + 2U] << 8) | payload[idx + 3U];
if (dp_len == 4U && (uint16_t)(idx + 7U) < length) {
uint32_t current_ma = ((uint32_t)payload[idx + 4U] << 24) |
((uint32_t)payload[idx + 5U] << 16) |
((uint32_t)payload[idx + 6U] << 8) |
payload[idx + 7U];
double current_a = (double)current_ma / 1000.0;
TUYA_LOG("Detected Current: %ld mA (%.3f A)", (long)current_ma, current_a);
return;
}
}
idx++;
}
}

static void APP_ZIGBEE_TuyaQueryPowerData(void)
{
struct ZbZclClusterCommandReqT req;
enum ZclStatusCodeT status;

if (zigbee_app_info.last_joined_nwkAddr == 0x0000 ||
zigbee_app_info.last_joined_nwkAddr == 0xffff ||
zigbee_app_info.last_joined_endpoint == 0U ||
!zigbee_app_info.tuya_bound) {
return;
}

memset(&req, 0, sizeof(req));
req.dst.mode = ZB_APSDE_ADDRMODE_SHORT;
req.dst.endpoint = TUYA_TARGET_ENDPOINT;
req.dst.nwkAddr = zigbee_app_info.last_joined_nwkAddr;
req.cmdId = 0x03;
req.noDefaultResp = ZCL_NO_DEFAULT_RESPONSE_FALSE;
req.payload = NULL;
req.length = 0U;

status = ZbZclClusterCommandReq(zigbee_app_info.tuya_client, &req, NULL, NULL);
if (status != ZCL_STATUS_SUCCESS) {
TUYA_LOG("Tuya query failed: 0x%02x", status);
}
}

static unsigned int APP_ZIGBEE_TuyaParseDpList(const uint8_t *payload, uint16_t len, uint16_t start_offset)
{
unsigned int count = 0U;
uint16_t idx = start_offset;

if (payload == NULL || len < (uint16_t)(start_offset + 4U)) {
return 0U;
}

TUYA_LOG("=== Parsing Tuya DP List ===");

while ((uint16_t)(idx + 4U) <= len) {
uint8_t dp_id = payload[idx];
uint8_t dp_type = payload[idx + 1U];
uint16_t dp_len = ((uint16_t)payload[idx + 2U] << 8) | payload[idx + 3U];
idx = (uint16_t)(idx + 4U);

if ((uint16_t)(idx + dp_len) > len) {
TUYA_LOG("DP parse truncated: id=%u type=0x%02x len=%u", dp_id, dp_type, dp_len);
break;
}

/* 处理电量数据 DP */
if (dp_id == TUYA_DP_POWER_DATA && dp_len == TUYA_DP_POWER_DATA_LEN) {
TUYA_LOG("--- DP #%u ---", count + 1U);
TUYA_LOG("DP ID: %u <<<===", dp_id);
TUYA_LOG("DP Type: 0x%02x (Power Data)", dp_type);
TUYA_LOG("DP Length: %u", dp_len);
TUYA_LOG("DP Raw:");
APP_ZIGBEE_TuyaDumpPayloadOneLine(&payload[idx], dp_len);
ParseTuya_PowerData(&payload[idx], (uint8_t)dp_len);
idx = (uint16_t)(idx + dp_len);
count++;
continue;
}

/* 处理温度 DP */
if (IsTuya_TemperatureDP(dp_id) && dp_len >= 1U && dp_len <= 4U) {
TUYA_LOG("--- DP #%u ---", count + 1U);
TUYA_LOG("DP ID: %u <<<===", dp_id);
TUYA_LOG("DP Type: 0x%02x (Temperature)", dp_type);
TUYA_LOG("DP Length: %u", dp_len);
TUYA_LOG("DP Raw:");
APP_ZIGBEE_TuyaDumpPayloadOneLine(&payload[idx], dp_len);
ParseTuya_TemperatureData(dp_id, &payload[idx], (uint8_t)dp_len);
idx = (uint16_t)(idx + dp_len);
count++;
continue;
}

// 璇︾粏鎵撳嵃 DP 淇℃伅
TUYA_LOG("--- DP #%u ---", count + 1);
TUYA_LOG("DP ID: %u <<<===", dp_id); // 楂樹寒鏄剧ずDP ID
TUYA_LOG("DP Type: 0x%02x (%s)", dp_type,
(dp_type == 0x01) ? "Boolean" :
(dp_type == 0x02) ? "Value" :
(dp_type == 0x03) ? "String" :
(dp_type == 0x04) ? "Enum" : "Unknown");
TUYA_LOG("DP Length: %u", dp_len);

// 瑙ｆ瀽鏁版嵁鍊?
if (dp_len > 0U) {
if (dp_type == 0x01 && dp_len == 1) {
// Boolean 绫诲瀷
TUYA_LOG("DP Raw:");
APP_ZIGBEE_TuyaDumpPayloadOneLine(&payload[idx], dp_len);
TUYA_LOG("DP Value (int): %u", payload[idx] ? 1U : 0U);
TUYA_LOG("DP Value (bool): %s", payload[idx] ? "TRUE" : "FALSE");

// DP ID 16: 鏂矾鍣ㄥ紑鍏崇姸鎬?
if (dp_id == TUYA_DP_BREAKER_SWITCH) {
bool new_state = (payload[idx] != 0);
if (zigbee_app_info.breaker_on != new_state) {
APP_ZIGBEE_UpdateBreakerStatus(new_state);
}
}
} else if (dp_type == 0x02 || dp_type == 0x04) {
TUYA_LOG("DP Raw:");
APP_ZIGBEE_TuyaDumpPayloadOneLine(&payload[idx], dp_len);
if (dp_len <= 4U) {
uint32_t value = APP_ZIGBEE_TuyaReadUintBE(&payload[idx], dp_len);
TUYA_LOG("DP Value (int): %lu", (unsigned long)value);

/* USER CODE BEGIN - DP ID 6: 鐢甸噺鏁版嵁 */
// DP ID 6: 鐢靛帇銆佺數娴併€佸姛鐜囨暟鎹?
if (dp_id == 6 && dp_len == 8U) {
ParseTuya_PowerData(&payload[idx], (uint8_t)dp_len);
}
/* USER CODE END - DP ID 6 */
} else {
TUYA_LOG("DP Value (int): <len=%u too large>", dp_len);
}
} else {
// 鍏朵粬绫诲瀷,鎵撳嵃鍘熷鏁版嵁
TUYA_LOG("DP Raw:");
APP_ZIGBEE_TuyaDumpPayloadOneLine(&payload[idx], dp_len);
if (dp_len <= 4U) {
uint32_t value = APP_ZIGBEE_TuyaReadUintBE(&payload[idx], dp_len);
TUYA_LOG("DP Value (int): %lu", (unsigned long)value);

/* USER CODE BEGIN - DP ID 6: 鐢甸噺鏁版嵁锛堝鐢級 */
// DP ID 6: 鐢靛帇銆佺數娴併€佸姛鐜囨暟鎹紙濡傛灉绫诲瀷涓嶆槸0x02锛?
if (dp_id == 6 && dp_len == 8U) {
ParseTuya_PowerData(&payload[idx], (uint8_t)dp_len);
}
/* USER CODE END - DP ID 6 */
}
}
}

idx = (uint16_t)(idx + dp_len);
count++;
}

TUYA_LOG("Total DPs found: %u", count);
return count;
}

/**
* @brief 鍙戦€?Tuya DP 鎺у埗鍛戒护(澧炲己鐗?- 澶氭牸寮忓皾璇?
* @param dp_id: DP ID
* @param dp_type: DP 鏁版嵁绫诲瀷 (0x01=Boolean, 0x02=Value, etc.)
* @param dp_data: DP 鏁版嵁鍐呭
* @param dp_len: DP 鏁版嵁闀垮害
* @retval None
*/
static void APP_ZIGBEE_TuyaSendDpCommand(uint8_t dp_id, uint8_t dp_type,
const uint8_t *dp_data, uint16_t dp_len)
{
struct ZbZclClusterCommandReqT req;
uint8_t payload[64];
uint16_t payload_len;
enum ZclStatusCodeT status;
static uint8_t cmd_sequence = 0;

if (zigbee_app_info.last_joined_nwkAddr == 0x0000 ||
zigbee_app_info.last_joined_nwkAddr == 0xffff ||
zigbee_app_info.last_joined_endpoint == 0U) {
APP_DBG("ERROR: Device not joined!");
return;
}

if (dp_data == NULL || dp_len == 0U || dp_len > 56U) {
APP_DBG("ERROR: Invalid DP data (len=%u)", dp_len);
return;
}

// Tuya 鏍囧噯鏍煎紡: [鐘舵€乚[搴忓垪鍙穄[DP ID][DP Type][DP Len High][DP Len Low][DP Data]
payload[0] = 0x00;
payload[1] = cmd_sequence++;
payload[2] = dp_id;
payload[3] = dp_type;
payload[4] = (uint8_t)(dp_len >> 8);
payload[5] = (uint8_t)(dp_len & 0xFF);
memcpy(&payload[6], dp_data, dp_len);
payload_len = (uint16_t)(6U + dp_len);

memset(&req, 0, sizeof(req));
req.dst.mode = ZB_APSDE_ADDRMODE_SHORT;
req.dst.endpoint = zigbee_app_info.last_joined_endpoint;
req.dst.nwkAddr = zigbee_app_info.last_joined_nwkAddr;
req.cmdId = 0x00;
req.noDefaultResp = ZCL_NO_DEFAULT_RESPONSE_FALSE;
req.payload = payload;
req.length = payload_len;

status = ZbZclClusterCommandReq(zigbee_app_info.tuya_client, &req, NULL, NULL);

if (status == ZCL_STATUS_SUCCESS) {
APP_DBG("DP command sent: ID=%u, Data=0x%02x", dp_id, dp_data[0]);
} else {
APP_DBG("DP command failed: 0x%02x", status);
}
}

/**
* @brief 浣跨敤 Tuya DP 鎺у埗鏂矾鍣ㄥ紑鍏?
* @param on: true=寮€闂? false=鍏抽椄
* @note 浣跨敤宸查獙璇佺殑 DP ID 16
* @retval None
*/
static void APP_ZIGBEE_TuyaControlBreaker(bool on)
{
uint8_t dp_data[1];
const uint8_t BREAKER_DP_ID = 16; // 宸查獙璇侊細DP ID 16 鏈夋晥锛?

dp_data[0] = on ? 0x01 : 0x00;

APP_DBG("Control breaker: %s (Target: 0x%04x:EP%u)",
on ? "ON" : "OFF",
zigbee_app_info.last_joined_nwkAddr,
zigbee_app_info.last_joined_endpoint);
APP_ZIGBEE_TuyaSendDpCommand(BREAKER_DP_ID, 0x01, dp_data, sizeof(dp_data));

// 鏇存柊 LED 鍜?LCD 鏄剧ず
APP_ZIGBEE_UpdateBreakerStatus(on);
}

/**
* @brief 鏇存柊鏂矾鍣ㄧ姸鎬佹樉绀猴紙LED 鍜?LCD锛?
* @param on: true=闂悎(ON), false=鏂紑(OFF)
* @retval None
*/
static void APP_ZIGBEE_UpdateBreakerStatus(bool on)
{
// 鏇存柊鐘舵€佸彉閲?
zigbee_app_info.breaker_on = on;

// 鏇存柊 LED 鐘舵€?
if (on) {
// 闂悎鐘舵€侊細绾㈣壊 LED锛堟牴鎹偍鐨勬弿杩帮細绾㈣壊=闂悎锛?
LED_Set_rgb(PWM_LED_GSDATA_47_0, PWM_LED_GSDATA_OFF, PWM_LED_GSDATA_OFF);
} else {
// 鏂紑鐘舵€侊細钃濊壊 LED锛堣摑鑹?鍏抽棴锛?
LED_Set_rgb(PWM_LED_GSDATA_OFF, PWM_LED_GSDATA_OFF, PWM_LED_GSDATA_47_0);
}

// 鏇存柊 LCD 鏄剧ず
char line1[20];
char line2[20];

// 娓呴櫎鏄剧ず
BSP_LCD_Clear(LCD_Inst, SSD1315_COLOR_BLACK);

// 绗竴琛岋細鏄剧ず鏍囬
UTIL_LCD_DisplayStringAt(0, 0, (uint8_t*)"Zigbee Breaker", CENTER_MODE);

// 绗簩琛岋細鏄剧ず鐘舵€?
if (on) {
snprintf(line1, sizeof(line1), "Status: ON");
snprintf(line2, sizeof(line2), "LED: RED");
} else {
snprintf(line1, sizeof(line1), "Status: OFF");
snprintf(line2, sizeof(line2), "LED: BLUE");
}

UTIL_LCD_DisplayStringAt(0, 16, (uint8_t*)line1, CENTER_MODE);
UTIL_LCD_DisplayStringAt(0, 32, (uint8_t*)line2, CENTER_MODE);

// 绗洓琛岋細鏄剧ず鍦板潃
char addr_line[20];
snprintf(addr_line, sizeof(addr_line), "Addr:0x%04X", zigbee_app_info.last_joined_nwkAddr);
UTIL_LCD_DisplayStringAt(0, 48, (uint8_t*)addr_line, CENTER_MODE);

// 鍒锋柊鏄剧ず
BSP_LCD_Refresh(LCD_Inst);
}

/**
* @brief 鏇存柊鐢靛帇銆佺數娴併€佸姛鐜囨樉绀哄埌 LCD
* @param None
* @retval None
*/
static void APP_ZIGBEE_UpdatePowerDisplay(void)
{
char line_voltage[20];
char line_current[20];
char line_power[20];

// 娓呴櫎鏄剧ず
BSP_LCD_Clear(LCD_Inst, SSD1315_COLOR_BLACK);

// 绗竴琛岋細鏍囬
UTIL_LCD_DisplayStringAt(0, 0, (uint8_t*)"Power Monitor", CENTER_MODE);

// 绗簩琛岋細鐢靛帇鏄剧ず锛堝崟浣嶏細V锛?
uint16_t voltage_int = zigbee_app_info.tuya_voltage_dv / 10U;
uint16_t voltage_dec = zigbee_app_info.tuya_voltage_dv % 10U;
snprintf(line_voltage, sizeof(line_voltage), "V:%u.%uV", voltage_int, voltage_dec);
UTIL_LCD_DisplayStringAt(0, 16, (uint8_t*)line_voltage, CENTER_MODE);

// 绗笁琛岋細鐢垫祦鏄剧ず锛堝崟浣嶏細A锛?
uint32_t current_int = zigbee_app_info.tuya_current_ma / 1000U;
uint32_t current_dec = (zigbee_app_info.tuya_current_ma % 1000U) / 10U; // 鍙栦袱浣嶅皬鏁?
snprintf(line_current, sizeof(line_current), "I:%lu.%02luA",
         (unsigned long)current_int, (unsigned long)current_dec);
UTIL_LCD_DisplayStringAt(0, 32, (uint8_t*)line_current, CENTER_MODE);

// 绗洓琛岋細鍔熺巼鏄剧ず锛堝崟浣嶏細W锛?
snprintf(line_power, sizeof(line_power), "P:%luW", (unsigned long)zigbee_app_info.tuya_power_w);
UTIL_LCD_DisplayStringAt(0, 48, (uint8_t*)line_power, CENTER_MODE);

// 鍒锋柊鏄剧ず
BSP_LCD_Refresh(LCD_Inst);
}

/**
* @brief APS 灞傛姤鏂囨姄鍙栧櫒锛堟姄鍙栨墍鏈夊彂寰€ SNIFFER_ENDPOINT 鐨勬姤鏂囷級
* @param dataInd: APS 鏁版嵁鎸囩ず缁撴瀯浣擄紙鍖呭惈瀹屾暣鐨?ASDU锛?
* @param arg: 鐢ㄦ埛鍙傛暟锛堟湭浣跨敤锛?
* @retval ZB_APS_FILTER_CONTINUE (0)锛氱户缁紶閫掔粰涓婂眰锛沍B_APS_FILTER_DISCARD (1)锛氫涪寮冩姤鏂?
*/
static int APP_ZIGBEE_ApsSniffer(struct ZbApsdeDataIndT *dataInd, void *arg)
{
UNUSED(arg);

#if TUYA_VERBOSE_LOG
APP_DBG("");
APP_DBG("========================================");
APP_DBG("*** APS SNIFFER: Packet Captured ***");
APP_DBG("========================================");
APP_DBG("Source Addr: 0x%04X (EP %u)", dataInd->src.nwkAddr, dataInd->src.endpoint);
APP_DBG("Dest Addr: 0x%04X (EP %u)", dataInd->dst.nwkAddr, dataInd->dst.endpoint);
APP_DBG("Profile ID: 0x%04X", dataInd->profileId);
APP_DBG("Cluster ID: 0x%04X", dataInd->clusterId);
APP_DBG("ASDU Length: %u bytes", dataInd->asduLength);
APP_DBG("========================================");

// 鎵撳嵃瀹屾暣鐨?ASDU锛堝崄鍏繘鍒舵牸寮忥級
if (dataInd->asdu != NULL && dataInd->asduLength > 0) {
APP_DBG(">>> COMPLETE ASDU (HEX):");

// 鍗曡鏍煎紡锛堟柟渚垮鍒讹級
static const char hex[] = "0123456789ABCDEF";
char line[256];
uint16_t pos = 0;
uint16_t display_len = (dataInd->asduLength > 80) ? 80 : dataInd->asduLength;

for (uint16_t i = 0; i < display_len; i++) {
uint8_t b = dataInd->asdu[i];
line[pos++] = hex[b >> 4];
line[pos++] = hex[b & 0x0F];
if (i + 1 < display_len) {
line[pos++] = ' ';
}
}
line[pos] = '\0';
APP_DBG(" %s%s", line, (dataInd->asduLength > 80) ? " ..." : "");

// 澶氳鏍煎紡锛堣缁嗚鍥撅紝姣忚 16 瀛楄妭锛?
APP_DBG(">>> [Multi-line View]:");
for (uint16_t offset = 0; offset < dataInd->asduLength; offset += 16) {
char detail_line[64];
uint16_t chunk = (dataInd->asduLength - offset > 16) ? 16 : (dataInd->asduLength - offset);
uint16_t dpos = 0;

for (uint16_t i = 0; i < chunk; i++) {
uint8_t b = dataInd->asdu[offset + i];
detail_line[dpos++] = hex[b >> 4];
detail_line[dpos++] = hex[b & 0x0F];
if (i + 1 < chunk) {
detail_line[dpos++] = ' ';
}
}
detail_line[dpos] = '\0';
APP_DBG(" [%04u]: %s", offset, detail_line);
}
} else {
APP_DBG(">>> ASDU: <empty>");
}

APP_DBG("========================================");
APP_DBG("");
#endif

// 缁х画浼犻€掔粰涓婂眰 ZCL 澶勭悊锛堜笉闃绘柇姝ｅ父娴佺▼锛?

/* If Tuya EF00 report is sent to SNIFFER_ENDPOINT, parse it here */
if (dataInd->clusterId == TUYA_CLUSTER_ID && dataInd->asdu != NULL && dataInd->asduLength > 0U) {
struct ZbZclHeaderT hdr;
int hdr_len = ZbZclParseHeader(&hdr, dataInd->asdu, dataInd->asduLength);
if (hdr_len >= 0) {
const uint8_t *payload = &dataInd->asdu[hdr_len];
uint16_t payload_len = (uint16_t)(dataInd->asduLength - (uint16_t)hdr_len);
unsigned int dp_count;

TUYA_LOG("APS SNIFFER: Tuya EF00 payload parse");
dp_count = APP_ZIGBEE_TuyaParseDpList(payload, payload_len, 2U);
if (dp_count == 0U) {
dp_count = APP_ZIGBEE_TuyaParseDpList(payload, payload_len, 1U);
}
if (dp_count == 0U) {
(void)APP_ZIGBEE_TuyaParseDpList(payload, payload_len, 0U);
}
} else {
TUYA_LOG("APS SNIFFER: invalid ZCL header (len=%u)", dataInd->asduLength);
}
}

return ZB_APS_FILTER_CONTINUE;
}

/**
* @brief 瀹氭椂鍣ㄥ洖璋冨嚱鏁帮紝瑙﹀彂鍔熺巼鏌ヨ浠诲姟
* @param None
* @retval None
*/
static void APP_ZIGBEE_TuyaQueryTimerCallback(void)
{
  /* 璁剧疆浠诲姟鏍囧織锛岃涓诲惊鐜墽琛屾煡璇?*/
  UTIL_SEQ_SetTask(1U << CFG_TASK_TUYA_QUERY_POWER, CFG_SCH_PRIO_0);
}

static uint32_t APP_ZIGBEE_GetTimestampSeconds(void)
{
  if (g_unix_time_valid) {
    uint32_t delta_ms = HAL_GetTick() - g_unix_time_tick;
    return g_unix_time_base + (delta_ms / 1000U);
  }
  return HAL_GetTick() / 1000U;
}

/**
* @brief  MQTT downlink message callback (remote control)
*         Called when a message arrives on pv/device_001/control
*         Parses {"command":"on"} or {"command":"off"} and controls breaker
*/
static void APP_ZIGBEE_MqttControlCallback(const char *topic, const char *payload, uint16_t len)
{
  APP_DBG("[REMOTE] Received on %s: %.*s", topic, (int)len, payload);

  if (strstr(payload, "\"on\"") != NULL) {
    APP_DBG("[REMOTE] >>> Breaker ON command received");
    if (zigbee_app_info.tuya_bound) {
      APP_ZIGBEE_TuyaControlBreaker(true);
    } else {
      APP_DBG("[REMOTE] WARNING: Tuya not bound, cannot control breaker");
    }
  } else if (strstr(payload, "\"off\"") != NULL) {
    APP_DBG("[REMOTE] >>> Breaker OFF command received");
    if (zigbee_app_info.tuya_bound) {
      APP_ZIGBEE_TuyaControlBreaker(false);
    } else {
      APP_DBG("[REMOTE] WARNING: Tuya not bound, cannot control breaker");
    }
  } else {
    APP_DBG("[REMOTE] Unknown command ignored");
  }
}

/**
* @brief Build JSON payload and publish to cloud via A7670E (MQTT over cellular)
* @param None
* @retval None
*/
static void APP_ZIGBEE_MqttUploadJson(void)
{
  static bool no_data_logged = false;
  static bool link_not_ready_logged = false;
  static bool enqueue_fail_logged = false;
  char json[MQTT_JSON_BUF_SIZE];
  uint16_t voltage_raw = zigbee_app_info.tuya_voltage_dv;
  uint32_t current_ma = zigbee_app_info.tuya_current_ma;
  uint32_t power_w = zigbee_app_info.tuya_power_w;
  uint16_t voltage_int = voltage_raw / 10U;
  uint16_t voltage_dec = voltage_raw % 10U;
  uint32_t current_int = current_ma / 1000U;
  uint32_t current_dec = (current_ma % 1000U) / 10U; /* two decimals */
  uint32_t efficiency_int = MQTT_DEFAULT_EFFICIENCY_X10 / 10U;
  uint32_t efficiency_dec = MQTT_DEFAULT_EFFICIENCY_X10 % 10U;

  /* 使用真实温度值（如果有效），否则使用默认值 */
  uint16_t temp_x10 = zigbee_app_info.tuya_temperature_valid
                      ? zigbee_app_info.tuya_temperature_x10
                      : MQTT_DEFAULT_TEMPERATURE_X10;
  uint32_t temperature_int = temp_x10 / 10U;
  uint32_t temperature_dec = temp_x10 % 10U;
  bool status = zigbee_app_info.breaker_on;
  bool data_ready = (voltage_raw != 0U) || (current_ma != 0U) || (power_w != 0U);

  if (!data_ready) {
    if (!no_data_logged) {
      APP_DBG("[MQTT] skip: no power data");
      no_data_logged = true;
    }
    return;
  }
  no_data_logged = false;

  if (!A7670E_IsNetworkReady() || !A7670E_IsMqttConnected()) {
    if (!link_not_ready_logged) {
      APP_DBG("[MQTT] wait: link not ready net=%u mqtt=%u",
              (unsigned int)A7670E_IsNetworkReady(),
              (unsigned int)A7670E_IsMqttConnected());
      link_not_ready_logged = true;
    }
    enqueue_fail_logged = false;
    return;
  }
  link_not_ready_logged = false;

  uint32_t ts = APP_ZIGBEE_GetTimestampSeconds();
  int n = snprintf(json, sizeof(json),
                   "{\"device_id\":\"%s\",\"timestamp\":%lu,"
                   "\"voltage\":%u.%u,\"current\":%lu.%02lu,"
                   "\"power\":%lu,\"status\":%s,"
                   "\"efficiency\":%lu.%lu,\"temperature\":%lu.%lu}",
                   MQTT_DEVICE_ID, (unsigned long)ts,
                   voltage_int, voltage_dec,
                   (unsigned long)current_int, (unsigned long)current_dec,
                   (unsigned long)power_w, status ? "true" : "false",
                   (unsigned long)efficiency_int, (unsigned long)efficiency_dec,
                   (unsigned long)temperature_int, (unsigned long)temperature_dec);
  if (n <= 0 || n >= (int)sizeof(json)) {
    APP_DBG("MQTT Upload: JSON build failed");
    return;
  }

  bool queued = A7670E_RequestPublish(MQTT_TOPIC, json, (size_t)n, 0U);
  if (queued) {
    enqueue_fail_logged = false;
  } else if (!enqueue_fail_logged) {
    APP_DBG("[MQTT] enqueue fail net=%u mqtt=%u",
            (unsigned int)A7670E_IsNetworkReady(),
            (unsigned int)A7670E_IsMqttConnected());
    enqueue_fail_logged = true;
  }
}

/**
* @brief 鍛ㄦ湡鎬ф煡璇uya鍔熺巼鏁版嵁浠诲姟
* @param None
* @retval None
*/
static void APP_ZIGBEE_TuyaQueryPowerTask(void)
{
  /* 浠呭湪缁戝畾鎴愬姛鍚庢墠鏌ヨ */
  if (zigbee_app_info.tuya_bound) {
    APP_ZIGBEE_TuyaQueryPowerData();
  }
  APP_ZIGBEE_MqttUploadJson();
}

/* USER CODE END FD_LOCAL_FUNCTIONS */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

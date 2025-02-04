/**
 * Copyright (c) 2012 - 2017, Nordic Semiconductor ASA
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 * 
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 * 
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 * 
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 * 
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */
/** @file
 *
 * @defgroup ble_sdk_app_hids_keyboard_main main.c
 * @{
 * @ingroup ble_sdk_app_hids_keyboard
 * @brief HID Keyboard Sample Application main file.
 *
 * This file contains is the source code for a sample application using the HID, Battery and Device
 * Information Services for implementing a simple keyboard functionality.
 * Pressing Button 0 will send text 'hello' to the connected peer. On receiving output report,
 * it toggles the state of LED 2 on the mother board based on whether or not Caps Lock is on.
 * This application uses the @ref app_scheduler.
 *
 * Also it would accept pairing requests from any peer device.
 */

#include <stdint.h>
#include <string.h>
#include "nordic_common.h"
#include "nrf.h"
#include "nrf_assert.h"
#include "app_error.h"
#include "nrf_gpio.h"
#include "ble.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "ble_advertising.h"
#include "ble_advdata.h"
#include "ble_hids.h"
#include "ble_bas.h"
#include "ble_dis.h"
#include "ble_conn_params.h"
#include "bsp.h"
#include "sensorsim.h"
#include "bsp_btn_ble.h"
#include "app_scheduler.h"
#include "softdevice_handler_appsh.h"
#include "app_timer.h"
#include "peer_manager.h"
#include "app_button.h"
#include "fds.h"
#include "fstorage.h"
#include "ble_conn_state.h"
#include "nrf_ble_gatt.h"

#define NRF_LOG_MODULE_NAME "APP"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"

#if BUTTONS_NUMBER < 2
#error "Not enough resources on board"
#endif


#define SHIFT_BUTTON_ID                  1                                          /**< Button used as 'SHIFT' Key. */

#define DEVICE_NAME                      "Nordic_Keyboard"                          /**< Name of device. Will be included in the advertising data. */
#define MANUFACTURER_NAME                "NordicSemiconductor"                      /**< Manufacturer. Will be passed to Device Information Service. */

#define BATTERY_LEVEL_MEAS_INTERVAL      APP_TIMER_TICKS(2000)                      /**< Battery level measurement interval (ticks). */
#define MIN_BATTERY_LEVEL                81                                         /**< Minimum simulated battery level. */
#define MAX_BATTERY_LEVEL                100                                        /**< Maximum simulated battery level. */
#define BATTERY_LEVEL_INCREMENT          1                                          /**< Increment between each simulated battery level measurement. */

#define PNP_ID_VENDOR_ID_SOURCE          0x02                                       /**< Vendor ID Source. */
#define PNP_ID_VENDOR_ID                 0x1915                                     /**< Vendor ID. */
#define PNP_ID_PRODUCT_ID                0xEEEE                                     /**< Product ID. */
#define PNP_ID_PRODUCT_VERSION           0x0001                                     /**< Product Version. */

#define APP_ADV_FAST_INTERVAL            0x0028                                     /**< Fast advertising interval (in units of 0.625 ms. This value corresponds to 25 ms.). */
#define APP_ADV_SLOW_INTERVAL            0x0C80                                     /**< Slow advertising interval (in units of 0.625 ms. This value corrsponds to 2 seconds). */
#define APP_ADV_FAST_TIMEOUT             30                                         /**< The duration of the fast advertising period (in seconds). */
#define APP_ADV_SLOW_TIMEOUT             180                                        /**< The duration of the slow advertising period (in seconds). */

/*lint -emacro(524, MIN_CONN_INTERVAL) // Loss of precision */
#define MIN_CONN_INTERVAL                MSEC_TO_UNITS(7.5, UNIT_1_25_MS)            /**< Minimum connection interval (7.5 ms) */
#define MAX_CONN_INTERVAL                MSEC_TO_UNITS(30, UNIT_1_25_MS)             /**< Maximum connection interval (30 ms). */
#define SLAVE_LATENCY                    6                                           /**< Slave latency. */
#define CONN_SUP_TIMEOUT                 MSEC_TO_UNITS(430, UNIT_10_MS)              /**< Connection supervisory timeout (430 ms). */

#define FIRST_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(5000)                      /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY    APP_TIMER_TICKS(30000)                     /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT     3                                           /**< Number of attempts before giving up the connection parameter negotiation. */

#define SEC_PARAM_BOND                   1                                           /**< Perform bonding. */
#define SEC_PARAM_MITM                   0                                           /**< Man In The Middle protection not required. */
#define SEC_PARAM_LESC                   0                                           /**< LE Secure Connections not enabled. */
#define SEC_PARAM_KEYPRESS               0                                           /**< Keypress notifications not enabled. */
#define SEC_PARAM_IO_CAPABILITIES        BLE_GAP_IO_CAPS_NONE                        /**< No I/O capabilities. */
#define SEC_PARAM_OOB                    0                                           /**< Out Of Band data not available. */
#define SEC_PARAM_MIN_KEY_SIZE           7                                           /**< Minimum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE           16                                          /**< Maximum encryption key size. */

#define OUTPUT_REPORT_INDEX              0                                           /**< Index of Output Report. */
#define OUTPUT_REPORT_MAX_LEN            1                                           /**< Maximum length of Output Report. */
#define INPUT_REPORT_KEYS_INDEX          0                                           /**< Index of Input Report. */
#define OUTPUT_REPORT_BIT_MASK_CAPS_LOCK 0x02                                        /**< CAPS LOCK bit in Output Report (based on 'LED Page (0x08)' of the Universal Serial Bus HID Usage Tables). */
#define INPUT_REP_REF_ID                 0                                           /**< Id of reference to Keyboard Input Report. */
#define OUTPUT_REP_REF_ID                0                                           /**< Id of reference to Keyboard Output Report. */

#define APP_FEATURE_NOT_SUPPORTED        BLE_GATT_STATUS_ATTERR_APP_BEGIN + 2        /**< Reply when unsupported features are requested. */

#define MAX_BUFFER_ENTRIES               5                                           /**< Number of elements that can be enqueued */

#define BASE_USB_HID_SPEC_VERSION        0x0101                                      /**< Version number of base USB HID Specification implemented by this application. */

#define INPUT_REPORT_KEYS_MAX_LEN        8                                           /**< Maximum length of the Input Report characteristic. */

#define DEAD_BEEF                        0xDEADBEEF                                  /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

#define SCHED_MAX_EVENT_DATA_SIZE        MAX(APP_TIMER_SCHED_EVENT_DATA_SIZE, \
                                             BLE_STACK_HANDLER_SCHED_EVT_SIZE)       /**< Maximum size of scheduler events. */
#ifdef SVCALL_AS_NORMAL_FUNCTION
#define SCHED_QUEUE_SIZE                 20                                          /**< Maximum number of events in the scheduler queue. More is needed in case of Serialization. */
#else
#define SCHED_QUEUE_SIZE                 10                                          /**< Maximum number of events in the scheduler queue. */
#endif

#define MODIFIER_KEY_POS                 0                                           /**< Position of the modifier byte in the Input Report. */
#define SCAN_CODE_POS                    2                                           /**< The start position of the key scan code in a HID Report. */
#define SHIFT_KEY_CODE                   0x02                                        /**< Key code indicating the press of the Shift Key. */

#define MAX_KEYS_IN_ONE_REPORT           (INPUT_REPORT_KEYS_MAX_LEN - SCAN_CODE_POS) /**< Maximum number of key presses that can be sent in one Input Report. */


/**Buffer queue access macros
 *
 * @{ */
/** Initialization of buffer list */
#define BUFFER_LIST_INIT()     \
    do                         \
    {                          \
        buffer_list.rp    = 0; \
        buffer_list.wp    = 0; \
        buffer_list.count = 0; \
    } while (0)

/** Provide status of data list is full or not */
#define BUFFER_LIST_FULL() \
    ((MAX_BUFFER_ENTRIES == buffer_list.count - 1) ? true : false)

/** Provides status of buffer list is empty or not */
#define BUFFER_LIST_EMPTY() \
    ((0 == buffer_list.count) ? true : false)

#define BUFFER_ELEMENT_INIT(i)                 \
    do                                         \
    {                                          \
        buffer_list.buffer[(i)].p_data = NULL; \
    } while (0)

/** @} */

/** Abstracts buffer element */
typedef struct hid_key_buffer
{
    uint8_t      data_offset; /**< Max Data that can be buffered for all entries */
    uint8_t      data_len;    /**< Total length of data */
    uint8_t    * p_data;      /**< Scanned key pattern */
    ble_hids_t * p_instance;  /**< Identifies peer and service instance */
} buffer_entry_t;

STATIC_ASSERT(sizeof(buffer_entry_t) % 4 == 0);

/** Circular buffer list */
typedef struct
{
    buffer_entry_t buffer[MAX_BUFFER_ENTRIES]; /**< Maximum number of entries that can enqueued in the list */
    uint8_t        rp;                         /**< Index to the read location */
    uint8_t        wp;                         /**< Index to write location */
    uint8_t        count;                      /**< Number of elements in the list */
} buffer_list_t;

STATIC_ASSERT(sizeof(buffer_list_t) % 4 == 0);

static ble_hids_t     m_hids;                                   /**< Structure used to identify the HID service. */
static ble_bas_t      m_bas;                                    /**< Structure used to identify the battery service. */
static nrf_ble_gatt_t m_gatt;                                   /**< GATT module instance. */
static bool           m_in_boot_mode = false;                   /**< Current protocol mode. */
static uint16_t       m_conn_handle  = BLE_CONN_HANDLE_INVALID; /**< Handle of the current connection. */

static sensorsim_cfg_t   m_battery_sim_cfg;                     /**< Battery Level sensor simulator configuration. */
static sensorsim_state_t m_battery_sim_state;                   /**< Battery Level sensor simulator state. */

APP_TIMER_DEF(m_battery_timer_id);                              /**< Battery timer. */

static pm_peer_id_t m_peer_id;                                  /**< Device reference handle to the current bonded central. */
static bool         m_caps_on = false;                          /**< Variable to indicate if Caps Lock is turned on. */

static pm_peer_id_t   m_whitelist_peers[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];  /**< List of peers currently in the whitelist. */
static uint32_t       m_whitelist_peer_cnt;                                 /**< Number of peers currently in the whitelist. */
static bool           m_is_wl_changed;                                      /**< Indicates if the whitelist has been changed since last time it has been updated in the Peer Manager. */

static ble_uuid_t m_adv_uuids[] = {{BLE_UUID_HUMAN_INTERFACE_DEVICE_SERVICE, BLE_UUID_TYPE_BLE}};

static uint8_t m_sample_key_press_scan_str[] = /**< Key pattern to be sent when the key press button has been pushed. */
{
    0x0b,                                      /* Key h */
    0x08,                                      /* Key e */
    0x0f,                                      /* Key l */
    0x0f,                                      /* Key l */
    0x12,                                      /* Key o */
    0x28                                       /* Key Return */
};

static uint8_t m_caps_on_key_scan_str[] = /**< Key pattern to be sent when the output report has been written with the CAPS LOCK bit set. */
{
    0x06,                                 /* Key C */
    0x04,                                 /* Key a */
    0x13,                                 /* Key p */
    0x16,                                 /* Key s */
    0x12,                                 /* Key o */
    0x11,                                 /* Key n */
};

static uint8_t m_caps_off_key_scan_str[] = /**< Key pattern to be sent when the output report has been written with the CAPS LOCK bit cleared. */
{
    0x06,                                  /* Key C */
    0x04,                                  /* Key a */
    0x13,                                  /* Key p */
    0x16,                                  /* Key s */
    0x12,                                  /* Key o */
    0x09,                                  /* Key f */
};


/** List to enqueue not just data to be sent, but also related information like the handle, connection handle etc */
static buffer_list_t buffer_list;

static void on_hids_evt(ble_hids_t * p_hids, ble_hids_evt_t * p_evt);

/**@brief Callback function for asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in]   line_num   Line number of the failing ASSERT call.
 * @param[in]   file_name  File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}


/**@brief Fetch the list of peer manager peer IDs.
 *
 * @param[inout] p_peers   The buffer where to store the list of peer IDs.
 * @param[inout] p_size    In: The size of the @p p_peers buffer.
 *                         Out: The number of peers copied in the buffer.
 */
static void peer_list_get(pm_peer_id_t * p_peers, uint32_t * p_size)
{
    pm_peer_id_t peer_id;
    uint32_t     peers_to_copy;

    peers_to_copy = (*p_size < BLE_GAP_WHITELIST_ADDR_MAX_COUNT) ?
                     *p_size : BLE_GAP_WHITELIST_ADDR_MAX_COUNT;

    peer_id = pm_next_peer_id_get(PM_PEER_ID_INVALID);
    *p_size = 0;

    while ((peer_id != PM_PEER_ID_INVALID) && (peers_to_copy--))
    {
        p_peers[(*p_size)++] = peer_id;
        peer_id = pm_next_peer_id_get(peer_id);
    }
}


/**@brief Clear bond information from persistent storage.
 */
static void delete_bonds(void)
{
    ret_code_t err_code;

    NRF_LOG_INFO("Erase bonds!\r\n");

    err_code = pm_peers_delete();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for starting advertising.
 */
static void advertising_start(bool erase_bonds)
{
    if (erase_bonds == true)
    {
        delete_bonds();
        // Advertising is started by PM_EVT_PEERS_DELETE_SUCCEEDED event.
    }
    else
    {
        ret_code_t ret;

        memset(m_whitelist_peers, PM_PEER_ID_INVALID, sizeof(m_whitelist_peers));
        m_whitelist_peer_cnt = (sizeof(m_whitelist_peers) / sizeof(pm_peer_id_t));

        peer_list_get(m_whitelist_peers, &m_whitelist_peer_cnt);

        ret = pm_whitelist_set(m_whitelist_peers, m_whitelist_peer_cnt);
        APP_ERROR_CHECK(ret);

        // Setup the device identies list.
        // Some SoftDevices do not support this feature.
        ret = pm_device_identities_list_set(m_whitelist_peers, m_whitelist_peer_cnt);
        if (ret != NRF_ERROR_NOT_SUPPORTED)
        {
            APP_ERROR_CHECK(ret);
        }

        m_is_wl_changed = false;

        ret = ble_advertising_start(BLE_ADV_MODE_FAST);
        APP_ERROR_CHECK(ret);
    }
}


/**@brief Function for handling Peer Manager events.
 *
 * @param[in] p_evt  Peer Manager event.
 */
static void pm_evt_handler(pm_evt_t const * p_evt)
{
    ret_code_t err_code;

    switch (p_evt->evt_id)
    {
        case PM_EVT_BONDED_PEER_CONNECTED:
        {
            NRF_LOG_INFO("Connected to a previously bonded device.\r\n");
        } break;

        case PM_EVT_CONN_SEC_SUCCEEDED:
        {
            NRF_LOG_INFO("Connection secured: role: %d, conn_handle: 0x%x, procedure: %d.\r\n",
                         ble_conn_state_role(p_evt->conn_handle),
                         p_evt->conn_handle,
                         p_evt->params.conn_sec_succeeded.procedure);

            m_peer_id = p_evt->peer_id;

            // Note: You should check on what kind of white list policy your application should use.
            if (p_evt->params.conn_sec_succeeded.procedure == PM_LINK_SECURED_PROCEDURE_BONDING)
            {
                NRF_LOG_INFO("New Bond, add the peer to the whitelist if possible\r\n");
                NRF_LOG_INFO("\tm_whitelist_peer_cnt %d, MAX_PEERS_WLIST %d\r\n",
                               m_whitelist_peer_cnt + 1,
                               BLE_GAP_WHITELIST_ADDR_MAX_COUNT);

                if (m_whitelist_peer_cnt < BLE_GAP_WHITELIST_ADDR_MAX_COUNT)
                {
                    // Bonded to a new peer, add it to the whitelist.
                    m_whitelist_peers[m_whitelist_peer_cnt++] = m_peer_id;
                    m_is_wl_changed = true;
                }
            }
        } break;

        case PM_EVT_CONN_SEC_FAILED:
        {
            /* Often, when securing fails, it shouldn't be restarted, for security reasons.
             * Other times, it can be restarted directly.
             * Sometimes it can be restarted, but only after changing some Security Parameters.
             * Sometimes, it cannot be restarted until the link is disconnected and reconnected.
             * Sometimes it is impossible, to secure the link, or the peer device does not support it.
             * How to handle this error is highly application dependent. */
        } break;

        case PM_EVT_CONN_SEC_CONFIG_REQ:
        {
            // Reject pairing request from an already bonded peer.
            pm_conn_sec_config_t conn_sec_config = {.allow_repairing = false};
            pm_conn_sec_config_reply(p_evt->conn_handle, &conn_sec_config);
        } break;

        case PM_EVT_STORAGE_FULL:
        {
            // Run garbage collection on the flash.
            err_code = fds_gc();
            if (err_code == FDS_ERR_BUSY || err_code == FDS_ERR_NO_SPACE_IN_QUEUES)
            {
                // Retry.
            }
            else
            {
                APP_ERROR_CHECK(err_code);
            }
        } break;

        case PM_EVT_PEERS_DELETE_SUCCEEDED:
        {
            advertising_start(false);
        } break;

        case PM_EVT_LOCAL_DB_CACHE_APPLY_FAILED:
        {
            // The local database has likely changed, send service changed indications.
            pm_local_database_has_changed();
        } break;

        case PM_EVT_PEER_DATA_UPDATE_FAILED:
        {
            // Assert.
            APP_ERROR_CHECK(p_evt->params.peer_data_update_failed.error);
        } break;

        case PM_EVT_PEER_DELETE_FAILED:
        {
            // Assert.
            APP_ERROR_CHECK(p_evt->params.peer_delete_failed.error);
        } break;

        case PM_EVT_PEERS_DELETE_FAILED:
        {
            // Assert.
            APP_ERROR_CHECK(p_evt->params.peers_delete_failed_evt.error);
        } break;

        case PM_EVT_ERROR_UNEXPECTED:
        {
            // Assert.
            APP_ERROR_CHECK(p_evt->params.error_unexpected.error);
        } break;

        case PM_EVT_CONN_SEC_START:
        case PM_EVT_PEER_DATA_UPDATE_SUCCEEDED:
        case PM_EVT_PEER_DELETE_SUCCEEDED:
        case PM_EVT_LOCAL_DB_CACHE_APPLIED:
        case PM_EVT_SERVICE_CHANGED_IND_SENT:
        case PM_EVT_SERVICE_CHANGED_IND_CONFIRMED:
        default:
            break;
    }
}


/**@brief Function for handling Service errors.
 *
 * @details A pointer to this function will be passed to each service which may need to inform the
 *          application about an error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void service_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for handling advertising errors.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void ble_advertising_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for performing a battery measurement, and update the Battery Level characteristic in the Battery Service.
 */
static void battery_level_update(void)
{
    ret_code_t err_code;
    uint8_t  battery_level;

    battery_level = (uint8_t)sensorsim_measure(&m_battery_sim_state, &m_battery_sim_cfg);

    err_code = ble_bas_battery_level_update(&m_bas, battery_level);
    if ((err_code != NRF_SUCCESS) &&
        (err_code != NRF_ERROR_INVALID_STATE) &&
        (err_code != NRF_ERROR_RESOURCES) &&
        (err_code != BLE_ERROR_GATTS_SYS_ATTR_MISSING)
       )
    {
        APP_ERROR_HANDLER(err_code);
    }
}


/**@brief Function for handling the Battery measurement timer timeout.
 *
 * @details This function will be called each time the battery level measurement timer expires.
 *
 * @param[in]   p_context   Pointer used for passing some arbitrary information (context) from the
 *                          app_start_timer() call to the timeout handler.
 */
static void battery_level_meas_timeout_handler(void * p_context)
{
    UNUSED_PARAMETER(p_context);
    battery_level_update();
}


/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module.
 */
static void timers_init(void)
{
    ret_code_t err_code;

    err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);

    // Create battery timer.
    err_code = app_timer_create(&m_battery_timer_id,
                                APP_TIMER_MODE_REPEATED,
                                battery_level_meas_timeout_handler);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for the GAP initialization.
 *
 * @details This function sets up all the necessary GAP (Generic Access Profile) parameters of the
 *          device including the device name, appearance, and the preferred connection parameters.
 */
static void gap_params_init(void)
{
    ret_code_t              err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *)DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    err_code = sd_ble_gap_appearance_set(BLE_APPEARANCE_HID_KEYBOARD);
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the GATT module.
 */
static void gatt_init(void)
{
    ret_code_t err_code = nrf_ble_gatt_init(&m_gatt, NULL);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing Device Information Service.
 */
static void dis_init(void)
{
    ret_code_t       err_code;
    ble_dis_init_t   dis_init_obj;
    ble_dis_pnp_id_t pnp_id;

    pnp_id.vendor_id_source = PNP_ID_VENDOR_ID_SOURCE;
    pnp_id.vendor_id        = PNP_ID_VENDOR_ID;
    pnp_id.product_id       = PNP_ID_PRODUCT_ID;
    pnp_id.product_version  = PNP_ID_PRODUCT_VERSION;

    memset(&dis_init_obj, 0, sizeof(dis_init_obj));

    ble_srv_ascii_to_utf8(&dis_init_obj.manufact_name_str, MANUFACTURER_NAME);
    dis_init_obj.p_pnp_id = &pnp_id;

    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&dis_init_obj.dis_attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&dis_init_obj.dis_attr_md.write_perm);

    err_code = ble_dis_init(&dis_init_obj);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing Battery Service.
 */
static void bas_init(void)
{
    ret_code_t     err_code;
    ble_bas_init_t bas_init_obj;

    memset(&bas_init_obj, 0, sizeof(bas_init_obj));

    bas_init_obj.evt_handler          = NULL;
    bas_init_obj.support_notification = true;
    bas_init_obj.p_report_ref         = NULL;
    bas_init_obj.initial_batt_level   = 100;

    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&bas_init_obj.battery_level_char_attr_md.cccd_write_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&bas_init_obj.battery_level_char_attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&bas_init_obj.battery_level_char_attr_md.write_perm);

    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&bas_init_obj.battery_level_report_read_perm);

    err_code = ble_bas_init(&m_bas, &bas_init_obj);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing HID Service.
 */
static void hids_init(void)
{
    ret_code_t                 err_code;
    ble_hids_init_t            hids_init_obj;
    ble_hids_inp_rep_init_t    input_report_array[1];
    ble_hids_inp_rep_init_t  * p_input_report;
    ble_hids_outp_rep_init_t   output_report_array[1];
    ble_hids_outp_rep_init_t * p_output_report;
    uint8_t                    hid_info_flags;

    memset((void *)input_report_array, 0, sizeof(ble_hids_inp_rep_init_t));
    memset((void *)output_report_array, 0, sizeof(ble_hids_outp_rep_init_t));

    static uint8_t report_map_data[] =
    {
        0x05, 0x01,       // Usage Page (Generic Desktop)
        0x09, 0x06,       // Usage (Keyboard)
        0xA1, 0x01,       // Collection (Application)
        0x05, 0x07,       // Usage Page (Key Codes)
        0x19, 0xe0,       // Usage Minimum (224)
        0x29, 0xe7,       // Usage Maximum (231)
        0x15, 0x00,       // Logical Minimum (0)
        0x25, 0x01,       // Logical Maximum (1)
        0x75, 0x01,       // Report Size (1)
        0x95, 0x08,       // Report Count (8)
        0x81, 0x02,       // Input (Data, Variable, Absolute)

        0x95, 0x01,       // Report Count (1)
        0x75, 0x08,       // Report Size (8)
        0x81, 0x01,       // Input (Constant) reserved byte(1)

        0x95, 0x05,       // Report Count (5)
        0x75, 0x01,       // Report Size (1)
        0x05, 0x08,       // Usage Page (Page# for LEDs)
        0x19, 0x01,       // Usage Minimum (1)
        0x29, 0x05,       // Usage Maximum (5)
        0x91, 0x02,       // Output (Data, Variable, Absolute), Led report
        0x95, 0x01,       // Report Count (1)
        0x75, 0x03,       // Report Size (3)
        0x91, 0x01,       // Output (Data, Variable, Absolute), Led report padding

        0x95, 0x06,       // Report Count (6)
        0x75, 0x08,       // Report Size (8)
        0x15, 0x00,       // Logical Minimum (0)
        0x25, 0x65,       // Logical Maximum (101)
        0x05, 0x07,       // Usage Page (Key codes)
        0x19, 0x00,       // Usage Minimum (0)
        0x29, 0x65,       // Usage Maximum (101)
        0x81, 0x00,       // Input (Data, Array) Key array(6 bytes)

        0x09, 0x05,       // Usage (Vendor Defined)
        0x15, 0x00,       // Logical Minimum (0)
        0x26, 0xFF, 0x00, // Logical Maximum (255)
        0x75, 0x08,       // Report Count (2)
        0x95, 0x02,       // Report Size (8 bit)
        0xB1, 0x02,       // Feature (Data, Variable, Absolute)

        0xC0              // End Collection (Application)
    };

    // Initialize HID Service
    p_input_report                      = &input_report_array[INPUT_REPORT_KEYS_INDEX];
    p_input_report->max_len             = INPUT_REPORT_KEYS_MAX_LEN;
    p_input_report->rep_ref.report_id   = INPUT_REP_REF_ID;
    p_input_report->rep_ref.report_type = BLE_HIDS_REP_TYPE_INPUT;

    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&p_input_report->security_mode.cccd_write_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&p_input_report->security_mode.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&p_input_report->security_mode.write_perm);

    p_output_report                      = &output_report_array[OUTPUT_REPORT_INDEX];
    p_output_report->max_len             = OUTPUT_REPORT_MAX_LEN;
    p_output_report->rep_ref.report_id   = OUTPUT_REP_REF_ID;
    p_output_report->rep_ref.report_type = BLE_HIDS_REP_TYPE_OUTPUT;

    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&p_output_report->security_mode.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&p_output_report->security_mode.write_perm);

    hid_info_flags = HID_INFO_FLAG_REMOTE_WAKE_MSK | HID_INFO_FLAG_NORMALLY_CONNECTABLE_MSK;

    memset(&hids_init_obj, 0, sizeof(hids_init_obj));

    hids_init_obj.evt_handler                    = on_hids_evt;
    hids_init_obj.error_handler                  = service_error_handler;
    hids_init_obj.is_kb                          = true;
    hids_init_obj.is_mouse                       = false;
    hids_init_obj.inp_rep_count                  = 1;
    hids_init_obj.p_inp_rep_array                = input_report_array;
    hids_init_obj.outp_rep_count                 = 1;
    hids_init_obj.p_outp_rep_array               = output_report_array;
    hids_init_obj.feature_rep_count              = 0;
    hids_init_obj.p_feature_rep_array            = NULL;
    hids_init_obj.rep_map.data_len               = sizeof(report_map_data);
    hids_init_obj.rep_map.p_data                 = report_map_data;
    hids_init_obj.hid_information.bcd_hid        = BASE_USB_HID_SPEC_VERSION;
    hids_init_obj.hid_information.b_country_code = 0;
    hids_init_obj.hid_information.flags          = hid_info_flags;
    hids_init_obj.included_services_count        = 0;
    hids_init_obj.p_included_services_array      = NULL;

    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&hids_init_obj.rep_map.security_mode.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&hids_init_obj.rep_map.security_mode.write_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&hids_init_obj.hid_information.security_mode.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&hids_init_obj.hid_information.security_mode.write_perm);

    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(
        &hids_init_obj.security_mode_boot_kb_inp_rep.cccd_write_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&hids_init_obj.security_mode_boot_kb_inp_rep.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&hids_init_obj.security_mode_boot_kb_inp_rep.write_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&hids_init_obj.security_mode_boot_kb_outp_rep.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&hids_init_obj.security_mode_boot_kb_outp_rep.write_perm);

    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&hids_init_obj.security_mode_protocol.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&hids_init_obj.security_mode_protocol.write_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&hids_init_obj.security_mode_ctrl_point.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&hids_init_obj.security_mode_ctrl_point.write_perm);

    err_code = ble_hids_init(&m_hids, &hids_init_obj);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing services that will be used by the application.
 */
static void services_init(void)
{
    dis_init();
    bas_init();
    hids_init();
}


/**@brief Function for initializing the battery sensor simulator.
 */
static void sensor_simulator_init(void)
{
    m_battery_sim_cfg.min          = MIN_BATTERY_LEVEL;
    m_battery_sim_cfg.max          = MAX_BATTERY_LEVEL;
    m_battery_sim_cfg.incr         = BATTERY_LEVEL_INCREMENT;
    m_battery_sim_cfg.start_at_max = true;

    sensorsim_init(&m_battery_sim_state, &m_battery_sim_cfg);
}


/**@brief Function for handling a Connection Parameters error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
    ret_code_t             err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = NULL;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for starting timers.
 */
static void timers_start(void)
{
    ret_code_t err_code;

    err_code = app_timer_start(m_battery_timer_id, BATTERY_LEVEL_MEAS_INTERVAL, NULL);
    APP_ERROR_CHECK(err_code);
}


/**@brief   Function for transmitting a key scan Press & Release Notification.
 *
 * @warning This handler is an example only. You need to analyze how you wish to send the key
 *          release.
 *
 * @param[in]  p_instance     Identifies the service for which Key Notifications are requested.
 * @param[in]  p_key_pattern  Pointer to key pattern.
 * @param[in]  pattern_len    Length of key pattern. 0 < pattern_len < 7.
 * @param[in]  pattern_offset Offset applied to Key Pattern for transmission.
 * @param[out] actual_len     Provides actual length of Key Pattern transmitted, making buffering of
 *                            rest possible if needed.
 * @return     NRF_SUCCESS on success, NRF_ERROR_RESOURCES in case transmission could not be
 *             completed due to lack of transmission buffer or other error codes indicating reason
 *             for failure.
 *
 * @note       In case of NRF_ERROR_RESOURCES, remaining pattern that could not be transmitted
 *             can be enqueued \ref buffer_enqueue function.
 *             In case a pattern of 'cofFEe' is the p_key_pattern, with pattern_len as 6 and
 *             pattern_offset as 0, the notifications as observed on the peer side would be
 *             1>    'c', 'o', 'f', 'F', 'E', 'e'
 *             2>    -  , 'o', 'f', 'F', 'E', 'e'
 *             3>    -  ,   -, 'f', 'F', 'E', 'e'
 *             4>    -  ,   -,   -, 'F', 'E', 'e'
 *             5>    -  ,   -,   -,   -, 'E', 'e'
 *             6>    -  ,   -,   -,   -,   -, 'e'
 *             7>    -  ,   -,   -,   -,   -,  -
 *             Here, '-' refers to release, 'c' refers to the key character being transmitted.
 *             Therefore 7 notifications will be sent.
 *             In case an offset of 4 was provided, the pattern notifications sent will be from 5-7
 *             will be transmitted.
 */
static uint32_t send_key_scan_press_release(ble_hids_t * p_hids,
                                            uint8_t    * p_key_pattern,
                                            uint16_t     pattern_len,
                                            uint16_t     pattern_offset,
                                            uint16_t   * p_actual_len)
{
    ret_code_t err_code;
    uint16_t offset;
    uint16_t data_len;
    uint8_t  data[INPUT_REPORT_KEYS_MAX_LEN];

    // HID Report Descriptor enumerates an array of size 6, the pattern hence shall not be any
    // longer than this.
    STATIC_ASSERT((INPUT_REPORT_KEYS_MAX_LEN - 2) == 6);

    ASSERT(pattern_len <= (INPUT_REPORT_KEYS_MAX_LEN - 2));

    offset   = pattern_offset;
    data_len = pattern_len;

    do
    {
        // Reset the data buffer.
        memset(data, 0, sizeof(data));

        // Copy the scan code.
        memcpy(data + SCAN_CODE_POS + offset, p_key_pattern + offset, data_len - offset);

        if (bsp_button_is_pressed(SHIFT_BUTTON_ID))
        {
            data[MODIFIER_KEY_POS] |= SHIFT_KEY_CODE;
        }

        if (!m_in_boot_mode)
        {
            err_code = ble_hids_inp_rep_send(p_hids,
                                             INPUT_REPORT_KEYS_INDEX,
                                             INPUT_REPORT_KEYS_MAX_LEN,
                                             data);
        }
        else
        {
            err_code = ble_hids_boot_kb_inp_rep_send(p_hids,
                                                     INPUT_REPORT_KEYS_MAX_LEN,
                                                     data);
        }

        if (err_code != NRF_SUCCESS)
        {
            break;
        }

        offset++;
    }
    while (offset <= data_len);

    *p_actual_len = offset;

    return err_code;
}


/**@brief   Function for initializing the buffer queue used to key events that could not be
 *          transmitted
 *
 * @warning This handler is an example only. You need to analyze how you wish to buffer or buffer at
 *          all.
 *
 * @note    In case of HID keyboard, a temporary buffering could be employed to handle scenarios
 *          where encryption is not yet enabled or there was a momentary link loss or there were no
 *          Transmit buffers.
 */
static void buffer_init(void)
{
    uint32_t buffer_count;

    BUFFER_LIST_INIT();

    for (buffer_count = 0; buffer_count < MAX_BUFFER_ENTRIES; buffer_count++)
    {
        BUFFER_ELEMENT_INIT(buffer_count);
    }
}


/**@brief Function for enqueuing key scan patterns that could not be transmitted either completely
 *        or partially.
 *
 * @warning This handler is an example only. You need to analyze how you wish to send the key
 *          release.
 *
 * @param[in]  p_hids         Identifies the service for which Key Notifications are buffered.
 * @param[in]  p_key_pattern  Pointer to key pattern.
 * @param[in]  pattern_len    Length of key pattern.
 * @param[in]  offset         Offset applied to Key Pattern when requesting a transmission on
 *                            dequeue, @ref buffer_dequeue.
 * @return     NRF_SUCCESS on success, else an error code indicating reason for failure.
 */
static uint32_t buffer_enqueue(ble_hids_t * p_hids,
                               uint8_t    * p_key_pattern,
                               uint16_t     pattern_len,
                               uint16_t     offset)
{
    buffer_entry_t * element;
    uint32_t         err_code = NRF_SUCCESS;

    if (BUFFER_LIST_FULL())
    {
        // Element cannot be buffered.
        err_code = NRF_ERROR_NO_MEM;
    }
    else
    {
        // Make entry of buffer element and copy data.
        element              = &buffer_list.buffer[(buffer_list.wp)];
        element->p_instance  = p_hids;
        element->p_data      = p_key_pattern;
        element->data_offset = offset;
        element->data_len    = pattern_len;

        buffer_list.count++;
        buffer_list.wp++;

        if (buffer_list.wp == MAX_BUFFER_ENTRIES)
        {
            buffer_list.wp = 0;
        }
    }

    return err_code;
}


/**@brief   Function to dequeue key scan patterns that could not be transmitted either completely of
 *          partially.
 *
 * @warning This handler is an example only. You need to analyze how you wish to send the key
 *          release.
 *
 * @param[in]  tx_flag   Indicative of whether the dequeue should result in transmission or not.
 * @note       A typical example when all keys are dequeued with transmission is when link is
 *             disconnected.
 *
 * @return     NRF_SUCCESS on success, else an error code indicating reason for failure.
 */
static uint32_t buffer_dequeue(bool tx_flag)
{
    buffer_entry_t * p_element;
    uint32_t         err_code = NRF_SUCCESS;
    uint16_t         actual_len;

    if (BUFFER_LIST_EMPTY())
    {
        err_code = NRF_ERROR_NOT_FOUND;
    }
    else
    {
        bool remove_element = true;

        p_element = &buffer_list.buffer[(buffer_list.rp)];

        if (tx_flag)
        {
            err_code = send_key_scan_press_release(p_element->p_instance,
                                                   p_element->p_data,
                                                   p_element->data_len,
                                                   p_element->data_offset,
                                                   &actual_len);
            // An additional notification is needed for release of all keys, therefore check
            // is for actual_len <= element->data_len and not actual_len < element->data_len
            if ((err_code == NRF_ERROR_RESOURCES) && (actual_len <= p_element->data_len))
            {
                // Transmission could not be completed, do not remove the entry, adjust next data to
                // be transmitted
                p_element->data_offset = actual_len;
                remove_element         = false;
            }
        }

        if (remove_element)
        {
            BUFFER_ELEMENT_INIT(buffer_list.rp);

            buffer_list.rp++;
            buffer_list.count--;

            if (buffer_list.rp == MAX_BUFFER_ENTRIES)
            {
                buffer_list.rp = 0;
            }
        }
    }

    return err_code;
}


/**@brief Function for sending sample key presses to the peer.
 *
 * @param[in]   key_pattern_len   Pattern length.
 * @param[in]   p_key_pattern     Pattern to be sent.
 */
static void keys_send(uint8_t key_pattern_len, uint8_t * p_key_pattern)
{
    ret_code_t err_code;
    uint16_t actual_len;

    err_code = send_key_scan_press_release(&m_hids,
                                           p_key_pattern,
                                           key_pattern_len,
                                           0,
                                           &actual_len);
    // An additional notification is needed for release of all keys, therefore check
    // is for actual_len <= key_pattern_len and not actual_len < key_pattern_len.
    if ((err_code == NRF_ERROR_RESOURCES) && (actual_len <= key_pattern_len))
    {
        // Buffer enqueue routine return value is not intentionally checked.
        // Rationale: Its better to have a a few keys missing than have a system
        // reset. Recommendation is to work out most optimal value for
        // MAX_BUFFER_ENTRIES to minimize chances of buffer queue full condition
        UNUSED_VARIABLE(buffer_enqueue(&m_hids, p_key_pattern, key_pattern_len, actual_len));
    }


    if ((err_code != NRF_SUCCESS) &&
        (err_code != NRF_ERROR_INVALID_STATE) &&
        (err_code != NRF_ERROR_RESOURCES) &&
        (err_code != BLE_ERROR_GATTS_SYS_ATTR_MISSING)
       )
    {
        APP_ERROR_HANDLER(err_code);
    }
}


/**@brief Function for handling the HID Report Characteristic Write event.
 *
 * @param[in]   p_evt   HID service event.
 */
static void on_hid_rep_char_write(ble_hids_evt_t * p_evt)
{
    if (p_evt->params.char_write.char_id.rep_type == BLE_HIDS_REP_TYPE_OUTPUT)
    {
        ret_code_t err_code;
        uint8_t  report_val;
        uint8_t  report_index = p_evt->params.char_write.char_id.rep_index;

        if (report_index == OUTPUT_REPORT_INDEX)
        {
            // This code assumes that the outptu report is one byte long. Hence the following
            // static assert is made.
            STATIC_ASSERT(OUTPUT_REPORT_MAX_LEN == 1);

            err_code = ble_hids_outp_rep_get(&m_hids,
                                             report_index,
                                             OUTPUT_REPORT_MAX_LEN,
                                             0,
                                             &report_val);
            APP_ERROR_CHECK(err_code);

            if (!m_caps_on && ((report_val & OUTPUT_REPORT_BIT_MASK_CAPS_LOCK) != 0))
            {
                // Caps Lock is turned On.
                NRF_LOG_INFO("Caps Lock is turned On!\r\n");
                err_code = bsp_indication_set(BSP_INDICATE_ALERT_3);
                APP_ERROR_CHECK(err_code);

                keys_send(sizeof(m_caps_on_key_scan_str), m_caps_on_key_scan_str);
                m_caps_on = true;
            }
            else if (m_caps_on && ((report_val & OUTPUT_REPORT_BIT_MASK_CAPS_LOCK) == 0))
            {
                // Caps Lock is turned Off .
                NRF_LOG_INFO("Caps Lock is turned Off!\r\n");
                err_code = bsp_indication_set(BSP_INDICATE_ALERT_OFF);
                APP_ERROR_CHECK(err_code);

                keys_send(sizeof(m_caps_off_key_scan_str), m_caps_off_key_scan_str);
                m_caps_on = false;
            }
            else
            {
                // The report received is not supported by this application. Do nothing.
            }
        }
    }
}


/**@brief Function for putting the chip into sleep mode.
 *
 * @note This function will not return.
 */
static void sleep_mode_enter(void)
{
    ret_code_t err_code;

    err_code = bsp_indication_set(BSP_INDICATE_IDLE);
    APP_ERROR_CHECK(err_code);

    // Prepare wakeup buttons.
    err_code = bsp_btn_ble_sleep_mode_prepare();
    APP_ERROR_CHECK(err_code);

    // Go to system-off mode (this function will not return; wakeup will cause a reset).
    err_code = sd_power_system_off();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling HID events.
 *
 * @details This function will be called for all HID events which are passed to the application.
 *
 * @param[in]   p_hids  HID service structure.
 * @param[in]   p_evt   Event received from the HID service.
 */
static void on_hids_evt(ble_hids_t * p_hids, ble_hids_evt_t * p_evt)
{
    switch (p_evt->evt_type)
    {
        case BLE_HIDS_EVT_BOOT_MODE_ENTERED:
            m_in_boot_mode = true;
            break;

        case BLE_HIDS_EVT_REPORT_MODE_ENTERED:
            m_in_boot_mode = false;
            break;

        case BLE_HIDS_EVT_REP_CHAR_WRITE:
            on_hid_rep_char_write(p_evt);
            break;

        case BLE_HIDS_EVT_NOTIF_ENABLED:
            break;

        default:
            // No implementation needed.
            break;
    }
}


/**@brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    ret_code_t err_code;

    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_DIRECTED:
            NRF_LOG_INFO("Directed advertising.\r\n");
            err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING_DIRECTED);
            APP_ERROR_CHECK(err_code);
            break; //BLE_ADV_EVT_DIRECTED

        case BLE_ADV_EVT_FAST:
            NRF_LOG_INFO("Fast advertising.\r\n");
            err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
            APP_ERROR_CHECK(err_code);
            break; //BLE_ADV_EVT_FAST

        case BLE_ADV_EVT_SLOW:
            NRF_LOG_INFO("Slow advertising.\r\n");
            err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING_SLOW);
            APP_ERROR_CHECK(err_code);
            break; //BLE_ADV_EVT_SLOW

        case BLE_ADV_EVT_FAST_WHITELIST:
            NRF_LOG_INFO("Fast advertising with whitelist.\r\n");
            err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING_WHITELIST);
            APP_ERROR_CHECK(err_code);
            break; //BLE_ADV_EVT_FAST_WHITELIST

        case BLE_ADV_EVT_SLOW_WHITELIST:
            NRF_LOG_INFO("Slow advertising with whitelist.\r\n");
            err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING_WHITELIST);
            APP_ERROR_CHECK(err_code);
            break; //BLE_ADV_EVT_SLOW_WHITELIST

        case BLE_ADV_EVT_IDLE:
            sleep_mode_enter();
            break; //BLE_ADV_EVT_IDLE

        case BLE_ADV_EVT_WHITELIST_REQUEST:
        {
            ble_gap_addr_t whitelist_addrs[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
            ble_gap_irk_t  whitelist_irks[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
            uint32_t       addr_cnt = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;
            uint32_t       irk_cnt  = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;

            err_code = pm_whitelist_get(whitelist_addrs, &addr_cnt,
                                        whitelist_irks,  &irk_cnt);
            APP_ERROR_CHECK(err_code);
            NRF_LOG_DEBUG("pm_whitelist_get returns %d addr in whitelist and %d irk whitelist\r\n",
                           addr_cnt,
                           irk_cnt);

            // Apply the whitelist.
            err_code = ble_advertising_whitelist_reply(whitelist_addrs, addr_cnt,
                                                       whitelist_irks,  irk_cnt);
            APP_ERROR_CHECK(err_code);
        } break; //BLE_ADV_EVT_WHITELIST_REQUEST

        case BLE_ADV_EVT_PEER_ADDR_REQUEST:
        {
            pm_peer_data_bonding_t peer_bonding_data;

            // Only Give peer address if we have a handle to the bonded peer.
            if (m_peer_id != PM_PEER_ID_INVALID)
            {
                err_code = pm_peer_data_bonding_load(m_peer_id, &peer_bonding_data);
                if (err_code != NRF_ERROR_NOT_FOUND)
                {
                    APP_ERROR_CHECK(err_code);

                    ble_gap_addr_t * p_peer_addr = &(peer_bonding_data.peer_ble_id.id_addr_info);
                    err_code = ble_advertising_peer_addr_reply(p_peer_addr);
                    APP_ERROR_CHECK(err_code);
                }
            }
        } break; //BLE_ADV_EVT_PEER_ADDR_REQUEST

        default:
            break;
    }
}


/**@brief Function for handling the Application's BLE Stack events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void on_ble_evt(ble_evt_t * p_ble_evt)
{
    ret_code_t err_code;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            NRF_LOG_INFO("Connected\r\n");
            err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
            APP_ERROR_CHECK(err_code);

            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            break; // BLE_GAP_EVT_CONNECTED

        case BLE_GATTS_EVT_HVN_TX_COMPLETE:
            // Send next key event
            (void) buffer_dequeue(true);
            break; // BLE_EVT_TX_COMPLETE

        case BLE_GAP_EVT_DISCONNECTED:
            NRF_LOG_INFO("Disconnected\r\n");
            // Dequeue all keys without transmission.
            (void) buffer_dequeue(false);

            m_conn_handle = BLE_CONN_HANDLE_INVALID;

            // Reset m_caps_on variable. Upon reconnect, the HID host will re-send the Output
            // report containing the Caps lock state.
            m_caps_on = false;
            // disabling alert 3. signal - used for capslock ON
            err_code = bsp_indication_set(BSP_INDICATE_ALERT_OFF);
            APP_ERROR_CHECK(err_code);

            if (m_is_wl_changed)
            {
                // The whitelist has been modified, update it in the Peer Manager.
                err_code = pm_whitelist_set(m_whitelist_peers, m_whitelist_peer_cnt);
                APP_ERROR_CHECK(err_code);

                err_code = pm_device_identities_list_set(m_whitelist_peers, m_whitelist_peer_cnt);
                if (err_code != NRF_ERROR_NOT_SUPPORTED)
                {
                    APP_ERROR_CHECK(err_code);
                }

                m_is_wl_changed = false;
            }
            break; // BLE_GAP_EVT_DISCONNECTED

        case BLE_GATTC_EVT_TIMEOUT:
            // Disconnect on GATT Client timeout event.
            NRF_LOG_DEBUG("GATT Client Timeout.\r\n");
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break; // BLE_GATTC_EVT_TIMEOUT

        case BLE_GATTS_EVT_TIMEOUT:
            // Disconnect on GATT Server timeout event.
            NRF_LOG_DEBUG("GATT Server Timeout.\r\n");
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break; // BLE_GATTS_EVT_TIMEOUT

        case BLE_EVT_USER_MEM_REQUEST:
            err_code = sd_ble_user_mem_reply(m_conn_handle, NULL);
            APP_ERROR_CHECK(err_code);
            break; // BLE_EVT_USER_MEM_REQUEST

        case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
        {
            ble_gatts_evt_rw_authorize_request_t  req;
            ble_gatts_rw_authorize_reply_params_t auth_reply;

            req = p_ble_evt->evt.gatts_evt.params.authorize_request;

            if (req.type != BLE_GATTS_AUTHORIZE_TYPE_INVALID)
            {
                if ((req.request.write.op == BLE_GATTS_OP_PREP_WRITE_REQ)     ||
                    (req.request.write.op == BLE_GATTS_OP_EXEC_WRITE_REQ_NOW) ||
                    (req.request.write.op == BLE_GATTS_OP_EXEC_WRITE_REQ_CANCEL))
                {
                    if (req.type == BLE_GATTS_AUTHORIZE_TYPE_WRITE)
                    {
                        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
                    }
                    else
                    {
                        auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_READ;
                    }
                    auth_reply.params.write.gatt_status = APP_FEATURE_NOT_SUPPORTED;
                    err_code = sd_ble_gatts_rw_authorize_reply(p_ble_evt->evt.gatts_evt.conn_handle,
                                                               &auth_reply);
                    APP_ERROR_CHECK(err_code);
                }
            }
        } break; // BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST

        default:
            // No implementation needed.
            break;
    }
}


/**@brief   Function for dispatching a BLE stack event to all modules with a BLE stack event handler.
 *
 * @details This function is called from the scheduler in the main loop after a BLE stack
 *          event has been received.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void ble_evt_dispatch(ble_evt_t * p_ble_evt)
{
    /** The Connection state module has to be fed BLE events in order to function correctly
     * Remember to call ble_conn_state_on_ble_evt before calling any ble_conns_state_* functions. */
    ble_conn_state_on_ble_evt(p_ble_evt);
    pm_on_ble_evt(p_ble_evt);
    bsp_btn_ble_on_ble_evt(p_ble_evt);
    on_ble_evt(p_ble_evt);
    ble_advertising_on_ble_evt(p_ble_evt);
    ble_conn_params_on_ble_evt(p_ble_evt);
    ble_hids_on_ble_evt(&m_hids, p_ble_evt);
    ble_bas_on_ble_evt(&m_bas, p_ble_evt);
    nrf_ble_gatt_on_ble_evt(&m_gatt, p_ble_evt);
}


/**@brief   Function for dispatching a system event to interested modules.
 *
 * @details This function is called from the System event interrupt handler after a system
 *          event has been received.
 *
 * @param[in]   sys_evt   System stack event.
 */
static void sys_evt_dispatch(uint32_t sys_evt)
{
    // Dispatch the system event to the fstorage module, where it will be
    // dispatched to the Flash Data Storage (FDS) module.
    fs_sys_event_handler(sys_evt);

    // Dispatch to the Advertising module last, since it will check if there are any
    // pending flash operations in fstorage. Let fstorage process system events first,
    // so that it can report correctly to the Advertising module.
    ble_advertising_on_sys_evt(sys_evt);
}


/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    ret_code_t err_code;

    nrf_clock_lf_cfg_t clock_lf_cfg = NRF_CLOCK_LFCLKSRC;

    // Initialize the SoftDevice handler module.
    SOFTDEVICE_HANDLER_APPSH_INIT(&clock_lf_cfg, true);

    // Fetch the start address of the application RAM.
    uint32_t ram_start = 0;
    err_code = softdevice_app_ram_start_get(&ram_start);
    APP_ERROR_CHECK(err_code);

    // Overwrite some of the default configurations for the BLE stack.
    ble_cfg_t ble_cfg;

    // Configure the number of custom UUIDS.
    memset(&ble_cfg, 0, sizeof(ble_cfg));
    ble_cfg.common_cfg.vs_uuid_cfg.vs_uuid_count = 0;
    err_code = sd_ble_cfg_set(BLE_COMMON_CFG_VS_UUID, &ble_cfg, ram_start);
    APP_ERROR_CHECK(err_code);

    // Configure the maximum number of connections.
    memset(&ble_cfg, 0, sizeof(ble_cfg));
    ble_cfg.gap_cfg.role_count_cfg.periph_role_count  = BLE_GAP_ROLE_COUNT_PERIPH_DEFAULT;
    ble_cfg.gap_cfg.role_count_cfg.central_role_count = 0;
    ble_cfg.gap_cfg.role_count_cfg.central_sec_count  = 0;
    err_code = sd_ble_cfg_set(BLE_GAP_CFG_ROLE_COUNT, &ble_cfg, ram_start);
    APP_ERROR_CHECK(err_code);

    // Enable BLE stack.
    err_code = softdevice_enable(&ram_start);
    APP_ERROR_CHECK(err_code);

    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
    APP_ERROR_CHECK(err_code);

    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_sys_evt_handler_set(sys_evt_dispatch);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for the Event Scheduler initialization.
 */
static void scheduler_init(void)
{
    APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
}


/**@brief Function for handling events from the BSP module.
 *
 * @param[in]   event   Event generated by button press.
 */
static void bsp_event_handler(bsp_event_t event)
{
    uint32_t         err_code;
    static uint8_t * p_key = m_sample_key_press_scan_str;
    static uint8_t   size  = 0;

    switch (event)
    {
        case BSP_EVENT_SLEEP:
            sleep_mode_enter();
            break;

        case BSP_EVENT_DISCONNECT:
            err_code = sd_ble_gap_disconnect(m_conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            if (err_code != NRF_ERROR_INVALID_STATE)
            {
                APP_ERROR_CHECK(err_code);
            }
            break;

        case BSP_EVENT_WHITELIST_OFF:
            if (m_conn_handle == BLE_CONN_HANDLE_INVALID)
            {
                err_code = ble_advertising_restart_without_whitelist();
                if (err_code != NRF_ERROR_INVALID_STATE)
                {
                    APP_ERROR_CHECK(err_code);
                }
            }
            break;

        case BSP_EVENT_KEY_0:
            if (m_conn_handle != BLE_CONN_HANDLE_INVALID)
            {
                keys_send(1, p_key);
                p_key++;
                size++;
                if (size == MAX_KEYS_IN_ONE_REPORT)
                {
                    p_key = m_sample_key_press_scan_str;
                    size  = 0;
                }
            }
            break;

        default:
            break;
    }
}


/**@brief Function for the Peer Manager initialization.
 */
static void peer_manager_init(void)
{
    ble_gap_sec_params_t sec_param;
    ret_code_t           err_code;

    err_code = pm_init();
    APP_ERROR_CHECK(err_code);

    memset(&sec_param, 0, sizeof(ble_gap_sec_params_t));

    // Security parameters to be used for all security procedures.
    sec_param.bond           = SEC_PARAM_BOND;
    sec_param.mitm           = SEC_PARAM_MITM;
    sec_param.lesc           = SEC_PARAM_LESC;
    sec_param.keypress       = SEC_PARAM_KEYPRESS;
    sec_param.io_caps        = SEC_PARAM_IO_CAPABILITIES;
    sec_param.oob            = SEC_PARAM_OOB;
    sec_param.min_key_size   = SEC_PARAM_MIN_KEY_SIZE;
    sec_param.max_key_size   = SEC_PARAM_MAX_KEY_SIZE;
    sec_param.kdist_own.enc  = 1;
    sec_param.kdist_own.id   = 1;
    sec_param.kdist_peer.enc = 1;
    sec_param.kdist_peer.id  = 1;

    err_code = pm_sec_params_set(&sec_param);
    APP_ERROR_CHECK(err_code);

    err_code = pm_register(pm_evt_handler);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the Advertising functionality.
 */
static void advertising_init(void)
{
    uint32_t               err_code;
    uint8_t                adv_flags;
    ble_advdata_t          advdata;
    ble_adv_modes_config_t options;

    // Build and set advertising data
    memset(&advdata, 0, sizeof(advdata));

    adv_flags                       = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    advdata.name_type               = BLE_ADVDATA_FULL_NAME;
    advdata.include_appearance      = true;
    advdata.flags                   = adv_flags;
    advdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    advdata.uuids_complete.p_uuids  = m_adv_uuids;

    memset(&options, 0, sizeof(options));
    options.ble_adv_whitelist_enabled      = true;
    options.ble_adv_directed_enabled       = true;
    options.ble_adv_directed_slow_enabled  = false;
    options.ble_adv_directed_slow_interval = 0;
    options.ble_adv_directed_slow_timeout  = 0;
    options.ble_adv_fast_enabled           = true;
    options.ble_adv_fast_interval          = APP_ADV_FAST_INTERVAL;
    options.ble_adv_fast_timeout           = APP_ADV_FAST_TIMEOUT;
    options.ble_adv_slow_enabled           = true;
    options.ble_adv_slow_interval          = APP_ADV_SLOW_INTERVAL;
    options.ble_adv_slow_timeout           = APP_ADV_SLOW_TIMEOUT;

    err_code = ble_advertising_init(&advdata,
                                    NULL,
                                    &options,
                                    on_adv_evt,
                                    ble_advertising_error_handler);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing buttons and leds.
 *
 * @param[out] p_erase_bonds  Will be true if the clear bonding button was pressed to wake the application up.
 */
static void buttons_leds_init(bool * p_erase_bonds)
{
    ret_code_t err_code;
    bsp_event_t startup_event;

    err_code = bsp_init(BSP_INIT_LED | BSP_INIT_BUTTONS, bsp_event_handler);
    APP_ERROR_CHECK(err_code);

    err_code = bsp_btn_ble_init(NULL, &startup_event);
    APP_ERROR_CHECK(err_code);

    *p_erase_bonds = (startup_event == BSP_EVENT_CLEAR_BONDING_DATA);
}


/**@brief Function for initializing the nrf log module.
 */
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for the Power manager.
 */
static void power_manage(void)
{
    ret_code_t err_code = sd_app_evt_wait();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for application main entry.
 */
int main(void)
{
    bool erase_bonds;

    // Initialize.
    log_init();
    timers_init();
    buttons_leds_init(&erase_bonds);
    ble_stack_init();
    scheduler_init();
    gap_params_init();
    gatt_init();
    advertising_init();
    services_init();
    sensor_simulator_init();
    conn_params_init();
    buffer_init();
    peer_manager_init();

    // Start execution.
    NRF_LOG_INFO("HID Keyboard example started.\r\n");
    timers_start();

    advertising_start(erase_bonds);

    // Enter main loop.
    for (;;)
    {
        app_sched_execute();
        if (NRF_LOG_PROCESS() == false)
        {
            power_manage();
        }
    }
}


/**
 * @}
 */

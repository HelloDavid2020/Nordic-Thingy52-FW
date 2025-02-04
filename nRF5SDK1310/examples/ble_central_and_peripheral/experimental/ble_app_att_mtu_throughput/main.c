/**
 * Copyright (c) 2016 - 2017, Nordic Semiconductor ASA
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
/**@cond To Make Doxygen skip documentation generation for this file.
 * @{
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "amt.h"
#include "counter.h"

#include "sdk_config.h"
#include "boards.h"
#include "bsp.h"
#include "bsp_btn_ble.h"
#include "nrf.h"
#include "ble.h"
#include "ble_hci.h"
#include "nordic_common.h"
#include "nrf_gpio.h"
#include "ble_advdata.h"
#include "ble_srv_common.h"
#include "softdevice_handler.h"
#include "nrf_ble_gatt.h"
#include "app_timer.h"
#include "app_error.h"
#include "ble_conn_params.h"

#define NRF_LOG_MODULE_NAME "APP"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"


#define CONN_INTERVAL_DEFAULT           (uint16_t)(MSEC_TO_UNITS(7.5, UNIT_1_25_MS))    /**< Default connection interval used at connection establishment by central side. */

#define CONN_INTERVAL_MIN               (uint16_t)(MSEC_TO_UNITS(7.5, UNIT_1_25_MS))    /**< Minimum acceptable connection interval, in 1.25 ms units. */
#define CONN_INTERVAL_MAX               (uint16_t)(MSEC_TO_UNITS(500, UNIT_1_25_MS))    /**< Maximum acceptable connection interval, in 1.25 ms units. */
#define CONN_SUP_TIMEOUT                (uint16_t)(MSEC_TO_UNITS(4000,  UNIT_10_MS))    /**< Connection supervisory timeout (4 seconds). */
#define SLAVE_LATENCY                   0                                               /**< Slave latency. */

#define SCAN_ADV_LED                    BSP_BOARD_LED_0
#define READY_LED                       BSP_BOARD_LED_1
#define PROGRESS_LED                    BSP_BOARD_LED_2
#define DONE_LED                        BSP_BOARD_LED_3

#define YES                             'y'
#define ONE                             '1'
#define TWO                             '2'
#define THREE                           '3'
#define FOUR                            '4'
#define ENTER                           '\r'

#define BOARD_TESTER_BUTTON             BSP_BUTTON_2                                    /**< Button to press at beginning of the test to indicate that this board is connected to the PC and takes input from it via the UART. */
#define BOARD_DUMMY_BUTTON              BSP_BUTTON_3                                    /**< Button to press at beginning of the test to indicate that this board is standalone (automatic behavior). */
#define BUTTON_DETECTION_DELAY          APP_TIMER_TICKS(50)                             /**< Delay from a GPIOTE event until a button is reported as pushed (in number of timer ticks). */

#define LL_HEADER_LEN                   4                                               /**< Link layer header length. */
#define CONN_CFG_TAG                    1                                               /**< A tag that refers to the BLE stack configuration we set with @ref sd_ble_cfg_set. Default tag is @ref BLE_CONN_CFG_TAG_DEFAULT. */

#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(500)                            /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (0.5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(500)                            /**< Time between each call to sd_ble_gap_conn_param_update after the first call (0.5 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    2                                               /**< Number of attempts before giving up the connection parameter negotiation. */


typedef enum
{
    NOT_SELECTED = 0x00,
    BOARD_TESTER,
    BOARD_DUMMY,
} board_role_t;

typedef struct
{
    uint16_t att_mtu;                   /**< GATT ATT MTU, in bytes. */
    uint16_t conn_interval;             /**< Connection interval expressed in units of 1.25 ms. */
    bool     data_len_ext_enabled;      /**< Data length extension status. */
    bool     conn_evt_len_ext_enabled;  /**< Connection event length extension status. */
    uint8_t  rxtx_phy;                  /**< Preferred PHY. */
} test_params_t;

/**@brief Variable length data encapsulation in terms of length and pointer to data. */
typedef struct
{
    uint8_t  * p_data;      /**< Pointer to data. */
    uint16_t   data_len;    /**< Length of data. */
} data_t;


static nrf_ble_amtc_t m_amtc;
static nrf_ble_amts_t m_amts;

static board_role_t volatile m_board_role  = NOT_SELECTED;

static bool volatile m_run_test;
static bool volatile m_print_menu;
static bool volatile m_notif_enabled;
static bool volatile m_mtu_exchanged;
static bool volatile m_data_length_updated;
#if defined(S140)
static bool volatile m_phy_updated;
#endif
static bool volatile m_conn_interval_configured;

static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;    /**< Handle of the current BLE connection .*/
static uint8_t m_gap_role     = BLE_GAP_ROLE_INVALID;       /**< BLE role for this connection, see @ref BLE_GAP_ROLES */

static nrf_ble_gatt_t     m_gatt;                           /**< GATT module instance. */
static ble_db_discovery_t m_ble_db_discovery;               /**< DB discovery module instance. */

// Name to use for advertising and connection.
static char const m_target_periph_name[] = DEVICE_NAME;

// Test parameters.
// Settings like ATT MTU size are set only once on the dummy board.
// Make sure that defaults are sensible.
static test_params_t m_test_params =
{
    .att_mtu                  = NRF_BLE_GATT_MAX_MTU_SIZE,
    .conn_interval            = CONN_INTERVAL_DEFAULT,
    .data_len_ext_enabled     = true,
    .conn_evt_len_ext_enabled = true,
#if defined(S140)
    .rxtx_phy                 = BLE_GAP_PHY_2MBPS | BLE_GAP_PHY_1MBPS | BLE_GAP_PHY_CODED,
#endif
};

// Scan parameters requested for scanning and connection.
static ble_gap_scan_params_t const m_scan_param =
{
    .active         = 0x00,
    .interval       = SCAN_INTERVAL,
    .window         = SCAN_WINDOW,
    .use_whitelist  = 0x00,
    .adv_dir_report = 0x00,
    .timeout        = 0x0000, // No timeout.
};

// Connection parameters requested for connection.
static ble_gap_conn_params_t m_conn_param =
{
    .min_conn_interval = CONN_INTERVAL_MIN,   // Minimum connection interval.
    .max_conn_interval = CONN_INTERVAL_MAX,   // Maximum connection interval.
    .slave_latency     = SLAVE_LATENCY,       // Slave latency.
    .conn_sup_timeout  = CONN_SUP_TIMEOUT     // Supervisory timeout.
};

static void test_terminate(void);


#ifdef S140
static uint32_t phy_str(uint8_t phy)
{
    static char const * str[] =
    {
        "1 Mbps",
        "2 Mbps",
        "Coded",
        "Unknown"
    };

    switch (phy)
    {
        case BLE_GAP_PHY_1MBPS:
            return (uint32_t)(str[0]);

        case BLE_GAP_PHY_2MBPS:
        case BLE_GAP_PHY_2MBPS | BLE_GAP_PHY_1MBPS:
        case BLE_GAP_PHY_2MBPS | BLE_GAP_PHY_1MBPS | BLE_GAP_PHY_CODED:
            return (uint32_t)(str[1]);

        case BLE_GAP_PHY_CODED:
            return (uint32_t)(str[2]);

        default:
            return (uint32_t)(str[3]);
    }
}
#endif

/**@brief Parses advertisement data, providing length and location of the field in case
 *        matching data is found.
 *
 * @param[in]  Type of data to be looked for in advertisement data.
 * @param[in]  Advertisement report length and pointer to report.
 * @param[out] If data type requested is found in the data report, type data length and
 *             pointer to data will be populated here.
 *
 * @retval NRF_SUCCESS if the data type is found in the report.
 * @retval NRF_ERROR_NOT_FOUND if the data type could not be found.
 */
static uint32_t adv_report_parse(uint8_t type, data_t * p_advdata, data_t * p_typedata)
{
    uint32_t  index = 0;
    uint8_t * p_data;

    p_data = p_advdata->p_data;

    while (index < p_advdata->data_len)
    {
        uint8_t field_length = p_data[index];
        uint8_t field_type   = p_data[index + 1];

        if (field_type == type)
        {
            p_typedata->p_data   = &p_data[index + 2];
            p_typedata->data_len = field_length - 1;
            return NRF_SUCCESS;
        }
        index += field_length + 1;
    }
    return NRF_ERROR_NOT_FOUND;
}


/**@brief Function for searching a given name in the advertisement packets.
 *
 * @details Use this function to parse received advertising data and to find a given
 * name in them either as 'complete_local_name' or as 'short_local_name'.
 *
 * @param[in]   p_adv_report   advertising data to parse.
 * @param[in]   name_to_find   name to search.
 * @return   true if the given name was found, false otherwise.
 */
static bool find_adv_name(ble_gap_evt_adv_report_t const * p_adv_report, char const * name_to_find)
{
    ret_code_t err_code;
    data_t   adv_data;
    data_t   dev_name;
    bool     found = false;

    // Initialize advertisement report for parsing.
    adv_data.p_data   = (uint8_t *)p_adv_report->data;
    adv_data.data_len = p_adv_report->dlen;

    // Search for matching advertising names.
    err_code = adv_report_parse(BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME, &adv_data, &dev_name);

    if (   (err_code == NRF_SUCCESS)
        && (strlen(name_to_find) == dev_name.data_len)
        && (memcmp(name_to_find, dev_name.p_data, dev_name.data_len) == 0))
    {
        found = true;
    }
    else
    {
        // Look for the short local name if the complete name was not found.
        err_code = adv_report_parse(BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME, &adv_data, &dev_name);

        if (   (err_code == NRF_SUCCESS)
            && (strlen(name_to_find) == dev_name.data_len)
            && (memcmp(m_target_periph_name, dev_name.p_data, dev_name.data_len) == 0))
        {
            found = true;
        }
    }

    return found;
}


/**@brief Function for handling BLE_GAP_ADV_REPORT events.
 * Search for a peer with matching device name.
 * If found, stop advertising and send a connection request to the peer.
 */
static void on_ble_gap_evt_adv_report(ble_gap_evt_t const * p_gap_evt)
{
    if (!find_adv_name(&p_gap_evt->params.adv_report, m_target_periph_name))
    {
        return;
    }

    NRF_LOG_INFO("Device \"%s\" found, sending a connection request.\r\n",
                 (uint32_t) m_target_periph_name);

    // Stop advertising.
    (void) sd_ble_gap_adv_stop();

    // Initiate connection.
    m_conn_param.min_conn_interval = CONN_INTERVAL_DEFAULT;
    m_conn_param.max_conn_interval = CONN_INTERVAL_DEFAULT;

    ret_code_t err_code;
    err_code = sd_ble_gap_connect(&p_gap_evt->params.adv_report.peer_addr,
                                  &m_scan_param,
                                  &m_conn_param,
                                  CONN_CFG_TAG);

    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("sd_ble_gap_connect() failed: 0x%x.\r\n", err_code);
    }
}


/**@brief Function for handling BLE_GAP_EVT_CONNECTED events.
 * Save the connection handle and GAP role, then discover the peer DB.
 */
static void on_ble_gap_evt_connected(ble_gap_evt_t const * p_gap_evt)
{
    m_conn_handle = p_gap_evt->conn_handle;
    m_gap_role    = p_gap_evt->params.connected.role;

    if (m_gap_role == BLE_GAP_ROLE_PERIPH)
    {
        NRF_LOG_INFO("Connected as a peripheral.\r\n");
    }
    else if (m_gap_role == BLE_GAP_ROLE_CENTRAL)
    {
        NRF_LOG_INFO("Connected as a central.\r\n");
    }

    // Stop scanning and advertising.
    (void) sd_ble_gap_scan_stop();
    (void) sd_ble_gap_adv_stop();

    NRF_LOG_INFO("Discovering GATT database...\r\n");

    // Zero the database before starting discovery.
    memset(&m_ble_db_discovery, 0x00, sizeof(m_ble_db_discovery));

    ret_code_t err_code;
    err_code  = ble_db_discovery_start(&m_ble_db_discovery, p_gap_evt->conn_handle);
    APP_ERROR_CHECK(err_code);

#if defined(S140)
    // Request PHY.
    ble_gap_phys_t phys =
    {
        .tx_phys = m_test_params.rxtx_phy,
        .rx_phys = m_test_params.rxtx_phy,
    };

    err_code = sd_ble_gap_phy_request(p_gap_evt->conn_handle, &phys);
    APP_ERROR_CHECK(err_code);
#endif

    bsp_board_leds_off();
}


/**@brief Function for handling BLE_GAP_EVT_DISCONNECTED events.
 * Unset the connection handle and terminate the test.
 */
static void on_ble_gap_evt_disconnected(ble_gap_evt_t const * p_gap_evt)
{
    m_conn_handle = BLE_CONN_HANDLE_INVALID;

    NRF_LOG_DEBUG("Disconnected: reason 0x%x.\r\n", p_gap_evt->params.disconnected.reason);

    if (m_run_test)
    {
        NRF_LOG_WARNING("GAP disconnection event received while test was running.\r\n")
    }

    bsp_board_leds_off();

    test_terminate();
}


/**@brief Function for handling BLE Stack events.
 *
 * @param[in] p_ble_evt  Bluetooth stack event.
 */
static void on_ble_evt(ble_evt_t * p_ble_evt)
{
    uint32_t              err_code;
    ble_gap_evt_t const * p_gap_evt = &p_ble_evt->evt.gap_evt;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_ADV_REPORT:
            on_ble_gap_evt_adv_report(p_gap_evt);
            break;

        case BLE_GAP_EVT_CONNECTED:
            on_ble_gap_evt_connected(p_gap_evt);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            on_ble_gap_evt_disconnected(p_gap_evt);
            break;

        case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST:
        {
            // Accept parameters requested by the peer.
            ble_gap_conn_params_t params;
            params = p_gap_evt->params.conn_param_update_request.conn_params;
            params.max_conn_interval = params.min_conn_interval;
            err_code = sd_ble_gap_conn_param_update(p_gap_evt->conn_handle, &params);
            APP_ERROR_CHECK(err_code);
        } break;

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
        {
            err_code = sd_ble_gatts_sys_attr_set(p_gap_evt->conn_handle, NULL, 0, 0);
            APP_ERROR_CHECK(err_code);
        } break;

        case BLE_GATTC_EVT_TIMEOUT: // Fallthrough.
        case BLE_GATTS_EVT_TIMEOUT:
        {
            NRF_LOG_DEBUG("GATT timeout, disconnecting.\r\n");
            err_code = sd_ble_gap_disconnect(m_conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
        } break;

        case BLE_EVT_USER_MEM_REQUEST:
        {
            err_code = sd_ble_user_mem_reply(p_ble_evt->evt.common_evt.conn_handle, NULL);
            APP_ERROR_CHECK(err_code);
        } break;

#if defined(S140)
        case BLE_GAP_EVT_PHY_UPDATE:
        {
            ble_gap_evt_phy_update_t * const p_phy_evt = &p_ble_evt->evt.gap_evt.params.phy_update;

            if (p_phy_evt->status == BLE_HCI_STATUS_CODE_LMP_ERROR_TRANSACTION_COLLISION)
            {
                // Ignore LL collisions.
                NRF_LOG_DEBUG("LL transaction collision during PHY.\r\n");
                break;
            }

            m_phy_updated = true;

            NRF_LOG_INFO("PHY update %s. PHY set to %s.\r\n",
                         (p_phy_evt->status == BLE_HCI_STATUS_CODE_SUCCESS) ?
                            (uint32_t)"accepted" : (uint32_t)"rejected",
                         phy_str(p_phy_evt->tx_phy));
        } break;
#endif

        default:
            // No implementation needed.
            break;
    }
}


/**@brief Function for dispatching a BLE stack event to all modules with a BLE stack event handler.
 *
 * @details This function is called from the BLE Stack event interrupt handler after a BLE stack
 *          event has been received.
 *
 * @param[in] p_ble_evt  Bluetooth stack event.
 */
static void ble_evt_dispatch(ble_evt_t * p_ble_evt)
{
    on_ble_evt(p_ble_evt);
    ble_conn_params_on_ble_evt(p_ble_evt);
    ble_db_discovery_on_ble_evt(&m_ble_db_discovery, p_ble_evt);
    nrf_ble_gatt_on_ble_evt(&m_gatt, p_ble_evt);
    nrf_ble_amts_on_ble_evt(&m_amts, p_ble_evt);
    nrf_ble_amtc_on_ble_evt(&m_amtc, p_ble_evt);
}


/**@brief Function for handling events from the button handler module.
 *
 * @param[in] pin_no        The pin that the event applies to.
 * @param[in] button_action The button action (press/release).
 */
static void button_evt_handler(uint8_t pin_no, uint8_t button_action)
{
    switch (pin_no)
    {
        case BOARD_TESTER_BUTTON:
        {
            NRF_LOG_INFO("This board will act as tester.\r\n");
            m_board_role = BOARD_TESTER;
        } break;

        case BOARD_DUMMY_BUTTON:
        {
            NRF_LOG_INFO("This board will act as responder.\r\n");
            m_board_role = BOARD_DUMMY;
        } break;

        default:
            break;
    }
}


/**@brief AMT Service Handler.
 */
static void amts_evt_handler(nrf_ble_amts_evt_t evt)
{
    ret_code_t err_code;

    switch (evt.evt_type)
    {
        case NRF_BLE_AMTS_EVT_NOTIF_ENABLED:
        {
            NRF_LOG_INFO("Notifications enabled.\r\n");

            bsp_board_led_on(READY_LED);
            m_notif_enabled = true;

            if (m_board_role != BOARD_TESTER)
            {
                return;
            }

            if (m_gap_role == BLE_GAP_ROLE_PERIPH)
            {
                m_conn_interval_configured     = false;
                m_conn_param.min_conn_interval = m_test_params.conn_interval;
                m_conn_param.max_conn_interval = m_test_params.conn_interval + 1;
                err_code = ble_conn_params_change_conn_params(&m_conn_param);
                if (err_code != NRF_SUCCESS)
                {
                    NRF_LOG_ERROR("ble_conn_params_change_conn_params() failed: 0x%x.\r\n",
                                  err_code);
                }
            }

            if (m_gap_role == BLE_GAP_ROLE_CENTRAL)
            {
                m_conn_interval_configured     = true;
                m_conn_param.min_conn_interval = m_test_params.conn_interval;
                m_conn_param.max_conn_interval = m_test_params.conn_interval;
                err_code = sd_ble_gap_conn_param_update(m_conn_handle, &m_conn_param);
                if (err_code != NRF_SUCCESS)
                {
                    NRF_LOG_ERROR("sd_ble_gap_conn_param_update() failed: 0x%x.\r\n", err_code);
                }
            }
        } break;

        case NRF_BLE_AMTS_EVT_NOTIF_DISABLED:
        {
            NRF_LOG_INFO("Notifications disabled.\r\n");
            bsp_board_led_off(READY_LED);
        } break;

        case NRF_BLE_AMTS_EVT_TRANSFER_1KB:
        {
            NRF_LOG_INFO("Sent %u KBytes\r\n", (evt.bytes_transfered_cnt / 1024));
            bsp_board_led_invert(PROGRESS_LED);
        } break;

        case NRF_BLE_AMTS_EVT_TRANSFER_FINISHED:
        {
            counter_stop();

            bsp_board_led_off(PROGRESS_LED);
            bsp_board_led_on(DONE_LED);

            uint32_t time_ms      = counter_get();
            uint32_t bit_count    = (evt.bytes_transfered_cnt * 8);
            float throughput_kbps = ((bit_count / (time_ms / 1000.f)) / 1000.f);

            NRF_LOG_INFO("Done.\r\n\r\n");
            NRF_LOG_INFO("=============================\r\n");
            NRF_LOG_INFO("Time: %u.%.2u seconds elapsed.\r\n", (time_ms / 1000), (time_ms % 1000));
            NRF_LOG_INFO("Throughput: " NRF_LOG_FLOAT_MARKER " Kbps.\r\n",
                         NRF_LOG_FLOAT(throughput_kbps));
            NRF_LOG_INFO("=============================\r\n");
            NRF_LOG_INFO("Sent %u bytes of ATT payload.\r\n", evt.bytes_transfered_cnt);
            NRF_LOG_INFO("Retrieving amount of bytes received from peer...\r\n");

            err_code = nrf_ble_amtc_rcb_read(&m_amtc);
            if (err_code != NRF_SUCCESS)
            {
                NRF_LOG_ERROR("nrf_ble_amtc_rcb_read() failed: 0x%x.\r\n", err_code);
                test_terminate();
            }
        } break;
    }
}


/**@brief AMT Client Handler.
 */
static void amtc_evt_handler(nrf_ble_amtc_t * p_amt_c, nrf_ble_amtc_evt_t * p_evt)
{
    ret_code_t err_code;

    switch (p_evt->evt_type)
    {
        case NRF_BLE_AMT_C_EVT_DISCOVERY_COMPLETE:
        {
            NRF_LOG_INFO("AMT service discovered at peer.\r\n");

            err_code = nrf_ble_amtc_handles_assign(p_amt_c,
                                                   p_evt->conn_handle,
                                                   &p_evt->params.peer_db);
            APP_ERROR_CHECK(err_code);

            // Enable notifications.
            err_code = nrf_ble_amtc_notif_enable(p_amt_c);
            APP_ERROR_CHECK(err_code);
        } break;

        case NRF_BLE_AMT_C_EVT_NOTIFICATION:
        {
            static uint32_t bytes_cnt  = 0;
            static uint32_t kbytes_cnt = 0;

            if (p_evt->params.hvx.bytes_sent == 0)
            {
                bytes_cnt  = 0;
                kbytes_cnt = 0;
            }

            bytes_cnt += p_evt->params.hvx.notif_len;

            if (bytes_cnt > 1024)
            {
                bsp_board_led_invert(PROGRESS_LED);

                bytes_cnt -= 1024;
                kbytes_cnt++;

                NRF_LOG_INFO("Received %u kbytes\r\n", kbytes_cnt);

                nrf_ble_amts_rbc_set(&m_amts, p_evt->params.hvx.bytes_rcvd);
            }

            if (p_evt->params.hvx.bytes_rcvd >= AMT_BYTE_TRANSFER_CNT)
            {
                bsp_board_led_off(PROGRESS_LED);

                bytes_cnt  = 0;
                kbytes_cnt = 0;

                NRF_LOG_INFO("Transfer complete, received %u bytes of ATT payload.\r\n",
                             p_evt->params.hvx.bytes_rcvd);

                nrf_ble_amts_rbc_set(&m_amts, p_evt->params.hvx.bytes_rcvd);
            }
        } break;

        case NRF_BLE_AMT_C_EVT_RBC_READ_RSP:
        {
            NRF_LOG_INFO("Peer received %u bytes of ATT payload.\r\n", (p_evt->params.rcv_bytes_cnt));
            test_terminate();
        } break;

        default:
            break;
    }
}


/**@brief Function for handling Database Discovery events.
 *
 * @details This function is a callback function to handle events from the database discovery module.
 *          Depending on the UUIDs that are discovered, this function should forward the events
 *          to their respective service instances.
 *
 * @param[in] p_event  Pointer to the database discovery event.
 */
static void db_disc_evt_handler(ble_db_discovery_evt_t * p_evt)
{
    nrf_ble_amtc_on_db_disc_evt(&m_amtc, p_evt);
}


/**@brief Function for handling events from the GATT library. */
static void gatt_evt_handler(nrf_ble_gatt_t * p_gatt, nrf_ble_gatt_evt_t const * p_evt)
{
    switch (p_evt->evt_id)
    {
        case NRF_BLE_GATT_EVT_ATT_MTU_UPDATED:
        {
            m_mtu_exchanged = true;
            NRF_LOG_INFO("ATT MTU exchange completed. MTU set to %u bytes.\r\n",
                         p_evt->params.att_mtu_effective);
        } break;

        case NRF_BLE_GATT_EVT_DATA_LENGTH_UPDATED:
        {
            m_data_length_updated = true;
            NRF_LOG_INFO("Data length updated to %u bytes.\r\n", p_evt->params.data_length);
        } break;
    }

    nrf_ble_amts_on_gatt_evt(&m_amts, p_evt);
}


/**@brief Function for handling a Connection Parameters event.
 *
 * @param[in] p_evt  event.
 */
static void conn_params_evt_handler(ble_conn_params_evt_t * p_evt)
{
    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_SUCCEEDED)
    {
        m_conn_interval_configured = true;
    }
}


/**@brief Function for handling a Connection Parameters error.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for setting up advertising data.
 */
static void advertising_data_set(void)
{
    ble_advdata_t const adv_data =
    {
        .name_type          = BLE_ADVDATA_FULL_NAME,
        .flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
        .include_appearance = false,
    };

    ret_code_t err_code = ble_advdata_set(&adv_data, NULL);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for starting advertising.
 */
static void advertising_start(void)
{
    ble_gap_adv_params_t const adv_params =
    {
        .type        = BLE_GAP_ADV_TYPE_ADV_IND,
        .p_peer_addr = NULL,
        .fp          = BLE_GAP_ADV_FP_ANY,
        .interval    = ADV_INTERVAL,
        .timeout     = 0,
    };

    NRF_LOG_INFO("Starting advertising.\r\n");

    bsp_board_led_on(SCAN_ADV_LED);

    ret_code_t err_code = sd_ble_gap_adv_start(&adv_params, CONN_CFG_TAG);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function to start scanning.
 */
static void scan_start(void)
{
    NRF_LOG_INFO("Starting scan.\r\n");

    bsp_board_led_on(SCAN_ADV_LED);

    ret_code_t err_code = sd_ble_gap_scan_start(&m_scan_param);
    APP_ERROR_CHECK(err_code);
}


static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for the LEDs initialization.
 *
 * @details Initializes all LEDs used by the application.
 */
static void leds_init(void)
{
    bsp_board_leds_init();
}


/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module. This creates and starts application timers.
 */
static void timer_init(void)
{
    ret_code_t err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the button library.
 */
static void buttons_init(void)
{
   // The array must be static because a pointer to it will be saved in the button library.
    static app_button_cfg_t buttons[] =
    {
        {BOARD_TESTER_BUTTON, false, BUTTON_PULL, button_evt_handler},
        {BOARD_DUMMY_BUTTON,  false, BUTTON_PULL, button_evt_handler}
    };

    ret_code_t err_code = app_button_init(buttons, ARRAY_SIZE(buttons), BUTTON_DETECTION_DELAY);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for enabling button input.
 */
static void buttons_enable(void)
{
    ret_code_t err_code = app_button_enable();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for disabling button input.
 */
static void buttons_disable(void)
{
    ret_code_t err_code = app_button_disable();
    APP_ERROR_CHECK(err_code);
}


static void client_init(void)
{
    ret_code_t err_code = ble_db_discovery_init(db_disc_evt_handler);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_ble_amtc_init(&m_amtc, amtc_evt_handler);
    APP_ERROR_CHECK(err_code);
}


static void server_init(void)
{
    nrf_ble_amts_init(&m_amts, amts_evt_handler);
}


/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    ret_code_t err_code;

    // Initialize the SoftDevice handler library.
    nrf_clock_lf_cfg_t clock_lf_cfg = NRF_CLOCK_LFCLKSRC;
    SOFTDEVICE_HANDLER_INIT(&clock_lf_cfg, NULL);

     // Fetch the start address of the application RAM.
    uint32_t ram_start = 0;
    err_code = softdevice_app_ram_start_get(&ram_start);
    APP_ERROR_CHECK(err_code);

    // Overwrite some of the default configurations for the BLE stack.
    ble_cfg_t ble_cfg;

    // Configure the maximum number of connections.
    memset(&ble_cfg, 0x00, sizeof(ble_cfg));
    ble_cfg.gap_cfg.role_count_cfg.periph_role_count  = 1;
    ble_cfg.gap_cfg.role_count_cfg.central_role_count = 1;
    ble_cfg.gap_cfg.role_count_cfg.central_sec_count  = 1;
    err_code = sd_ble_cfg_set(BLE_GAP_CFG_ROLE_COUNT, &ble_cfg, ram_start);
    APP_ERROR_CHECK(err_code);

    // Configure the maximum ATT MTU.
    memset(&ble_cfg, 0x00, sizeof(ble_cfg));
    ble_cfg.conn_cfg.conn_cfg_tag                 = CONN_CFG_TAG;
    ble_cfg.conn_cfg.params.gatt_conn_cfg.att_mtu = NRF_BLE_GATT_MAX_MTU_SIZE;
    err_code = sd_ble_cfg_set(BLE_CONN_CFG_GATT, &ble_cfg, ram_start);
    APP_ERROR_CHECK(err_code);

    // Configure the maximum event length.
    memset(&ble_cfg, 0x00, sizeof(ble_cfg));
    ble_cfg.conn_cfg.conn_cfg_tag                     = CONN_CFG_TAG;
    ble_cfg.conn_cfg.params.gap_conn_cfg.event_length = 320;
    ble_cfg.conn_cfg.params.gap_conn_cfg.conn_count   = BLE_GAP_CONN_COUNT_DEFAULT;
    err_code = sd_ble_cfg_set(BLE_CONN_CFG_GAP, &ble_cfg, ram_start);
    APP_ERROR_CHECK(err_code);

    // Configure the number of custom UUIDs.
    memset(&ble_cfg, 0x00, sizeof(ble_cfg));
    ble_cfg.common_cfg.vs_uuid_cfg.vs_uuid_count = 1;
    err_code = sd_ble_cfg_set(BLE_COMMON_CFG_VS_UUID, &ble_cfg, ram_start);
    APP_ERROR_CHECK(err_code);

    // Enable BLE stack.
    err_code = softdevice_enable(&ram_start);
    APP_ERROR_CHECK(err_code);

    // Register a BLE event handler with the SoftDevice handler library.
    err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing GAP parameters.
 *
 * @details This function sets up all the necessary GAP (Generic Access Profile) parameters of the
 *          device including the device name and the preferred connection parameters.
 */
static void gap_params_init(void)
{
    ret_code_t              err_code;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (uint8_t const *)DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    err_code = sd_ble_gap_ppcp_set(&m_conn_param);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the GATT library. */
static void gatt_init(void)
{
    ret_code_t err_code = nrf_ble_gatt_init(&m_gatt, gatt_evt_handler);
    APP_ERROR_CHECK(err_code);
}


static void wait_for_event(void)
{
    (void) sd_app_evt_wait();
}


/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
    ble_conn_params_init_t const connection_params_init =
    {
        .p_conn_params                  = NULL,
        .first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY,
        .next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY,
        .max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT,
        .disconnect_on_fail             = true,
        .evt_handler                    = conn_params_evt_handler,
        .error_handler                  = conn_params_error_handler,
    };

    ret_code_t err_code = ble_conn_params_init(&connection_params_init);
    APP_ERROR_CHECK(err_code);
}


#ifdef S140
static void preferred_phy_set(uint8_t phy)
{
    ble_opt_t opts;
    memset(&opts, 0x00, sizeof(ble_opt_t));

    opts.gap_opt.preferred_phys.tx_phys = phy;
    opts.gap_opt.preferred_phys.rx_phys = phy;

    ret_code_t err_code = sd_ble_opt_set(BLE_GAP_OPT_PREFERRED_PHYS_SET, &opts);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEBUG("Setting preferred phy (rxtx) to %u: 0x%x\r\n", phy, err_code);
}
#endif


static void gatt_mtu_set(uint16_t att_mtu)
{
    ret_code_t err_code;

    err_code = nrf_ble_gatt_att_mtu_periph_set(&m_gatt, att_mtu);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_ble_gatt_att_mtu_central_set(&m_gatt, att_mtu);
    APP_ERROR_CHECK(err_code);
}


static void conn_evt_len_ext_set(bool status)
{
    ret_code_t err_code;
    ble_opt_t  opt;

    memset(&opt, 0x00, sizeof(opt));
    opt.common_opt.conn_evt_ext.enable = status ? 1 : 0;

    err_code = sd_ble_opt_set(BLE_COMMON_OPT_CONN_EVT_EXT, &opt);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEBUG("Setting connection event length extension to %u: 0x%x\r\n", status, err_code);
}


static void data_len_ext_set(bool status)
{
    uint8_t data_length = status ? (247 + LL_HEADER_LEN) : (23 + LL_HEADER_LEN);
    (void) nrf_ble_gatt_data_length_set(&m_gatt, BLE_CONN_HANDLE_INVALID, data_length);
}


#if defined(S140)
static void preferred_phy_select(void)
{
    NRF_LOG_INFO("Select preferred PHY:\r\n");
    NRF_LOG_INFO(" 1) 1 Mbps.\r\n");
    NRF_LOG_INFO(" 2) 2 Mbps.\r\n");
    NRF_LOG_INFO(" 3) Coded.\r\n");
    NRF_LOG_FLUSH();

    switch (NRF_LOG_GETCHAR())
    {
        default:
        case ONE:
            m_test_params.rxtx_phy = BLE_GAP_PHY_1MBPS;
            break;

        case TWO:
            m_test_params.rxtx_phy = BLE_GAP_PHY_2MBPS;
            break;

        case THREE:
            m_test_params.rxtx_phy = BLE_GAP_PHY_CODED;
            break;
    }

    preferred_phy_set(m_test_params.rxtx_phy);

    NRF_LOG_INFO("Preferred PHY set to %s.\r\n", phy_str(m_test_params.rxtx_phy));
    NRF_LOG_FLUSH();
}
#endif


static void att_mtu_select(void)
{
    NRF_LOG_INFO("Select an ATT MTU size:\r\n");
    NRF_LOG_INFO(" 1) 23 bytes.\r\n");
    NRF_LOG_INFO(" 2) 158 bytes.\r\n");
    NRF_LOG_INFO(" 3) 247 bytes.\r\n");
    NRF_LOG_FLUSH();

    switch (NRF_LOG_GETCHAR())
    {
        default:
        case ONE:
            m_test_params.att_mtu = 23;
            break;

        case TWO:
            m_test_params.att_mtu = 158;
            break;

        case THREE:
            m_test_params.att_mtu = 247;
            break;
    }

    gatt_mtu_set(m_test_params.att_mtu);

    NRF_LOG_INFO("ATT MTU set to %u bytes.\r\n", m_test_params.att_mtu);
    NRF_LOG_FLUSH();
}


static void conn_interval_select(void)
{
    NRF_LOG_INFO("Select a connection interval:\r\n");
    NRF_LOG_INFO(" 1) 7.5 ms\r\n");
    NRF_LOG_INFO(" 2) 50 ms\r\n");
    NRF_LOG_INFO(" 3) 400 ms\r\n");
    NRF_LOG_FLUSH();

    switch (NRF_LOG_GETCHAR())
    {
        default:
        case ONE:
            m_test_params.conn_interval = (uint16_t)(MSEC_TO_UNITS(7.5, UNIT_1_25_MS));
            NRF_LOG_INFO("Connection interval set to 7.5 ms.\r\n");
            break;

        case TWO:
            m_test_params.conn_interval = (uint16_t)(MSEC_TO_UNITS(50, UNIT_1_25_MS));
            NRF_LOG_INFO("Connection interval set to 50 ms.\r\n");
            break;

        case THREE:
            m_test_params.conn_interval = (uint16_t)(MSEC_TO_UNITS(400, UNIT_1_25_MS));
            NRF_LOG_INFO("Connection interval set to 400 ms.\r\n");
            break;
    }

    NRF_LOG_FLUSH();
}


static void data_len_ext_select(void)
{
    NRF_LOG_INFO("Turn on Data Length Extension? (y/n)\r\n");
    NRF_LOG_FLUSH();

    char answer = NRF_LOG_GETCHAR();
    m_test_params.data_len_ext_enabled = (answer == YES) ? 1 : 0;

    data_len_ext_set(m_test_params.data_len_ext_enabled);

    NRF_LOG_INFO("Data Length Extension is %s\r\n", (answer == YES) ?
                 (uint32_t) "ON" :
                 (uint32_t) "OFF");
    NRF_LOG_FLUSH();
}


static void test_param_adjust(void)
{
    bool done = false;

    while (!done)
    {
        NRF_LOG_RAW_INFO("\r\n");
        NRF_LOG_INFO("Adjust test parameters:\r\n");
        NRF_LOG_INFO(" 1) Select ATT MTU size.\r\n");
        NRF_LOG_INFO(" 2) Select connection interval.\r\n");
        NRF_LOG_INFO(" 3) Turn on/off Data length extension (DLE).\r\n");
#if defined(S140)
        NRF_LOG_INFO(" 4) Select preferred PHY.\r\n")
#endif
        NRF_LOG_RAW_INFO("\r\n");
        NRF_LOG_INFO("Press ENTER when finished.\r\n");
        NRF_LOG_RAW_INFO("\r\n");
        NRF_LOG_FLUSH();

        switch (NRF_LOG_GETCHAR())
        {
            case ONE:
                att_mtu_select();
                break;

            case TWO:
                conn_interval_select();
                break;

            case THREE:
                data_len_ext_select();
                break;

#if defined(S140)
            case FOUR:
                preferred_phy_select();
                break;
#endif

            default:
            case ENTER:
                done = true;
                break;
        }
    }
}


static void test_params_print(void)
{
    NRF_LOG_RAW_INFO("\r\n");
    NRF_LOG_INFO("Current test configuration:\r\n");
    NRF_LOG_INFO("===============================\r\n");
    NRF_LOG_INFO("ATT MTU size: %u\r\n", m_test_params.att_mtu);
    NRF_LOG_INFO("Conn. interval: %u units (1.25 ms per unit).\r\n", m_test_params.conn_interval);
    NRF_LOG_INFO("Data length extension (DLE): %s\r\n", m_test_params.data_len_ext_enabled ?
                 (uint32_t)"ON" :
                 (uint32_t)"OFF");
    NRF_LOG_DEBUG("Conn. event length ext.: %s\r\n", m_test_params.conn_evt_len_ext_enabled ?
                 (uint32_t)"ON" :
                 (uint32_t)"OFF");
#if defined(S140)
    NRF_LOG_INFO("Preferredy PHY: %s\r\n", phy_str(m_test_params.rxtx_phy));
#endif
    NRF_LOG_INFO("===============================\r\n");
    NRF_LOG_RAW_INFO("\r\n");
    NRF_LOG_FLUSH();
}


static void test_begin(void)
{
    NRF_LOG_INFO("Preparing the test.\r\n");
    NRF_LOG_FLUSH();

    switch (m_gap_role)
    {
        default:
            // If no connection was established, the role won't be either.
            // In this case, start both advertising and scanning.
            advertising_start();
            scan_start();
            break;

        case BLE_GAP_ROLE_PERIPH:
            advertising_start();
            break;

        case BLE_GAP_ROLE_CENTRAL:
            scan_start();
            break;
    }
}


static void test_run(void)
{
    NRF_LOG_RAW_INFO("\r\n");
    NRF_LOG_INFO("Test is ready. Press any key to run.\r\n");
    NRF_LOG_FLUSH();

    (void) NRF_LOG_GETCHAR();

    counter_start();
    nrf_ble_amts_notif_spam(&m_amts);
}


static void test_terminate(void)
{
    m_run_test                 = false;
    m_notif_enabled            = false;
    m_mtu_exchanged            = false;
    m_data_length_updated      = false;
#if defined(S140)
    m_phy_updated              = false;
#endif
    m_conn_interval_configured = false;

    if (m_conn_handle != BLE_CONN_HANDLE_INVALID)
    {
        NRF_LOG_INFO("Disconnecting...\r\n");

        ret_code_t err_code;
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);

        if (err_code != NRF_SUCCESS)
        {
            NRF_LOG_ERROR("sd_ble_gap_disconnect() failed: 0x%0x.\r\n", err_code);
        }
    }
    else
    {
        if (m_board_role == BOARD_TESTER)
        {
            m_print_menu = true;
        }
        if (m_board_role == BOARD_DUMMY)
        {
            if (m_gap_role == BLE_GAP_ROLE_PERIPH)
            {
                advertising_start();
            }
            else
            {
                scan_start();
            }
        }
    }
}


static void menu_print(void)
{
    bool begin_test = false;

    while (!begin_test)
    {
        test_params_print();
        NRF_LOG_INFO("Throughput example:\r\n");
        NRF_LOG_INFO(" 1) Run test.\r\n");
        NRF_LOG_INFO(" 2) Adjust test parameters.\r\n");
        NRF_LOG_FLUSH();

        switch (NRF_LOG_GETCHAR())
        {
            case ONE:
            default:
                begin_test = true;
                test_begin();
                break;

            case TWO:
                test_param_adjust();
                break;
        }
    }

    m_print_menu = false;
}


static bool is_test_ready()
{
    return (   (m_board_role == BOARD_TESTER)
            && m_conn_interval_configured
            && m_notif_enabled
            && m_mtu_exchanged
            && m_data_length_updated
#if defined(S140)
            && m_phy_updated
#endif
            && !m_run_test);
}


static void board_role_select(void)
{
    buttons_enable();

    while (m_board_role == NOT_SELECTED)
    {
        wait_for_event();
    }

    buttons_disable();
}


int main(void)
{
    log_init();
    leds_init();
    timer_init();
    counter_init();
    buttons_init();
    ble_stack_init();
    gap_params_init();
    conn_params_init();
    gatt_init();
    advertising_data_set();

    server_init();
    client_init();

    gatt_mtu_set(m_test_params.att_mtu);
    data_len_ext_set(m_test_params.data_len_ext_enabled);
    conn_evt_len_ext_set(m_test_params.conn_evt_len_ext_enabled);
#if defined(S140)
    preferred_phy_set(m_test_params.rxtx_phy);
#endif

    NRF_LOG_INFO("ATT MTU example started.\r\n");
    NRF_LOG_INFO("Press button 3 on the board connected to the PC.\r\n");
    NRF_LOG_INFO("Press button 4 on other board.\r\n");
    NRF_LOG_FLUSH();

    board_role_select();

    if (m_board_role == BOARD_TESTER)
    {
        m_print_menu = true;
    }
    if (m_board_role == BOARD_DUMMY)
    {
        advertising_start();
        scan_start();
    }

    // Enter main loop.
    NRF_LOG_DEBUG("Entering main loop.\r\n");

    for (;;)
    {
        if (m_print_menu)
        {
            menu_print();
        }

        if (is_test_ready())
        {
            m_run_test = true;
            test_run();
        }

        if (!NRF_LOG_PROCESS())
        {
            wait_for_event();
        }

    }
}


/**
 * @}
 */

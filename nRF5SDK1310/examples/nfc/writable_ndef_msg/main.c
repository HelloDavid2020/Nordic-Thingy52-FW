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
/** @file
 *
 * @defgroup nfc_writable_ndef_msg_example_main main.c
 * @{
 * @ingroup nfc_writable_ndef_msg_example
 * @brief The application main file of NFC writable NDEF message example.
 *
 */
#include <stdint.h>
#include <stdbool.h>
#include "app_error.h"
#include "boards.h"
#include "nfc_t4t_lib.h"
#include "nfc_uri_msg.h"
#include "nrf_log_ctrl.h"

static const uint8_t m_url[] =
    {'n', 'o', 'r', 'd', 'i', 'c', 's', 'e', 'm', 'i', '.', 'c', 'o', 'm'}; // URL "nordicsemi.com"

uint8_t       m_ndef_msg_buf[NDEF_FILE_SIZE];                               // Buffer for NDEF file
volatile bool m_update_state;                                               // Flag indicating that Type 4 Tag performs NDEF message update procedure.

/**
 * @brief Callback function for handling NFC events.
 */
static void nfc_callback(void          * context,
                         nfc_t4t_event_t event,
                         const uint8_t * data,
                         size_t          dataLength,
                         uint32_t        flags)
{
    (void)context;

    switch (event)
    {
        case NFC_T4T_EVENT_FIELD_ON:
            bsp_board_led_on(BSP_BOARD_LED_0);
            break;

        case NFC_T4T_EVENT_FIELD_OFF:
            bsp_board_leds_off();
            m_update_state = false;
            break;

        case NFC_T4T_EVENT_NDEF_READ:
            bsp_board_led_on(BSP_BOARD_LED_3);
            break;

        case NFC_T4T_EVENT_NDEF_UPDATED:
            if (dataLength == 0)
            {
                m_update_state = true;
            }
            else if (m_update_state == true)
            {
                m_update_state = false;
                bsp_board_led_on(BSP_BOARD_LED_1);
            }
            break;

        default:
            break;
    }
}


/**
 * @brief Function for application main entry.
 */
int main(void)
{
    uint32_t err_code;

    err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    /* Configure LED-pins as outputs */
    bsp_board_leds_init();

    /* Set up NFC */
    err_code = nfc_t4t_setup(nfc_callback, NULL);
    APP_ERROR_CHECK(err_code);

    /* Provide information about available buffer size to encoding function */
    uint32_t len = sizeof(m_ndef_msg_buf);

    /* Encode URI message into buffer */
    err_code = nfc_uri_msg_encode(NFC_URI_HTTP_WWW,
                                  m_url,
                                  sizeof(m_url),
                                  m_ndef_msg_buf,
                                  &len);
    APP_ERROR_CHECK(err_code);

    /* Run Read-Write mode for Type 4 Tag platform */
    err_code = nfc_t4t_ndef_rwpayload_set(m_ndef_msg_buf, sizeof(m_ndef_msg_buf));
    APP_ERROR_CHECK(err_code);

    /* Start sensing NFC field */
    err_code = nfc_t4t_emulation_start();
    APP_ERROR_CHECK(err_code);

    while (1)
    {
        NRF_LOG_FLUSH();
    }
}


/** @} */

/*
 * esp01_wifi.h
 *
 *  Created on: Mar 10, 2026
 *      Author: PC
 */

#ifndef INC_ESP01_WIFI_H_
#define INC_ESP01_WIFI_H_

#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define ESP01_UART huart2
#define DEBUG_UART huart1

extern UART_HandleTypeDef ESP01_UART;
extern UART_HandleTypeDef DEBUG_UART;

#define PRINTF_BUFFER_SIZE 256

extern uint8_t uart_rxBuffer[1024];
extern uint8_t uart_txBuffer[1024];

extern char wifi_ip[16];
extern char wifi_mac[18];

extern volatile uint8_t data_received;

typedef enum modes {
	STATION,
	AP,
	ST_AP
}mode;

// ========== responds ==========
#define RES_OK 1

// ========== debug ==========
void uart_printf(UART_HandleTypeDef *huart, const char *fmt, ...);


// ========== core functions ==========
void wifi_send(const char* data);
void wifi_receive(void);
void wifi_clear_rx(void);
void wifi_clear_tx(void);
uint8_t wifi_waitForRespond(const char* res);
uint8_t wifi_waitForRespond_finicky(const char* res);



// ========== command functions ==========
void wifi_reset(void);
void wifi_echoOff(void);
void wifi_mode(uint8_t mode);
void wifi_scannet(void);
void wifi_connect(const char *ssid, const char *password);
void wifi_getIP(void);
void wifi_connectTCP(const char* ip, uint16_t port);
void wifi_publishMQTT(const char* topic, const char* payload);

void wifi_uart_rx_callback(void);

#endif /* INC_ESP01_WIFI_H_ */

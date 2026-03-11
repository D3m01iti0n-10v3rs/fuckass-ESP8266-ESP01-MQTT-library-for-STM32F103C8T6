/*
 * esp01_wifi.c
 *
 *  Created on: Mar 10, 2026
 *      Author: PC
 */

#include "esp01_wifi.h"

uint8_t uart_rxBuffer[1024];
uint8_t uart_txBuffer[1024];

char wifi_ip[16] = {0};
char wifi_mac[18] = {0};

volatile uint8_t data_received = 0;

// ========= debug ==========
void uart_printf(UART_HandleTypeDef *huart, const char *fmt, ...)
{
    char buffer[PRINTF_BUFFER_SIZE];

    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buffer, PRINTF_BUFFER_SIZE, fmt, args);
    va_end(args);

    if (len > 0)
    {
        if (len > PRINTF_BUFFER_SIZE)
            len = PRINTF_BUFFER_SIZE;

        HAL_UART_Transmit(huart, (uint8_t*)buffer, len, HAL_MAX_DELAY);
    }
}

// ========== core functions ==========
void wifi_send(const char* data){
	memset(uart_txBuffer, 0, 1024);
    int len = strlen(data);
    strncpy((char*)uart_txBuffer, data, len);
    data_received = 0;
    HAL_UART_Transmit_DMA(&ESP01_UART, uart_txBuffer, len);
}

void wifi_receive(void){
	//memset(uart_rxBuffer, 0, sizeof(uart_rxBuffer));
	HAL_UARTEx_ReceiveToIdle_DMA(&ESP01_UART, uart_rxBuffer, 1024);
}

void wifi_clear_rx(void){
    memset(uart_rxBuffer, 0, 1024);
}

void wifi_clear_tx(void){
    memset(uart_txBuffer, 0, 1024);
}

uint8_t wifi_waitForRespond(const char* res){
    while(data_received != 1);
    data_received = 0;

    if (strcmp((char*)uart_rxBuffer, res) == 0) return 1;
    else return 0;
}

uint8_t wifi_waitForRespond_finicky(const char* res){
    while(data_received != 1);
    data_received = 0;

    for(uint8_t i = 0; i < strlen(res); i++){
        if (uart_rxBuffer[i] != res[i]) return 0;
    }
    return 1;
}



// ========== commands functions ==========
void wifi_reset(void){
	wifi_send("AT+RST\r\n");
	uart_printf(&DEBUG_UART, "Sent: AT+RST\r\n");
}

void wifi_echoOff(void){
	wifi_send("ATE0\r\n");
	uart_printf(&DEBUG_UART, "Sent: ATE0\r\n");
}
/*
 * 1: station
 * 2: ap
 * 3: station + ap
 */
void wifi_mode(uint8_t mode){
    char cmd[16];
    snprintf(cmd, sizeof(cmd), "AT+CWMODE=%d\r\n", mode);
    wifi_send(cmd);
    uart_printf(&DEBUG_UART, "Sent: %s", cmd);
}

void wifi_scannet(void){
	wifi_send("AT+CWLAP\r\n");
	uart_printf(&DEBUG_UART, "Sent: AT+CWLAP\r\n");
}

void wifi_connect(const char *ssid, const char *password){
	char cmd[128];
	snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, password);
	wifi_send(cmd);
	uart_printf(&DEBUG_UART, "Sent: %s", cmd);
	while (!wifi_waitForRespond("\r\nOK\r\n"));
}

void wifi_getIP(void){
	wifi_send("AT+CIFSR\r\n");
	uart_printf(&DEBUG_UART, "Sent: AT+CIFSR\r\n");
	while (!wifi_waitForRespond_finicky("+CIFSR"));
}



// ========== MQTT ==========
void wifi_connectTCP(const char* ip, uint16_t port){
    wifi_send("AT+CIPMUX=1\r\n");
    uart_printf(&DEBUG_UART, "Sent: AT+CIPMUX=1\r\n");
    while (!wifi_waitForRespond("\r\nOK\r\n"));

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=4,\"TCP\",\"%s\",%d\r\n", ip, port);
    wifi_send(cmd);
    uart_printf(&DEBUG_UART, "Sent: %s", cmd);
    while (!wifi_waitForRespond("4,CONNECT\r\n\r\nOK\r\n"));

    uint8_t mqtt_connect[] = {
        0x10, 0x12,                     // remaining length = 18
        0x00, 0x04, 'M','Q','T','T',
        0x04, 0x02,
        0x00, 0x3C,
        0x00, 0x06, 'c','l','i','e','n','t'
    };

    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=4,%d\r\n", sizeof(mqtt_connect));
    wifi_send(cmd);
    uart_printf(&DEBUG_UART, "Sent: %s", cmd);
    while (!wifi_waitForRespond("\r\nOK\r\n> "));

    HAL_UART_Transmit_DMA(&ESP01_UART, mqtt_connect, sizeof(mqtt_connect));
    uart_printf(&DEBUG_UART, "Sent: MQTT connect packet\r\n");
}

void wifi_publishMQTT(const char* topic, const char* payload){
    uint16_t topic_len   = (uint16_t)strlen(topic);
    uint16_t payload_len = (uint16_t)strlen(payload);
    int remaining = 2 + topic_len + payload_len;
    int total_len = 2 + remaining;

    uint8_t packet[4 + topic_len + payload_len];
    int p = 0;

    packet[p++] = 0x30;
    packet[p++] = (uint8_t)remaining;
    packet[p++] = (uint8_t)((topic_len >> 8) & 0xFF);
    packet[p++] = (uint8_t)(topic_len & 0xFF);
    memcpy(&packet[p], topic, topic_len); p += topic_len;
    memcpy(&packet[p], payload, payload_len); p += payload_len;

    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=4,%d\r\n", total_len);
    wifi_send(cmd);
    uart_printf(&DEBUG_UART, "Sent: %s", cmd);

    while (!wifi_waitForRespond("\r\nOK\r\n> "));

    HAL_UART_Transmit_DMA(&ESP01_UART, packet, total_len);
    uart_printf(&DEBUG_UART, "Sent: MQTT publish packet\r\n");
}



// ========== DMA callbacks ==========
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART2)
    {
        uart_rxBuffer[Size] = '\0';
        data_received = 1;
        uart_printf(&DEBUG_UART, "Received: %s", uart_rxBuffer);
        wifi_receive();
    }
}

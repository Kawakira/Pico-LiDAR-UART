#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "hardware/irq.h"

#define UART_ID uart0
#define UART_BAUD   (921600)
#define UART0_TX    (0)
#define UART0_RX    (1)
#define HEADER 0xA5A5A5A5
//#define PAYLOAD_LENG 5

int num = 0;
int Rx_Busy = 0;
int sum = 0;
int AnswerLeng = 5;
uint8_t temp;
uint8_t data[512];
uint8_t payload[327];
int payload_index = 0;

uint32_t header_buf = 0;
bool header_detected = false;
bool IsScanning = false;

typedef enum
{
    GetAddr,
    GetParam,
    GetVersion,
    StartScan,
    StopScan,
    Restart,
    SetBaud,
    SetEdge
}CommandNum;

typedef struct
{
    int command;
    int answer_length;
}Command;

void on_uart_rx() {
    while (uart_is_readable(UART_ID)) {
        temp = uart_getc(UART_ID);
        header_buf = (header_buf << 8) | temp;
        if(header_detected){
            payload[payload_index++] = temp;
            if(payload_index >= AnswerLeng){
                header_detected = false;
                header_buf = 0;
                if(IsScanning){
                    AnswerLeng = 327;
                    IsScanning = false;
                }
                Rx_Busy--;
            }
        }
        if(!header_detected && header_buf == HEADER){
            header_detected = true;
            payload_index = 0;
            if(AnswerLeng == 0){
                IsScanning = true;
                AnswerLeng = 5;
                Rx_Busy = 2;
            }else{
                Rx_Busy = 1;
            }
        }
    }
}

void LiDAR(Command command){
    int RxData[9] = {0xA5,0xA5,0xA5,0xA5,0x00,0x00,0x00,0x00,0x00};
    RxData[5] = command.command;
    RxData[8] = command.command;
    AnswerLeng = command.answer_length;
    uart_write_blocking(UART_ID, (const uint8_t *)RxData, 9);
}

Command CommandList[] = {
    {0x60,5},
    {0x61,14},
    {0x62,24},
    {0x63,0},
    {0x64,5},
    {0x67,5},
    {0x68,6},
    {0x69,6}
};

int main(void) {
    //To use USB
    stdio_init_all();
    uart_init(UART_ID, UART_BAUD);
    gpio_set_function(UART0_TX, GPIO_FUNC_UART);
    gpio_set_function(UART0_RX, GPIO_FUNC_UART);
    uart_set_hw_flow(UART_ID, false, false);
    uart_set_format(UART_ID,8,1,UART_PARITY_NONE);
    uart_set_fifo_enabled(UART_ID, false);
    int UART_IRQ = UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;

    irq_set_exclusive_handler(UART_IRQ, on_uart_rx);
    irq_set_enabled(UART_IRQ, true);
    uart_set_irq_enables(UART_ID, true, false);

    //To use GPIO 25pin
    gpio_init(25);
    gpio_set_dir(25, GPIO_OUT);
    gpio_put(25,true);
    sleep_ms(1000);
    //Loop
    while (true)
    {
        if(Rx_Busy == 0){
            for(int i=3;i<163;i++){
                data[i-3] = ((payload[i*2+1] << 8) | payload[i*2]) & 0x1ff;
                printf("%d ",data[i-3]);
            }
            for(int i=0;i<327;i++){
                sum += payload[i];
            }
            if((sum - payload[326]) % 256 == payload[326]){
                printf("←OK?");
            }else{
                printf("←Fucked Up");
            }
            printf("%d\n\n",sum);
            Rx_Busy = 0;
            sum = 0;
        }
    }
}

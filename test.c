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
#define BUF_SIZE 1024

volatile int Rx_write_index = 0;
volatile int Rx_read_index = 0;
int Rx_Busy = 1;
int checksum = 0;
int AnswerLeng = 5;
uint8_t temp;
uint8_t Data[512];
volatile uint8_t RxBuf[BUF_SIZE];
uint8_t payload[BUF_SIZE];
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

void enqueue(uint8_t byte){
    RxBuf[Rx_write_index] = byte;
    Rx_write_index = (Rx_write_index + 1) % BUF_SIZE;
}

int dequeue(uint8_t *byte){
    if (Rx_read_index == Rx_write_index) {
        return 0;
    }
    *byte = RxBuf[Rx_read_index];
    Rx_read_index = (Rx_read_index + 1) % BUF_SIZE;
    return 1;
}

void on_uart_rx() {
    while (uart_is_readable(UART_ID)) {
        enqueue(uart_getc(UART_ID));
    }
}

void LiDAR(Command command){
    uint8_t TxData[9] = {0xA5,0xA5,0xA5,0xA5,0x00,0x00,0x00,0x00,0x00};
    TxData[5] = command.command;
    TxData[8] = command.command;
    AnswerLeng = command.answer_length;
    uart_write_blocking(UART_ID, (const uint8_t *)TxData, 9);
}

void Process_rx_data() {
    uint8_t byte;
    while (dequeue(&byte)) {
        if(Rx_Busy == 0){
            header_buf = (header_buf << 8) | byte;
            if (header_detected) {
                payload[payload_index++] = byte;
                if (payload_index >= AnswerLeng) {
                    header_detected = false;
                    header_buf = 0;
                    if (IsScanning) {
                        AnswerLeng = 327;
                        IsScanning = false;
                    }
                    Rx_Busy = 1;
                }
            }
            if (!header_detected && header_buf == HEADER) {
                header_detected = true;
                payload_index = 0;
                if (AnswerLeng == 0) {
                    IsScanning = true;
                    AnswerLeng = 5;
                }
            }
        }
    }
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
    sleep_ms(2000);
    LiDAR(CommandList[StartScan]);
    //Loop
    while (true)
    {
        Process_rx_data();
        if(Rx_Busy == 1) {
            checksum = 0;
            for (int i=0;i<80;i++) {
                Data[i] = ((payload[i*2+167] << 8) | payload[i*2+166]) & 0x1ff;
            }
            for (int i=0;i<80;i++) {
                Data[i+80] = ((payload[i*2+7] << 8) | payload[i*2+6]) & 0x1ff;
            }
            for(int i=0;i<160;i++){
                printf("%d ",Data[i]);
            }
            printf("\n");
            for(int i=0;i<326;i++){
                checksum += payload[i];
            }
            if(checksum % 256 == payload[326]){
                printf("Ok\n");
            }else{
                printf("Fucked Up\n");
            }
            Rx_Busy = 0;
        }
    }
}
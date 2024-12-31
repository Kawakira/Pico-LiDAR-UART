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
int PAYLOAD_LENG = 5;
uint8_t temp;
uint8_t data[512];
uint8_t command[9] = {0xA5,0xA5,0xA5,0xA5,0x00,0x63,0x00,0x00,0x63};
uint8_t payload[327];
int payload_index = 0;

uint32_t header_buf = 0;
bool header_detected = false;

void on_uart_rx() {
    while (uart_is_readable(UART_ID)) {
        temp = uart_getc(UART_ID);
        //data[num++] = temp;
        if(Rx_Busy < 2){
        header_buf = (header_buf << 8) | temp;
            if(header_detected){
                payload[payload_index++] = temp;
                if(payload_index >= PAYLOAD_LENG){
                    header_detected = false;
                    header_buf = 0;
                    PAYLOAD_LENG = 327;
                    Rx_Busy++;
                }
            }
            if(!header_detected && header_buf == HEADER){
                header_detected = true;
                payload_index = 0;
            }
        }
    }
}

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
    sleep_ms(5000);
    uart_write_blocking(UART_ID, (const uint8_t *)command, 9);
    sleep_ms(2000);
    //Loop
    while (true)
    {
        if(Rx_Busy == 2){
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
            Rx_Busy = 1;
            sum = 0;
        }
        //Blink green LED
        /*if(num>0){
            printf("Get:");
            int i=0;
            sum = 0;
            while(num > 0){
                printf("%d ",data[i]);
                sum += data[i];
                num--;
                i++;
            }
            sum -= 165 * 4;
            sum -= data[i-1];
            if(sum % 256 == data[i-1]){
                printf("←OK?");
            }else{
                printf("←Fucked Up");
            }
            printf("\n");
            num = 0;
            sleep_ms(100);
        }*/
    }
}

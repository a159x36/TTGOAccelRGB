/* TTGO Demo example for WS2812B RGB LEDs and I2C MPU6050 Accelerometer for 159236

*/
#include <driver/adc.h>
#include <driver/gpio.h>
#include <esp_adc_cal.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <esp_log.h>
#include <esp_sntp.h>
#include <nvs_flash.h>

#include "fonts.h"
#include "graphics.h"
#include "image_wave.h"
#include "accelerometer.h"

#include "esp32_digital_led_lib.h"


const char *tag = "T Display";
static time_t time_now;
static struct tm *tm_info;

// for button inputs
QueueHandle_t inputQueue;
uint64_t lastkeytime=0;

// interrupt handler for button presses on GPIO0 and GPIO35
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    static int pval[2]={1,1};
    uint32_t gpio_num = (uint32_t) arg;
    int gpio_index=(gpio_num==35);
    int val=(1-pval[gpio_index]);
    
    uint64_t time=esp_timer_get_time();
    uint64_t timesince=time-lastkeytime;
 //   ets_printf("gpio_isr_handler %d %d %lld\n",gpio_num,val, timesince);
    // the buttons can be very bouncy so debounce by checking that it's been .5ms since the last
    // change and that it's pressed down
    if(timesince>500 && val==0) {
        xQueueSendFromISR(inputQueue,&gpio_num,0); 
        
    }
    pval[gpio_index]=val;
    lastkeytime=time;   
    gpio_set_intr_type(gpio_num,val==0?GPIO_INTR_HIGH_LEVEL:GPIO_INTR_LOW_LEVEL);
    
}

// get a button press, returns -1 if no button has been pressed
// otherwise the gpio of the button. 
int get_input() {
    int key;
    if(xQueueReceive(inputQueue,&key,0)==pdFALSE)
        return -1;
    return key;
}

extern image_header  bubble;
typedef struct pos {
    int x;
    int y;
    int speed;
    int colour;
} pos;

// simple spirit level, drawing a bubble using accelerometer data
void accelerometer_demo() {
    int x;
    int y;
    pos stars[100];
    for(int i=0;i<100;i++) {
        stars[i].x=rand()%display_width;
        stars[i].y=rand()%(display_height*256);
        stars[i].speed=rand()%512+64;
        stars[i].colour=rand();
    }
    setFont(FONT_DEJAVU18);
    while(1) {
        cls(0);
        for(int i=0;i<100;i++) {
            draw_pixel(stars[i].x,stars[i].y>>8,stars[i].colour);
            stars[i].y += stars[i].speed;
            if(stars[i].y>=display_height*256) {
                stars[i].x=rand()%display_width;
                stars[i].y=0;
                stars[i].speed=rand()%512+64;
            }
        }
        int16_t buf[7];
        read_mpu6050(buf);
        x=(display_width*buf[0])/(16384*2)+display_width/2;
        y=-(display_height*buf[1])/(16384*2)+display_height/2;
        draw_image(&bubble, x,y);

        char str[16];
        snprintf(str,16,"%.2f°C",buf[3]/340+36.53);
        print_xy(str,1,1);

        flip_frame();

        int key=get_input();
        if(key==0) return;
    }
}


#define SZ (20<<16)
// menu with a rotating cube, because... why not.
int demo_menu(int select) {
    // for fps calculation
    int64_t current_time;
    int64_t last_time = esp_timer_get_time();
    while(1) {
    // cube vertices
        struct { int x; int y; int z;} vertex[8]={
            {-SZ,-SZ,-SZ},{-SZ,-SZ,SZ},{-SZ,SZ,-SZ},{-SZ,SZ,SZ},
            {SZ,-SZ,-SZ},{SZ,-SZ,SZ},{SZ,SZ,-SZ},{SZ,SZ,SZ}
        };
        for(int frame=0;frame < 4000; frame++) {
            cls(rgbToColour(100,20,20));
            setFont(FONT_DEJAVU18);
            setFontColour(255, 255, 255);
            draw_rectangle(0,5,display_width,24,rgbToColour(220,220,0));
            draw_rectangle(0,select*24+24+5,display_width,24,rgbToColour(0,180,180));
            int offsetx=display_width/2;
            int offsety=display_height/2;
            if(get_orientation()) offsety+=display_height/4;
            else offsetx+=display_width/4;
            for(int i=0;i<8;i++) {
                for(int j=i+1;j<8;j++) {
                    int v=i ^ j; // bit difference
                    if(v && !(v&(v-1))) { // single bit different!
                        int x=vertex[i].x>>16;
                        int y=vertex[i].y>>16;
                        int z=vertex[i].z>>16;
                        x=x+((z*x)>>9)+offsetx;
                        y=y+((z*y)>>9)+offsety;
                        int x1=vertex[j].x>>16;
                        int y1=vertex[j].y>>16;
                        int z1=vertex[j].z>>16;
                        x1=x1+((z1*x1)>>9)+offsetx;
                        y1=y1+((z1*y1)>>9)+offsety;
                        draw_line(x,y,x1,y1,rgbToColour(255,255,255));
                    }
                }
            }
            print_xy("LED Menu", 10, 10);
            print_xy("16 LED Ring",10,LASTY+24);
            print_xy("16x16 Panel",10,LASTY+24);
            print_xy("60 LED Strip",10,LASTY+24);
            print_xy("Accelerometer",10,LASTY+24);
            
            send_frame();
            // rotate cube
            for(int i=0;i<8;i++) {
                int x=vertex[i].x;
                int y=vertex[i].y;
                int z=vertex[i].z;
                x=x+(y>>6);
                y=y-(vertex[i].x>>6);
                y=y+(vertex[i].z>>7);
                z=z-(vertex[i].y>>7);
                x=x+(vertex[i].z>>8);
                z=z-(vertex[i].x>>8);
                vertex[i].x=x;
                vertex[i].y=y;
                vertex[i].z=z;
            }
            wait_frame();
            current_time = esp_timer_get_time();
            if ((frame % 10) == 0) {
                printf("FPS:%f %d\n", 1.0e6 / (current_time - last_time),frame);
                vTaskDelay(1);
            }
            last_time = current_time;
            int key=get_input();
            if(key==0) select=(select+1)%4;
            if(key==35) return select;
        }
    }
}

void app_main() {
    // queue for button presses
    gpio_set_direction(4,GPIO_MODE_OUTPUT);
    
    inputQueue = xQueueCreate(4,4);
    ESP_ERROR_CHECK(nvs_flash_init());
    // ===== Set time zone ======
    setenv("TZ", "	NZST-12", 0);
    tzset();
    // ==========================
    time(&time_now);
    tm_info = localtime(&time_now);
    // interrupts for button presses
    gpio_set_direction(0, GPIO_MODE_INPUT);
    gpio_set_direction(35, GPIO_MODE_INPUT);
    gpio_set_intr_type(0, GPIO_INTR_LOW_LEVEL);
    gpio_set_intr_type(35, GPIO_INTR_LOW_LEVEL);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(0, gpio_isr_handler, (void*) 0);
    gpio_isr_handler_add(35, gpio_isr_handler, (void*) 35);
  //  mpu6050_init();

    graphics_init();
    cls(0);
    
    // Initialize the effect displayed
    int sel=0;
    while(1) {
        sel=demo_menu(sel);
        switch(sel) {
            case 0:
            {
            strand_t STRAND = {.rmtChannel = 0,
                .gpioNum = 15,
                .ledType = LED_WS2812B_V3,
                .brightLimit = 255,
                .numPixels = 16, };
                gpio_set_direction(15, GPIO_MODE_OUTPUT);
                digitalLeds_initStrands(&STRAND, 1);
                digitalLeds_resetPixels(&STRAND);
                int v=0;
                int delay=100;
                while (1) {
                    for (uint16_t i = 0; i < STRAND.numPixels; i++) {
                        if(i==v) STRAND.pixels[i] = pixelFromRGB(0,100,200);
                        else STRAND.pixels[i] = pixelFromRGB(0,0,0);
                    }
                    v=(v+1)%STRAND.numPixels;
                    digitalLeds_updatePixels(&STRAND);
                    ets_delay_us(delay*100);
                    if(!gpio_get_level(0)) delay--;
                    if(!gpio_get_level(35)) delay++;
                    if(delay<0) delay=0;
                }
            }
            break;
            case 1: 
            {
                float xo=7.5,yo=7.5;
                strand_t STRANDS[] = { 
                    {.rmtChannel = 0, .gpioNum = 15, .ledType = LED_WS2812B_V3, .brightLimit = 16, .numPixels = 256},
                };
                strand_t *pStrand= &STRANDS[0];
                gpio_set_direction(15, GPIO_MODE_OUTPUT);
                digitalLeds_initStrands(pStrand, 1);
                digitalLeds_resetPixels(pStrand);
                float offset=0;
                int delay=100;
                while(1) {
                    for (uint16_t i = 0; i < pStrand->numPixels; i++) {
                        int y1 = i / 16;
                        int x1 = i % 16;
                        if (y1 % 2) x1 = 15 - x1;
                        y1 = 15 - y1;
                        float d =
                            (sqrt((y1 - yo) * (y1 - yo) + (x1 - xo) * (x1 - xo)) +
                            offset) /
                            4.0;
                        int red=(int)round(4.0*sin(1.0299*d)+4.0);
                        int green=(int)round(4.0*cos(3.2235*d)+4.0);
                        int blue=(int)round(4.0*sin(5.1234*d)+4.0);
                        pStrand->pixels[i] = pixelFromRGB(red/2, green/2, blue/2);
                    }
                    digitalLeds_updatePixels(pStrand);
                    offset-=0.1f;
                    ets_delay_us(delay*100);
                    if(!gpio_get_level(0)) delay--;
                    if(!gpio_get_level(35)) delay++;
                    if(delay<0) delay=0;
                }
            }
            break;
            case 2: 
            {
                strand_t STRAND = {.rmtChannel = 0,
                                   .gpioNum = 15,
                                   .ledType = LED_WS2812B_V3,
                                   .brightLimit = 16,
                                   .numPixels = 60,
                                   .pixels = 0,
                                   ._stateVars = 0};
                gpio_set_direction(15, GPIO_MODE_OUTPUT);
                digitalLeds_initStrands(&STRAND, 1);
                digitalLeds_resetPixels(&STRAND);
                int v=0;
                int delay=100;
                while (1) {
                    for (uint16_t i = 0; i < STRAND.numPixels; i++) {
                        if((i%30)==v) STRAND.pixels[i] = pixelFromRGB(128,(255*v)/30,(255*i)/30);
                        else STRAND.pixels[i] = pixelFromRGB(0,0,0);;
                    }
                    v=(v+1)%30;
                    digitalLeds_updatePixels(&STRAND);
                    ets_delay_us(delay*100);
                    if(!gpio_get_level(0)) delay--;
                    if(!gpio_get_level(35)) delay++;
                    if(delay<0) delay=0;
                }
            }
            break;
            case 3:
               accelerometer_demo();
            break;
        }
    }
}

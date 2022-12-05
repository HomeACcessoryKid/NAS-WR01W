#include <espressif/esp_common.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <BL0937.h>

static uint8_t _cf0_pin;
static uint8_t _cf1_pin;
static uint8_t _sel_pin;
static uint8_t _model;

static BL0937_data_t *_cf0=NULL;
static BL0937_data_t *_cf1=NULL;
// SemaphoreHandle_t   *semaphore; //must set
// uint32_t            mintime;    //must set in microseconds
// uint32_t            count;      //autoinit
// uint32_t            now;        //autoinit
// uint32_t            time[BL0937_N]; //autoinit

static void  IRAM _cf0_interrupt_handler(uint8_t gpio_num) {
//     BaseType_t yield = pdFALSE;
    BL0937_data_t *cf=_cf0;
    if (cf) {
        cf->now=sdk_system_get_time();
        if (cf->count  < BL0937_N) cf->time[cf->count]=cf->now;
        if (cf->count++> BL0937_N-2 && (cf->now-cf->time[0])>cf->mintime) xSemaphoreGiveFromISR(cf->semaphore,NULL);
//         portYIELD_FROM_ISR(yield);
    }
}

static void  IRAM _cf1_interrupt_handler(uint8_t gpio_num) {
//     BaseType_t yield = pdFALSE;
    BL0937_data_t *cf=_cf1;
    if (cf) {
        cf->now=sdk_system_get_time();
        if (cf->count  < BL0937_N) cf->time[cf->count]=cf->now;
        if (cf->count++> BL0937_N-2 && (cf->now-cf->time[0])>cf->mintime) xSemaphoreGiveFromISR(cf->semaphore,NULL);
//         portYIELD_FROM_ISR(yield);
    }
}

void BL0937_init(uint8_t cf_pin, uint8_t cf1_pin, uint8_t sel_pin, BL0937_model_t model) {
    _cf0_pin = cf_pin;
    _cf1_pin = cf1_pin;
    _sel_pin = sel_pin;
    _model = model;

    gpio_enable(_cf0_pin, GPIO_INPUT);
    gpio_set_pullup(_cf0_pin, true, false);
    gpio_set_interrupt( _cf0_pin, GPIO_INTTYPE_EDGE_POS, _cf0_interrupt_handler);

    gpio_enable(_sel_pin, GPIO_OUTPUT);
    gpio_write(_sel_pin, 0); //TODO: ??

    gpio_enable(_cf1_pin, GPIO_INPUT);
    gpio_set_pullup(_cf1_pin, true, false);
    gpio_set_interrupt( _cf1_pin, GPIO_INTTYPE_EDGE_POS, _cf1_interrupt_handler);
}

void BL0937_collect(BL0937_source_t source, BL0937_data_t *data) {
    if (data->semaphore) {
        if (source==SOURCE_CF) {
            _cf0=NULL; //to prevent interrupt from touching this fresh dataset
            data->count=0;
            data->now=0;
            for (int i=0; i<BL0937_N; i++) data->time[i]=0;
            _cf0=data; //ready to go
        } else { //SOURCE_CF1x
            _cf1=NULL; //to prevent interrupt from touching this fresh dataset
            data->count=0;
            data->now=0;
            for (int i=0; i<BL0937_N; i++) data->time[i]=0;
            gpio_write(_sel_pin, (source==SOURCE_CF1A)?0:1); //TODO: make model dependant
            sdk_os_delay_us(12);//wait >10 microseconds
            _cf1=data; //ready to go
        }
    } else printf("ERROR: Must set semaphore!\n");
}

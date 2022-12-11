#ifndef BL0937_h
#define BL0937_h

#include <esp8266.h>
#include <FreeRTOS.h>
// #include <task.h>
#include <semphr.h>

#ifndef BL0937_N
#define BL0937_N 8
#endif

typedef enum {
    MODEL_HLW8012 = 0,
    MODEL_BL0937
} BL0937_model_t;

typedef enum {
    SOURCE_CF1A = 0,
    SOURCE_CF1V,
    SOURCE_CF
} BL0937_source_t;

typedef struct {
    SemaphoreHandle_t   semaphore;  //must set
    uint32_t            mintime;    //must set in microseconds
    uint32_t            total;      //does not clear
    uint32_t            to_count;   //autoinit
    uint32_t            count;      //autoinit
    uint32_t            now;        //autoinit
    uint32_t            time[BL0937_N]; //autoinit
} BL0937_data_t;

void BL0937_init(uint8_t cf_pin, uint8_t cf1_pin, uint8_t sel_pin, BL0937_model_t model);

void BL0937_collect(BL0937_source_t source, BL0937_data_t *data);

bool BL0937_process(BL0937_data_t *data, BaseType_t taken);


#endif

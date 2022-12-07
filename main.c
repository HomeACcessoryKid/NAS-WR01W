/*
 * Copyright 2022 HomeAccessoryKid @gmail.com
 * modified from 2018 David B Brown (@maccoylton)
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Homekit firmware SWA9 SmartSocket
 */

#define SAVE_DELAY 500
#define POWER_MONITOR_POLL_PERIOD 10000


#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_common.h>
#include <esplibs/libmain.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <sysparam.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>

#include <adv_button.h>
#include <udplogger.h>
#include <HLW8012_ESP82.h>
#include <etstimer.h>
#include <BL0937.h>

/* ============== BEGIN HOMEKIT CHARACTERISTIC DECLARATIONS =============================================================== */
// add this section to make your device OTA capable
// create the extra characteristic &ota_trigger, at the end of the primary service (before the NULL)
// it can be used in Eve, which will show it, where Home does not
// and apply the four other parameters in the accessories_information section

#include <ota-api.h>
#include <main.h> //contains more custom characteristics

void relay_callback(homekit_characteristic_t *_ch, homekit_value_t    on, void *context);
void watts_callback(homekit_characteristic_t *_ch, homekit_value_t value, void *context);
void volts_callback(homekit_characteristic_t *_ch, homekit_value_t value, void *context);
void  amps_callback(homekit_characteristic_t *_ch, homekit_value_t value, void *context);
void calibrate_pow_set   (homekit_value_t value);
void calibrate_volts_set (homekit_value_t value);
void calibrate_power_set (homekit_value_t value);


homekit_characteristic_t calibrate_pow   = HOMEKIT_CHARACTERISTIC_(CUSTOM_CALIBRATE_POW, false, .setter=calibrate_pow_set);
homekit_characteristic_t calibrate_volts = HOMEKIT_CHARACTERISTIC_(CUSTOM_CALIBRATE_VOLTS, 240, .setter=calibrate_volts_set);
homekit_characteristic_t calibrate_power = HOMEKIT_CHARACTERISTIC_(CUSTOM_CALIBRATE_WATTS,  60, .setter=calibrate_power_set);

homekit_characteristic_t ota_trigger  = API_OTA_TRIGGER;
homekit_characteristic_t manufacturer = HOMEKIT_CHARACTERISTIC_(MANUFACTURER,  "X");
homekit_characteristic_t serial       = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, "1");
homekit_characteristic_t model        = HOMEKIT_CHARACTERISTIC_(MODEL,         "Z");
homekit_characteristic_t revision     = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION,  "0.0.0");
homekit_characteristic_t relay        = HOMEKIT_CHARACTERISTIC_(ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(relay_callback));
homekit_characteristic_t watts        = HOMEKIT_CHARACTERISTIC_(CUSTOM_WATTS, 0, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(watts_callback));
homekit_characteristic_t volts        = HOMEKIT_CHARACTERISTIC_(CUSTOM_VOLTS, 0, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(volts_callback));
homekit_characteristic_t amps         = HOMEKIT_CHARACTERISTIC_(CUSTOM_AMPS,  0, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK( amps_callback));

const int BUTTON_GPIO =  0;
const int CF_GPIO     =  4;
const int CF1_GPIO    =  5;
const int SELi_GPIO   = 12;
const int LED_GPIO    = 13;
const int relay_gpio  = 14;

ETSTimer save_timer;
void relay_write(bool on, int gpio) {
    gpio_write(gpio, on ? 1 : 0);
}
void led_write(bool on, int gpio) {
    gpio_write(gpio, on ? 0 : 1);
}
void save_float_param( const char *description, float new_float_value) {    
    sysparam_set_int32(description, (int32_t)(new_float_value*100));
}
void load_float_param( const char *description, float *new_float_value) {
    static sysparam_status_t status = SYSPARAM_OK;
    static int32_t int32_value;
    status = sysparam_get_int32(description, &int32_value);
    if (status == SYSPARAM_OK ) *new_float_value = int32_value * 1.0f / 100;
}






float calibrated_volts_multiplier=0;
float calibrated_current_multiplier=0;
float calibrated_power_multiplier=0;

void relay_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context) {
    printf("Relay callback\n");
    relay_write(relay.value.bool_value, relay_gpio);
    led_write(relay.value.bool_value, LED_GPIO);
}
void volts_callback(homekit_characteristic_t *_ch, homekit_value_t value, void *context) {
    printf("Volts on callback\n");
/*    volts.value.int_value = HLW8012_getVoltage();*/
}
void amps_callback(homekit_characteristic_t *_ch, homekit_value_t value, void *context) {
    printf("Amps on callback\n");
/*    amps.value.float_value = HLW8012_getCurrent();*/
}
void watts_callback(homekit_characteristic_t *_ch, homekit_value_t value, void *context) {
    printf("Watts on callback\n");
/*    watts.value.int_value = HLW8012_getActivePower();*/
/*    watts.value.int_value = (int) (volts.value.int_value * amps.value.float_value);*/
}

void save_characteristics() {
    //called by a timer function to save charactersitics
    save_float_param ( "wattsx", calibrated_power_multiplier);
    save_float_param ( "voltsx", calibrated_volts_multiplier);
    save_float_param ( "currentx", calibrated_current_multiplier);
}

void calibrate_task() {
    HLW8012_set_calibrated_mutipliers (&calibrated_current_multiplier, &calibrated_volts_multiplier, &calibrated_power_multiplier, calibrate_volts.value.int_value, calibrate_power.value.int_value) ;
    
    sdk_os_timer_arm (&save_timer, SAVE_DELAY, 0 );
    
    calibrate_pow.value.bool_value = false;
    homekit_characteristic_notify(&calibrate_pow, calibrate_pow.value);

    vTaskDelete (NULL);
}
void calibrate_pow_set(homekit_value_t value) {
    xTaskCreate(calibrate_task, "Calibrate task", 512, NULL, tskIDLE_PRIORITY+1, NULL);
}

void calibrate_volts_set(homekit_value_t value) {
    calibrate_volts.value.int_value = value.int_value;
}

void calibrate_power_set(homekit_value_t value) {
    calibrate_power.value.int_value = value.int_value;
}


void homekit_characteristic_bounds_check(homekit_characteristic_t *ch) {
    printf ("%s: %s: ",__func__, ch->description);
    switch (ch->format) {
        case homekit_format_bool:
            printf ("Checking boolean bounds");
            if ((ch->value.bool_value != 0) && (ch->value.bool_value != 1)){
                printf (" Out of bounds setting to false");
                ch->value.bool_value = false;
            }
            break;
        case homekit_format_uint8:
            printf ("Checking uint8 bounds");
            if (ch->value.int_value > *ch->max_value){
                printf (" Greater than max, setting to max");
                ch->value.int_value = *ch->max_value;
            }
            if (ch->value.int_value < *ch->min_value){
                printf (" Lower than min, setting to min");
                ch->value.int_value = *ch->min_value;
            }
            break;
        case homekit_format_int:
        case homekit_format_uint16:
        case homekit_format_uint32:
            printf ("Checking integer bounds");
            if (ch->value.int_value > *ch->max_value){
                printf (" Greater than max, setting to max");
                ch->value.int_value = *ch->max_value;
            }
            if (ch->value.int_value < *ch->min_value){
                printf (" Lower than min, setting to min");
                ch->value.int_value = *ch->min_value;
            }
            break;
        case homekit_format_string:
            break;
        case homekit_format_float:
            printf ("Checking float bounds");
            if (ch->value.float_value > *ch->max_value){
                printf (" Greater than max, setting to max");
                ch->value.float_value = *ch->max_value;
            }
            if (ch->value.float_value < *ch->min_value){
                printf (" Lower than min, setting to min");
                ch->value.float_value = *ch->min_value;
            }
            break;
        case homekit_format_uint64:
        case homekit_format_tlv:
        default:
            printf ("Unknown characteristic format\n");
    }
    printf("\n");
}


void power_monitoring_task(void *_args) {
    printf ("%s:\n", __func__);
    vTaskDelete(NULL);

    while (1) {
        volts.value.int_value = HLW8012_getVoltage();
        amps.value.float_value = HLW8012_getCurrent();
        /*watts.value.int_value = HLW8012_getActivePower();*/
        watts.value.int_value = (int) (volts.value.int_value * amps.value.float_value);
        
        printf("%s: [HLW] Active Power (W)    :%d\n", __func__, HLW8012_getActivePower());
        printf("%s: [HLW] Voltage (V)         :%d\n", __func__, HLW8012_getVoltage());
        printf("%s: [HLW] Current (A)         :%f\n", __func__, HLW8012_getCurrent());
        printf("%s: [HLW] Apparent Power (VA) :%d\n", __func__, HLW8012_getApparentPower());
        printf("%s: [HLW] Power Factor (%%)    :%d\n", __func__, (int) (100 * HLW8012_getPowerFactor()));
        printf("%s: [HLW] Agg. energy (Ws)    :%d\n", __func__, HLW8012_getEnergy());
        
        homekit_characteristic_bounds_check( &volts);
        homekit_characteristic_bounds_check( &amps);
        homekit_characteristic_bounds_check( &watts);
        
        homekit_characteristic_notify(&volts, volts.value);
        homekit_characteristic_notify(&amps, amps.value);
        homekit_characteristic_notify(&watts, watts.value);
        vTaskDelay(POWER_MONITOR_POLL_PERIOD / portTICK_PERIOD_MS);
    }
}

//  SemaphoreHandle_t   semaphore;  //must set
//  uint32_t            mintime;    //must set in microseconds
//  uint32_t            count;      //autoinit
//  uint32_t            now;        //autoinit
//  uint32_t            time[BL0937_N]; //autoinit

void CF0_task(void *arg) {
    printf ("%s:\n", __func__);
    SemaphoreHandle_t mySemaphore=xSemaphoreCreateBinary();
    BL0937_data_t dataW;
    dataW.semaphore=mySemaphore;
    dataW.mintime=1000000; //1 second
    BL0937_collect(SOURCE_CF,&dataW);
    
    printf("Initial:      c=%d, n=%d, t0=%d, t1=%d, t2=%d, t3=%d\n",dataW.count,dataW.now,dataW.time[0],dataW.time[1],dataW.time[2],dataW.time[3]);
    while (1) {
        if (xSemaphoreTake(mySemaphore, 10000/portTICK_PERIOD_MS) == pdTRUE) {    
            printf("CF   taken:   ");
        } else {
            printf("CF   timeout: ");
        }
        printf("c=%d, n=%u, t0=%u, t1=%u, t2=%u, t3=%u",dataW.count,dataW.now,dataW.time[0],dataW.time[1],dataW.time[2],dataW.time[3]);
        if (dataW.count>3*BL0937_N/2) {
            BL0937_collect(SOURCE_CF,&dataW);
        } else if (dataW.count>=BL0937_N) {
            // do something to shift the time values if speed is slow
        } else {
            //keep collecting
        }
        if (dataW.count) printf(", avg=%u microseconds",(dataW.now-dataW.time[0])/dataW.count);
        printf("\n");
    }
    vTaskDelete(NULL);
}

void CF1_task(void *arg) {
    printf ("%s:\n", __func__);
    int timedout;
    SemaphoreHandle_t mySemaphore=xSemaphoreCreateBinary();
    BL0937_data_t dataV;
    BL0937_data_t dataA;
    dataV.semaphore=mySemaphore;
    dataA.semaphore=mySemaphore;
    dataV.mintime=  50000; //50 msecond
    dataA.mintime=1000000; // 1 second
    while (1) {
        BL0937_collect(SOURCE_CF1V,&dataV);
        if (xSemaphoreTake(mySemaphore,   100/portTICK_PERIOD_MS) == pdTRUE) {    
            printf("CF1V taken:   ");
        } else {
            printf("CF1V timeout: ");
        }
        printf("c=%d, n=%u, t0=%u, t1=%u, t2=%u, t3=%u",dataV.count,dataV.now,dataV.time[0],dataV.time[1],dataV.time[2],dataV.time[3]);
        if (dataV.count) printf(", avg=%u microseconds",(dataV.now-dataV.time[0])/dataV.count);
        printf("\n");
        
        BL0937_collect(SOURCE_CF1A,&dataA);
        timedout=0;
        while (timedout<10 && dataA.count<BL0937_N) {
            if (xSemaphoreTake(mySemaphore, 10000/portTICK_PERIOD_MS) == pdTRUE) {    
                printf("CF1A taken:   ");
            } else {
                timedout++;
                printf("CF1A timeout: ");
            }
            printf("c=%d, n=%u, t0=%u, t1=%u, t2=%u, t3=%u",dataA.count,dataA.now,dataA.time[0],dataA.time[1],dataA.time[2],dataA.time[3]);
            if (dataA.count) printf(", avg=%u microseconds",(dataA.now-dataA.time[0])/dataA.count);
            printf("\n");
        }
    }
    vTaskDelete(NULL);
}

// void identify_task(void *_args) {
//     vTaskDelete(NULL);
// }

void identify(homekit_value_t _value) {
    printf("Identify\n");
    // xTaskCreate(identify_task, "Identify", 128, NULL, tskIDLE_PRIORITY+1, NULL);
}

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(
        .id=1,
        .category=homekit_accessory_category_switch,
        .services=(homekit_service_t*[]){
            HOMEKIT_SERVICE(ACCESSORY_INFORMATION,
                .characteristics=(homekit_characteristic_t*[]){
                    HOMEKIT_CHARACTERISTIC(NAME, "NAS-WR01W"),
                    &manufacturer,
                    &serial,
                    &model,
                    &revision,
                    HOMEKIT_CHARACTERISTIC(IDENTIFY, identify),
                    NULL
                }),
            HOMEKIT_SERVICE(SWITCH, .primary=true,
                .characteristics=(homekit_characteristic_t*[]){
                    HOMEKIT_CHARACTERISTIC(NAME, "Smartplug"),
                    &relay,
                    &volts,
                    &watts,
                    &amps,
                    &calibrate_pow,
                    &calibrate_volts,
                    &calibrate_power,
                    &ota_trigger,
                    NULL
                }),
            NULL
        }),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111"
};

void button_callback(uint8_t gpio, void *args) {
    printf("Toggling relay\n");
    relay.value.bool_value = !relay.value.bool_value;
    relay_write(relay.value.bool_value, relay_gpio);
    led_write(relay.value.bool_value, LED_GPIO);
    homekit_characteristic_notify(&relay, relay.value);
    // sdk_os_timer_arm (&save_timer, SAVE_DELAY, 0 );
}

void device_init() {
    adv_button_set_evaluate_delay(10);
    adv_button_create(BUTTON_GPIO, true, false); // GPIO for button, pull-up resistor, inverted 
    adv_button_register_callback_fn(BUTTON_GPIO, button_callback, 1, NULL);
    
    
    gpio_enable(LED_GPIO, GPIO_OUTPUT);
    led_write(false, LED_GPIO);
    gpio_enable(relay_gpio, GPIO_OUTPUT);
    relay_write(relay.value.bool_value, relay_gpio);

//     HLW8012_init(CF_GPIO, CF1_GPIO, SELi_GPIO, 0, 1); //NAS-WR01W uses a BL0937
    // currentWhen  - 1 for HLW8012 (old Sonoff Pow), 0 for BL0937
    // model - 0 for HLW8012, 1 or other value for BL0937
    
    load_float_param ( "wattsx", &calibrated_power_multiplier);
    load_float_param ( "voltsx", &calibrated_volts_multiplier);
    load_float_param ( "currentx", &calibrated_current_multiplier);
    
    if (calibrated_power_multiplier !=0 && calibrated_current_multiplier !=0 && calibrated_volts_multiplier !=0) {
        printf ("%s:Setting calibrated multipliers, current: %f, voltage: %f, watts: %f\n", __func__, calibrated_current_multiplier, calibrated_volts_multiplier, calibrated_power_multiplier);
//         HLW8012_setCurrentMultiplier(calibrated_current_multiplier);
//         HLW8012_setVoltageMultiplier(calibrated_volts_multiplier);
//         HLW8012_setPowerMultiplier(calibrated_power_multiplier);
    } else {
        printf ("%s:calibrated mutipliers not available, current: %f, voltage: %f, watts: %f\n", __func__, calibrated_current_multiplier, calibrated_volts_multiplier, calibrated_power_multiplier);
    }
    
    xTaskCreate(power_monitoring_task, "Power Monitoring Task", 512, NULL, tskIDLE_PRIORITY+1, NULL);
    BL0937_init(CF_GPIO, CF1_GPIO, SELi_GPIO, MODEL_BL0937);
    xTaskCreate(CF0_task, "CF0_Task", 512, NULL, tskIDLE_PRIORITY+1, NULL);
    xTaskCreate(CF1_task, "CF1_task", 512, NULL, tskIDLE_PRIORITY+1, NULL);
    sdk_os_timer_setfn(&save_timer, save_characteristics, NULL);

    // homekit_characteristic_notify(&relay, relay.value);
}

void user_init(void) {
    uart_set_baud(0, 115200);
    udplog_init(3);
    UDPLUS("\n\n\nNAS-WR01W " VERSION "\n");

    device_init();
    
    int c_hash=ota_read_sysparam(&manufacturer.value.string_value,&serial.value.string_value,
                                      &model.value.string_value,&revision.value.string_value);
    //c_hash=1; revision.value.string_value="0.0.1"; //cheat line
    config.accessories[0]->config_number=c_hash;
    
    homekit_server_init(&config);
}

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
 * Homekit firmware NAS-WR01W SmartSocket
 */

#define SAVE_DELAY 500


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
void mamps_callback(homekit_characteristic_t *_ch, homekit_value_t value, void *context);
void calibrate_pow_set   (homekit_value_t value);
void calibrate_volts_set (homekit_value_t value);
void calibrate_power_set (homekit_value_t value);


homekit_characteristic_t calibrate_pow   = HOMEKIT_CHARACTERISTIC_(CUSTOM_CALIBRATE_POW, false, .setter=calibrate_pow_set);
homekit_characteristic_t calibrate_volts = HOMEKIT_CHARACTERISTIC_(CUSTOM_CALIBRATE_VOLTS, 234, .setter=calibrate_volts_set);
homekit_characteristic_t calibrate_power = HOMEKIT_CHARACTERISTIC_(CUSTOM_CALIBRATE_WATTS,1840, .setter=calibrate_power_set);

homekit_characteristic_t ota_trigger  = API_OTA_TRIGGER;
homekit_characteristic_t manufacturer = HOMEKIT_CHARACTERISTIC_(MANUFACTURER,  "X");
homekit_characteristic_t serial       = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, "1");
homekit_characteristic_t model        = HOMEKIT_CHARACTERISTIC_(MODEL,         "Z");
homekit_characteristic_t revision     = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION,  "0.0.0");
homekit_characteristic_t relay        = HOMEKIT_CHARACTERISTIC_(ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(relay_callback));
homekit_characteristic_t watts        = HOMEKIT_CHARACTERISTIC_(CUSTOM_WATTS, 0, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(watts_callback));
homekit_characteristic_t volts        = HOMEKIT_CHARACTERISTIC_(CUSTOM_VOLTS, 0, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(volts_callback));
homekit_characteristic_t mamps        = HOMEKIT_CHARACTERISTIC_(CUSTOM_MAMPS, 0, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(mamps_callback));
homekit_characteristic_t mWhs         = HOMEKIT_CHARACTERISTIC_(CUSTOM_mWh,   0                                                           );

const int BUTTON_GPIO =  0;
const int CF_GPIO     =  4;
const int CF1_GPIO    =  5;
const int SELi_GPIO   = 12;
const int LED_GPIO    = 13;
const int relay_gpio  = 14;

ETSTimer save_timer;
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


float calibrated_volts_multiplier=142000;
float calibrated_current_multiplier=13118710;
float calibrated_power_multiplier=1668220;
float calibrated_energy_multiplier=0.460727;
bool  cf0_done,cf1_done;


void relay_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context) {
    printf("Relay callback\n");
    relay_write(relay.value.bool_value, relay_gpio);
    led_write(relay.value.bool_value, LED_GPIO);
    cf0_done=cf1_done=true; //when switching, always start new measurements ASAP
}
void volts_callback(homekit_characteristic_t *_ch, homekit_value_t value, void *context) {
//     printf("Volts on callback\n");
}
void mamps_callback(homekit_characteristic_t *_ch, homekit_value_t value, void *context) {
//     printf("mAmps on callback\n");
}
void watts_callback(homekit_characteristic_t *_ch, homekit_value_t value, void *context) {
//     printf("Watts on callback\n");
}

void save_characteristics() {
    //called by a timer function to save charactersitics
    save_float_param ( "wattsx", calibrated_power_multiplier);
    save_float_param ( "voltsx", calibrated_volts_multiplier);
    save_float_param ( "currentx", calibrated_current_multiplier);
}

void calibrate_task() {
//     HLW8012_set_calibrated_mutipliers (&calibrated_current_multiplier, &calibrated_volts_multiplier, &calibrated_power_multiplier, calibrate_volts.value.int_value, calibrate_power.value.int_value) ;
    
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

//  SemaphoreHandle_t   semaphore;  //must set
//  uint32_t            mintime;    //must set in microseconds
//  uint32_t            total;      //does not clear
//  uint32_t            to_count;   //autoinit
//  uint32_t            count;      //autoinit
//  uint32_t            now;        //autoinit
//  uint32_t            time[BL0937_N]; //autoinit

void CF0_task(void *arg) {
    printf ("%s:\n", __func__);
    SemaphoreHandle_t mySemaphore=xSemaphoreCreateBinary();
    BL0937_data_t dataW;
    dataW.semaphore=mySemaphore;
    dataW.mintime=1000*1000; //1 second
    dataW.total=0;
    BaseType_t taken;
    float old_value=0;
    
    while (1) {
        BL0937_collect(SOURCE_CF,&dataW);
        cf0_done=false;
        while(!cf0_done) {
            taken=xSemaphoreTake(mySemaphore, 10000/portTICK_PERIOD_MS);
            // process current results, be aware that the count could be as high as 2500 so first part just under 2^32
            watts.value.float_value=(dataW.count>1)?((int)((1668220*(dataW.count-1))/((dataW.now-dataW.time[0])/10)))/10.0:0;
            homekit_characteristic_bounds_check(&watts);
            homekit_characteristic_notify(&watts,watts.value);
            mWhs.value.int_value=(uint32_t)(dataW.total*0.460727);
            homekit_characteristic_bounds_check(&mWhs);
            homekit_characteristic_notify(&mWhs,mWhs.value);
            if (taken) printf("CF   taken:   "); else printf("CF   timeout: ");
            printf("c=%d, n=%u, t0=%u, t1=%u, t2=%u, t3=%u, t4=%u, t=%u",dataW.count,dataW.now,dataW.time[0],dataW.time[1],dataW.time[2],dataW.time[3],dataW.time[4],dataW.total);
            printf(", avg=%u us, %.1fW, %umWh\n",(dataW.count>1)?(dataW.now-dataW.time[0])/(dataW.count-1):0,watts.value.float_value,mWhs.value.int_value);
            // prepare future results
            cf0_done=(cf0_done || 20*watts.value.float_value<old_value || BL0937_process(&dataW,taken));
            if (20*watts.value.float_value<old_value) cf1_done=true; //when connected device switches off, detect ASAP
            old_value=watts.value.float_value;
        }
    }
    vTaskDelete(NULL);
}

void CF1_task(void *arg) {
    printf ("%s:\n", __func__);
    SemaphoreHandle_t mySemaphore=xSemaphoreCreateBinary();
    BL0937_data_t dataV;
    BL0937_data_t dataA;
    dataV.semaphore=mySemaphore;
    dataA.semaphore=mySemaphore;
    dataV.mintime=  50*1000; //50 msecond
    dataA.mintime=1000*1000; // 1  second
    BaseType_t taken;
    uint16_t old_value=0;
    
    while (1) {
        BL0937_collect(SOURCE_CF1V,&dataV);
        taken=xSemaphoreTake(mySemaphore, 100/portTICK_PERIOD_MS);
        // process current results
        volts.value.int_value=(dataV.count>1)?(int)142000*(dataV.count-1)/(dataV.now-dataV.time[0]):0;
        homekit_characteristic_bounds_check(&volts);
        homekit_characteristic_notify(&volts,volts.value);
        if (taken) printf("CF1V taken:   "); else printf("CF1V timeout: ");
        printf("c=%d, n=%u, t0=%u",dataV.count,dataV.now,dataV.time[0]);
        printf(", avg=%u us, %uV\n",(dataV.count>1)?(dataV.now-dataV.time[0])/(dataV.count-1):0,volts.value.int_value);
        // no point in slow shifting, move on to Current(mAmps)
        
        BL0937_collect(SOURCE_CF1A,&dataA);
        cf1_done=false;
        while(!cf1_done) {
            taken=xSemaphoreTake(mySemaphore, 10000/portTICK_PERIOD_MS);
            // process current results, be aware that the count could be as high as 3000 so first part just under 2^32
            mamps.value.int_value=(dataA.count>1)?(int)(((13118710/10)*(dataA.count-1)/((dataA.now-dataA.time[0])/10))+0.5):0;
            homekit_characteristic_bounds_check(&mamps);
            homekit_characteristic_notify(&mamps,mamps.value);
            if (taken) printf("CF1A taken:   "); else printf("CF1A timeout: ");
            printf("c=%d, n=%u, t0=%u, t1=%u, t2=%u, t3=%u, t4=%u",dataA.count,dataA.now,dataA.time[0],dataA.time[1],dataA.time[2],dataA.time[3],dataA.time[4]);
            printf(", avg=%u us, %umA\n",(dataA.count>1)?(dataA.now-dataA.time[0])/(dataA.count-1):0,mamps.value.int_value);
            // prepare future results
            cf1_done=(cf1_done || 20*mamps.value.int_value<old_value || BL0937_process(&dataA,taken));
            if (20*mamps.value.int_value<old_value) cf0_done=true; //when connected device switches off, detect ASAP
            old_value=mamps.value.int_value;
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
                    &mamps,
                    &mWhs,
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

void button_callback(uint8_t gpio, void *args) {
    printf("Toggling relay\n");
    relay.value.bool_value = !relay.value.bool_value;
    relay_write(relay.value.bool_value, relay_gpio);
    led_write(relay.value.bool_value, LED_GPIO);
    homekit_characteristic_notify(&relay, relay.value);
    // sdk_os_timer_arm (&save_timer, SAVE_DELAY, 0 );
}

homekit_server_config_t config;
void device_init() {
  if (homekit_is_paired()) {
    udplog_init(3);
    UDPLUS("\n\n\nNAS-WR01W " VERSION "\n");
    config.on_event=NULL;

    adv_button_set_evaluate_delay(10);
    adv_button_create(BUTTON_GPIO, true, false); // GPIO for button, pull-up resistor, inverted 
    adv_button_register_callback_fn(BUTTON_GPIO, button_callback, 1, NULL);
    
    
    gpio_enable(LED_GPIO, GPIO_OUTPUT);
    led_write(false, LED_GPIO);
    gpio_enable(relay_gpio, GPIO_OUTPUT);
    relay_write(relay.value.bool_value, relay_gpio);

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
    
    BL0937_init(CF_GPIO, CF1_GPIO, SELi_GPIO, MODEL_BL0937);
    xTaskCreate(CF0_task, "CF0_Task", 512, NULL, tskIDLE_PRIORITY+1, NULL);
    xTaskCreate(CF1_task, "CF1_task", 512, NULL, tskIDLE_PRIORITY+1, NULL);
    sdk_os_timer_setfn(&save_timer, save_characteristics, NULL);

    // homekit_characteristic_notify(&relay, relay.value);
  }
}

homekit_server_config_t config = {
    .accessories = accessories,
    .on_event=device_init,
    .password = "111-11-111"
};

void user_init(void) {
    uart_set_baud(0, 115200);

    device_init();
    
    int c_hash=ota_read_sysparam(&manufacturer.value.string_value,&serial.value.string_value,
                                      &model.value.string_value,&revision.value.string_value);
    //c_hash=1; revision.value.string_value="0.0.1"; //cheat line
    config.accessories[0]->config_number=c_hash;
    
    homekit_server_init(&config);
}

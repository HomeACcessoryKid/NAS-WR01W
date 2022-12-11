/*
 * HomeKit Custom Characteristics
 *
 * Copyright 2022 HomeAccessoryKid @gmail.com
 * modified from 2018 David B Brown (@maccoylton)
 *
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
 */

#include <homekit/homekit.h>
#include <homekit/characteristics.h>

#ifndef __HOMEKIT_CUSTOM_CHARACTERISTICS__
#define __HOMEKIT_CUSTOM_CHARACTERISTICS__


#define HOMEKIT_CUSTOM_UUID(value) (value"-0e36-4a42-ad11-745a73b84f2b")


#endif

#define HOMEKIT_CHARACTERISTIC_CUSTOM_WATTS HOMEKIT_CUSTOM_UUID("F000000A")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_WATTS(_value, ...) \
.type = HOMEKIT_CHARACTERISTIC_CUSTOM_WATTS, \
.description = "WATTS", \
.format = homekit_format_uint16, \
.permissions = homekit_permissions_paired_read \
| homekit_permissions_notify, \
.min_value = (float[]) {0}, \
.max_value = (float[]) {4500}, \
.min_step = (float[]) {1}, \
.value = HOMEKIT_UINT16_(_value), \
##__VA_ARGS__


#define HOMEKIT_CHARACTERISTIC_CUSTOM_VOLTS HOMEKIT_CUSTOM_UUID("F000000B")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_VOLTS(_value, ...) \
.type = HOMEKIT_CHARACTERISTIC_CUSTOM_VOLTS, \
.description = "VOLTS", \
.format = homekit_format_uint16, \
.permissions = homekit_permissions_paired_read \
| homekit_permissions_notify, \
.min_value = (float[]) {0}, \
.max_value = (float[]) {250}, \
.min_step = (float[]) {1}, \
.value = HOMEKIT_UINT16_(_value), \
##__VA_ARGS__


#define HOMEKIT_CHARACTERISTIC_CUSTOM_MAMPS HOMEKIT_CUSTOM_UUID("F000000C")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_MAMPS(_value, ...) \
.type = HOMEKIT_CHARACTERISTIC_CUSTOM_MAMPS, \
.description = "mAMPS", \
.format = homekit_format_uint16, \
.permissions = homekit_permissions_paired_read \
| homekit_permissions_notify, \
.min_value = (float[]) {0}, \
.max_value = (float[]) {18000}, \
.min_step = (float[]) {10}, \
.value = HOMEKIT_UINT16_(_value), \
##__VA_ARGS__


#define HOMEKIT_CHARACTERISTIC_CUSTOM_CALIBRATE_POW HOMEKIT_CUSTOM_UUID("F000000D")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_CALIBRATE_POW(_value, ...) \
.type = HOMEKIT_CHARACTERISTIC_CUSTOM_CALIBRATE_POW, \
.description = "}Calibrate(d)", \
.format = homekit_format_bool, \
.permissions = homekit_permissions_paired_read \
| homekit_permissions_paired_write \
| homekit_permissions_notify, \
.value = HOMEKIT_BOOL_(_value), \
##__VA_ARGS__


#define HOMEKIT_CHARACTERISTIC_CUSTOM_CALIBRATE_VOLTS HOMEKIT_CUSTOM_UUID("F000000E")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_CALIBRATE_VOLTS(_value, ...) \
.type = HOMEKIT_CHARACTERISTIC_CUSTOM_CALIBRATE_VOLTS, \
.description = "}Calibration Volts", \
.format = homekit_format_uint16, \
.permissions = homekit_permissions_paired_read \
| homekit_permissions_paired_write \
| homekit_permissions_notify, \
.min_value = (float[]) {1}, \
.max_value = (float[]) {240}, \
.min_step = (float[]) {1}, \
.value = HOMEKIT_UINT16_(_value), \
##__VA_ARGS__


#define HOMEKIT_CHARACTERISTIC_CUSTOM_CALIBRATE_WATTS HOMEKIT_CUSTOM_UUID("F000000F")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_CALIBRATE_WATTS(_value, ...) \
.type = HOMEKIT_CHARACTERISTIC_CUSTOM_CALIBRATE_WATTS, \
.description = "}Calibration WATTS", \
.format = homekit_format_uint16, \
.permissions = homekit_permissions_paired_read \
| homekit_permissions_paired_write \
| homekit_permissions_notify, \
.min_value = (float[]) {1}, \
.max_value = (float[]) {3840}, \
.min_step = (float[]) {1}, \
.value = HOMEKIT_UINT16_(_value), \
##__VA_ARGS__

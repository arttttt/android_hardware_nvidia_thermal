/*
 * Copyright (C) 2017 The Android Open Source Project
 * Copyright (C) 2017-2018 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "android.hardware.thermal@1.0-service-nvidia"

// #define LOG_NDEBUG 0

#include <vector>

#include <android/log.h>
#include <utils/Log.h>
#include "Thermal.h"
#include "thermalhal.h"

/*
 * The C interface below and the HIDL types above it describe the same
 * readings, and are not the same structures.
 *
 * Every one of these pairs differs in its name field: the C side keeps a
 * `const char *`, four bytes, while the HIDL side keeps a hidl_string --
 * a pointer, a length and a flag saying whether it owns the buffer, twelve
 * bytes on this architecture. So a Temperature is thirty-two bytes against
 * temperature_t's twenty-four, and every field after the name sits at a
 * different offset.
 *
 * This code used to hand the HIDL vector's own storage to the C function
 * through a reinterpret_cast and let it write records in its own shape.
 * What that overwrites is the hidl_string's internals: its buffer pointer
 * takes whatever the C code happened to put there and its length becomes
 * nonsense. Nothing complains until the string is next read, which is when
 * the service is answering the framework -- and then it dies in memmove
 * copying from an address that was never a string:
 *
 *   Fatal signal 11 (SIGSEGV), code 1 (SEGV_MAPERR), fault addr 0x51601f81
 *     #01 hidl_string::copyFrom(char const*, unsigned int)
 *     #04 thermal::V1_0::implementation::Thermal::getTemperatures
 *
 * init restarts a crashing HAL, the crash repeats on the next query, and
 * after enough of them it reboots the board into the bootloader.
 *
 * So the C structures are filled in their own array now, and copied across
 * a field at a time.
 */

namespace android {
namespace hardware {
namespace thermal {
namespace V1_0 {
namespace implementation {

Thermal::Thermal() {
  pd_count = thermal_init();
}

// Methods from ::android::hardware::thermal::V1_0::IThermal follow.
Return<void> Thermal::getTemperatures(getTemperatures_cb _hidl_cb) {
  ThermalStatus status;
  status.code = ThermalStatusCode::SUCCESS;
  hidl_vec<Temperature> temperatures;

  ssize_t count = pd_count < 0 ? pd_count : get_temperatures(NULL, 0);
  if (count < 0) {
    status.code = ThermalStatusCode::FAILURE;
    status.debugMessage = strerror(-count);
  }
  else if (count > 0) {
    std::vector<temperature_t> list(count);
    ssize_t size = get_temperatures(list.data(), count);
    if (size < 0) {
      status.code = ThermalStatusCode::FAILURE;
      status.debugMessage = strerror(-size);
    }
    else {
      temperatures.resize(size);
      for (ssize_t i = 0; i < size; i++) {
        temperatures[i].type = static_cast<TemperatureType>(list[i].type);
        temperatures[i].name = list[i].name ? list[i].name : "";
        temperatures[i].currentValue = list[i].current_value;
        temperatures[i].throttlingThreshold = list[i].throttling_threshold;
        temperatures[i].shutdownThreshold = list[i].shutdown_threshold;
        temperatures[i].vrThrottlingThreshold = list[i].vr_throttling_threshold;
      }
    }
  }

  _hidl_cb(status, temperatures);
  return Void();
}

Return<void> Thermal::getCpuUsages(getCpuUsages_cb _hidl_cb) {
  ThermalStatus status;
  status.code = ThermalStatusCode::SUCCESS;
  hidl_vec<CpuUsage> cpuUsages;

  ssize_t count = pd_count < 0 ? pd_count : get_cpu_usages(NULL);
  if (count < 0) {
    status.code = ThermalStatusCode::FAILURE;
    status.debugMessage = strerror(-count);
  }
  else if (count > 0) {
    std::vector<cpu_usage_t> list(count);
    ssize_t size = get_cpu_usages(list.data());
    if (size < 0) {
      status.code = ThermalStatusCode::FAILURE;
      status.debugMessage = strerror(-size);
    }
    else {
      cpuUsages.resize(size);
      for (ssize_t i = 0; i < size; i++) {
        cpuUsages[i].name = list[i].name ? list[i].name : "";
        cpuUsages[i].active = list[i].active;
        cpuUsages[i].total = list[i].total;
        cpuUsages[i].isOnline = list[i].is_online;
      }
    }
  }

  _hidl_cb(status, cpuUsages);
  return Void();
}

Return<void> Thermal::getCoolingDevices(getCoolingDevices_cb _hidl_cb) {
  ThermalStatus status;
  status.code = ThermalStatusCode::SUCCESS;
  hidl_vec<CoolingDevice> coolingDevices;

  ssize_t count = pd_count < 0 ? pd_count : get_cooling_devices(NULL, 0);
  if (count < 0) {
    status.code = ThermalStatusCode::FAILURE;
    status.debugMessage = strerror(-count);
  }
  else if (count > 0) {
    std::vector<cooling_device_t> list(count);
    ssize_t size = get_cooling_devices(list.data(), count);
    if (size < 0) {
      status.code = ThermalStatusCode::FAILURE;
      status.debugMessage = strerror(-size);
    }
    else {
      coolingDevices.resize(size);
      for (ssize_t i = 0; i < size; i++) {
        coolingDevices[i].type = static_cast<CoolingType>(list[i].type);
        coolingDevices[i].name = list[i].name ? list[i].name : "";
        coolingDevices[i].currentValue = list[i].current_value;
      }
    }
  }

  _hidl_cb(status, coolingDevices);
  return Void();
}

status_t Thermal::registerAsSystemService() {
    status_t ret = 0;

    ret = IThermal::registerAsService();
    if (ret != 0) {
        ALOGE("Failed to register IThermal (%d)", ret);
        goto fail;
    } else {
        ALOGI("Successfully registered IThermal");
    }

fail:
    return ret;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace thermal
}  // namespace hardware
}  // namespace android

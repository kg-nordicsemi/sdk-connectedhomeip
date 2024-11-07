/*
 *
 *    Copyright (c) 2022 Project CHIP Authors
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "Reboot.h"

#include <lib/support/TypeTraits.h>

#include <zephyr/sys/reboot.h>

#if !(defined(CONFIG_ARCH_POSIX) || defined(CONFIG_SOC_SERIES_NRF54HX))
#include <hal/nrf_power.h>
#endif

#include "../../../../../nrf/samples/matter/common/src/persistent_storage/backends/persistent_storage_settings.h"
#include "../../../../../nrf/samples/matter/common/src/persistent_storage/persistent_storage.h"
#include "../../../../../nrf/samples/matter/common/src/persistent_storage/persistent_storage_common.h"

#include <zephyr/kernel.h>

namespace chip {
namespace DeviceLayer {

#if defined(CONFIG_ARCH_POSIX) || defined(CONFIG_SOC_SERIES_NRF54HX)

void Reboot(SoftwareRebootReason reason)
{
    sys_reboot(SYS_REBOOT_WARM);
}
Nrf::PersistentStorageNode mBootReason("bootReasonKey", strlen("bootReasonKey"));

SoftwareRebootReason GetSoftwareRebootReason()
{
    size_t bufferSize    = 1024;
    uint8_t * dataBuffer = (uint8_t *) k_malloc(bufferSize);  // Allocate memory using Zephyr's kernel malloc
    __ASSERT(dataBuffer != NULL, "Memory allocation failed"); // Ensure memory was allocated

    SoftwareRebootReason BootReason;

    size_t actualSize = 0;

    // Pass the address of BootReason and use dataBuffer to get the pointer to the buffer
    Nrf::PSErrorCode result = Nrf::GetPersistentStorage().NonSecureLoad(&mBootReason, dataBuffer, bufferSize, actualSize);

    if (result == Nrf::PSErrorCode::Success && actualSize == sizeof(SoftwareRebootReason))
    {
        // Assuming the data read is exactly the size of SoftwareRebootReason, copy it to BootReason
        memcpy(&BootReason, dataBuffer, sizeof(SoftwareRebootReason));
    }
    else
    {
        // Handle error or invalid size
        // You might want to log this situation or handle it according to your application's needs
    }

    k_free(dataBuffer); // Free the memory allocated with k_malloc
    return BootReason;
}

#else

using RetainedReason = decltype(nrf_power_gpregret_get(NRF_POWER, 0));

constexpr RetainedReason EncodeReason(SoftwareRebootReason reason)
{
    // Set MSB to avoid collission with Zephyr's pre-defined reboot reasons.
    constexpr RetainedReason kCustomReasonFlag = 0x80;

    return static_cast<RetainedReason>(reason) | kCustomReasonFlag;
}

void Reboot(SoftwareRebootReason reason)
{
    const RetainedReason retainedReason = EncodeReason(reason);

    nrf_power_gpregret_set(NRF_POWER, 0, retainedReason);

    sys_reboot(retainedReason);
}

SoftwareRebootReason GetSoftwareRebootReason()
{
    switch (nrf_power_gpregret_get(NRF_POWER, 0))
    {
    case EncodeReason(SoftwareRebootReason::kSoftwareUpdate):
        nrf_power_gpregret_set(NRF_POWER, 0, 0);
        return SoftwareRebootReason::kSoftwareUpdate;
    default:
        return SoftwareRebootReason::kOther;
    }
}

#endif

} // namespace DeviceLayer
} // namespace chip

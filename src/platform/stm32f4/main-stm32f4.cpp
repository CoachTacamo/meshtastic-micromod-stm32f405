#include "configuration.h"
#include "NodeDB.h"
#include "HardwareRNG.h"
#include "concurrency/OSThread.h"
#include <SPI.h>
#include <stm32f4xx_hal.h>

void setBluetoothEnable(bool) {}
void playStartMelody() {}
void playShutdownMelody() {}
void updateBatteryLevel(uint8_t) {}
void cpuDeepSleep(uint32_t msec) { delay(msec); }

void getMacAddr(uint8_t *mac)
{
    const uint32_t uid[] = {HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2()};
    uint64_t hash = 14695981039346656037ULL;
    for (uint8_t b : reinterpret_cast<const uint8_t (&)[12]>(uid)) {
        hash ^= b;
        hash *= 1099511628211ULL;
    }
    for (unsigned i = 0; i < 6; ++i) mac[i] = hash >> (8 * i);
    mac[0] = (mac[0] & 0xfc) | 2;
}

void earlyInitVariant()
{
    SerialUSB.begin(115200);
    delay(3000);
    SerialUSB.println("F405 platform startup");
    SerialUSB.flush();
    digitalWrite(RF95_TXEN, LOW); pinMode(RF95_TXEN, OUTPUT);
    digitalWrite(RF95_RXEN, LOW); pinMode(RF95_RXEN, OUTPUT);
    digitalWrite(LORA_CS, HIGH); pinMode(LORA_CS, OUTPUT);
    digitalWrite(3, LOW); pinMode(3, OUTPUT);
    digitalWrite(LORA_RESET, LOW); pinMode(LORA_RESET, OUTPUT);
    digitalWrite(3, HIGH); delay(100);
    digitalWrite(LORA_RESET, HIGH); delay(10);
    uint32_t seed;
    if (!HardwareRNG::fill(reinterpret_cast<uint8_t *>(&seed), sizeof(seed), false)) {
        SerialUSB.println("F405 hardware RNG failed"); SerialUSB.flush(); abort();
    }
    SerialUSB.println("F405 hardware RNG ready"); SerialUSB.flush();
    randomSeed(seed);
    SPI.setSCLK(LORA_SCK); SPI.setMOSI(LORA_MOSI); SPI.setMISO(LORA_MISO);
}

void variantDefaultConfig()
{
    config.device.role = meshtastic_Config_DeviceConfig_Role_ROUTER;
    config.lora.region = meshtastic_Config_LoRaConfig_RegionCode_US;
    config.lora.use_preset = true;
    config.lora.modem_preset = meshtastic_Config_LoRaConfig_ModemPreset_LONG_FAST;
    config.lora.channel_num = 20;
    config.lora.tx_enabled = true;
    config.lora.tx_power = 2;
}

class F405Watchdog : public concurrency::OSThread {
public:
    F405Watchdog() : OSThread("IWDG", 1000) {
        IWDG->KR = 0xCCCC; IWDG->KR = 0x5555;
        IWDG->PR = 6; IWDG->RLR = 4095;
        IWDG->KR = 0xAAAA;
    }
protected:
    int32_t runOnce() override { IWDG->KR = 0xAAAA; return 1000; }
};
void lateInitVariant() { new F405Watchdog(); }

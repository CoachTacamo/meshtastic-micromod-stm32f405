#include "LittleFS.h"
#include "configuration.h"
#include "mesh/Throttle.h"
#include <SPI.h>

namespace {
SPIClass storage(44, 43, 42);
constexpr uint32_t cs = 45;
constexpr uint32_t base = 15 * 1024 * 1024;
constexpr uint32_t blockSize = 4096;
constexpr uint32_t blockCount = 256;

void start(uint8_t command) {
    storage.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    digitalWrite(cs, LOW); storage.transfer(command);
}
void stop() { digitalWrite(cs, HIGH); storage.endTransaction(); }
void address(uint32_t value) {
    storage.transfer(value >> 16); storage.transfer(value >> 8); storage.transfer(value);
}
uint8_t status() { start(5); uint8_t value = storage.transfer(0); stop(); return value; }
bool waitReady() {
    uint32_t started = millis();
    while (status() & 1) {
        if (!Throttle::isWithinTimespanMs(started, 3000)) return false;
        delay(1);
    }
    return true;
}
bool writeEnable() {
    if (!waitReady()) return false;
    start(6); stop(); return (status() & 2) != 0;
}
bool valid(lfs_block_t block, lfs_off_t off, lfs_size_t size) {
    return block < blockCount && off <= blockSize && size <= blockSize - off;
}
int read(const lfs_config *, lfs_block_t block, lfs_off_t off, void *data, lfs_size_t size) {
    if (!data || !valid(block, off, size)) return LFS_ERR_INVAL;
    if (!waitReady()) return LFS_ERR_IO;
    start(3); address(base + block * blockSize + off);
    auto out = static_cast<uint8_t *>(data);
    for (lfs_size_t i = 0; i < size; ++i) out[i] = storage.transfer(0);
    stop(); return 0;
}
int prog(const lfs_config *, lfs_block_t block, lfs_off_t off, const void *data, lfs_size_t size) {
    if (!data || !valid(block, off, size) || (off % 256) || (size % 256)) return LFS_ERR_INVAL;
    auto in = static_cast<const uint8_t *>(data);
    for (lfs_size_t i = 0; i < size; i += 256) {
        if (!writeEnable()) return LFS_ERR_IO;
        start(2); address(base + block * blockSize + off + i);
        for (unsigned j = 0; j < 256; ++j) storage.transfer(in[i + j]);
        stop(); if (!waitReady()) return LFS_ERR_IO;
        uint8_t check[256];
        if (read(nullptr, block, off + i, check, sizeof(check)) || memcmp(check, in + i, sizeof(check))) return LFS_ERR_IO;
    }
    return 0;
}
int erase(const lfs_config *, lfs_block_t block) {
    if (block >= blockCount) return LFS_ERR_INVAL;
    if (!writeEnable()) return LFS_ERR_IO;
    start(0x20); address(base + block * blockSize); stop();
    return waitReady() ? 0 : LFS_ERR_IO;
}
int sync(const lfs_config *) { return waitReady() ? 0 : LFS_ERR_IO; }
lfs_config cfg = {nullptr, read, prog, erase, sync, 256, 256, blockSize, blockCount, 128,
                  nullptr, nullptr, nullptr, nullptr};
}

LittleFS InternalFS;
LittleFS::LittleFS() : STM32_LittleFS(&cfg) {}
bool LittleFS::begin() {
    digitalWrite(cs, HIGH); pinMode(cs, OUTPUT); storage.begin();
    start(0xab); stop(); delay(1);
    start(0x9f); uint8_t vendor = storage.transfer(0); uint8_t type = storage.transfer(0); uint8_t capacity = storage.transfer(0); stop();
    LOG_INFO("Storage JEDEC %02x:%02x:%02x", vendor, type, capacity);
    if (vendor != 0xef || (type != 0x40 && type != 0x70) || capacity != 0x18) return false;
    if (STM32_LittleFS::begin()) return true;
    // Only initialize a blank reserved region; preserve any pre-existing data.
    uint8_t buffer[256];
    for (uint32_t block = 0; block < blockCount; ++block) {
        for (uint32_t off = 0; off < blockSize; off += sizeof(buffer)) {
            if (read(&cfg, block, off, buffer, sizeof(buffer))) return false;
            for (uint8_t byte : buffer) if (byte != 0xff) return false;
        }
    }
    return format() && STM32_LittleFS::begin();
}

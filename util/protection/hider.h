#pragma once


#include <vector>
#include <random>
#include <wtypes.h>
#include <map>
#include <functional>


class FunctionHider {
private:
    std::map<std::string, std::function<int()>> hiddenFunctions;
    uint32_t encryptionKey = 0xFF99a0;

public:
    template<typename Func>
    void hideFunction(const std::string& obfuscatedName, Func func) {
        hiddenFunctions[obfuscatedName] = func;
    }

    int callHiddenFunction(const std::string& obfuscatedName) {
        if (hiddenFunctions.count(obfuscatedName)) {
            return hiddenFunctions[obfuscatedName]();
        }
        return -1;
    }
};

class RuntimeFunctionEncryptor {
public:
    void encryptMemory(void* memoryBlock, size_t size) {
        DWORD oldProtect;
        VirtualProtect(memoryBlock, size, PAGE_EXECUTE_READWRITE, &oldProtect);

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);

        uint8_t* bytes = static_cast<uint8_t*>(memoryBlock);
        for (size_t i = 0; i < size; ++i) {
            bytes[i] ^= dis(gen);
        }

        VirtualProtect(memoryBlock, size, oldProtect, &oldProtect);
    }
};

#pragma once
#include "core/memory/Memory.hpp"
#include "Offsets.hpp"

inline const DWORD MAX_BLOCK_SIZE = 409600;

class Dumper {
public:
    ~Dumper()                           = default;
    Dumper(const Dumper&)            = delete;
    Dumper(Dumper&&)                 = delete;
    Dumper& operator=(const Dumper&) = delete;
    Dumper& operator=(Dumper&&)      = delete;

   static bool Init();

   // Re-scan a specific offset pattern.  Called when the initial offset
   // returns null (stale pattern for a different CS2 build).
   static bool RescanEntityList();
private:
    Dumper() {};

    static Dumper& GetInstance()
    {
        static Dumper i{};
        return i;
    }

    bool InitImpl();
private:
    std::vector<WORD> StrSigToArray(const std::string& sig);
    DWORD64 Scan(const offsets::signatures::Signature& sig, ProcessModule module);
    void GetNextArray(std::vector<short>& next, const std::vector<WORD>& signature);
    std::vector<DWORD64> ScanMemory(const std::string& sig, DWORD64 start, DWORD64 end, int number = 1);
    void ScanBlock(byte* buffer, const std::vector<short>& next, const std::vector<WORD>& signature, DWORD64 start, DWORD size, std::vector<DWORD64>& result);
};
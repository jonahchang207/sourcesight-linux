#include "Dumper.hpp"

#include "core/engine/Engine.hpp"

bool Dumper::Init() {
    return GetInstance().InitImpl();
}

bool Dumper::InitImpl() {
    auto process = Engine::GetProcess();
    auto client = Engine::GetClient();
    auto engine = Engine::GetEngine();

    DWORD64 temp = 0;

    // client.dll

    // View Matrix
    if (!(temp = Scan(offsets::signatures::viewMatrix, client))) {
        LOGF(FATAL, "Could not find offset for 'viewMatrix'");
        return false;
    }

    offsets::viewMatrix = temp - client.base;
    LOGF(VERBOSE, "Found 'viewMatrix' offset at 0x{:X}", offsets::viewMatrix);

    // Global Variables
    if (!(temp = Scan(offsets::signatures::globalVars, client))) {
        LOGF(FATAL, "Could not find offset for 'globalVars'");
        return false;
    }

    offsets::globalVars = temp - client.base;
    LOGF(VERBOSE, "Found 'globalVars' offset at 0x{:X}", offsets::globalVars);

    // Entity List: try the current a2x linux pattern first, then fall back
    // to patterns from previous builds so a changed scan site degrades
    // gracefully instead of aborting startup.
    const offsets::signatures::Signature entityListCandidates[] = {
        offsets::signatures::entityList,
        offsets::signatures::entityListAlt,
        offsets::signatures::entityListAlt2,
    };

    temp = 0;
    for (const auto& sig : entityListCandidates) {
        if ((temp = Scan(sig, client)))
            break;
    }

    if (!temp) {
        LOGF(FATAL, "Could not find offset for 'entityList'");
        return false;
    }

    offsets::entityList = temp - client.base;
    LOGF(VERBOSE, "Found 'entityList' offset at 0x{:X}", offsets::entityList);

    // Local Player Controller
    if (!(temp = Scan(offsets::signatures::localPlayerController, client))) {
        LOGF(FATAL, "Could not find offset for 'localPlayerController'");
        return false;
    }

    offsets::localPlayerController = temp - client.base;
    LOGF(VERBOSE, "Found 'localPlayerController' offset at 0x{:X}", offsets::localPlayerController);

    // C4
    if (!(temp = Scan(offsets::signatures::plantedC4, client))) {
        LOGF(FATAL, "Could not find offset for 'weaponC4'");
        return false;
    }

    offsets::plantedC4 = temp - client.base;
    LOGF(VERBOSE, "Found 'weaponC4' offset at 0x{:X}", offsets::plantedC4);

    // C4 carrier pointer (no Linux pattern available yet, so this is best-effort)
    if (!(temp = Scan(offsets::signatures::weaponC4, client))) {
        LOGF(WARNING, "Could not find offset for 'weaponC4 carrier', disabling C4 carrier ESP");
    } else {
        offsets::weaponC4 = temp - client.base;
        LOGF(VERBOSE, "Found 'weaponC4 carrier' offset at 0x{:X}", offsets::weaponC4);
    }

#if 0
    // Local Player Pawn (tbh idk how to read it :1)
    if (temp = Scan(offsets::signatures::localPlayerPawn, client); !temp) {
        LOGF(FATAL, "Could not find offset for 'localPlayerPawn'");
        return false;
    }

    offsets::localPlayerPawn = temp + 0x138 - client.base;
    LOGF(VERBOSE, "Found 'localPlayerPawn' offset at 0x{:X}", offsets::localPlayerPawn);
 

    // Input
    if (temp = Scan(offsets::signatures::csgoInput, client); !temp) {
        LOGF(FATAL, "Could not find offset for 'csgoInput'");
        return false;
    }

    offsets::csgoInput = temp - client.base;
    LOGF(VERBOSE, "Found 'csgoInput' offset at 0x{:X}", offsets::csgoInput);
#endif

    // engine2.dll

    // Build Number
    if (!(temp = Scan(offsets::signatures::buildNumber, engine))) {
        LOGF(FATAL, "Could not find offset for 'buildNumber'");
        return false;
    }

    offsets::buildNumber = temp - engine.base;
    LOGF(VERBOSE, "Found 'buildNumber' offset at 0x{:X}", offsets::buildNumber);

    LOGF(INFO, "Successfully dumped offsets...");

    return true;
}

bool Dumper::RescanEntityList() {
    auto process = Engine::GetProcess();
    auto client = Engine::GetClient();

    if (!process || !client.base)
        return false;

    Dumper& d = GetInstance();

    const offsets::signatures::Signature entityListCandidates[] = {
        offsets::signatures::entityList,
        offsets::signatures::entityListAlt,
        offsets::signatures::entityListAlt2,
        { "48 8B 3D ?? ?? ?? ?? 48 85 FF 74", 3, 7 },
        { "48 8B 1D ?? ?? ?? ?? 48 85 DB 74", 3, 7 },
    };

    for (const auto& sig : entityListCandidates) {
        DWORD64 temp = d.Scan(sig, client);
        if (temp) {
            offsets::entityList = temp - client.base;
            LOGF(INFO, "[dumper] entity list re-scanned: offset=0x{:X}", offsets::entityList);
            return true;
        }
    }

    LOGF(WARNING, "[dumper] entity list re-scan failed — all patterns exhausted");
    return false;
}

DWORD64 Dumper::Scan(const offsets::signatures::Signature& sig, ProcessModule module) {
    auto process = Engine::GetProcess();

    if (!process)
        return 0;

    DWORD offsets = 0;
    DWORD64 address = 0;
    std::vector<DWORD64> list;

    if (!module.base || !module.size || !sig.bytes || !sig.bytes[0])
        return 0;

#ifdef _WIN32
    list = ScanMemory(sig.bytes, module.base, module.base + module.size);
#else
    std::vector<uint8_t> signature;
    for (const auto byte : StrSigToArray(sig.bytes))
        signature.push_back(byte == 256 ? 0 : static_cast<uint8_t>(byte));

    const auto match = process->FindSignature(module, std::move(signature));
    if (match)
        list.push_back(match);
#endif

    if (!list.size())
        return 0;

    // The disp32 may sit `pre_sub` bytes before the matched block (a2x "sub"
    // operation); the RIP target is then match - pre_sub + disp + instr_len.
    const DWORD64 base = list.at(0) - sig.pre_sub;

    if (!process->read_raw(base + sig.disp_offset, &offsets, sizeof(DWORD)))
        return 0;

    address = base + offsets + sig.instr_len;
    return address;
}

std::vector<WORD> Dumper::StrSigToArray(const std::string& sig) {
    std::istringstream iss(sig);
    std::vector<WORD> bytes;
    std::string byte_str;

    while (iss >> byte_str) {
        if (byte_str == "??" || byte_str == "?")
            bytes.push_back(256);
        else
            bytes.push_back(static_cast<WORD>(std::stoul(byte_str, nullptr, 16)));
    }
    return bytes;
}

void Dumper::GetNextArray(std::vector<short>& next, const std::vector<WORD>& signature)
{
    auto size = signature.size();
    for (int i = 0; i < size; i++)
        next[signature[i]] = i;
}

void Dumper::ScanBlock(byte* buffer, const std::vector<short>& next, const std::vector<WORD>& signature, DWORD64 start, DWORD size, std::vector<DWORD64>& result)
{
    auto process = Engine::GetProcess();

    if (!process->read_raw(start, buffer, size))
        return;

    int length = signature.size();

    for (int i = 0, j, k; i < size;)
    {
        j = i; k = 0;

        for (; k < length && j < size && (signature[k] == buffer[j] || signature[k] == 256); k++, j++);

        if (k == length)
            result.push_back(start + i);

        if ((i + length) >= size)
            return;

        int Num = next[buffer[i + length]];
        if (Num == -1)
            i += (length - next[256]);
        else
            i += (length - Num);
    }
}

std::vector<DWORD64> Dumper::ScanMemory(const std::string& sig, DWORD64 start, DWORD64 end, int number)
{
    std::vector<DWORD64> result;
    std::vector<short> next(260, -1);

    auto process = Engine::GetProcess();

    if (!process)
        return result;

    std::unique_ptr<byte[]> buffer = std::make_unique<byte[]>(MAX_BLOCK_SIZE);

    auto signature = StrSigToArray(sig);
    if (!signature.size())
        return result;

    GetNextArray(next, signature);

#ifdef _WIN32
	MEMORY_BASIC_INFORMATION mbi;
	while (VirtualQueryEx(process->handle_, reinterpret_cast<LPCVOID>(start), &mbi, sizeof(mbi)) != 0)
    {
        int searches = 0;
        auto size = mbi.RegionSize;

        while (size >= MAX_BLOCK_SIZE)
        {
            if (result.size() >= number) {
                return result;
	        }
            ScanBlock(buffer.get(), next, signature, start + (MAX_BLOCK_SIZE * searches), MAX_BLOCK_SIZE, result);

            size -= MAX_BLOCK_SIZE;
            searches++;
        }

        ScanBlock(buffer.get(), next, signature, start + (MAX_BLOCK_SIZE * searches), size, result);

        start += mbi.RegionSize;

        if (result.size() >= number || end != 0 && start > end)
            break;
    }
#else
	while (start < end)
	{
		const auto size = static_cast<DWORD>(std::min<DWORD64>(MAX_BLOCK_SIZE, end - start));
		ScanBlock(buffer.get(), next, signature, start, size, result);
		if (result.size() >= static_cast<size_t>(number)) break;
		start += size;
	}
#endif

	return result;
}

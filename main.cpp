#include <iostream>
// #include <vector>
#include <chrono>
#include <string>
#include <cctype>
#include <map>
#include <regex>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <capstone/capstone.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <curl/curl.h>

using namespace std;

// globals
uint8_t* g_Data = nullptr;
size_t g_Size = 0;
uint64_t g_TextVmAddr = 0, g_TextFileOff = 0, g_TextSize = 0;

// functions
size_t writeCB(void* ptr, size_t size, size_t nmemb, string* data) {
    data->append((char*)ptr, size * nmemb);
    return size * nmemb;
}

string GetClientVersion() { // asumes latest version, doesnt get the binary version, could probably fix that but this was easier
    auto* curl = curl_easy_init();
    string resp, ver = "waddo";

    if (!curl) return ver;
    curl_easy_setopt(curl, CURLOPT_URL, "https://clientsettingscdn.roblox.com/v2/client-version/MacPlayer");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCB);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "david/baszucki"); // unpatchable
    
    if (curl_easy_perform(curl) == CURLE_OK) {
        auto pos = resp.find("\"clientVersionUpload\"");
        if (pos != string::npos) {
            auto start = resp.find("version-", pos);
            if (start != string::npos) {
                auto end = resp.find("\"", start);
                if (end != string::npos) ver = resp.substr(start, end - start);
            }
        }
    }
    curl_easy_cleanup(curl);
    return ver;
}

const char* PtrFromAddr(uint64_t addr) {
    return (const char*)(g_Data + (addr - g_TextVmAddr + g_TextFileOff));
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Failed >> usage is %s <roblox_binary_path>\n", argv[0]);
        return 1;
    }

    auto start = chrono::high_resolution_clock::now(); // timer

    string version = GetClientVersion();

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) return 1;
    struct stat st;
    fstat(fd, &st);
    g_Size = st.st_size;
    g_Data = (uint8_t*)mmap(nullptr, g_Size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    // finds the __TEXT segment of roblox
    auto* header = (mach_header_64*)g_Data;
    auto* cmd = (uint8_t*)(header + 1);
    
    for (int i = 0; i < header->ncmds; i++) {
        auto* lc = (load_command*)cmd;
        if (lc->cmd == LC_SEGMENT_64) {
            auto* seg = (segment_command_64*)lc;
            if (strcmp(seg->segname, "__TEXT") == 0) {
                g_TextVmAddr = seg->vmaddr;
                g_TextFileOff = seg->fileoff;
                g_TextSize = seg->filesize;
                break;
            }
        }
        cmd += lc->cmdsize;
    }

    csh cs;
    cs_open(CS_ARCH_X86, CS_MODE_64, &cs);
    cs_option(cs, CS_OPT_DETAIL, CS_OPT_ON);

    const char* fflagString = "PktDropStatsReportThreshold"; // random FFlag string to base the search off
    uint64_t anchorAddr = 0; // fflagstring address
    
    void* found = memmem(g_Data, g_Size, fflagString, strlen(fflagString));
    if (!found) {
        printf("Failed >> Couldnt find fflag string.\n");
        return 1;
    }
    
    uint64_t foundOffset = (uint8_t*)found - g_Data;

    // get vm address for this string
    cmd = (uint8_t*)(header + 1);
    for (int i = 0; i < header->ncmds; i++, cmd += ((load_command*)cmd)->cmdsize) {
        auto* seg = (segment_command_64*)cmd;
        
        if (seg->cmd == LC_SEGMENT_64 && foundOffset >= seg->fileoff && foundOffset < (seg->fileoff + seg->filesize)) {
            anchorAddr = seg->vmaddr + (foundOffset - seg->fileoff);
            break;
        }
    }

    // get the function that registers all the fflags that we will dump
    // lea reg, [rip + disp]
    uint64_t registerFuncAddr = 0;
    uint8_t* textStart = g_Data + g_TextFileOff;
    
    for (size_t i = 0; i < g_TextSize - 7; i++) {
        // lea instruction pattern: 0x48 0x8D or 0x4C 0x8D
        if ((textStart[i] != 0x48 && textStart[i] != 0x4C) || textStart[i+1] != 0x8D) continue;

        uint32_t disp = *(uint32_t*)(textStart + i + 3);
        uint64_t rip = g_TextVmAddr + i + 7;
        
        if (rip + (int32_t)disp != anchorAddr) continue;

        cs_insn* insn;
        size_t count = cs_disasm(cs, textStart + i, 64, g_TextVmAddr + i, 0, &insn); // dissasembles a little bit foward from the lea to find a call or jmp following it
        for (size_t j = 0; j < count; j++) {
            if (insn[j].id == X86_INS_JMP || insn[j].id == X86_INS_CALL) {
                if (insn[j].detail->x86.operands[0].type == X86_OP_IMM) {
                    registerFuncAddr = insn[j].detail->x86.operands[0].imm;
                    break;
                }
            }
        }
        cs_free(insn, count);
        if (registerFuncAddr) break;
    }

    if (!registerFuncAddr) {
        printf("Failed >> Could not find register function.\n");
        return 1;
    }

    // scan for direct call/jmp references to the register function
    map<string, uint64_t> results;
    
    //  scan 1 byte at a time for E8/E9 (call/jmp)
    for (size_t i = 0; i < g_TextSize - 5; i++) {
        if (textStart[i] != 0xE8 && textStart[i] != 0xE9) continue;

        int32_t disp = *(int32_t*)(textStart + i + 1);
        uint64_t target = (g_TextVmAddr + i) + 5 + disp;

        if (target != registerFuncAddr) continue;
        
        // registerfflag("PktDropStatsReportThreshold", &dword_1058BC19C, 2LL); 
        // we check the RDI (first argument) for the name string and then RSI (second argument) for the fflag address

        uint64_t currentAddr = g_TextVmAddr + i;
        uint64_t lookBackAddr = (currentAddr > 40) ? currentAddr - 40 : g_TextVmAddr;
        
        cs_insn* insn;
        size_t count = cs_disasm(cs, textStart + (lookBackAddr - g_TextVmAddr), 
                                    currentAddr - lookBackAddr, lookBackAddr, 0, &insn);
        
        uint64_t nameAddr = 0, varAddr = 0;

        if (count > 0) {
            for (size_t j = 0; j < count; j++) {
                if (insn[j].id == X86_INS_LEA) {
                    if (insn[j].detail->x86.operands[1].mem.base == X86_REG_RIP) {
                        uint64_t leaTarget = insn[j].address + insn[j].size + 
                                                insn[j].detail->x86.operands[1].mem.disp;
                        
                        x86_reg destReg = insn[j].detail->x86.operands[0].reg;
                        if (destReg == X86_REG_RDI) nameAddr = leaTarget;
                        if (destReg == X86_REG_RSI) varAddr = leaTarget;
                    }
                }
            }
            cs_free(insn, count);
        }

        if (nameAddr && varAddr) {
            const char* namePtr = PtrFromAddr(nameAddr);
            if (namePtr && strlen(namePtr) > 2) {
                results[namePtr] = varAddr;
                // fprintf(stderr, "Found FFlag '%s' nameAddr=0x%llx varAddr=0x%llx\n", namePtr, (unsigned long long)nameAddr, (unsigned long long)varAddr);
            }
        }
    }

    auto end = chrono::high_resolution_clock::now();
    auto ms = chrono::duration_cast<chrono::milliseconds>(end - start).count();

    // we can just use cout because all output is directed to the .hpp file (assuming it was run correctly)
    cout << "// FFlag dumper by waddotron - (" << version << ")" << endl;
    cout << "// FFlags dumped - " << results.size() << endl;
    cout << "// Dumped in - " << ms << "ms" << endl << endl; // if this is more than 1000 ms ill give you one dollar

    for (auto const& [name, addr] : results) {
        cout << "constexpr uintptr_t " << name << " = 0x" << hex << addr << ";" << endl;
    }

    cs_close(&cs);
    munmap(g_Data, g_Size);
    return 0;
}

#include <bitset>
#include <string>

/**
 * @brief CPU ISA runtime detection
 *
 * Adapted from:
 * https://learn.microsoft.com/fr-fr/cpp/intrinsics/cpuid-cpuidex?view=msvc-170
 *
 */

namespace kb
{

namespace log
{
class Channel;
}

class CPUInfo
{
public:
    // clang-format off
    static inline const std::string& vendor()  { return CPU_.vendor_; }
    static inline const std::string& brand()   { return CPU_.brand_; }

    static inline bool has_ISA_SSE3()          { return CPU_.f_1_ECX_[0]; }
    static inline bool has_ISA_PCLMULQDQ()     { return CPU_.f_1_ECX_[1]; }
    static inline bool has_ISA_MONITOR()       { return CPU_.f_1_ECX_[3]; }
    static inline bool has_ISA_SSSE3()         { return CPU_.f_1_ECX_[9]; }
    static inline bool has_ISA_FMA()           { return CPU_.f_1_ECX_[12]; }
    static inline bool has_ISA_CMPXCHG16B()    { return CPU_.f_1_ECX_[13]; }
    static inline bool has_ISA_SSE4_1()        { return CPU_.f_1_ECX_[19]; }
    static inline bool has_ISA_SSE4_2()        { return CPU_.f_1_ECX_[20]; }
    static inline bool has_ISA_MOVBE()         { return CPU_.f_1_ECX_[22]; }
    static inline bool has_ISA_POPCNT()        { return CPU_.f_1_ECX_[23]; }
    static inline bool has_ISA_AES()           { return CPU_.f_1_ECX_[25]; }
    static inline bool has_ISA_XSAVE()         { return CPU_.f_1_ECX_[26]; }
    static inline bool has_ISA_OSXSAVE()       { return CPU_.f_1_ECX_[27]; }
    static inline bool has_ISA_AVX()           { return CPU_.f_1_ECX_[28]; }
    static inline bool has_ISA_F16C()          { return CPU_.f_1_ECX_[29]; }
    static inline bool has_ISA_RDRAND()        { return CPU_.f_1_ECX_[30]; }
    static inline bool has_ISA_MSR()           { return CPU_.f_1_EDX_[5]; }
    static inline bool has_ISA_CX8()           { return CPU_.f_1_EDX_[8]; }
    static inline bool has_ISA_SEP()           { return CPU_.f_1_EDX_[11]; }
    static inline bool has_ISA_CMOV()          { return CPU_.f_1_EDX_[15]; }
    static inline bool has_ISA_CLFSH()         { return CPU_.f_1_EDX_[19]; }
    static inline bool has_ISA_MMX()           { return CPU_.f_1_EDX_[23]; }
    static inline bool has_ISA_FXSR()          { return CPU_.f_1_EDX_[24]; }
    static inline bool has_ISA_SSE()           { return CPU_.f_1_EDX_[25]; }
    static inline bool has_ISA_SSE2()          { return CPU_.f_1_EDX_[26]; }
    static inline bool has_ISA_FSGSBASE()      { return CPU_.f_7_EBX_[0]; }
    static inline bool has_ISA_BMI1()          { return CPU_.f_7_EBX_[3]; }
    static inline bool has_ISA_HLE()           { return CPU_.is_intel_ && CPU_.f_7_EBX_[4]; }
    static inline bool has_ISA_AVX2()          { return CPU_.f_7_EBX_[5]; }
    static inline bool has_ISA_BMI2()          { return CPU_.f_7_EBX_[8]; }
    static inline bool has_ISA_ERMS()          { return CPU_.f_7_EBX_[9]; }
    static inline bool has_ISA_INVPCID()       { return CPU_.f_7_EBX_[10]; }
    static inline bool has_ISA_RTM()           { return CPU_.is_intel_ && CPU_.f_7_EBX_[11]; }
    static inline bool has_ISA_AVX512F()       { return CPU_.f_7_EBX_[16]; }
    static inline bool has_ISA_RDSEED()        { return CPU_.f_7_EBX_[18]; }
    static inline bool has_ISA_ADX()           { return CPU_.f_7_EBX_[19]; }
    static inline bool has_ISA_AVX512PF()      { return CPU_.f_7_EBX_[26]; }
    static inline bool has_ISA_AVX512ER()      { return CPU_.f_7_EBX_[27]; }
    static inline bool has_ISA_AVX512CD()      { return CPU_.f_7_EBX_[28]; }
    static inline bool has_ISA_SHA()           { return CPU_.f_7_EBX_[29]; }
    static inline bool has_ISA_PREFETCHWT1()   { return CPU_.f_7_ECX_[0]; }
    static inline bool has_ISA_LAHF()          { return CPU_.f_81_ECX_[0]; }
    static inline bool has_ISA_LZCNT()         { return CPU_.is_intel_ && CPU_.f_81_ECX_[5]; }
    static inline bool has_ISA_ABM()           { return CPU_.is_AMD_ && CPU_.f_81_ECX_[5]; }
    static inline bool has_ISA_SSE4a()         { return CPU_.is_AMD_ && CPU_.f_81_ECX_[6]; }
    static inline bool has_ISA_XOP()           { return CPU_.is_AMD_ && CPU_.f_81_ECX_[11]; }
    static inline bool has_ISA_TBM()           { return CPU_.is_AMD_ && CPU_.f_81_ECX_[21]; }
    static inline bool has_ISA_SYSCALL()       { return CPU_.is_intel_ && CPU_.f_81_EDX_[11]; }
    static inline bool has_ISA_MMXEXT()        { return CPU_.is_AMD_ && CPU_.f_81_EDX_[22]; }
    static inline bool has_ISA_RDTSCP()        { return CPU_.is_intel_ && CPU_.f_81_EDX_[27]; }
    static inline bool has_ISA_3DNOWEXT()      { return CPU_.is_AMD_ && CPU_.f_81_EDX_[30]; }
    static inline bool has_ISA_3DNOW()         { return CPU_.is_AMD_ && CPU_.f_81_EDX_[31]; }
    // clang-format on

    static void dump(const log::Channel& chan);

private:
    class Internal
    {
    public:
        Internal() noexcept;

        std::string vendor_;
        std::string brand_;
        bool is_intel_;
        bool is_AMD_;
        std::bitset<32> f_1_ECX_;
        std::bitset<32> f_1_EDX_;
        std::bitset<32> f_7_EBX_;
        std::bitset<32> f_7_ECX_;
        std::bitset<32> f_81_ECX_;
        std::bitset<32> f_81_EDX_;
    };

    static const Internal CPU_;
};

} // namespace kb
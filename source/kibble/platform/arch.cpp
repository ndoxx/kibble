#include "kibble/platform/arch.h"
#include "kibble/logger/logger.h"
#include "kibble/platform/platform.h"

#include "fmt/color.h"
#include <array>
#include <cstring>
#include <vector>

#if defined(K_COMPILER_MSVC)
#include <intrin.h>
#else
#include <cpuid.h>
#endif

namespace kb
{

const CPUInfo::Internal CPUInfo::CPU_;

inline void cpuidex(int cpu_info[4], int function_id, int subfunction_id)
{
#if defined(K_COMPILER_MSVC)
    __cpuidex(cpu_info, function_id, subfunction_id);
#else
    __cpuid_count(function_id, subfunction_id, cpu_info[0], cpu_info[1], cpu_info[2], cpu_info[3]);
#endif
}

// Simple CPUID without subfunction
inline void cpuid(int cpu_info[4], int function_id)
{
#if defined(K_COMPILER_MSVC)
    __cpuid(cpu_info, function_id);
#else
    __cpuid(function_id, cpu_info[0], cpu_info[1], cpu_info[2], cpu_info[3]);
#endif
}

CPUInfo::Internal::Internal()
    : is_intel_{false}, is_AMD_{false}, f_1_ECX_{0}, f_1_EDX_{0}, f_7_EBX_{0}, f_7_ECX_{0}, f_81_ECX_{0}, f_81_EDX_{0}
{
    std::array<int, 4> cpui;
    std::vector<std::array<int, 4>> data;
    std::vector<std::array<int, 4>> extdata;

    // Get highest function ID
    cpuid(cpui.data(), 0);
    unsigned int n_ids = static_cast<unsigned int>(cpui[0]);

    for (unsigned int ii = 0; ii <= n_ids; ++ii)
    {
        cpuidex(cpui.data(), int(ii), 0);
        data.push_back(cpui);
    }

    // Capture vendor string
    char vendor[0x20];
    std::memset(vendor, 0, sizeof(vendor));
    *reinterpret_cast<int*>(vendor) = data[0][1];
    *reinterpret_cast<int*>(vendor + 4) = data[0][3];
    *reinterpret_cast<int*>(vendor + 8) = data[0][2];
    vendor_ = vendor;
    if (vendor_ == "GenuineIntel")
    {
        is_intel_ = true;
    }
    else if (vendor_ == "AuthenticAMD")
    {
        is_AMD_ = true;
    }

    // Load feature flags
    if (n_ids >= 1)
    {
        f_1_ECX_ = static_cast<unsigned long long>(data[1][2]);
        f_1_EDX_ = static_cast<unsigned long long>(data[1][3]);
    }
    if (n_ids >= 7)
    {
        f_7_EBX_ = static_cast<unsigned long long>(data[7][1]);
        f_7_ECX_ = static_cast<unsigned long long>(data[7][2]);
    }

    // Get highest extended function ID
    cpuid(cpui.data(), int32_t(0x80000000L));
    unsigned int n_ex_ids = static_cast<unsigned int>(cpui[0]);

    char brand[0x40];
    std::memset(brand, 0, sizeof(brand));

    for (unsigned int ii = 0x80000000; ii <= n_ex_ids; ++ii)
    {
        cpuidex(cpui.data(), int(ii), 0);
        extdata.push_back(cpui);
    }

    // Load extended feature flags
    if (n_ex_ids >= 0x80000001)
    {
        f_81_ECX_ = static_cast<unsigned long long>(extdata[1][2]);
        f_81_EDX_ = static_cast<unsigned long long>(extdata[1][3]);
    }

    // Interpret CPU brand string if reported
    if (n_ex_ids >= 0x80000004)
    {
        std::memcpy(brand, extdata[2].data(), sizeof(cpui));
        std::memcpy(brand + 16, extdata[3].data(), sizeof(cpui));
        std::memcpy(brand + 32, extdata[4].data(), sizeof(cpui));
        brand_ = brand;
    }
}

void CPUInfo::dump(const log::Channel& chan)
{
    auto support_message = [&chan](const std::string& isa_feature, bool is_supported) {
        if (is_supported)
        {
            klog(chan).info("{} {}", isa_feature, fmt::styled("yes", fmt::fg(fmt::color::green)));
        }
        else
        {
            klog(chan).info("{} {}", isa_feature, fmt::styled("no", fmt::fg(fmt::color::red)));
        }
    };

    klog(chan).info("CPU vendor: {}", CPUInfo::vendor());
    klog(chan).info("CPU brand:  {}", CPUInfo::brand());

    support_message("3DNOW      ", CPUInfo::has_ISA_3DNOW());
    support_message("3DNOWEXT   ", CPUInfo::has_ISA_3DNOWEXT());
    support_message("ABM        ", CPUInfo::has_ISA_ABM());
    support_message("ADX        ", CPUInfo::has_ISA_ADX());
    support_message("AES        ", CPUInfo::has_ISA_AES());
    support_message("AVX        ", CPUInfo::has_ISA_AVX());
    support_message("AVX2       ", CPUInfo::has_ISA_AVX2());
    support_message("AVX512CD   ", CPUInfo::has_ISA_AVX512CD());
    support_message("AVX512ER   ", CPUInfo::has_ISA_AVX512ER());
    support_message("AVX512F    ", CPUInfo::has_ISA_AVX512F());
    support_message("AVX512PF   ", CPUInfo::has_ISA_AVX512PF());
    support_message("BMI1       ", CPUInfo::has_ISA_BMI1());
    support_message("BMI2       ", CPUInfo::has_ISA_BMI2());
    support_message("CLFSH      ", CPUInfo::has_ISA_CLFSH());
    support_message("CMPXCHG16B ", CPUInfo::has_ISA_CMPXCHG16B());
    support_message("CX8        ", CPUInfo::has_ISA_CX8());
    support_message("ERMS       ", CPUInfo::has_ISA_ERMS());
    support_message("F16C       ", CPUInfo::has_ISA_F16C());
    support_message("FMA        ", CPUInfo::has_ISA_FMA());
    support_message("FSGSBASE   ", CPUInfo::has_ISA_FSGSBASE());
    support_message("FXSR       ", CPUInfo::has_ISA_FXSR());
    support_message("HLE        ", CPUInfo::has_ISA_HLE());
    support_message("INVPCID    ", CPUInfo::has_ISA_INVPCID());
    support_message("LAHF       ", CPUInfo::has_ISA_LAHF());
    support_message("LZCNT      ", CPUInfo::has_ISA_LZCNT());
    support_message("MMX        ", CPUInfo::has_ISA_MMX());
    support_message("MMXEXT     ", CPUInfo::has_ISA_MMXEXT());
    support_message("MONITOR    ", CPUInfo::has_ISA_MONITOR());
    support_message("MOVBE      ", CPUInfo::has_ISA_MOVBE());
    support_message("MSR        ", CPUInfo::has_ISA_MSR());
    support_message("OSXSAVE    ", CPUInfo::has_ISA_OSXSAVE());
    support_message("PCLMULQDQ  ", CPUInfo::has_ISA_PCLMULQDQ());
    support_message("POPCNT     ", CPUInfo::has_ISA_POPCNT());
    support_message("PREFETCHWT1", CPUInfo::has_ISA_PREFETCHWT1());
    support_message("RDRAND     ", CPUInfo::has_ISA_RDRAND());
    support_message("RDSEED     ", CPUInfo::has_ISA_RDSEED());
    support_message("RDTSCP     ", CPUInfo::has_ISA_RDTSCP());
    support_message("RTM        ", CPUInfo::has_ISA_RTM());
    support_message("SEP        ", CPUInfo::has_ISA_SEP());
    support_message("SHA        ", CPUInfo::has_ISA_SHA());
    support_message("SSE        ", CPUInfo::has_ISA_SSE());
    support_message("SSE2       ", CPUInfo::has_ISA_SSE2());
    support_message("SSE3       ", CPUInfo::has_ISA_SSE3());
    support_message("SSE4.1     ", CPUInfo::has_ISA_SSE4_1());
    support_message("SSE4.2     ", CPUInfo::has_ISA_SSE4_2());
    support_message("SSE4a      ", CPUInfo::has_ISA_SSE4a());
    support_message("SSSE3      ", CPUInfo::has_ISA_SSSE3());
    support_message("SYSCALL    ", CPUInfo::has_ISA_SYSCALL());
    support_message("TBM        ", CPUInfo::has_ISA_TBM());
    support_message("XOP        ", CPUInfo::has_ISA_XOP());
    support_message("XSAVE      ", CPUInfo::has_ISA_XSAVE());
}

} // namespace kb
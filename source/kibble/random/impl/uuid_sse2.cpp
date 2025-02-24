#include <array>
#include <cstddef>
#include <cstdint>
#include <emmintrin.h>
#include <immintrin.h>
#include <smmintrin.h>

namespace kb::UUIDv4::impl::sse2
{

// Helper macros, these are not defined in SSE2
#define mm_cmpge_epi8__(a, b) _mm_or_si128(_mm_cmpgt_epi8(a, b), _mm_cmpeq_epi8(a, b))
#define mm_cmple_epi8__(a, b) _mm_or_si128(_mm_cmplt_epi8(a, b), _mm_cmpeq_epi8(a, b))

/**
 * @brief Converts a 128-bits unsigned int to an UUIDv4 string representation.
 * Uses SIMD via Intel's SSE2 instruction set.
 *
 * @param x The 128-bit value to convert
 * @param mem Pointer to output character buffer
 */
void m128itos(__m128i x, char* mem)
{
    // Lookup tables for faster hex conversion (store in memory for SSE2 compatibility)
    alignas(16) constexpr char hex_digits[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                                 '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

    // Extract bytes from the 128-bit value
    alignas(16) std::array<uint8_t, 16> bytes;
    _mm_store_si128(reinterpret_cast<__m128i*>(bytes.data()), x);

    // Process each byte individually since we need to add dashes in between
    int pos = 0;
    for (size_t ii = 0; ii < 16; ++ii)
    {
        // Add dashes according to UUID format
        if (ii == 4 || ii == 6 || ii == 8 || ii == 10)
        {
            mem[pos++] = '-';
        }

        mem[pos++] = hex_digits[(bytes[ii] >> 4) & 0xF];
        mem[pos++] = hex_digits[bytes[ii] & 0xF];
    }

    // Null terminate
    mem[pos] = '\0';
}

/**
 * @brief Converts an UUIDv4 string representation to a 128-bits unsigned int.
 * Uses SIMD via Intel's SSE2 instruction set.
 *
 * @param mem Pointer to input character buffer
 * @return __m128i The 128-bit value
 */
__m128i stom128i(const char* mem)
{
    alignas(16) char hex_only[32] = {0};
    alignas(16) std::array<uint8_t, 16> bytes = {0};

    // First, extract hex characters while ignoring dashes
    int dst = 0;
    for (int src = 0; src < 36; src++)
    {
        if (mem[src] != '-')
        {
            hex_only[dst++] = mem[src];
        }
    }

    /*
        NOTE(ndx): Benchmarks show that there is a performance gain but in Release only.
        In Debug, a scalar implementation is faster than the SIMD version. Could be because
        of runtime checks for aligned memory access which we do a lot, bounds checking...
        Because I somewhat care about performances in Debug too, I'm keeping a scalar
        implementation in Debug.
    */

#ifdef K_DEBUG
    // Process hex pairs directly (scalar)
    for (size_t ii = 0; ii < 16; ii++)
    {
        char high = hex_only[ii * 2];
        char low = hex_only[ii * 2 + 1];

        uint8_t high_nibble = (high <= '9')   ? uint8_t(high - '0')
                              : (high <= 'F') ? uint8_t(high - 'A' + 10)
                                              : uint8_t(high - 'a' + 10);
        uint8_t low_nibble = (low <= '9')   ? uint8_t(low - '0')
                             : (low <= 'F') ? uint8_t(low - 'A' + 10)
                                            : uint8_t(low - 'a' + 10);

        bytes[ii] = uint8_t((high_nibble << 4) | low_nibble);
    }
#else
    // ASCII values for comparison
    const __m128i ascii_zero = _mm_set1_epi8('0');
    const __m128i ascii_nine = _mm_set1_epi8('9');
    const __m128i ascii_a = _mm_set1_epi8('a');
    const __m128i ascii_f = _mm_set1_epi8('f');
    const __m128i ascii_A = _mm_set1_epi8('A');
    const __m128i ascii_F = _mm_set1_epi8('F');

    // Process 8 bytes (16 hex chars) at a time
    alignas(16) uint8_t nibbles[16];
    for (int ii = 0; ii < 2; ii++)
    {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(hex_only + ii * 16));

        // Check character ranges and convert
        __m128i is_digit = _mm_and_si128(mm_cmpge_epi8__(chunk, ascii_zero), mm_cmple_epi8__(chunk, ascii_nine));
        __m128i val_digit = _mm_sub_epi8(chunk, ascii_zero);

        __m128i is_lower = _mm_and_si128(mm_cmpge_epi8__(chunk, ascii_a), mm_cmple_epi8__(chunk, ascii_f));
        __m128i val_lower = _mm_add_epi8(_mm_sub_epi8(chunk, ascii_a), _mm_set1_epi8(10));

        __m128i is_upper = _mm_and_si128(mm_cmpge_epi8__(chunk, ascii_A), mm_cmple_epi8__(chunk, ascii_F));
        __m128i val_upper = _mm_add_epi8(_mm_sub_epi8(chunk, ascii_A), _mm_set1_epi8(10));

        // Combine results based on which range the character falls in
        __m128i values =
            _mm_or_si128(_mm_and_si128(is_digit, val_digit),
                         _mm_or_si128(_mm_and_si128(is_lower, val_lower), _mm_and_si128(is_upper, val_upper)));

        // Store the values to process pairs of hex characters
        _mm_store_si128(reinterpret_cast<__m128i*>(nibbles), values);

        // Combine pairs of nibbles to form bytes
        for (int jj = 0; jj < 8; jj++)
        {
            bytes[size_t(ii * 8 + jj)] = uint8_t((nibbles[jj * 2] << 4) | nibbles[jj * 2 + 1]);
        }
    }
#endif

    // Load the processed bytes into the result
    return _mm_load_si128(reinterpret_cast<__m128i*>(bytes.data()));
}

} // namespace kb::UUIDv4::impl::sse2
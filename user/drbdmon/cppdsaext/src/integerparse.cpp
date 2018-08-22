#include <climits>
#include <integerparse.h>
#include <integerparse_impl.h>
#include <dsaext.h>

namespace dsaext
{
    // Decimal number parsers -- std::string
    //

    // @throws NumberFormatException
    uint64_t parse_unsigned_int64(const std::string& text)
    {
        return parse_unsigned_decimal_impl<uint64_t>(text.c_str(), text.length(), UINT64_MAX);
    }

    // @throws NumberFormatException
    uint32_t parse_unsigned_int32(const std::string& text)
    {
        return parse_unsigned_decimal_impl<uint32_t>(text.c_str(), text.length(), UINT32_MAX);
    }

    // @throws NumberFormatException
    uint16_t parse_unsigned_int16(const std::string& text)
    {
        return parse_unsigned_decimal_impl<uint16_t>(text.c_str(), text.length(), UINT16_MAX);
    }

    // @throws NumberFormatException
    uint8_t parse_unsigned_int8(const std::string& text)
    {
        return parse_unsigned_decimal_impl<uint8_t>(text.c_str(), text.length(), UINT8_MAX);
    }

    // @throws NumberFormatException
    unsigned long long parse_unsigned_long_long(const std::string& text)
    {
        return parse_unsigned_decimal_impl<unsigned long long>(text.c_str(), text.length(), ULLONG_MAX);
    }

    // @throws NumberFormatException
    unsigned long parse_unsigned_long(const std::string& text)
    {
        return parse_unsigned_decimal_impl<unsigned long>(text.c_str(), text.length(), ULONG_MAX);
    }

    // @throws NumberFormatException
    unsigned int parse_unsigned_int(const std::string& text)
    {
        return parse_unsigned_decimal_impl<unsigned int>(text.c_str(), text.length(), UINT_MAX);
    }

    // @throws NumberFormatException
    unsigned short parse_unsigned_short(const std::string& text)
    {
        return parse_unsigned_decimal_impl<unsigned short>(text.c_str(), text.length(), USHRT_MAX);
    }

    // @throws NumberFormatException
    unsigned char parse_unsigned_char(const std::string& text)
    {
        return parse_unsigned_decimal_impl<unsigned char>(text.c_str(), text.length(), UCHAR_MAX);
    }

    // @throws NumberFormatException
    int64_t parse_signed_int64(const std::string& text)
    {
        return parse_signed_decimal_impl<int64_t>(text.c_str(), text.length(), INT64_MIN, INT64_MAX);
    }

    // @throws NumberFormatException
    int32_t parse_signed_int32(const std::string& text)
    {
        return parse_signed_decimal_impl<int32_t>(text.c_str(), text.length(), INT32_MIN, INT32_MAX);
    }

    // @throws NumberFormatException
    int16_t parse_signed_int16(const std::string& text)
    {
        return parse_signed_decimal_impl<int16_t>(text.c_str(), text.length(), INT16_MIN, INT16_MAX);
    }

    // @throws NumberFormatException
    int8_t parse_signed_int8(const std::string& text)
    {
        return parse_signed_decimal_impl<int8_t>(text.c_str(), text.length(), INT8_MIN, INT8_MAX);
    }

    // @throws NumberFormatException
    signed long long parse_signed_long_long(const std::string& text)
    {
        return parse_signed_decimal_impl<signed long long>(text.c_str(), text.length(), LLONG_MIN, LLONG_MAX);
    }

    // @throws NumberFormatException
    signed long parse_signed_long(const std::string& text)
    {
        return parse_signed_decimal_impl<signed long>(text.c_str(), text.length(), LONG_MIN, LONG_MAX);
    }

    // @throws NumberFormatException
    signed int parse_signed_int(const std::string& text)
    {
        return parse_signed_decimal_impl<signed int>(text.c_str(), text.length(), INT_MIN, INT_MAX);
    }

    // @throws NumberFormatException
    signed short parse_signed_short(const std::string& text)
    {
        return parse_signed_decimal_impl<signed short>(text.c_str(), text.length(), SHRT_MIN, SHRT_MAX);
    }

    // @throws NumberFormatException
    signed char parse_signed_char(const std::string& text)
    {
        return parse_signed_decimal_impl<signed char>(text.c_str(), text.length(), SCHAR_MIN, SCHAR_MAX);
    }



    // Binary number parsers -- std::string
    //

    // @throws NumberFormatException
    uint64_t parse_binary_64(const std::string& text)
    {
        return parse_binary_impl<uint64_t>(text.c_str(), text.length(), UINT64_MAX);
    }

    // @throws NumberFormatException
    uint32_t parse_binary_32(const std::string& text)
    {
        return parse_binary_impl<uint32_t>(text.c_str(), text.length(), UINT32_MAX);
    }

    // @throws NumberFormatException
    uint16_t parse_binary_16(const std::string& text)
    {
        return parse_binary_impl<uint16_t>(text.c_str(), text.length(), UINT16_MAX);
    }

    // @throws NumberFormatException
    uint8_t parse_binary_8(const std::string& text)
    {
        return parse_binary_impl<uint8_t>(text.c_str(), text.length(), UINT8_MAX);
    }

    // @throws NumberFormatException
    unsigned long long parse_binary_unsigned_long_long(const std::string& text)
    {
        return parse_binary_impl<unsigned long long>(text.c_str(), text.length(), ULLONG_MAX);
    }

    // @throws NumberFormatException
    unsigned long parse_binary_unsigned_long(const std::string& text)
    {
        return parse_binary_impl<unsigned long>(text.c_str(), text.length(), ULONG_MAX);
    }

    // @throws NumberFormatException
    unsigned int parse_binary_unsigned_int(const std::string& text)
    {
        return parse_binary_impl<unsigned int>(text.c_str(), text.length(), UINT_MAX);
    }

    // @throws NumberFormatException
    unsigned short parse_binary_unsigned_short(const std::string& text)
    {
        return parse_binary_impl<unsigned short>(text.c_str(), text.length(), USHRT_MAX);
    }

    // @throws NumberFormatException
    unsigned char parse_binary_unsigned_char(const std::string& text)
    {
        return parse_binary_impl<unsigned char>(text.c_str(), text.length(), UCHAR_MAX);
    }



    // Octal number parsers -- std::string
    //

    // @throws NumberFormatException
    uint64_t parse_octal_64(const std::string& text)
    {
        return parse_octal_impl<uint64_t>(text.c_str(), text.length(), UINT64_MAX);
    }

    // @throws NumberFormatException
    uint32_t parse_octal_32(const std::string& text)
    {
        return parse_octal_impl<uint32_t>(text.c_str(), text.length(), UINT32_MAX);
    }

    // @throws NumberFormatException
    uint16_t parse_octal_16(const std::string& text)
    {
        return parse_octal_impl<uint16_t>(text.c_str(), text.length(), UINT16_MAX);
    }

    // @throws NumberFormatException
    uint8_t parse_octal_8(const std::string& text)
    {
        return parse_octal_impl<uint8_t>(text.c_str(), text.length(), UINT8_MAX);
    }

    // @throws NumberFormatException
    unsigned long long parse_octal_unsigned_long_long(const std::string& text)
    {
        return parse_octal_impl<unsigned long long>(text.c_str(), text.length(), ULLONG_MAX);
    }

    // @throws NumberFormatException
    unsigned long parse_octal_unsigned_long(const std::string& text)
    {
        return parse_octal_impl<unsigned long>(text.c_str(), text.length(), ULONG_MAX);
    }

    // @throws NumberFormatException
    unsigned int parse_octal_unsigned_int(const std::string& text)
    {
        return parse_octal_impl<unsigned int>(text.c_str(), text.length(), UINT_MAX);
    }

    // @throws NumberFormatException
    unsigned short parse_octal_unsigned_short(const std::string& text)
    {
        return parse_octal_impl<unsigned short>(text.c_str(), text.length(), USHRT_MAX);
    }

    // @throws NumberFormatException
    unsigned char parse_octal_unsigned_char(const std::string& text)
    {
        return parse_octal_impl<unsigned char>(text.c_str(), text.length(), UCHAR_MAX);
    }



    // Hexadecimal number parsers -- std::string
    //

    // @throws NumberFormatException
    uint64_t parse_hexadecimal_64(const std::string& text)
    {
        return parse_hexadecimal_impl<uint64_t>(text.c_str(), text.length(), UINT64_MAX);
    }

    // @throws NumberFormatException
    uint32_t parse_hexadecimal_32(const std::string& text)
    {
        return parse_hexadecimal_impl<uint32_t>(text.c_str(), text.length(), UINT32_MAX);
    }

    // @throws NumberFormatException
    uint16_t parse_hexadecimal_16(const std::string& text)
    {
        return parse_hexadecimal_impl<uint16_t>(text.c_str(), text.length(), UINT16_MAX);
    }

    // @throws NumberFormatException
    uint8_t parse_hexadecimal_8(const std::string& text)
    {
        return parse_hexadecimal_impl<uint8_t>(text.c_str(), text.length(), UINT8_MAX);
    }

    // @throws NumberFormatException
    unsigned long long parse_hexadecimal_unsigned_long_long(const std::string& text)
    {
        return parse_hexadecimal_impl<unsigned long long>(text.c_str(), text.length(), ULLONG_MAX);
    }

    // @throws NumberFormatException
    unsigned long parse_hexadecimal_unsigned_long(const std::string& text)
    {
        return parse_hexadecimal_impl<unsigned long>(text.c_str(), text.length(), ULONG_MAX);
    }

    // @throws NumberFormatException
    unsigned int parse_hexadecimal_unsigned_int(const std::string& text)
    {
        return parse_hexadecimal_impl<unsigned int>(text.c_str(), text.length(), UINT_MAX);
    }

    // @throws NumberFormatException
    unsigned short parse_hexadecimal_unsigned_short(const std::string& text)
    {
        return parse_hexadecimal_impl<unsigned short>(text.c_str(), text.length(), USHRT_MAX);
    }

    // @throws NumberFormatException
    unsigned char parse_hexadecimal_unsigned_char(const std::string& text)
    {
        return parse_hexadecimal_impl<unsigned char>(text.c_str(), text.length(), UCHAR_MAX);
    }



    // Decimal number parsers -- character array
    //

    // @throws NumberFormatException
    uint64_t parse_unsigned_int64_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_unsigned_decimal_impl<uint64_t>(text_buffer, text_length, UINT64_MAX);
    }

    // @throws NumberFormatException
    uint32_t parse_unsigned_int32_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_unsigned_decimal_impl<uint32_t>(text_buffer, text_length, UINT32_MAX);
    }

    // @throws NumberFormatException
    uint16_t parse_unsigned_int16_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_unsigned_decimal_impl<uint16_t>(text_buffer, text_length, UINT16_MAX);
    }

    // @throws NumberFormatException
    uint8_t parse_unsigned_int8_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_unsigned_decimal_impl<uint8_t>(text_buffer, text_length, UINT8_MAX);
    }

    // @throws NumberFormatException
    unsigned long long parse_unsigned_long_long_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_unsigned_decimal_impl<unsigned long long>(text_buffer, text_length, ULLONG_MAX);
    }

    // @throws NumberFormatException
    unsigned long parse_unsigned_long_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_unsigned_decimal_impl<unsigned long>(text_buffer, text_length, ULONG_MAX);
    }

    // @throws NumberFormatException
    unsigned int parse_unsigned_int_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_unsigned_decimal_impl<unsigned int>(text_buffer, text_length, UINT_MAX);
    }

    // @throws NumberFormatException
    unsigned short parse_unsigned_short_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_unsigned_decimal_impl<unsigned short>(text_buffer, text_length, USHRT_MAX);
    }

    // @throws NumberFormatException
    unsigned char parse_unsigned_char_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_unsigned_decimal_impl<unsigned char>(text_buffer, text_length, UCHAR_MAX);
    }

    // @throws NumberFormatException
    int64_t parse_signed_int64_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_signed_decimal_impl<int64_t>(text_buffer, text_length, INT64_MIN, INT64_MAX);
    }

    // @throws NumberFormatException
    int32_t parse_signed_int32_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_signed_decimal_impl<int32_t>(text_buffer, text_length, INT32_MIN, INT32_MAX);
    }

    // @throws NumberFormatException
    int16_t parse_signed_int16_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_signed_decimal_impl<int16_t>(text_buffer, text_length, INT16_MIN, INT16_MAX);
    }

    // @throws NumberFormatException
    int8_t parse_signed_int8_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_signed_decimal_impl<int8_t>(text_buffer, text_length, INT8_MIN, INT8_MAX);
    }

    // @throws NumberFormatException
    signed long long parse_signed_long_long_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_signed_decimal_impl<signed long long>(text_buffer, text_length, LLONG_MIN, LLONG_MAX);
    }

    // @throws NumberFormatException
    signed long parse_signed_long_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_signed_decimal_impl<signed long>(text_buffer, text_length, LONG_MIN, LONG_MAX);
    }

    // @throws NumberFormatException
    signed int parse_signed_int_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_signed_decimal_impl<signed int>(text_buffer, text_length, INT_MIN, INT_MAX);
    }

    // @throws NumberFormatException
    signed short parse_signed_short_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_signed_decimal_impl<signed short>(text_buffer, text_length, SHRT_MIN, SHRT_MAX);
    }

    // @throws NumberFormatException
    signed char parse_signed_char_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_signed_decimal_impl<signed char>(text_buffer, text_length, SCHAR_MIN, SCHAR_MAX);
    }



    // Binary number parsers -- character array
    //

    // @throws NumberFormatException
    uint64_t parse_binary_64_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_binary_impl<uint64_t>(text_buffer, text_length, UINT64_MAX);
    }

    // @throws NumberFormatException
    uint32_t parse_binary_32_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_binary_impl<uint32_t>(text_buffer, text_length, UINT32_MAX);
    }

    // @throws NumberFormatException
    uint16_t parse_binary_16_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_binary_impl<uint16_t>(text_buffer, text_length, UINT16_MAX);
    }

    // @throws NumberFormatException
    uint8_t parse_binary_8_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_binary_impl<uint8_t>(text_buffer, text_length, UINT8_MAX);
    }

        // @throws NumberFormatException
    unsigned long long parse_binary_unsigned_long_long_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_binary_impl<unsigned long long>(text_buffer, text_length, ULLONG_MAX);
    }

    // @throws NumberFormatException
    unsigned long parse_binary_unsigned_long_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_binary_impl<unsigned long>(text_buffer, text_length, ULONG_MAX);
    }

    // @throws NumberFormatException
    unsigned int parse_binary_unsigned_int_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_binary_impl<unsigned int>(text_buffer, text_length, UINT_MAX);
    }

    // @throws NumberFormatException
    unsigned short parse_binary_unsigned_short_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_binary_impl<unsigned short>(text_buffer, text_length, USHRT_MAX);
    }

    // @throws NumberFormatException
    unsigned char parse_binary_unsigned_char_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_binary_impl<unsigned char>(text_buffer, text_length, UCHAR_MAX);
    }



    // Octal number parsers -- character array
    //

    // @throws NumberFormatException
    uint64_t parse_octal_64_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_octal_impl<uint64_t>(text_buffer, text_length, UINT64_MAX);
    }

    // @throws NumberFormatException
    uint32_t parse_octal_32_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_octal_impl<uint32_t>(text_buffer, text_length, UINT32_MAX);
    }

    // @throws NumberFormatException
    uint16_t parse_octal_16_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_octal_impl<uint16_t>(text_buffer, text_length, UINT16_MAX);
    }

    // @throws NumberFormatException
    uint8_t parse_octal_8_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_octal_impl<uint8_t>(text_buffer, text_length, UINT8_MAX);
    }

        // @throws NumberFormatException
    unsigned long long parse_octal_unsigned_long_long_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_octal_impl<unsigned long long>(text_buffer, text_length, ULLONG_MAX);
    }

    // @throws NumberFormatException
    unsigned long parse_octal_unsigned_long_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_octal_impl<unsigned long>(text_buffer, text_length, ULONG_MAX);
    }

    // @throws NumberFormatException
    unsigned int parse_octal_unsigned_int_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_octal_impl<unsigned int>(text_buffer, text_length, UINT_MAX);
    }

    // @throws NumberFormatException
    unsigned short parse_octal_unsigned_short_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_octal_impl<unsigned short>(text_buffer, text_length, USHRT_MAX);
    }

    // @throws NumberFormatException
    unsigned char parse_octal_unsigned_char_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_octal_impl<unsigned char>(text_buffer, text_length, UCHAR_MAX);
    }



    // Hexadecimal number parsers -- character array
    //

    // @throws NumberFormatException
    uint64_t parse_hexadecimal_64_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_hexadecimal_impl<uint64_t>(text_buffer, text_length, UINT64_MAX);
    }

    // @throws NumberFormatException
    uint32_t parse_hexadecimal_32_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_hexadecimal_impl<uint32_t>(text_buffer, text_length, UINT32_MAX);
    }

    // @throws NumberFormatException
    uint16_t parse_hexadecimal_16_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_hexadecimal_impl<uint16_t>(text_buffer, text_length, UINT16_MAX);
    }

    // @throws NumberFormatException
    uint8_t parse_hexadecimal_8_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_hexadecimal_impl<uint8_t>(text_buffer, text_length, UINT8_MAX);
    }

        // @throws NumberFormatException
    unsigned long long parse_hexadecimal_unsigned_long_long_c_str(
        const char* const text_buffer,
        const size_t text_length
    )
    {
        return parse_hexadecimal_impl<unsigned long long>(text_buffer, text_length, ULLONG_MAX);
    }

    // @throws NumberFormatException
    unsigned long parse_hexadecimal_unsigned_long_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_hexadecimal_impl<unsigned long>(text_buffer, text_length, ULONG_MAX);
    }

    // @throws NumberFormatException
    unsigned int parse_hexadecimal_unsigned_int_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_hexadecimal_impl<unsigned int>(text_buffer, text_length, UINT_MAX);
    }

    // @throws NumberFormatException
    unsigned short parse_hexadecimal_unsigned_short_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_hexadecimal_impl<unsigned short>(text_buffer, text_length, USHRT_MAX);
    }

    // @throws NumberFormatException
    unsigned char parse_hexadecimal_unsigned_char_c_str(const char* const text_buffer, const size_t text_length)
    {
        return parse_octal_impl<unsigned char>(text_buffer, text_length, UCHAR_MAX);
    }
}

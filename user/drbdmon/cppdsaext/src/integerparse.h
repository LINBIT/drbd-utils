#ifndef INTEGERPARSE_H
#define	INTEGERPARSE_H

#include <cstdlib>
#include <cstdint>
#include <climits>
#include <string>
#include <stdexcept>

namespace dsaext
{
    // Decimal number parsers -- std::string
    //

    // @throws NumberFormatException
    uint64_t parse_unsigned_int64(const std::string& text);

    // @throws NumberFormatException
    uint32_t parse_unsigned_int32(const std::string& text);

    // @throws NumberFormatException
    uint16_t parse_unsigned_int16(const std::string& text);

    // @throws NumberFormatException
    uint8_t parse_unsigned_int8(const std::string& text);

    // @throws NumberFormatException
    unsigned long long parse_unsigned_long_long(const std::string& text);

    // @throws NumberFormatException
    unsigned long parse_unsigned_long(const std::string& text);

    // @throws NumberFormatException
    unsigned int parse_unsigned_int(const std::string& text);

    // @throws NumberFormatException
    unsigned short parse_unsigned_short(const std::string& text);

    // @throws NumberFormatException
    unsigned char parse_unsigned_char(const std::string& text);

    // @throws NumberFormatException
    int64_t parse_signed_int64(const std::string& text);

    // @throws NumberFormatException
    int32_t parse_signed_int32(const std::string& text);

    // @throws NumberFormatException
    int16_t parse_signed_int16(const std::string& text);

    // @throws NumberFormatException
    int8_t parse_signed_int8(const std::string& text);

    // @throws NumberFormatException
    signed long long parse_signed_long_long(const std::string& text);

    // @throws NumberFormatException
    signed long parse_signed_long(const std::string& text);

    // @throws NumberFormatException
    signed int parse_signed_int(const std::string& text);

    // @throws NumberFormatException
    signed short parse_signed_short(const std::string& text);

    // @throws NumberFormatException
    signed char parse_signed_char(const std::string& text);

    // @throws NumberFormatException
    uint64_t parse_unsigned_int64(const std::string& text);

    // @throws NumberFormatException
    uint32_t parse_unsigned_int32(const std::string& text);

    // @throws NumberFormatException
    uint16_t parse_unsigned_int16(const std::string& text);

    // @throws NumberFormatException
    uint8_t parse_unsigned_int8(const std::string& text);



    // Binary number parsers -- std::string
    //

    // @throws NumberFormatException
    uint64_t parse_binary_64(const std::string& text);

    // @throws NumberFormatException
    uint32_t parse_binary_32(const std::string& text);

    // @throws NumberFormatException
    uint16_t parse_binary_16(const std::string& text);

    // @throws NumberFormatException
    uint8_t parse_binary_8(const std::string& text);

    // @throws NumberFormatException
    unsigned long long parse_binary_unsigned_long_long(const std::string& text);

    // @throws NumberFormatException
    unsigned long parse_binary_unsigned_long(const std::string& text);

    // @throws NumberFormatException
    unsigned int parse_binary_unsigned_int(const std::string& text);

    // @throws NumberFormatException
    unsigned short parse_binary_unsigned_short(const std::string& text);

    // @throws NumberFormatException
    unsigned char parse_binary_unsigned_char(const std::string& text);



    // Octal number parsers -- std::string
    //

    // @throws NumberFormatException
    uint64_t parse_octal_64(const std::string& text);

    // @throws NumberFormatException
    uint32_t parse_octal_32(const std::string& text);

    // @throws NumberFormatException
    uint16_t parse_octal_16(const std::string& text);

    // @throws NumberFormatException
    uint8_t parse_octal_8(const std::string& text);

    // @throws NumberFormatException
    unsigned long long parse_octal_unsigned_long_long(const std::string& text);

    // @throws NumberFormatException
    unsigned long parse_octal_unsigned_long(const std::string& text);

    // @throws NumberFormatException
    unsigned int parse_octal_unsigned_int(const std::string& text);

    // @throws NumberFormatException
    unsigned short parse_octal_unsigned_short(const std::string& text);

    // @throws NumberFormatException
    unsigned char parse_octal_unsigned_char(const std::string& text);



    // Hexadecimal number parsers -- std::string
    //

    // @throws NumberFormatException
    uint64_t parse_hexadecimal_64(const std::string& text);

    // @throws NumberFormatException
    uint32_t parse_hexadecimal_32(const std::string& text);

    // @throws NumberFormatException
    uint16_t parse_hexadecimal_16(const std::string& text);

    // @throws NumberFormatException
    uint8_t parse_hexadecimal_8(const std::string& text);

    // @throws NumberFormatException
    unsigned long long parse_hexadecimal_unsigned_long_long(const std::string& text);

    // @throws NumberFormatException
    unsigned long parse_hexadecimal_unsigned_long(const std::string& text);

    // @throws NumberFormatException
    unsigned int parse_hexadecimal_unsigned_int(const std::string& text);

    // @throws NumberFormatException
    unsigned short parse_hexadecimal_unsigned_short(const std::string& text);

    // @throws NumberFormatException
    unsigned char parse_hexadecimal_unsigned_char(const std::string& text);



    // Decimal number parsers -- character array
    //

    // @throws NumberFormatException
    uint64_t parse_unsigned_int64_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    uint32_t parse_unsigned_int32_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    uint16_t parse_unsigned_int16_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    uint8_t parse_unsigned_int8_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    unsigned long long parse_unsigned_long_long_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    unsigned long parse_unsigned_long_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    unsigned int parse_unsigned_int_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    unsigned short parse_unsigned_short_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    unsigned char parse_unsigned_char_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    int64_t parse_signed_int64_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    int32_t parse_signed_int32_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    int16_t parse_signed_int16_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    int8_t parse_signed_int8_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    signed long long parse_signed_long_long_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    signed long parse_signed_long_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    signed int parse_signed_int_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    signed short parse_signed_short_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    signed char parse_signed_char_c_str(const char* text_buffer, size_t text_length);



    // Binary number parsers -- character array
    //

    // @throws NumberFormatException
    uint64_t parse_binary_64_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    uint32_t parse_binary_32_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    uint16_t parse_binary_16_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    uint8_t parse_binary_8_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    unsigned long long parse_binary_unsigned_long_long_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    unsigned long parse_binary_unsigned_long_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    unsigned int parse_binary_unsigned_int_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    unsigned short parse_binary_unsigned_short_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    unsigned char parse_binary_unsigned_char_c_str(const char* text_buffer, size_t text_length);



    // Octal number parsers -- character array
    //

    // @throws NumberFormatException
    uint64_t parse_octal_64_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    uint32_t parse_octal_32_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    uint16_t parse_octal_16_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    uint8_t parse_octal_8_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    unsigned long long parse_octal_unsigned_long_long_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    unsigned long parse_octal_unsigned_long_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    unsigned int parse_octal_unsigned_int_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    unsigned short parse_octal_unsigned_short_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    unsigned char parse_octal_unsigned_char_c_str(const char* text_buffer, size_t text_length);



    // Hexadecimal number parsers -- character array
    //

    // @throws NumberFormatException
    uint64_t parse_hexadecimal_64_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    uint32_t parse_hexadecimal_32_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    uint16_t parse_hexadecimal_16_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    uint8_t parse_hexadecimal_8_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    unsigned long long parse_hexadecimal_unsigned_long_long_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    unsigned long parse_hexadecimal_unsigned_long_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    unsigned int parse_hexadecimal_unsigned_int_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    unsigned short parse_hexadecimal_unsigned_short_c_str(const char* text_buffer, size_t text_length);

    // @throws NumberFormatException
    unsigned char parse_hexadecimal_unsigned_char_c_str(const char* text_buffer, size_t text_length);
}

#endif	/* INTEGERPARSE_H */

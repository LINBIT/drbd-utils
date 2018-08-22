#ifndef INTEGERPARSE_IMPL_H
#define	INTEGERPARSE_IMPL_H

#include <string>
#include <integerparse.h>
#include <dsaext.h>

namespace dsaext
{
    static const std::string DIGITS_BINARY      = "01";
    static const std::string DIGITS_OCTAL       = "01234567";
    static const std::string DIGITS_DECIMAL     = "0123456789";
    static const std::string CHARS_HEXADECIMAL  = "AaBbCcDdEeFf";
    static const std::string SIGN_CHARS         = "-+";

    static const uint8_t BASE_BINARY        = 2;
    static const uint8_t BASE_OCTAL         = 8;
    static const uint8_t BASE_DECIMAL       = 10;
    static const uint8_t BASE_HEXADECIMAL   = 16;

    template<typename T>
    static T parse_unsigned_numeric_impl(
        const char* const   data_buffer,
        const size_t        data_length,
        const T             max_value,
        const T             base,
        const std::string&  digit_set
    )
    {
        if (data_length < 1)
        {
            throw NumberFormatException();
        }

        const T max_value_base = max_value / base;
        T result = 0;
        for (size_t index = 0; index < data_length; ++index)
        {
            if (result > max_value_base)
            {
                throw NumberFormatException();
            }
            result *= base;

            size_t digit_index = digit_set.find(data_buffer[index], 0);
            if (digit_index == std::string::npos)
            {
                throw NumberFormatException();
            }
            T digit_value = static_cast<T> (digit_index);
            if (digit_value > max_value - result)
            {
                throw NumberFormatException();
            }
            result += digit_value;
        }

        return result;
    }

    template<typename T>
    static T parse_signed_numeric_impl(
        const char* const   data_buffer,
        const size_t        data_length,
        const T             min_value,
        const T             max_value,
        const T             base,
        const std::string&  digit_set
    )
    {

        if (data_length < 1)
        {
            throw NumberFormatException();
        }

        size_t sign_index = dsaext::SIGN_CHARS.find(data_buffer[0], 0);
        bool positive_signed = sign_index != 0;
        const size_t numbers_offset = (sign_index == std::string::npos ? 0 : 1);

        T result = 0;
        size_t index = numbers_offset;
        if (positive_signed)
        {
            const T max_positive_base = max_value / base;
            while (index < data_length)
            {
                if (result > max_positive_base)
                {
                    throw NumberFormatException();
                }
                result *= base;

                size_t digit_index = digit_set.find(data_buffer[index], 0);
                if (digit_index == std::string::npos)
                {
                    throw NumberFormatException();
                }
                T digit_value = static_cast<T> (digit_index);
                if (digit_value > max_value - result)
                {
                    throw NumberFormatException();
                }
                result += digit_value;

                ++index;
            }
        }
        else
        {
            const T min_negative_base = min_value / base;
            while (index < data_length)
            {
                if (result < min_negative_base)
                {
                    throw NumberFormatException();
                }
                result *= base;

                size_t digit_index = digit_set.find(data_buffer[index], 0);
                if (digit_index == std::string::npos)
                {
                    throw NumberFormatException();
                }
                T digit_value = static_cast<T> (digit_index);
                if ((digit_value * -1) < min_value - result)
                {
                    throw NumberFormatException();
                }
                result -= digit_value;

                ++index;
            }
        }
        if (!(index > numbers_offset))
        {
            // No digit was parsed
            throw NumberFormatException();
        }

        return result;
    }

    template<typename T>
    static T parse_alphanumeric_impl(
        const char* const   data_buffer,
        const size_t        data_length,
        const T             max_value,
        const T             base,
        const std::string&  digit_set,
        const std::string&  alpha_set,
        const size_t        alpha_offset
    )
    {
        if (data_length < 1)
        {
            throw NumberFormatException();
        }

        const T max_value_base = max_value / base;
        T result = 0;
        for (size_t index = 0; index < data_length; ++index)
        {
            if (result > max_value_base)
            {
                throw NumberFormatException();
            }
            result *= base;

            size_t digit_index = digit_set.find(data_buffer[index], 0);
            if (digit_index == std::string::npos)
            {
                digit_index = alpha_set.find(data_buffer[index], 0);
                if (digit_index == std::string::npos)
                {
                    throw NumberFormatException();
                }
                digit_index = (digit_index >> 1) + alpha_offset;
            }
            T digit_value = static_cast<T> (digit_index);
            if (digit_value > max_value - result)
            {
                throw NumberFormatException();
            }
            result += digit_value;
        }

        return result;
    }

    template<typename T>
    T parse_binary_impl(
        const char* const   data_buffer,
        const size_t        data_length,
        const T             max_value
    )
    {
        return parse_unsigned_numeric_impl<T>(
            data_buffer,
            data_length,
            max_value,
            BASE_BINARY,
            DIGITS_BINARY
        );
    }

    template<typename T>
    T parse_octal_impl(
        const char* const   data_buffer,
        const size_t        data_length,
        const T             max_value
    )
    {
        return parse_unsigned_numeric_impl<T>(
            data_buffer,
            data_length,
            max_value,
            BASE_OCTAL,
            DIGITS_OCTAL
        );
    }

    template<typename T>
    T parse_unsigned_decimal_impl(
        const char* const   data_buffer,
        const size_t        data_length,
        const T             max_value
    )
    {
        return parse_unsigned_numeric_impl<T>(
            data_buffer,
            data_length,
            max_value,
            BASE_DECIMAL,
            DIGITS_DECIMAL
        );
    }

    template<typename T>
    T parse_signed_decimal_impl(
        const char* const   data_buffer,
        const size_t        data_length,
        const T             min_value,
        const T             max_value
    )
    {
        return parse_signed_numeric_impl<T>(
            data_buffer,
            data_length,
            min_value,
            max_value,
            BASE_DECIMAL,
            DIGITS_DECIMAL
        );
    }

    template<typename T>
    T parse_hexadecimal_impl(
        const char* const   data_buffer,
        const size_t        data_length,
        const T             max_value
    )
    {
        return parse_alphanumeric_impl<T>(
            data_buffer,
            data_length,
            max_value,
            BASE_HEXADECIMAL,
            DIGITS_DECIMAL,
            CHARS_HEXADECIMAL,
            DIGITS_DECIMAL.length()
        );
    }
}

#endif	/* INTEGERPARSE_IMPL_H */

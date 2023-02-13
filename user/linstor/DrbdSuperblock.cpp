/**
 * DRBD meta data superblock extraction utility
 *
 * Used by LINSTOR
 *
 * This utility locates and extracts or wipes internal and external DRBD meta data on arbitrarily
 * sized devices (or files).
 * In the dump mode, it writes a binary copy of the extracted superblock to the standard output
 * stream, unless the standard output stream is connected to a terminal, in that case it writes a
 * hexadecimal representation of the superblock data to the standard output stream instead.
 * In the hexdump mode, a hexadecimal representation of the superblock data is written to the
 * standard output stream, even if the standard output stream is not connected to a terminal.
 * In the wipe mode, it zeroes out the superblock on the target device (or in the target file).
 *
 * The utility can also be used to create a dump of the DRBD meta data superblock by redirecting
 * the standard output stream to a file.
 *
 * Copyright (C) LINBIT HA Solutions GmbH 2023
 */
#include <cstddef>
#include <iostream>
#include <fstream>
#include <new>
#include <memory>
#include <cstring>
#include <stdexcept>

extern "C"
{
    #include <unistd.h>
}

class AppException : public std::exception
{
  public:
    AppException()
    {
    }
    virtual ~AppException() noexcept
    {
    }
    AppException(const AppException& orig) = default;
    AppException& operator=(const AppException& orig) = default;
    AppException(AppException&& orig) = default;
    AppException& operator=(AppException&& orig) = default;
};

namespace drbdsuperblk
{
    static constexpr size_t SUPERBLK_SIZE = 4096;

    enum class meta_data_type : uint8_t
    {
        INTERNAL,
        EXTERNAL
    };

    const char* const HEX_CHARS = "0123456789ABCDEF";

    static void run(int argc, char* argv[]);
    static void device_path_sanity_check(const std::string& device_path);

    // Wipes (zeroes) the DRBD meta data superblock
    static void wipe_superblk(
        char* const buffer,
        const std::string& device_path,
        const meta_data_type md_type
    );

    // Loads the DRBD meta data superblock into the buffer
    // @throws AppException if unsuccessful
    static void load_superblk(
        char* const buffer,
        const std::string& device_path,
        const meta_data_type md_type
    );

    // Writes a dump of the DRBD meta data superblock to the standard output stream
    // If the standard output stream is connected to a terminal or the hex_mode flag is set,
    // a hexadecimal representation of the superblock is written to the standard output stream
    // instead of the raw superblock data.
    static void output_superblk(const char* const buffer, const bool hex_mode);

    // Prints a hex representation of an offset address within the DRBD meta data superblock
    static void print_hex(const size_t address);

    // Prints a hex representation of a single byte
    static void print_hex(const unsigned char byte_value);

    // Writes program usage information to the standard error stream
    static void syntax();

    static void run(int argc, char* argv[])
    {
        if (argc == 4)
        {
            std::string action(argv[1]);
            std::string md_type_str(argv[2]);
            std::string device_path(argv[3]);

            meta_data_type md_type = meta_data_type::INTERNAL;
            if (md_type_str == "external")
            {
                md_type = meta_data_type::EXTERNAL;
            }
            else
            if (md_type_str != "internal")
            {
                std::cerr << "Invalid meta data type \"" << md_type_str << "\"" << std::endl;
                throw AppException();
            }

            device_path_sanity_check(device_path);

            std::unique_ptr<char[]> buffer_mgr(new char[SUPERBLK_SIZE]);
            char* const buffer = buffer_mgr.get();
            if (action == "dump" || action == "hexdump")
            {
                load_superblk(buffer, device_path, md_type);
                output_superblk(buffer, action == "hexdump");
            }
            else
            if (action == "wipe")
            {
                wipe_superblk(buffer, device_path, md_type);
            }
            else
            {
                std::cerr << "Invalid action \"" << action << "\"" << std::endl;
                throw AppException();
            }
        }
        else
        {
            syntax();
        }
    }

    static void device_path_sanity_check(const std::string& device_path)
    {
        if (device_path.find("/dev/") != 0 || device_path.find("..") != std::string::npos)
        {
            std::cerr << "Invalid device path \"" << device_path << "\"" << std::endl;
            throw AppException();
        }
    }

    // @throws AppException, std::bad_alloc
    static void load_superblk(
        char* const buffer,
        const std::string& device_path,
        const meta_data_type md_type
    )
    {
        std::ifstream data_in(device_path, std::ios_base::in | std::ios_base::binary);
        if (data_in.fail())
        {
            std::cerr << "Error: Cannot open device \"" << device_path << "\"" << std::endl;
            throw AppException();
        }

        try
        {
            data_in.exceptions(std::ios_base::failbit);
            data_in.seekg(0, std::ios_base::end);
            size_t cursor_pos = data_in.tellg();
            cursor_pos &= ~static_cast<size_t> (0xFFF);
            if (cursor_pos < static_cast<size_t> (SUPERBLK_SIZE))
            {
                std::cerr << "Error: Device \"" << device_path <<
                    "\" is too small to contain a DRBD superblock" << std::endl;
                throw AppException();
            }
            if (md_type == meta_data_type::INTERNAL)
            {
                cursor_pos -= SUPERBLK_SIZE;
                data_in.seekg(cursor_pos, std::ios_base::beg);
            }
            else
            {
                data_in.seekg(0, std::ios_base::beg);
            }
            data_in.read(buffer, SUPERBLK_SIZE);

            if (data_in.gcount() != SUPERBLK_SIZE)
            {
                std::cerr << "Error: Short read on device \"" << device_path << "\"" << std::endl;
                throw AppException();
            }
        }
        catch (std::ios::failure&)
        {
            std::cerr << "Error: I/O error on device \"" << device_path << "\"" << std::endl;
            throw AppException();
        }
    }

    static void wipe_superblk(
        char* const buffer,
        const std::string& device_path,
        const meta_data_type md_type
    )
    {
        std::ofstream data_out(device_path, std::ios_base::out | std::ios_base::binary);
        if (data_out.fail())
        {
            std::cerr << "Error: Cannot open device \"" << device_path << "\"" << std::endl;
            throw AppException();
        }

        try
        {
            data_out.exceptions(std::ios_base::failbit);
            data_out.seekp(0, std::ios_base::end);
            size_t cursor_pos = data_out.tellp();
            cursor_pos &= ~static_cast<size_t> (0xFFF);
            if (cursor_pos < static_cast<size_t> (SUPERBLK_SIZE))
            {
                std::cerr << "Error: Device \"" << device_path <<
                    "\" is too small to contain a DRBD superblock" << std::endl;
                throw AppException();
            }
            if (md_type == meta_data_type::INTERNAL)
            {
                cursor_pos -= SUPERBLK_SIZE;
                data_out.seekp(cursor_pos, std::ios_base::beg);
            }
            else
            {
                data_out.seekp(0, std::ios_base::beg);
            }
            std::memset(static_cast<void*> (buffer), 0, SUPERBLK_SIZE);
            data_out.write(buffer, SUPERBLK_SIZE);

            if (!data_out.good())
            {
                std::cerr << "Error: I/O error while wiping meta data on device \"" << device_path <<
                    "\"" << std::endl;
                throw AppException();
            }
        }
        catch (std::ios::failure&)
        {
            std::cerr << "Error: I/O error on device \"" << device_path << "\"" << std::endl;
            throw AppException();
        }
    }

    static void output_superblk(const char* const buffer, const bool hex_mode)
    {
        const bool is_stdout_tty = isatty(STDOUT_FILENO) == 1;
        if (is_stdout_tty && !hex_mode)
        {
            std::cout << "Standard output is connected to a terminal, binary output suppressed" << std::endl;
        }
        if (is_stdout_tty || hex_mode)
        {
            for (size_t idx = 0; idx < SUPERBLK_SIZE; idx += 16)
            {
                print_hex(idx);
                std::cout << ":";
                for (size_t sub_idx = 0; sub_idx < 16; ++sub_idx)
                {
                    const unsigned char byte_value = static_cast<unsigned char> (buffer[idx | sub_idx]);
                    std::cout << " ";
                    print_hex(byte_value);
                }
                std::cout << "\n";
            }
            std::cout << std::flush;
        }
        else
        {
            std::cout.write(buffer, SUPERBLK_SIZE);
            std::cout << std::flush;
        }
        if (!std::cout.good())
        {
            std::cerr << "Error: I/O error on the standard output stream" << std::endl;
            throw AppException();
        }
    }

    static void print_hex(const size_t address)
    {
        std::cout << HEX_CHARS[(address >> 12) & 0xF];
        std::cout << HEX_CHARS[(address >> 8) & 0xF];
        std::cout << HEX_CHARS[(address >> 4) & 0xF];
        std::cout << HEX_CHARS[address & 0xF];
    }

    static void print_hex(const unsigned char byte_value)
    {
        std::cout << HEX_CHARS[(byte_value >> 4) & 0xF];
        std::cout << HEX_CHARS[byte_value & 0xF];
    }

    static void syntax()
    {
        std::cerr << "DRBD meta data superblock extraction utility\n";
        std::cerr << "Parameters: { dump | hexdump | wipe } { internal | external } device_path" << std::endl;
    }
}

int main(int argc, char* argv[])
{
    int rc = EXIT_FAILURE;

    if (argv != nullptr)
    {
        try
        {
            drbdsuperblk::run(argc, argv);
            rc = EXIT_SUCCESS;
        }
        catch (AppException& exc)
        {
            // No-op; error reported to stderr by the thrower
        }
        catch (std::bad_alloc&)
        {
            std::cerr << "Error: Out of memory" << std::endl;
        }
    }
    else
    {
        std::cerr << "Error: Command line arguments array not present" << std::endl;
    }

    return rc;
}

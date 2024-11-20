#include <cstddef>
#include <iostream>
#include <fstream>
#include <string>

extern "C"
{
    #include <unistd.h>
}

constexpr int ERR_OUT_OF_MEMORY     = 2;
constexpr int ERR_IO                = 3;

const std::string INIT_STATE_SEPA("exists -");
const size_t      INIT_STATE_SEPA_LENGTH = INIT_STATE_SEPA.length();

int process_events(const std::string& path);
bool is_init_state_sepa(const std::string& event_line);

/**
 * DRBD events log file supplier for DRBDmon
 */
int main(int argc, char* argv[])
{
    int exit_code = EXIT_FAILURE;

    try
    {
        if (argc == 2)
        {
            std::string path(argv[1]);
            exit_code = process_events(path);
        }
        else
        {
            std::cerr << "DRBD Events Log File Supplier | Arguments:\n" <<
                "    path .......... Path to a file containing DBRD event lines\n";
        }
    }
    catch (std::bad_alloc&)
    {
        std::cerr << "Out of memory" << std::endl;
        exit_code = ERR_OUT_OF_MEMORY;
    }

    std::cout << std::flush;
    std::cerr << std::flush;

    return exit_code;
}

/**
 * Emits event lines from an events log file and appends the initial state separator
 * if it is absent from the file. The output is stdout, which DRBDmon pipes to itself.
 *
 * DRBDmon recovers its events source process by respawning it if it exits for any reason.
 * Therefore, this method does not return if successful, instead, it will
 * suspend indefinitely. DRBDmon will terminate this process when the user exits DRBDmon.
 */
int process_events(const std::string& path)
{
    int rc = ERR_IO;

    // Internal pipe used for suspending execution
    int pipe_fd[2];
    int pipe_rc = pipe(pipe_fd);
    if (pipe_rc == 0)
    {
        std::ifstream in_file(path);
        if (in_file.good())
        {
            bool have_init_state_sepa = false;

            // Emit events from the file to DRBDmon
            std::string event_line;
            while (in_file.good())
            {
                std::getline(in_file, event_line);
                if (event_line.length() > 0)
                {
                    std::cout << event_line << '\n';
                }
                if (is_init_state_sepa(event_line))
                {
                    have_init_state_sepa = true;
                }

                event_line.clear();
            }

            if (in_file.eof())
            {
                // If there was no initial state separator in the events log file,
                // emit one to DRBDmon
                if (!have_init_state_sepa)
                {
                    std::cout << INIT_STATE_SEPA << '\n';
                }
                std::cout << std::flush;

                rc = EXIT_SUCCESS;

                // Suspend execution indefinitely. DRBDmon will terminate this process when it exits.
                char in_char = 0;
                const ssize_t read_count = read(pipe_fd[0], &in_char, 1);
                // Nothing to do with that result...
                static_cast<void> (read_count);
            }
            else
            {
                // I/O failed before the entire file was processed, I/O error.
                std::cerr << "I/O error while reading DRBD events file \"" << path << "\"\n";
            }
        }
        else
        {
            // Cannot open/read the file. Typically a non-existent path/file or permissions problem.
            std::cerr << "I/O error, cannot read DRBD events file \"" << path << "\"\n";
        }
    }
    else
    {
        // Cannot create the pipe used for suspending.
        // Typically a resource exhaustion problem, e.g. the OS being out of memory.
        std::cerr << "I/O error, pipe creation failed\n";
    }

    return rc;
}

/**
 * Checks whether an events line is the initial state separator
 */
bool is_init_state_sepa(const std::string& event_line)
{
    bool result = false;
    if (event_line.length() >= INIT_STATE_SEPA_LENGTH)
    {
        size_t idx = 0;
        while (idx < INIT_STATE_SEPA_LENGTH && event_line[idx] == INIT_STATE_SEPA[idx])
        {
            ++idx;
        }
        result = idx == INIT_STATE_SEPA_LENGTH;
    }
    return result;
}

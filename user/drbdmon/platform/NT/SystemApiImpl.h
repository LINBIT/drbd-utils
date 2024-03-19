#ifndef NT_SYSTEMAPIIMPL_H
#define NT_SYSTEMAPIIMPL_H

#include <default_types.h>
#include <platform/SystemApi.h>

class NtApi : public SystemApi
{
  public:
    NtApi();
    virtual ~NtApi() noexcept override;

    virtual std::unique_ptr<SubProcess> create_subprocess_handler() override;
    virtual std::unique_ptr<TerminalControl> create_terminal_control() override;
    virtual std::string get_config_file_path() override;
    virtual std::string file_name_for_path(const std::string path) const override;
    virtual bool is_file_accessible(const char* const file_path) const override;

  private:
    static const char* const    CONFIG_FILE_NAME;
};

#endif /* NT_SYSTEMAPIIMPL_H */

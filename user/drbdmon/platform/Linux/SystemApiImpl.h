#ifndef LINUX_SYSTEMAPIIMPL_H
#define LINUX_SYSTEMAPIIMPL_H

#include <default_types.h>
#include <platform/SystemApi.h>

extern "C"
{
    #include <signal.h>
}

class LinuxApi : public SystemApi
{
  public:
    LinuxApi();
    virtual ~LinuxApi() noexcept override;

    virtual void pre_thread_invocation() override;

    virtual void post_thread_invocation() override;

    virtual std::unique_ptr<SubProcess> create_subprocess_handler() override;
    virtual std::unique_ptr<TerminalControl> create_terminal_control() override;
    virtual std::string get_config_file_path() override;
    virtual void prepare_save_config_file() override;
    virtual std::string file_name_for_path(const std::string path) const override;
    virtual bool is_file_accessible(const char* const file_path) const override;

  private:
    static const char* const    CONFIG_FILE_NAME;
    static const char* const    ENV_KEY_HOME;

    sigset_t saved_mask;
};

#endif /* LINUX_SYSTEMAPIIMPL_H */

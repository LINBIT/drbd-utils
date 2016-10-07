#ifndef ARGS_H
#define	ARGS_H

class Args
{
  public:
    Args(int argc, char* argv[]);

    Args(const Args& orig) = delete;
    Args& operator=(const Args& orig) = delete;
    Args(Args&& orig) = default;
    Args& operator=(Args&& orig) = default;

    virtual ~Args() noexcept;

    virtual bool has_next() const;
    virtual char* next();

  private:
    const int    arg_count;
    char** const arg_values;

    int index {1};
};

#endif	/* ARGS_H */

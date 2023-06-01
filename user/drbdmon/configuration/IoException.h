#ifndef IOEXCEPTION_H
#define IOEXCEPTION_H

#include <string>
#include <stdexcept>

class IoException : public std::exception
{
  public:
    IoException();
    IoException(const std::string& message);
    virtual ~IoException() noexcept;
    virtual const char* what() const noexcept;

  private:
    std::string error_message;
};

#endif /* IOEXCEPTION_H */

#ifndef DRBDROLE_H
#define	DRBDROLE_H

#include <string>
#include <exceptions.h>
#include <cstdint>

class DrbdRole
{
  public:
    enum resource_role : uint16_t
    {
        PRIMARY,
        SECONDARY,
        UNKNOWN
    };

    static const std::string PROP_KEY_ROLE;

    static const char* ROLE_LABEL_PRIMARY;
    static const char* ROLE_LABEL_SECONDARY;
    static const char* ROLE_LABEL_UNKNOWN;

    DrbdRole() = default;
    DrbdRole(const DrbdRole& orig) = default;
    DrbdRole& operator=(const DrbdRole& orig) = default;
    DrbdRole(DrbdRole&& orig) = default;
    DrbdRole& operator=(DrbdRole&& orig) = default;
    virtual ~DrbdRole() noexcept
    {
    }

    virtual resource_role get_role() const;
    virtual const char* get_role_label() const;

    // @throws EventMessageException
    static resource_role parse_role(std::string& role_name);

  protected:
    resource_role role;
};

#endif	/* DRBDROLE_H */

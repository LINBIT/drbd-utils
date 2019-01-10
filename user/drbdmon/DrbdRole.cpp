#include <DrbdRole.h>

const std::string DrbdRole::PROP_KEY_ROLE = "role";

const char* DrbdRole::ROLE_LABEL_PRIMARY   = "Primary";
const char* DrbdRole::ROLE_LABEL_SECONDARY = "Secondary";
const char* DrbdRole::ROLE_LABEL_UNKNOWN   = "Unknown";

DrbdRole::resource_role DrbdRole::get_role() const
{
    return role;
}

const char* DrbdRole::get_role_label() const
{
    const char* label = ROLE_LABEL_UNKNOWN;
    switch (role)
    {
        case DrbdRole::resource_role::PRIMARY:
            label = ROLE_LABEL_PRIMARY;
            break;
        case DrbdRole::resource_role::SECONDARY:
            label = ROLE_LABEL_SECONDARY;
            break;
        case DrbdRole::resource_role::UNKNOWN:
            // fall-through
        default:
            break;
    }
    return label;
}

// @throws std::bad_alloc, EventMessageException
DrbdRole::resource_role DrbdRole::parse_role(std::string& role_name)
{
    DrbdRole::resource_role role = DrbdRole::resource_role::UNKNOWN;

    if (role_name == ROLE_LABEL_PRIMARY)
    {
        role = DrbdRole::resource_role::PRIMARY;
    }
    else
    if (role_name == ROLE_LABEL_SECONDARY)
    {
        role = DrbdRole::resource_role::SECONDARY;
    }
    else
    if (role_name != ROLE_LABEL_UNKNOWN)
    {
        std::string error_msg("Invalid DRBD event: Invalid DRBD role");
        std::string debug_info("Invalid DRBD role");
        throw new EventMessageException(&error_msg, &debug_info, nullptr);
    }

    return role;
}

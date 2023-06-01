#include <terminal/ClickableCommand.h>
#include <comparators.h>

ClickableCommand::ClickableCommand(
    const std::string&      command_text,
    const uint16_t          pos_page,
    const uint16_t          pos_row,
    const uint16_t          pos_start_col,
    const uint16_t          pos_end_col,
    std::function<void()>&  handler_ref
):
    command(command_text),
    clickable_area(pos_page, pos_row, pos_start_col, pos_end_col),
    handler_func(&handler_ref)
{
}

ClickableCommand::ClickableCommand(
    const std::string&      command_text,
    const Position&         pos_ref,
    std::function<void()>&  handler_ref
):
    ClickableCommand::ClickableCommand(
        command_text,
        pos_ref.page, pos_ref.row, pos_ref.start_col, pos_ref.end_col,
        handler_ref
    )
{
}

ClickableCommand::~ClickableCommand() noexcept
{
}

int ClickableCommand::compare_position(
    const ClickableCommand::Position* const key,
    const ClickableCommand::Position* const other
) noexcept
{
    int result = comparators::compare(&(key->page), &(other->page));
    if (result == 0)
    {
        result = comparators::compare(&(key->row), &(other->row));
        if (result == 0)
        {
            result = comparators::compare(&(key->start_col), &(other->start_col));
        }
    }
    return result;
}

ClickableCommand::Position::Position(
   const uint16_t  init_page,
   const uint16_t  init_row,
   const uint16_t  init_start_col,
   const uint16_t  init_end_col
)
{
    page        = init_page;
    row         = init_row;
    start_col   = init_start_col;
    end_col     = init_end_col;
}

ClickableCommand::Position::~Position() noexcept
{
}

bool ClickableCommand::Position::is_click_in_area(
    uint16_t click_page,
    uint16_t click_row,
    uint16_t click_col
) const noexcept
{
    return (click_page == page && click_row == row && click_col >= start_col && click_col <= end_col);
}

ClickableCommand::Builder::Builder():
    coords(1, 1, 1, 1)
{
}

ClickableCommand::Builder::~Builder() noexcept
{
}

ClickableCommand* ClickableCommand::Builder::create_with_auto_nr(std::function<void()>& handler_ref)
{
    ClickableCommand* const cmd = new ClickableCommand(
        std::to_string(static_cast<unsigned int> (auto_nr)),
        coords.page, coords.row, coords.start_col, coords.end_col,
        handler_ref
    );
    ++auto_nr;
    ++coords.row;
    return cmd;
}

ClickableCommand* ClickableCommand::Builder::create_with_id(const std::string& id, std::function<void()>& handler_ref)
{
    ClickableCommand* const cmd = new ClickableCommand(
        id,
        coords.page, coords.row, coords.start_col, coords.end_col,
        handler_ref
    );
    ++coords.row;
    return cmd;
}

#ifndef CLICKABLECOMMAND_H
#define CLICKABLECOMMAND_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <functional>
#include <QTree.h>
#include <terminal/MouseEvent.h>

class ClickableCommand
{
  public:
    class Position
    {
      public:
        uint16_t    page        {0};
        uint16_t    row         {0};
        uint16_t    start_col   {0};
        uint16_t    end_col     {0};

        Position(
            const uint16_t  init_page,
            const uint16_t  init_row,
            const uint16_t  init_start_col,
            const uint16_t  init_end_col
        );
        virtual ~Position() noexcept;

        bool is_click_in_area(
            uint16_t click_page,
            uint16_t click_row,
            uint16_t click_col
        ) const noexcept;
    };

    using CommandMap = QTree<std::string, ClickableCommand>;
    using ClickableMap = QTree<Position, ClickableCommand>;

    static int compare_position(const Position* const key, const Position* const other) noexcept;

    // Command or option string, must be uppercase
    const std::string   command;
    // Page number, screen row, first and last column of the clickable area
    // Row/Column origin is at [1, 1], like cursor navigation
    Position            clickable_area;

    std::function<void()>* const    handler_func;

    ClickableCommand(
        const std::string&      command_text,
        const uint16_t          pos_page,
        const uint16_t          pos_row,
        const uint16_t          pos_start_col,
        const uint16_t          pos_end_col,
        std::function<void()>&  handler_ref
    );
    ClickableCommand(
        const std::string&      command_text,
        const Position&         pos_ref,
        std::function<void()>&  handler_ref
    );
    virtual ~ClickableCommand() noexcept;

    class Builder
    {
      public:
        uint16_t        auto_nr         {0};
        Position        coords;

        Builder();
        virtual ~Builder() noexcept;

        virtual ClickableCommand* create_with_auto_nr(std::function<void()>& handler_ref);
        virtual ClickableCommand* create_with_id(const std::string& id, std::function<void()>& handler_ref);
    };
};

#endif /* CLICKABLECOMMAND_H */

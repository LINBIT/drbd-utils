#ifndef INPUTFIELD_H
#define INPUTFIELD_H

#include <terminal/ComponentsHub.h>
#include <terminal/MouseEvent.h>
#include <cstddef>
#include <cstdint>
#include <string>

class InputField
{
  public:
    class Position
    {
      public:
        uint16_t    column;
        uint16_t    row;
    };

    InputField(const ComponentsHub& dsp_comp_hub_ref, const uint16_t length);
    InputField(
        const ComponentsHub&    dsp_comp_hub_ref,
        const uint32_t          coord_column,
        const uint32_t          coord_row,
        const uint16_t          length,
        const uint16_t          field_length
    );
    virtual ~InputField() noexcept;

    virtual void key_pressed(const uint32_t key);
    virtual const std::string& get_text();
    virtual bool is_empty() const;
    virtual void set_text(const std::string& new_text);
    virtual void set_text(const char* const new_text);
    virtual void clear_text();
    virtual void set_position(const uint16_t coord_column, const uint16_t coord_row);
    virtual Position get_position() const;
    virtual uint16_t get_cursor_position();
    virtual void set_cursor_position(const uint16_t new_cursor_pos);
    virtual uint16_t get_offset();
    virtual void set_offset(const uint16_t new_offset);
    virtual void set_field_length(const uint16_t field_length);
    virtual void display();
    virtual void cursor();
    virtual bool mouse_action(MouseEvent& mouse);

  private:
    static const std::string    ALLOWED_SPECIAL_CHARS;

    const ComponentsHub&        dsp_comp_hub;

    std::string     text;

    uint16_t        column          {1};
    uint16_t        row             {1};

    uint16_t        max_length      {1000};
    uint16_t        dsp_length      {40};
    uint16_t        offset          {0};
    uint16_t        cursor_pos      {0};

    void update_positions();
};

#endif /* INPUTFIELD_H */

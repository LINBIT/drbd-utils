#ifndef DISPLAYSTYLECOLLECTION_H
#define DISPLAYSTYLECOLLECTION_H

#include <new>
#include <memory>
#include <stdexcept>

#include <terminal/CharacterTable.h>
#include <terminal/ColorTable.h>

class DisplayStyleCollection
{
  public:
    enum class ColorStyle : uint16_t
    {
        DEFAULT             = 0,
        DARK_BG_256_COLORS  = 1,
        DARK_BG_16_COLORS   = 2,
        LIGHT_BG_256_COLORS = 3,
        LIGHT_BG_16_COLORS  = 4
    };

    enum class CharacterStyle : uint16_t
    {
        DEFAULT     = 0,
        UNICODE     = 1,
        ASCII       = 2
    };

    // @throws std::bad_alloc
    DisplayStyleCollection();
    virtual ~DisplayStyleCollection() noexcept;

    virtual const ColorTable& get_color_table(const ColorStyle style) const;
    virtual const CharacterTable& get_character_table(const CharacterStyle style) const;

    virtual ColorStyle get_color_style_by_numeric_id(const uint16_t id) const;
    virtual CharacterStyle get_character_style_by_numeric_id(const uint16_t id) const;

  private:
    std::unique_ptr<CharacterTable>     unicode_tbl;
    std::unique_ptr<CharacterTable>     ascii_tbl;

    std::unique_ptr<ColorTable>         dark_bg_256clr;
    std::unique_ptr<ColorTable>         light_bg_256clr;
    std::unique_ptr<ColorTable>         dark_bg_16clr;
    std::unique_ptr<ColorTable>         light_bg_16clr;
};

#endif /* DISPLAYSTYLECOLLECTION_H */

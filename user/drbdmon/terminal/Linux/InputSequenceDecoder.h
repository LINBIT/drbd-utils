#ifndef INPUTSEQUENCEDECODER_H
#define INPUTSEQUENCEDECODER_H

#include <default_types.h>
#include <string>

#include <terminal/MouseEvent.h>

class InputSequenceDecoder
{
  public:
    static const size_t MAX_SEQ_LENGTH;

    MouseEvent mouse_event_data;

    // @throws std::bad_alloc
    InputSequenceDecoder();
    virtual ~InputSequenceDecoder() noexcept;

    // @throws std::bad_alloc
    virtual uint32_t input_char(const char in_char, MouseEvent& mouse);

  private:
    enum engine_state : uint8_t
    {
        DIRECT          = 0,
        ESC_SEQ         = 1,
        SS3_SEQ         = 2,
        CSI_SEQ         = 3,
        MOUSE_SEQ       = 4,
        CSI_SEQ_IGN     = 5
    };

    enum engine_mouse_state : uint8_t
    {
        INITIAL         = 0,
        COORD_COL       = 1,
        COORD_ROW       = 2
    };

    engine_state        state;
    engine_mouse_state  mouse_state;
    uint32_t            saved_key_code  {0};
    std::string         saved_seq;

    uint32_t update_mouse_event(const char in_char, MouseEvent& mouse);

    void reset_state();
    void reset_state(const engine_state next_state);
    void reset_mouse_state();
    void reset_mouse_event_data();
};

#endif /* INPUTSEQUENCEDECODER_H */

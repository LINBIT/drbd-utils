#include <terminal/Linux/InputSequenceDecoder.h>
#include <terminal/KeyCodes.h>
#include <terminal/InputCharCodes.h>
#include <terminal/EscSeqCodes.h>
#include <cppdsaext/src/dsaext.h>
#include <cppdsaext/src/integerparse.h>

const size_t InputSequenceDecoder::MAX_SEQ_LENGTH   = 16;

// @throws std::bad_alloc
InputSequenceDecoder::InputSequenceDecoder()
{
    saved_seq.reserve(MAX_SEQ_LENGTH);
    reset_state();
    reset_mouse_state();
}

InputSequenceDecoder::~InputSequenceDecoder() noexcept
{
}

// @throws std::bad_alloc
uint32_t InputSequenceDecoder::input_char(const char in_char, MouseEvent& mouse)
{
    uint32_t key_code = KeyCodes::NONE;
    switch (state)
    {
        case engine_state::DIRECT:
        {
            if (in_char == EscSeqCodes::ESC)
            {
                state = engine_state::ESC_SEQ;
            }
            else
            if (in_char == InputCharCodes::ENTER_1 || in_char == InputCharCodes::ENTER_2)
            {
                key_code = KeyCodes::ENTER;
            }
            else
            if (in_char == InputCharCodes::TAB)
            {
                key_code = KeyCodes::TAB;
            }
            else
            if (in_char == InputCharCodes::BACKSPACE)
            {
                key_code = KeyCodes::BACKSPACE;
            }
            else
            {
                key_code = static_cast<const unsigned char> (in_char);
            }
            break;
        }
        case engine_state::ESC_SEQ:
        {
            if (in_char == EscSeqCodes::SS3)
            {
                state = engine_state::SS3_SEQ;
            }
            else
            if (in_char == EscSeqCodes::CSI)
            {
                state = engine_state::CSI_SEQ;
            }
            else
            {
                state = engine_state::DIRECT;
            }
            break;
        }
        case engine_state::CSI_SEQ:
        {
            if (in_char >= '0' && in_char <= '9' && saved_seq.length() < MAX_SEQ_LENGTH)
            {
                saved_seq += in_char;
            }
            else
            if (saved_seq.length() == 0)
            {
                if (in_char == EscSeqCodes::MOUSE_EVENT)
                {
                    state = engine_state::MOUSE_SEQ;
                }
                else
                {
                    if (in_char == EscSeqCodes::CURSOR_UP)
                    {
                        key_code = KeyCodes::ARROW_UP;
                    }
                    else
                    if (in_char == EscSeqCodes::CURSOR_DOWN)
                    {
                        key_code = KeyCodes::ARROW_DOWN;
                    }
                    else
                    if (in_char == EscSeqCodes::CURSOR_LEFT)
                    {
                        key_code = KeyCodes::ARROW_LEFT;
                    }
                    else
                    if (in_char == EscSeqCodes::CURSOR_RIGHT)
                    {
                        key_code = KeyCodes::ARROW_RIGHT;
                    }
                    reset_state();
                }
            }
            else
            {
                if (in_char == EscSeqCodes::FUNC_TERM)
                {
                    if (saved_seq == EscSeqCodes::FUNC_05)
                    {
                        key_code = KeyCodes::FUNC_05;
                    }
                    else
                    if (saved_seq == EscSeqCodes::FUNC_06)
                    {
                        key_code = KeyCodes::FUNC_06;
                    }
                    else
                    if (saved_seq == EscSeqCodes::FUNC_07)
                    {
                        key_code = KeyCodes::FUNC_07;
                    }
                    else
                    if (saved_seq == EscSeqCodes::FUNC_08)
                    {
                        key_code = KeyCodes::FUNC_08;
                    }
                    else
                    if (saved_seq == EscSeqCodes::FUNC_09)
                    {
                        key_code = KeyCodes::FUNC_09;
                    }
                    else
                    if (saved_seq == EscSeqCodes::FUNC_10)
                    {
                        key_code = KeyCodes::FUNC_10;
                    }
                    else
                    if (saved_seq == EscSeqCodes::FUNC_11)
                    {
                        key_code = KeyCodes::FUNC_11;
                    }
                    else
                    if (saved_seq == EscSeqCodes::FUNC_12)
                    {
                        key_code = KeyCodes::FUNC_12;
                    }
                    else
                    if (saved_seq == EscSeqCodes::HOME_1)
                    {
                        key_code = KeyCodes::HOME;
                    }
                    else
                    if (saved_seq == EscSeqCodes::END_1)
                    {
                        key_code = KeyCodes::END;
                    }
                    else
                    if (saved_seq == EscSeqCodes::HOME_2)
                    {
                        key_code = KeyCodes::HOME;
                    }
                    else
                    if (saved_seq == EscSeqCodes::END_2)
                    {
                        key_code = KeyCodes::END;
                    }
                    else
                    if (saved_seq == EscSeqCodes::INSERT)
                    {
                        key_code = KeyCodes::INSERT;
                    }
                    else
                    if (saved_seq == EscSeqCodes::DELETE)
                    {
                        key_code = KeyCodes::DELETE;
                    }
                    else
                    if (saved_seq == EscSeqCodes::PG_UP)
                    {
                        key_code = KeyCodes::PG_UP;
                    }
                    else
                    if (saved_seq == EscSeqCodes::PG_DOWN)
                    {
                        key_code = KeyCodes::PG_DOWN;
                    }
                }
                reset_state();
            }
            break;
        }
        case engine_state::SS3_SEQ:
        {
            if (in_char == EscSeqCodes::FUNC_01)
            {
                key_code = KeyCodes::FUNC_01;
            }
            else
            if (in_char == EscSeqCodes::FUNC_02)
            {
                key_code = KeyCodes::FUNC_02;
            }
            else
            if (in_char == EscSeqCodes::FUNC_03)
            {
                key_code = KeyCodes::FUNC_03;
            }
            else
            if (in_char == EscSeqCodes::FUNC_04)
            {
                key_code = KeyCodes::FUNC_04;
            }
            reset_state();
            break;
        }
        case engine_state::CSI_SEQ_IGN:
        {
            // Ignore the rest of a sequence
            if (!(in_char >= '0' && in_char <= '9') && in_char != EscSeqCodes::CSI_SEP)
            {
                reset_state();
            }
            break;
        }
        case engine_state::MOUSE_SEQ:
        {
            if (in_char >= '0' && in_char <= '9')
            {
                if (saved_seq.length() < MAX_SEQ_LENGTH)
                {
                    saved_seq += in_char;
                }
                else
                {
                    reset_state(engine_state::CSI_SEQ_IGN);
                }
            }
            else
            {
                key_code = update_mouse_event(in_char, mouse);
            }
            break;
        }
        default:
        {
            state = engine_state::DIRECT;
            break;
        }
    }
    return key_code;
}

uint32_t InputSequenceDecoder::update_mouse_event(const char in_char, MouseEvent& mouse)
{
    uint32_t key_code = KeyCodes::NONE;
    switch (mouse_state)
    {
        case engine_mouse_state::INITIAL:
        {
            reset_mouse_event_data();
            if (in_char == EscSeqCodes::CSI_SEP)
            {
                try
                {
                    const uint8_t action = dsaext::parse_unsigned_int8(saved_seq);
                    if (action == 0)
                    {
                        mouse_event_data.button = MouseEvent::button_id::BUTTON_01;
                    }
                    else
                    if (action == 1)
                    {
                        mouse_event_data.button = MouseEvent::button_id::BUTTON_02;
                    }
                    else
                    if (action == 2)
                    {
                        mouse_event_data.button = MouseEvent::button_id::BUTTON_03;
                    }
                    else
                    if (action == 64)
                    {
                        mouse_event_data.event = MouseEvent::event_id::SCROLL_UP;
                    }
                    else
                    if (action == 65)
                    {
                        mouse_event_data.event = MouseEvent::event_id::SCROLL_DOWN;
                    }
                    saved_seq.clear();
                    mouse_state = engine_mouse_state::COORD_COL;
                }
                catch (dsaext::NumberFormatException&)
                {
                    reset_mouse_state();
                    state = engine_state::CSI_SEQ_IGN;
                }
            }
            else
            {
                reset_mouse_state();
                state = engine_state::CSI_SEQ_IGN;
            }
            break;
        }
        case engine_mouse_state::COORD_COL:
        {
            if (in_char == EscSeqCodes::CSI_SEP)
            {
                try
                {
                    mouse_event_data.coord_column = dsaext::parse_unsigned_int16(saved_seq);
                    mouse_state = engine_mouse_state::COORD_ROW;
                    saved_seq.clear();
                }
                catch (dsaext::NumberFormatException&)
                {
                    reset_mouse_state();
                    state = engine_state::CSI_SEQ_IGN;
                }
            }
            else
            {
                reset_mouse_state();
                state = engine_state::CSI_SEQ_IGN;
            }
            break;
        }
        case engine_mouse_state::COORD_ROW:
        {
            if (in_char == EscSeqCodes::MOUSE_DOWN || in_char == EscSeqCodes::MOUSE_RELEASE)
            {
                try
                {
                    mouse_event_data.coord_row = dsaext::parse_unsigned_int16(saved_seq);

                    if (mouse_event_data.event != MouseEvent::event_id::SCROLL_DOWN &&
                        mouse_event_data.event != MouseEvent::event_id::SCROLL_UP)
                    {
                        if (in_char == EscSeqCodes::MOUSE_DOWN)
                        {
                            mouse_event_data.event = MouseEvent::event_id::MOUSE_DOWN;
                        }
                        else
                        if (in_char == EscSeqCodes::MOUSE_RELEASE)
                        {
                            mouse_event_data.event = MouseEvent::event_id::MOUSE_RELEASE;
                        }
                    }
                }
                catch (dsaext::NumberFormatException& ignored)
                {
                }

                mouse = mouse_event_data;
                key_code = KeyCodes::MOUSE_EVENT;

                reset_state();
            }
            else
            if (in_char == EscSeqCodes::CSI_SEP)
            {
                // Incomplete sequence
                state = engine_state::CSI_SEQ_IGN;
            }
            else
            {
                reset_state();
            }
            reset_mouse_state();
            break;
        }
        default:
        {
            reset_mouse_state();
            state = engine_state::CSI_SEQ_IGN;
            break;
        }
    }
    return key_code;
}

void InputSequenceDecoder::reset_state()
{
    reset_state(engine_state::DIRECT);
}

void InputSequenceDecoder::reset_state(const engine_state next_state)
{
    state = next_state;
    saved_key_code = KeyCodes::NONE;
    saved_seq.clear();
}

void InputSequenceDecoder::reset_mouse_state()
{
    mouse_state = engine_mouse_state::INITIAL;

    reset_mouse_event_data();
}

void InputSequenceDecoder::reset_mouse_event_data()
{
    mouse_event_data.event          = MouseEvent::event_id::MOUSE_MOVE;
    mouse_event_data.button         = MouseEvent::button_id::NONE;
    mouse_event_data.coord_column   = 0;
    mouse_event_data.coord_row      = 0;
}

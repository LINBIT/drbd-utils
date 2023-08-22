#ifndef ESCSEQCODES_H
#define ESCSEQCODES_H

#include <default_types.h>

class EscSeqCodes
{
  public:
    static const char   ESC;
    static const char   SS3;
    static const char   CSI;
    static const char   CSI_SEP;
    static const char   FUNC_EXT_1;
    static const char   FUNC_EXT_2;

    static const char   CURSOR_UP;
    static const char   CURSOR_DOWN;
    static const char   CURSOR_RIGHT;
    static const char   CURSOR_LEFT;

    static const char   FUNC_01;
    static const char   FUNC_02;
    static const char   FUNC_03;
    static const char   FUNC_04;
    static const char   FUNC_TERM;

    static const char   MOUSE_EVENT;
    static const char   MOUSE_RELEASE;
    static const char   MOUSE_DOWN;

    static const char* const    FUNC_05;
    static const char* const    FUNC_06;
    static const char* const    FUNC_07;
    static const char* const    FUNC_08;
    static const char* const    FUNC_09;
    static const char* const    FUNC_10;
    static const char* const    FUNC_11;
    static const char* const    FUNC_12;

    static const char* const    HOME_1;
    static const char* const    INSERT;
    static const char* const    DELETE;
    static const char* const    END_1;
    static const char* const    PG_UP;
    static const char* const    PG_DOWN;
    static const char* const    HOME_2;
    static const char* const    END_2;
};

#endif /* ESCSEQCODES_H */

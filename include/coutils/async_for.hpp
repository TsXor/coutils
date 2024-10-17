#pragma once
#ifndef __COUTILS_ASYNC_FOR__
#define __COUTILS_ASYNC_FOR__

#include "coutils/utility.hpp"

#define _COUTILS_FOR_HEADER(var_begin, var_end, var_obj) \
    auto var_begin = COUTILS_AWAIT(var_obj.begin()); \
    auto var_end = COUTILS_AWAIT(var_obj.end()); \
    for (; var_begin != var_end; co_await ++var_begin)

#define _COUTILS_FOR_IMPL(init, decl, expr, var_begin, var_end, var_obj) \
    { init; auto&& var_obj = expr; \
        _COUTILS_FOR_HEADER(var_begin, var_end, var_obj) \
        { decl = *var_begin;

#define _COUTILS_FOR3(init, decl, expr) \
    COUTILS_CALL(_COUTILS_FOR_IMPL, \
        init, decl, expr, \
        COUTILS_ANON_VAR(__iterator_begin), \
        COUTILS_ANON_VAR(__iterator_end), \
        COUTILS_ANON_VAR(__iterable) \
    )

#define _COUTILS_FOR2(decl, expr) _COUTILS_FOR3({}, decl, expr)

#define COUTILS_FOR(...) COUTILS_CALL_OVERLOAD(_COUTILS_FOR, __VA_ARGS__)

#define COUTILS_ENDFOR() \
        } \
    }

#endif // __COUTILS_ASYNC_FOR__

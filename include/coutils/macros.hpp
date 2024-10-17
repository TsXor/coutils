#pragma once
#ifndef __COUTILS_MACROS__
#define __COUTILS_MACROS__



/* Some macro magics here ... */


/**
 * As @ned14 once said, "Infinity = 8".
 * Quote from `quickcpplib`:
 * > 2014-10-9 ned: I lost today figuring out the below. I really hate the C preprocessor now.
 * > Anyway, infinity = 8. It's easy to expand below if needed.
 */
#define _COUTILS_GET_8TH_ARG(dummy, \
    _0, _1, _2, _3, _4, _5, _6, _7, _8, \
    ...) _8
/**
 * @brief Count number of passed argument.
 * 
 * Currently, It can only count to 8.
 */
#define COUTILS_ARG_COUNT(...) \
    _COUTILS_GET_8TH_ARG(dummy, ##__VA_ARGS__, \
        8, 7, 6, 5, 4, 3, 2, 1, 0 \
    )


/**
 * @brief Expand parameters when calling other macros.
 */
#define COUTILS_CALL(macro, ...) macro(__VA_ARGS__)


#define _COUTILS_OVERLOAD(macro, argcnt) macro##argcnt
/**
 * @brief Call different variant of a macro based on number of passed arguments.
 */
#define COUTILS_CALL_OVERLOAD(macro, ...) \
    COUTILS_CALL(_COUTILS_OVERLOAD, macro, COUTILS_ARG_COUNT(__VA_ARGS__))(__VA_ARGS__)


#define _COUTILS_ANON_VAR(name, counter, line) name##_##counter##_##line
/**
 * @brief Creates a unique variable based on `name`.
 */
#define COUTILS_ANON_VAR(name) \
    COUTILS_CALL(_COUTILS_ANON_VAR, name, __COUNTER__, __LINE__)


/**
 * @brief Shortcut to forward a variable.
 */
#define COUTILS_FWD(var) std::forward<decltype(var)>(var)


#endif // __COUTILS_MACROS__

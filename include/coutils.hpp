#pragma once
#ifndef __COUTILS__
#define __COUTILS__

#include "coutils/crt/task.hpp"
#include "coutils/crt/async_fn.hpp"
#include "coutils/crt/generator.hpp"
#include "coutils/crt/async_generator.hpp"

#include "coutils/async_for.hpp"
#include "coutils/wait.hpp"
#include "coutils/multi_await.hpp"

namespace coutils {

using crt::task;
using crt::async_fn;
using crt::generator;
using crt::async_generator;

} // namespace coutils

#endif // __COUTILS__

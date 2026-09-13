#pragma once

#define VK_NON_COPYABLE(className)                                                                                     \
    className(const className &) = delete;                                                                             \
    className &operator=(const className &) = delete;

#define VK_NON_MOVABLE(className)                                                                                      \
    className(className &&) = delete;                                                                                  \
    className &operator=(className &&) = delete;

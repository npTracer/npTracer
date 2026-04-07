#pragma once

#include <cstdint>

#define NP_TRACER_NAMESPACE_BEGIN                                                                  \
    namespace np                                                                                   \
    {

#define NP_TRACER_NAMESPACE_END }

NP_TRACER_NAMESPACE_BEGIN

/*
 * global debug flag 
 * modern best practices is to convert a preprocessor definition into a `constexpr`.
 * it has the added benefit of having the compiler still check inactive code pathways
 */
inline constexpr bool gDEBUG = NPTRACER_DEBUG;

inline constexpr uint32_t DEFAULT_WIDTH = 2560u;  // default width for swapchain
inline constexpr uint32_t DEFAULT_HEIGHT = 1440u;  // default width for swapchain

NP_TRACER_NAMESPACE_END

// global compile definitions
#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

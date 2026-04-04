#pragma once

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

NP_TRACER_NAMESPACE_END

// global compile definitions
#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

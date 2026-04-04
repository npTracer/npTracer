/**
 * this file is the singular translational unit that will contain any desired implementations from "stb-style" single header files.
 * 
 * this design pattern is quite interesting, allowing a huge library to come in a singular header. 
 * how it works is when `*_IMPLEMENTATION` (or a similar preprocessor value) is NOT defined, 
 * the header compiles out to the form of a standard header file with class / function declarations.
 * then, when `*_IMPLEMENTATION` IS defined, and the SAME header is included,
 * the library's desired implementations are compiled directly into our binary and no extra linking steps are required. 
 * quite satisfying!
 * 
 * tldr: do not define any `*_IMPLEMENTATION` values anywhere else in the codebase.
 * just include the corresponding header when needed.
 * 
 * TODO: read this! https://github.com/nothings/stb/blob/master/docs/stb_howto.txt
 * TODO: how does one achieve having a design pattern named after them? 
 */

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

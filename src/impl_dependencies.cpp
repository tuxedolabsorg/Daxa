// volk must be included before VMA, otherwise VMA's statically imported Vulkan functions
// link against volk's function pointer variables of the same name and call into data.
#include "impl_core.hpp"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "context.hpp"

using namespace vks;

#ifdef WIN32
__declspec(thread) vk::CommandPool Context::s_cmdPool;
#else
thread_local vk::CommandPool Context::s_cmdPool;
#endif

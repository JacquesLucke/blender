#ifndef __FN_MULTI_FUNCTION_COMMON_CONTEXT_IDS_H__
#define __FN_MULTI_FUNCTION_COMMON_CONTEXT_IDS_H__

#ifdef FN_COMMON_CONTEXT_IDS_CC
#  define DEFINE_UNIQUE_CONTEXT_ID(name) \
    namespace UniqueVars { \
    char variable_with_unique_address_##name; \
    } \
    const void *name = (const void *)&UniqueVars::variable_with_unique_address_##name;

#else
#  define DEFINE_UNIQUE_CONTEXT_ID(name) extern const void *name;
#endif

namespace FN {
namespace ContextIDs {

DEFINE_UNIQUE_CONTEXT_ID(vertex_locations);

}  // namespace ContextIDs
}  // namespace FN

#endif /* __FN_MULTI_FUNCTION_COMMON_CONTEXT_IDS_H__ */

#if!defined(OMRGLUE_HPP_)
#define OMRGLUE_HPP_

#include "CollectorLanguageInterfaceImpl.hpp"
#include "MarkingScheme.hpp"

namespace omr
{
namespace gc
{

using Env = ::MM_EnvironmentBase;
using MarkingScheme = ::MM_MarkingScheme;

} // namespace gc
} // namespace omr

static inline void
markRange(MM_EnvironmentBase *env, MM_MarkingScheme *ms,
              size_t size, omrobjectptr_t* array) {

    for(size_t i = 0; i < size; i +=1) {
        ms->markObject(env, array[i]);
    }
}

#endif // OMRGLUE_HPP_
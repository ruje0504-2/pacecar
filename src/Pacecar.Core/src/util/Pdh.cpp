#include "pacecar/util/Pdh.h"

#ifdef __MINGW32__
#include <system_error>
#else
#include <wil/result_macros.h>
#endif

#pragma comment(lib, "pdh.lib")

namespace pacecar
{
void ThrowIfPdhError(PDH_STATUS status)
{
    if (status != ERROR_SUCCESS)
    {
#ifdef __MINGW32__
        throw std::system_error(static_cast<int>(status), std::system_category(), "PDH query failed");
#else
        throw wil::ResultException(static_cast<HRESULT>(status));
#endif
    }
}
} // namespace pacecar

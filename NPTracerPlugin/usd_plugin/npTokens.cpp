#include "usd_plugin/npTokens.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace Np
{
const FormatTokens& GetFormatTokens(HdFormat fmt)
{
    switch (fmt)
    {
        case HdFormatFloat32: return kDepthFormatTokens;
        case HdFormatFloat32Vec3: return kNormalFormatTokens;
        case HdFormatInt32: return kIdFormatTokens;
        default: return kColorFormatTokens;
    }
}
}  // namespace Np

PXR_NAMESPACE_CLOSE_SCOPE

#pragma once
#include <base/Color.h>
namespace BatchFlushReason {
    enum Enum {
        NewCamera,
        NewFlags,
        NewTarget,
        NewTexture,
        NewEffect,
        NewColor,
        NewFBO,
        End,
        Full,
    };
}
struct BatchFlushInfo {
    BatchFlushInfo(const BatchFlushReason::Enum e) : reason(e) {}
    BatchFlushInfo(const BatchFlushReason::Enum e, unsigned f)
        : reason(e), newFlags(f) {}
    BatchFlushInfo(const BatchFlushReason::Enum e, const Color& r)
        : reason(e), newColor(r) {}

    BatchFlushReason::Enum reason;
#if !SAC_WINDOWS
    union {
#endif
        unsigned newFlags;
        TextureRef newTexture;
        Color newColor;
#if !SAC_WINDOWS
    };
#endif
};
static std::vector<std::pair<BatchFlushInfo, int>> batchSizes;
static std::vector<std::vector<RenderingSystem::RenderCommand>> batchContent;
static std::string enumToString(BatchFlushReason::Enum e) {
    switch (e) {
    case BatchFlushReason::NewCamera: return "NewCamera";
    case BatchFlushReason::NewFlags: return "NewFlags";
    case BatchFlushReason::NewTarget: return "NewTarget";
    case BatchFlushReason::NewTexture: return "NewTexture";
    case BatchFlushReason::NewEffect: return "NewEffect";
    case BatchFlushReason::NewColor: return "NewColor";
    case BatchFlushReason::NewFBO: return "NewFBO";
    case BatchFlushReason::End: return "End";
    case BatchFlushReason::Full: return "Full";
    }
    return "";
}

inline std::ostream& operator<<(std::ostream& stream, const BatchFlushInfo& v) {
    stream << enumToString(v.reason);
    switch (v.reason) {
    case BatchFlushReason::NewFlags:
        stream << " [ ";
        if (v.newFlags & EnableZWriteBit)
            stream << "EnableZWriteBit ";
        else
            stream << "DisableZWriteBit ";
        if (v.newFlags & EnableBlendingBit)
            stream << "EnableBlendingBit ";
        else
            stream << "DisableBlendingBit ";
        if (v.newFlags & EnableColorWriteBit)
            stream << "EnableColorWriteBit ";
        else
            stream << "DisableColorWriteBit ";
        if (v.newFlags & EnableConstantBit)
            stream << "EnableConstantBit ";
        else
            stream << "DisableConstantBit ";
        stream << "]";
        break;
    case BatchFlushReason::NewColor:
        LOGT("fixme");
        // stream << " [ " << v.newColor << " ]";
        break;
    case BatchFlushReason::NewTexture: {
        if (v.newTexture == InvalidTextureRef) {
            stream << " [ No Texture ]";
        } else {
            const TextureInfo* info =
                theRenderingSystem.textureLibrary.get(v.newTexture, false);
            if (!info) { stream << " [ --- ]"; } else {
                if (info->atlasIndex >= 0) {
                    stream << " [ "
                           << theRenderingSystem.atlas[info->atlasIndex].name
                           << " ]";
                } else {
                    stream << " [ "
                           << theRenderingSystem.textureLibrary.ref2Name(
                                  v.newTexture) << " ]";
                }
            }
        }
        break;
    }
    default: break;
    }
    return stream;
}

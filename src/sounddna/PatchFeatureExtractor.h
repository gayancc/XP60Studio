#pragma once

#include "sounddna/SoundDnaTypes.h"

namespace xp60studio::sounddna {

class PatchFeatureExtractor
{
public:
    static constexpr std::string_view kSchemaVersion = "xp60-patch-features/2";
    [[nodiscard]] PatchFeatureVector extract(const xpmodel::Xp60Patch& patch,
                                             PatchContext context = {}) const;
};

} // namespace xp60studio::sounddna

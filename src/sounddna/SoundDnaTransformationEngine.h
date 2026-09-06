#pragma once

#include "sounddna/PatchFeatureExtractor.h"
#include "sounddna/SoundDnaAnalyzer.h"

namespace xp60studio::sounddna {

class SoundDnaTransformationEngine
{
public:
    explicit SoundDnaTransformationEngine(const SoundDnaKnowledgeModel& model)
        : m_model(model), m_analyzer(model) {}

    [[nodiscard]] SoundDnaTransformationResult transform(const xpmodel::Xp60Patch& patch,
                                                         const PatchContext& context,
                                                         const SoundDnaChangeRequest& request) const;

private:
    const SoundDnaKnowledgeModel& m_model;
    SoundDnaAnalyzer m_analyzer;
    PatchFeatureExtractor m_extractor;
};

} // namespace xp60studio::sounddna

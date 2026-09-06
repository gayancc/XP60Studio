#pragma once

#include "sounddna/SoundDnaKnowledgeModel.h"

namespace xp60studio::sounddna {

class SoundDnaAnalyzer
{
public:
    explicit SoundDnaAnalyzer(const SoundDnaKnowledgeModel& model) : m_model(model) {}
    [[nodiscard]] SoundDnaProfile analyze(const PatchFeatureVector& features) const;

private:
    const SoundDnaKnowledgeModel& m_model;
};

} // namespace xp60studio::sounddna

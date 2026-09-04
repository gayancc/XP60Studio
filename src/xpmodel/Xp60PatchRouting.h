#pragma once

#include "xpmodel/Xp60Patch.h"

#include <vector>

namespace xp60studio::xpmodel {

// Documentation-derived configuration topology, not an audio meter.
// XP-60/XP-80 Owner's Manual pp.60-64 (PDF pp.62-66): Tone output,
// Structure pair ownership, EFX output, parallel sends and Chorus output.
// Tone enable/Solo/Mute and System effect switches are outside this view.
enum class RoutingNode { Source, Efx, Chorus, Reverb, Mix, Direct, Unknown };

struct RoutingEdge {
    RoutingNode from;
    RoutingNode to;
    int level; // XP raw value; -1 means the route/level is undocumented.
    bool open; // A nonzero configured path from this source, not audible signal.
};

struct PatchRouting {
    int structureType = 0; // printed 1..10, zero if unknown
    int outputTone = 0;    // 1..4, zero if unknown
    bool combined = false;
    bool unknown = false;
    std::vector<RoutingEdge> edges;
};

inline PatchRouting patchRouting(const Xp60Patch& patch, ToneIndex selected)
{
    PatchRouting result;
    const auto structure = selected.index() < 2 ? CommonParameter::StructureType12 : CommonParameter::StructureType34;
    const int type = patch.raw(structure);
    if (type < 0 || type > 9) {
        result.unknown = true;
        result.edges.push_back({RoutingNode::Source, RoutingNode::Unknown, -1, false});
        return result;
    }
    result.structureType = type + 1;
    result.combined = type != 0;
    const auto output = result.combined ? (selected.index() < 2 ? ToneIndex::tone2() : ToneIndex::tone4()) : selected;
    result.outputTone = static_cast<int>(output.index()) + 1;
    const auto tone = [&](ToneParameter p) { return patch.raw(output, p); };
    const auto common = [&](CommonParameter p) { return patch.raw(p); };
    const auto add = [&](RoutingNode from, RoutingNode to, int level, bool upstream = true) {
        result.edges.push_back({from, to, level, upstream && level > 0});
    };
    const auto unknown = [&](RoutingNode from) {
        result.unknown = true;
        result.edges.push_back({from, RoutingNode::Unknown, -1, false});
    };
    const int assign = tone(ToneParameter::OutputAssign);
    const int dry = tone(ToneParameter::MixEfxSendLevel);
    if (assign == 2) {
        add(RoutingNode::Source, RoutingNode::Direct, dry);
        return result; // Tone Chorus/Reverb sends are ignored for DIRECT.
    }
    if (assign != 0 && assign != 1) {
        unknown(RoutingNode::Source);
        return result;
    }
    add(RoutingNode::Source, assign == 0 ? RoutingNode::Mix : RoutingNode::Efx, dry);
    const int chorus = tone(ToneParameter::ChorusSendLevel);
    const int reverb = tone(ToneParameter::ReverbSendLevel);
    add(RoutingNode::Source, RoutingNode::Chorus, chorus);
    add(RoutingNode::Source, RoutingNode::Reverb, reverb);
    bool chorusInput = chorus > 0;
    bool reverbInput = reverb > 0;
    if (assign == 1) {
        const int efxAssign = common(CommonParameter::EfxOutputAssign);
        if (efxAssign == 0 || efxAssign == 1) {
            add(RoutingNode::Efx, efxAssign == 0 ? RoutingNode::Mix : RoutingNode::Direct,
                common(CommonParameter::EfxMixOutSendLevel), dry > 0);
            if (efxAssign == 0) {
                const int efxChorus = common(CommonParameter::EfxChorusSendLevel);
                const int efxReverb = common(CommonParameter::EfxReverbSendLevel);
                add(RoutingNode::Efx, RoutingNode::Chorus, efxChorus, dry > 0);
                add(RoutingNode::Efx, RoutingNode::Reverb, efxReverb, dry > 0);
                chorusInput |= dry > 0 && efxChorus > 0;
                reverbInput |= dry > 0 && efxReverb > 0;
            }
        } else {
            unknown(RoutingNode::Efx);
        }
        // EFX DIRECT suppresses EFX sends, not the independent Tone sends.
    }
    const int chorusOutput = common(CommonParameter::ChorusOutput);
    const int chorusLevel = common(CommonParameter::ChorusLevel);
    if (chorusOutput == 0 || chorusOutput == 2)
        add(RoutingNode::Chorus, RoutingNode::Mix, chorusLevel, chorusInput);
    if (chorusOutput == 1 || chorusOutput == 2) {
        add(RoutingNode::Chorus, RoutingNode::Reverb, chorusLevel, chorusInput);
        reverbInput |= chorusInput && chorusLevel > 0;
    }
    if (chorusOutput < 0 || chorusOutput > 2) unknown(RoutingNode::Chorus);
    add(RoutingNode::Reverb, RoutingNode::Mix, common(CommonParameter::ReverbLevel), reverbInput);
    return result;
}

} // namespace xp60studio::xpmodel

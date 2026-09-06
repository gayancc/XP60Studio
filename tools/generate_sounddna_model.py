"""Validate the reviewed Sound DNA artifact and generate the C++ runtime model.

Research candidates never enter this file. A dimension in sounddna_model.json
must already satisfy every analysis gate; editable dimensions must also satisfy
the listening/identity transformation gates. The generated constructor repeats
the gate at runtime, providing defence in depth.
"""
import argparse
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "docs/data/sounddna_model.json"
TARGET = ROOT / "src/sounddna/generated/SoundDnaModel.generated.h"

ANALYSIS_GATES = {
    "independent_patch_count": (lambda value: int(value) >= 30),
    "rater_count": (lambda value: int(value) >= 5),
    "reliability": (lambda value: float(value) >= 0.70),
    "holdout_rank_correlation": (lambda value: float(value) >= 0.65),
    "holdout_pairwise_accuracy": (lambda value: float(value) >= 0.70),
    "calibrated_interval_width": (lambda value: int(value) <= 20),
    "independent_from_other_dimensions": (lambda value: value is True),
}


def cpp_string(value):
    return json.dumps(str(value), ensure_ascii=False)


def validate(data):
    if data.get("format") != "xp60studio.sounddna-model/1":
        raise ValueError("unsupported Sound DNA model format")
    if not data.get("version") or not data.get("feature_schema"):
        raise ValueError("model version and feature_schema are required")
    seen = set()
    for dimension in data.get("dimensions", []):
        categories = dimension.get("categories", [])
        if (not isinstance(categories, list) or any(not isinstance(value, str) or not value for value in categories)
                or len(categories) != len(set(categories))):
            raise ValueError(f"dimension {dimension.get('id')} has invalid categories")
        identity = (dimension.get("id"), tuple(sorted(categories)))
        if not identity[0] or identity in seen:
            raise ValueError(f"duplicate or empty dimension/cohort identity: {identity}")
        seen.add(identity)
        evidence = dimension.get("evidence", {})
        if (not isinstance(evidence.get("independent_patch_count"), int)
                or isinstance(evidence.get("independent_patch_count"), bool)
                or not isinstance(evidence.get("rater_count"), int)
                or isinstance(evidence.get("rater_count"), bool)):
            raise ValueError(f"dimension {identity[0]} evidence counts must be integers")
        failed = [name for name, predicate in ANALYSIS_GATES.items()
                  if name not in evidence or not predicate(evidence[name])]
        if failed:
            raise ValueError(f"published dimension {identity[0]} fails analysis gates: {', '.join(failed)}")
        if evidence.get("transformation_validated"):
            if float(evidence.get("transformation_direction_accuracy", 0)) < 0.75:
                raise ValueError(f"editable dimension {identity[0]} fails direction gate")
            if float(evidence.get("median_identity_preservation", 0)) < 4.0:
                raise ValueError(f"editable dimension {identity[0]} fails identity gate")
        if not isinstance(evidence.get("transformation_validated", False), bool):
            raise ValueError(f"dimension {identity[0]} transformation_validated must be boolean")
        if not dimension.get("label") or not dimension.get("cohort"):
            raise ValueError(f"dimension {identity[0]} requires label and cohort")
        if not math.isfinite(float(dimension.get("latent_intercept", 0))):
            raise ValueError(f"dimension {identity[0]} latent_intercept must be finite")
        score_curve = dimension.get("percentile_curve", [])
        if (len(score_curve) < 2 or any(not isinstance(point, list) or len(point) != 2 for point in score_curve)
                or any(not math.isfinite(float(value)) for point in score_curve for value in point)
                or any(score_curve[i][0] >= score_curve[i + 1][0] for i in range(len(score_curve) - 1))
                or any(score_curve[i][1] > score_curve[i + 1][1] for i in range(len(score_curve) - 1))
                or any(not 0 <= point[1] <= 100 for point in score_curve)
                or score_curve[0][1] > 1 or score_curve[-1][1] < 99
                or score_curve[0][1] == score_curve[-1][1]):
            raise ValueError(f"dimension {identity[0]} has an invalid empirical percentile_curve")
        term_ids = set()
        for term in dimension.get("terms", []):
            feature_id = term.get("feature_id")
            if not feature_id or feature_id in term_ids:
                raise ValueError(f"{identity[0]} has a duplicate or empty feature term")
            term_ids.add(feature_id)
            points = term.get("curve", [])
            if any(not isinstance(point, list) or len(point) != 2 for point in points):
                raise ValueError(f"{identity[0]} term {feature_id} curve points must be [x, y]")
            values = [float(value) for point in points for value in point]
            reference = float(term.get("reference_value", 0.5))
            low = float(term.get("support_low", 0.0))
            high = float(term.get("support_high", 1.0))
            if (len(points) < 2 or any(not math.isfinite(value) for value in values)
                    or any(points[i][0] >= points[i + 1][0] for i in range(len(points) - 1))
                    or all(points[0][1] == point[1] for point in points[1:])
                    or not points[0][0] <= low <= reference <= high <= points[-1][0]):
                raise ValueError(f"{identity[0]} term {feature_id} has an invalid curve/support")
            if not isinstance(term.get("transformable", False), bool):
                raise ValueError(f"{identity[0]} term {feature_id} transformable must be boolean")
            cost = float(term.get("transformation_cost", 1.0))
            maximum_delta = float(term.get("maximum_normalized_delta", 0.25))
            if not math.isfinite(cost) or cost <= 0 or not math.isfinite(maximum_delta) or not 0 < maximum_delta <= 1:
                raise ValueError(f"{identity[0]} term {feature_id} has invalid transformation constraints")
        if not term_ids:
            raise ValueError(f"dimension {identity[0]} has no feature terms")
        interaction_pairs = set()
        for interaction in dimension.get("interactions", []):
            first = interaction.get("first_feature_id")
            second = interaction.get("second_feature_id")
            if first == second or first not in term_ids or second not in term_ids:
                raise ValueError(f"{identity[0]} interaction must reference two distinct feature terms")
            pair = tuple(sorted((first, second)))
            if pair in interaction_pairs:
                raise ValueError(f"{identity[0]} has a duplicate interaction pair")
            interaction_pairs.add(pair)
            if not math.isfinite(float(interaction.get("weight", 0))):
                raise ValueError(f"{identity[0]} interaction weight must be finite")
        if not isinstance(dimension.get("uses_waveform_metadata", False), bool):
            raise ValueError(f"dimension {identity[0]} uses_waveform_metadata must be boolean")
        for field in ("reliability", "holdout_rank_correlation", "holdout_pairwise_accuracy",
                      "transformation_direction_accuracy", "category_coverage", "waveform_metadata_quality"):
            value = float(evidence.get(field, 0))
            if not math.isfinite(value) or not 0 <= value <= 1:
                raise ValueError(f"dimension {identity[0]} evidence {field} must be finite and within 0..1")
        identity_score = float(evidence.get("median_identity_preservation", 0))
        if not math.isfinite(identity_score) or not 0 <= identity_score <= 5:
            raise ValueError(f"dimension {identity[0]} median_identity_preservation must be within 0..5")


def initializer(dimension):
    terms = []
    for term in dimension.get("terms", []):
        points = ", ".join("{%s, %s}" % (float(x), float(y)) for x, y in term["curve"])
        terms.append("{%s, {%s}, %.17g, %.17g, %.17g, %s, %.17g, %.17g}" % (
            cpp_string(term["feature_id"]), points,
            float(term.get("reference_value", 0.5)), float(term.get("support_low", 0.0)),
            float(term.get("support_high", 1.0)), str(bool(term.get("transformable", False))).lower(),
            float(term.get("transformation_cost", 1.0)), float(term.get("maximum_normalized_delta", 0.25))))
    interactions = ["{%s, %s, %s}" % (cpp_string(item["first_feature_id"]),
                                        cpp_string(item["second_feature_id"]), float(item["weight"]))
                    for item in dimension.get("interactions", [])]
    evidence = dimension["evidence"]
    evidence_cpp = "{%d, %d, %.17g, %.17g, %.17g, %d, %s, %s, %.17g, %.17g, %.17g, %.17g}" % (
        int(evidence["independent_patch_count"]), int(evidence["rater_count"]), float(evidence["reliability"]),
        float(evidence["holdout_rank_correlation"]), float(evidence["holdout_pairwise_accuracy"]),
        int(evidence["calibrated_interval_width"]),
        str(bool(evidence["independent_from_other_dimensions"])).lower(),
        str(bool(evidence.get("transformation_validated", False))).lower(),
        float(evidence.get("transformation_direction_accuracy", 0)),
        float(evidence.get("median_identity_preservation", 0)),
        float(evidence.get("category_coverage", 0)), float(evidence.get("waveform_metadata_quality", 0)))
    categories = ", ".join(cpp_string(value) for value in dimension.get("categories", []))
    percentile = ", ".join("{%s, %s}" % (float(x), float(y)) for x, y in dimension["percentile_curve"])
    return "{%s, %s, %s, %s, %.17g, {%s}, {%s}, {%s}, %s, %s, {%s}}" % (
        cpp_string(dimension["id"]), cpp_string(dimension["label"]), cpp_string(dimension["cohort"]),
        cpp_string(dimension.get("reference_template", "")), float(dimension.get("latent_intercept", 0)),
        ", ".join(terms), ", ".join(interactions), percentile, evidence_cpp,
        str(bool(dimension.get("uses_waveform_metadata", False))).lower(), categories)


def generate(data):
    dimensions = ",\n        ".join(initializer(item) for item in data.get("dimensions", []))
    return f'''// Generated by tools/generate_sounddna_model.py; do not edit.
#pragma once
#include "sounddna/SoundDnaKnowledgeModel.h"

namespace xp60studio::sounddna::generated {{
inline SoundDnaKnowledgeModel model()
{{
    return SoundDnaKnowledgeModel({cpp_string(data["version"])}, {cpp_string(data["feature_schema"])},
        {{{dimensions}}}, {cpp_string(data.get("unavailable_reason", ""))});
}}
}} // namespace xp60studio::sounddna::generated
'''


def self_test():
    evidence = {
        "independent_patch_count": 64, "rater_count": 5, "reliability": 0.8,
        "holdout_rank_correlation": 0.7, "holdout_pairwise_accuracy": 0.75,
        "calibrated_interval_width": 15, "independent_from_other_dimensions": True,
        "transformation_validated": False, "transformation_direction_accuracy": 0.0,
        "median_identity_preservation": 0.0, "category_coverage": 0.8,
        "waveform_metadata_quality": 0.0,
    }
    dimension = {
        "id": "test", "label": "Test", "cohort": "test patches",
        "latent_intercept": 0.0, "percentile_curve": [[-1, 0], [0, 50], [1, 100]],
        "terms": [{"feature_id": "tone.1.tone_level", "curve": [[0, -1], [1, 1]],
                   "reference_value": 0.5, "support_low": 0, "support_high": 1,
                   "transformable": False}],
        "interactions": [], "evidence": evidence,
    }
    model = {"format": "xp60studio.sounddna-model/1", "version": "test/1",
             "feature_schema": "xp60-patch-features/2", "dimensions": [dimension]}
    validate(model)
    broken = json.loads(json.dumps(model))
    broken["dimensions"][0]["evidence"]["independent_from_other_dimensions"] = "true"
    try:
        validate(broken)
        raise AssertionError("string boolean passed validation")
    except ValueError:
        pass
    broken = json.loads(json.dumps(model))
    broken["dimensions"][0]["percentile_curve"] = [[0, 50], [1, 50]]
    try:
        validate(broken)
        raise AssertionError("constant percentile curve passed validation")
    except ValueError:
        pass
    print("generate_sounddna_model self-test passed")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    data = json.loads(SOURCE.read_text(encoding="utf-8"))
    validate(data)
    output = generate(data)
    if args.check:
        if not TARGET.exists() or TARGET.read_text(encoding="utf-8") != output:
            raise SystemExit("Sound DNA generated model is stale")
    else:
        TARGET.parent.mkdir(parents=True, exist_ok=True)
        TARGET.write_text(output, encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()

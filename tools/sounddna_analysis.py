"""Reproducible, dependency-free audit and perceptual-rating utilities.

This tool intentionally does not turn feature correlations into published DNA.
It audits exported Patch records, fits category-conditioned Bradley-Terry
scores from blinded pairwise ratings, reports reliability, and evaluates the
same publication gates enforced by the C++ runtime/model generator.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import tempfile
from collections import Counter, defaultdict
from pathlib import Path

GATES = {
    "independent_patch_count": (">=", 30),
    "rater_count": (">=", 5),
    "reliability": (">=", 0.70),
    "holdout_rank_correlation": (">=", 0.65),
    "holdout_pairwise_accuracy": (">=", 0.70),
    "calibrated_interval_width": ("<=", 20),
    "independent_from_other_dimensions": ("is", True),
}


def json_lines(path: Path):
    with path.open(encoding="utf-8") as handle:
        for number, line in enumerate(handle, 1):
            if line.strip():
                yield number, json.loads(line)


def audit_features(path: Path):
    records = [record for _, record in json_lines(path)]
    if not records:
        raise ValueError("feature corpus is empty")
    schemas = Counter(record.get("feature_schema") for record in records)
    ids = [record.get("patch_id") for record in records]
    duplicates = len(ids) - len(set(ids))
    acoustic_keys = []
    for record in records:
        key = tuple((item.get("id"), item.get("categorical_value", item.get("raw")))
                    for item in record.get("features", [])
                    if not str(item.get("id", "")).startswith("common.name.")
                    and (not item.get("tone") or item.get("active", True))
                    and item.get("kind") != "derived")
        acoustic_keys.append(key)
    acoustic_duplicates = len(acoustic_keys) - len(set(acoustic_keys))
    categories = Counter(record.get("verified_category") or "<unlabelled>" for record in records)
    sources = Counter(record.get("source_digest") or "<missing>" for record in records)
    # Compare the feature contract, not patch state. `active` legitimately
    # changes with each patch; its presence and type are part of the schema.
    feature_sets = {tuple((item.get("id"), item.get("kind"), item.get("raw_minimum"),
                           item.get("raw_maximum"), item.get("tone"),
                           type(item.get("active")).__name__, "categorical_value" in item)
                          for item in record.get("features", [])) for record in records}
    report = {
        "format": "xp60studio.corpus-audit/1",
        "source_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "patch_count": len(records),
        "exact_fingerprint_duplicates": duplicates,
        "parameter_duplicates_ignoring_name_and_inactive_tones": acoustic_duplicates,
        "independent_parameter_cluster_upper_bound": len(set(acoustic_keys)),
        "feature_schemas": dict(schemas),
        "consistent_feature_schema": len(feature_sets) == 1 and len(schemas) == 1,
        "categories": dict(categories),
        "source_cohorts": dict(sources),
        "warnings": [],
    }
    if len(records) < 512:
        report["warnings"].append("Fewer than the required 512 onboard preset captures.")
    if categories.get("<unlabelled>", 0):
        report["warnings"].append("Unlabelled patches cannot support category-conditioned publication.")
    if duplicates:
        report["warnings"].append("Split exact/near-duplicate clusters before train/holdout assignment.")
    if acoustic_duplicates > duplicates:
        report["warnings"].append(
            "Additional parameter-identical sounds differ only by name or inactive Tone storage; keep them in one split.")
    if not report["consistent_feature_schema"]:
        report["warnings"].append("Feature rows or schema versions differ across the corpus.")
    return report


def krippendorff_nominal(items):
    usable = [values for values in items if len(values) >= 2]
    if not usable:
        return 0.0
    observed_numerator = 0
    observed_denominator = 0
    totals = Counter()
    for values in usable:
        counts = Counter(values)
        size = len(values)
        observed_numerator += sum(count * (size - count) for count in counts.values())
        observed_denominator += size * (size - 1)
        totals.update(values)
    observed = observed_numerator / observed_denominator if observed_denominator else 0.0
    total = sum(totals.values())
    expected_numerator = sum(count * (total - count) for count in totals.values())
    expected_denominator = total * (total - 1)
    expected = expected_numerator / expected_denominator if expected_denominator else 0.0
    return 1.0 - observed / expected if expected else (1.0 if observed == 0 else 0.0)


def fit_bradley_terry(rows, iterations=1000, tolerance=1e-10):
    patches = sorted({row["left_patch_id"] for row in rows} | {row["right_patch_id"] for row in rows})
    if len(patches) < 2:
        raise ValueError("a rating group needs at least two distinct patches")
    strength = {patch: 1.0 for patch in patches}
    wins = Counter({patch: 1e-9 for patch in patches})  # numerical floor, not a synthetic comparison
    meetings = Counter()
    votes = defaultdict(list)
    neighbours = defaultdict(set)
    for row in rows:
        left, right = row["left_patch_id"], row["right_patch_id"]
        winner = row["winner"].strip().lower()
        if left == right or winner not in {"left", "right", "tie"}:
            raise ValueError("ratings require distinct patches and winner=left/right/tie")
        meetings[tuple(sorted((left, right)))] += 1
        neighbours[left].add(right)
        neighbours[right].add(left)
        if winner == "left":
            wins[left] += 1
            canonical = tuple(sorted((left, right)))
            votes[canonical].append("first" if left == canonical[0] else "second")
        elif winner == "right":
            wins[right] += 1
            canonical = tuple(sorted((left, right)))
            votes[canonical].append("first" if right == canonical[0] else "second")
        else:
            wins[left] += 0.5
            wins[right] += 0.5
            votes[tuple(sorted((left, right)))].append("tie")
    reached = {patches[0]}
    pending = [patches[0]]
    while pending:
        current = pending.pop()
        for other in neighbours[current]:
            if other not in reached:
                reached.add(other)
                pending.append(other)
    if len(reached) != len(patches):
        raise ValueError("pairwise comparison graph is disconnected; percentiles would not share one scale")
    for _ in range(iterations):
        updated = {}
        for patch in patches:
            denominator = 0.0
            for pair, count in meetings.items():
                if patch not in pair:
                    continue
                other = pair[0] if pair[1] == patch else pair[1]
                denominator += count / max(1e-12, strength[patch] + strength[other])
            updated[patch] = wins[patch] / max(1e-12, denominator)
        mean = sum(updated.values()) / len(updated)
        updated = {patch: value / mean for patch, value in updated.items()}
        delta = max(abs(math.log(max(updated[p], 1e-12) / max(strength[p], 1e-12))) for p in patches)
        strength = updated
        if delta < tolerance:
            break
    ordered = sorted(strength.items(), key=lambda item: (item[1], item[0]))
    percentiles = {patch: round(100 * rank / max(1, len(ordered) - 1), 3)
                   for rank, (patch, _) in enumerate(ordered)}
    reliability = krippendorff_nominal(votes.values())
    return strength, percentiles, reliability


def fit_ratings(path: Path):
    groups = defaultdict(list)
    raters = set()
    seen_votes = set()
    with path.open(encoding="utf-8", newline="") as handle:
        for row in csv.DictReader(handle):
            required = {"rater_id", "dimension_id", "category", "left_patch_id", "right_patch_id", "winner"}
            if not required.issubset(row):
                raise ValueError("ratings CSV is missing required columns")
            if any(not row[field].strip() for field in required):
                raise ValueError("ratings CSV contains an empty required value")
            pair = tuple(sorted((row["left_patch_id"], row["right_patch_id"])))
            vote_key = (row["dimension_id"], row["category"], pair, row["rater_id"])
            if vote_key in seen_votes:
                raise ValueError("a rater may judge a patch pair only once per dimension/category")
            seen_votes.add(vote_key)
            raters.add(row["rater_id"])
            groups[(row["dimension_id"], row["category"])].append(row)
    output = {"format": "xp60studio.perceptual-scores/1", "rater_count": len(raters), "groups": []}
    for (dimension, category), rows in sorted(groups.items()):
        strengths, percentiles, reliability = fit_bradley_terry(rows)
        group_raters = {row["rater_id"] for row in rows}
        output["groups"].append({
            "dimension_id": dimension,
            "category": category,
            "comparison_count": len(rows),
            "independent_patch_count": len(strengths),
            "rater_count": len(group_raters),
            "reliability": reliability,
            "strengths": strengths,
            "percentiles": percentiles,
        })
    return output


def average_ranks(values):
    order = sorted(range(len(values)), key=lambda index: (values[index], index))
    ranks = [0.0] * len(values)
    start = 0
    while start < len(order):
        end = start + 1
        while end < len(order) and values[order[end]] == values[order[start]]:
            end += 1
        rank = (start + end - 1) / 2.0
        for position in range(start, end):
            ranks[order[position]] = rank
        start = end
    return ranks


def pearson(left, right):
    if len(left) != len(right) or len(left) < 2:
        return 0.0
    left_mean = sum(left) / len(left)
    right_mean = sum(right) / len(right)
    numerator = sum((a - left_mean) * (b - right_mean) for a, b in zip(left, right))
    left_energy = sum((a - left_mean) ** 2 for a in left)
    right_energy = sum((b - right_mean) ** 2 for b in right)
    denominator = math.sqrt(left_energy * right_energy)
    return numerator / denominator if denominator else 0.0


def spearman(left, right):
    return pearson(average_ranks(left), average_ranks(right))


def analyze_relations(feature_path: Path, perceptual_path: Path):
    records = {record["patch_id"]: record for _, record in json_lines(feature_path)}
    perceptual = json.loads(perceptual_path.read_text(encoding="utf-8"))
    groups = []
    for group in perceptual.get("groups", []):
        category = group["category"]
        scores = group.get("percentiles", {})
        selected = []
        for patch_id, score in scores.items():
            record = records.get(patch_id)
            if not record:
                continue
            record_category = record.get("verified_category") or ""
            if category and category != "global" and record_category != category:
                continue
            selected.append((record, float(score)))
        feature_ids = sorted(set.intersection(*[
            {item["id"] for item in record.get("features", [])
             if item.get("kind") in {"continuous", "ordinal", "derived"}}
            for record, _ in selected
        ])) if selected else []
        targets = [score for _, score in selected]
        vectors = {}
        associations = []
        for feature_id in feature_ids:
            vector = []
            for record, _ in selected:
                item = next(row for row in record["features"] if row["id"] == feature_id)
                vector.append(float(item["value"]))
            vectors[feature_id] = vector
            correlation = spearman(vector, targets)
            associations.append({"feature_id": feature_id, "spearman": round(correlation, 6)})
        associations.sort(key=lambda item: (-abs(item["spearman"]), item["feature_id"]))

        # Candidate interactions are explicitly exploratory. They are limited
        # to the strongest marginal features and still require clustered
        # holdout and controlled transformation validation before publication.
        interactions = []
        collinearity = []
        top = [item["feature_id"] for item in associations[:12]]
        for first_index, first in enumerate(top):
            for second in top[first_index + 1:]:
                first_mean = sum(vectors[first]) / len(vectors[first])
                second_mean = sum(vectors[second]) / len(vectors[second])
                product = [(a - first_mean) * (b - second_mean)
                           for a, b in zip(vectors[first], vectors[second])]
                interactions.append({"first": first, "second": second,
                                     "spearman_product": round(spearman(product, targets), 6)})
                collinearity.append({"first": first, "second": second,
                                     "spearman": round(spearman(vectors[first], vectors[second]), 6)})
        interactions.sort(key=lambda item: (-abs(item["spearman_product"]), item["first"], item["second"]))
        collinearity.sort(key=lambda item: (-abs(item["spearman"]), item["first"], item["second"]))
        groups.append({
            "dimension_id": group["dimension_id"],
            "category": category,
            "matched_patch_count": len(selected),
            "feature_associations": associations,
            "candidate_interactions": interactions[:24],
            "strong_feature_collinearity": [item for item in collinearity if abs(item["spearman"]) >= 0.8],
            "warning": "Exploratory association is not causal transformation evidence.",
        })
    overlap = []
    perceptual_groups = perceptual.get("groups", [])
    for left_index, left in enumerate(perceptual_groups):
        for right in perceptual_groups[left_index + 1:]:
            if left.get("category") != right.get("category") or left.get("dimension_id") == right.get("dimension_id"):
                continue
            common = sorted(set(left.get("percentiles", {})) & set(right.get("percentiles", {})))
            if len(common) < 5:
                continue
            correlation = spearman([float(left["percentiles"][patch]) for patch in common],
                                   [float(right["percentiles"][patch]) for patch in common])
            overlap.append({"category": left.get("category"), "left_dimension": left.get("dimension_id"),
                            "right_dimension": right.get("dimension_id"), "patch_count": len(common),
                            "spearman": round(correlation, 6),
                            "fails_independence_gate": abs(correlation) > 0.8})
    return {
        "format": "xp60studio.sounddna-feature-relations/1",
        "features_sha256": hashlib.sha256(feature_path.read_bytes()).hexdigest(),
        "perceptual_sha256": hashlib.sha256(perceptual_path.read_bytes()).hexdigest(),
        "groups": groups,
        "dimension_overlap": overlap,
    }


def gate_evidence(path: Path):
    data = json.loads(path.read_text(encoding="utf-8"))
    decisions = []
    for candidate in data.get("dimensions", []):
        evidence = candidate.get("evidence", {})
        failures = []
        for field, (operator, threshold) in GATES.items():
            value = evidence.get(field)
            passed = value is not None and ((operator == ">=" and value >= threshold)
                                             or (operator == "<=" and value <= threshold)
                                             or (operator == "is" and value is threshold))
            if not passed:
                failures.append({"field": field, "observed": value, "required": f"{operator} {threshold}"})
        editable = bool(evidence.get("transformation_validated", False)
                        and evidence.get("transformation_direction_accuracy", 0) >= 0.75
                        and evidence.get("median_identity_preservation", 0) >= 4.0)
        decisions.append({"id": candidate.get("id"), "publish": not failures,
                          "editable": not failures and editable, "failures": failures})
    return {"format": "xp60studio.sounddna-gate-report/1",
            "source_sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "decisions": decisions}


def main():
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)
    audit = sub.add_parser("audit-corpus")
    audit.add_argument("features", type=Path)
    ratings = sub.add_parser("fit-ratings")
    ratings.add_argument("ratings", type=Path)
    relations = sub.add_parser("analyze-relations")
    relations.add_argument("features", type=Path)
    relations.add_argument("perceptual", type=Path)
    gate = sub.add_parser("gate-evidence")
    gate.add_argument("evidence", type=Path)
    sub.add_parser("self-test")
    for command in (audit, ratings, relations, gate):
        command.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.command == "self-test":
        rows = [
            {"left_patch_id": "bright", "right_patch_id": "dark", "winner": "left"},
            {"left_patch_id": "bright", "right_patch_id": "dark", "winner": "left"},
            {"left_patch_id": "middle", "right_patch_id": "dark", "winner": "left"},
            {"left_patch_id": "bright", "right_patch_id": "middle", "winner": "left"},
        ]
        _, percentiles, reliability = fit_bradley_terry(rows)
        assert percentiles["bright"] > percentiles["middle"] > percentiles["dark"]
        assert reliability == 1.0
        assert spearman([1, 2, 3, 4], [10, 20, 30, 40]) == 1.0
        assert spearman([1, 2, 3, 4], [40, 30, 20, 10]) == -1.0
        assert krippendorff_nominal([["a", "a"], ["a", "b"]]) < 1.0
        # Tone activity is patch state, not a schema difference.
        with tempfile.TemporaryDirectory() as directory:
            corpus = Path(directory) / "features.jsonl"
            base = {"feature_schema": "test/1", "verified_category": "test", "source_digest": "source"}
            records = []
            for patch_id, active in (("a", True), ("b", False)):
                record = dict(base, patch_id=patch_id,
                              features=[{"id": "tone.1.level", "kind": "continuous", "raw": 64,
                                         "raw_minimum": 0, "raw_maximum": 127, "tone": 1,
                                         "active": active, "categorical_value": ""}])
                records.append(json.dumps(record))
            corpus.write_text("\n".join(records) + "\n", encoding="utf-8")
            assert audit_features(corpus)["consistent_feature_schema"]
        print("sounddna_analysis self-test passed")
        return
    if args.command == "audit-corpus":
        result = audit_features(args.features)
    elif args.command == "fit-ratings":
        result = fit_ratings(args.ratings)
    elif args.command == "analyze-relations":
        result = analyze_relations(args.features, args.perceptual)
    else:
        result = gate_evidence(args.evidence)
    rendered = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")


if __name__ == "__main__":
    main()

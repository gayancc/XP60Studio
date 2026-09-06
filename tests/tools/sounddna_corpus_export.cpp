#include "library/SyxImport.h"
#include "sounddna/PatchFeatureExtractor.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

namespace {

QString featureKind(xp60studio::sounddna::FeatureKind kind)
{
    using xp60studio::sounddna::FeatureKind;
    switch (kind) {
    case FeatureKind::Continuous: return QStringLiteral("continuous");
    case FeatureKind::Ordinal: return QStringLiteral("ordinal");
    case FeatureKind::Categorical: return QStringLiteral("categorical");
    case FeatureKind::Derived: return QStringLiteral("derived");
    }
    return QStringLiteral("unknown");
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    const auto arguments = app.arguments();
    if (arguments.size() != 3) {
        QTextStream(stderr) << "Usage: xp60studio_sounddna_export INPUT.syx OUTPUT.jsonl\n";
        return 2;
    }
    QFile input(arguments.at(1));
    if (!input.open(QIODevice::ReadOnly)) {
        QTextStream(stderr) << "Cannot read " << input.fileName() << "\n";
        return 2;
    }
    const QByteArray bytes = input.readAll();
    const xp60studio::roland::ByteSpan span(reinterpret_cast<const xp60studio::roland::Byte*>(bytes.constData()),
                                            static_cast<std::size_t>(bytes.size()));
    xp60studio::library::SyxImportOptions options;
    options.sourceName = QFileInfo(input).fileName().toStdString();
    const auto imported = xp60studio::library::importSyxStream(span, options);
    if (imported.entries.empty() || !imported.rejected.empty() || !imported.partial.empty()) {
        QTextStream(stderr) << QString::fromStdString(imported.summary()) << "\n";
        return 1;
    }

    QFile output(arguments.at(2));
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream(stderr) << "Cannot write " << output.fileName() << "\n";
        return 2;
    }
    QTextStream stream(&output);
    xp60studio::sounddna::PatchFeatureExtractor extractor;
    for (std::size_t index = 0; index < imported.entries.size(); ++index) {
        const auto& entry = imported.entries[index];
        const auto features = extractor.extract(entry.patch());
        QJsonObject record;
        record.insert(QStringLiteral("format"), QStringLiteral("xp60studio.patch-features/2"));
        record.insert(QStringLiteral("feature_schema"), QString::fromStdString(features.schemaVersion));
        record.insert(QStringLiteral("patch_id"), QString::fromStdString(entry.fingerprint().toHexString()));
        record.insert(QStringLiteral("name"), QString::fromStdString(entry.displayName()));
        record.insert(QStringLiteral("source_name"), QString::fromStdString(entry.provenance().sourceName));
        record.insert(QStringLiteral("source_digest"), QString::fromStdString(entry.provenance().sourceDigest));
        record.insert(QStringLiteral("source_index"), static_cast<qint64>(index));
        record.insert(QStringLiteral("user_number"), entry.provenance().userNumber.value_or(0));
        record.insert(QStringLiteral("verified_category"), QString());
        QJsonArray featureRows;
        for (const auto& feature : features.values) {
            QJsonObject row;
            row.insert(QStringLiteral("id"), QString::fromStdString(feature.id));
            row.insert(QStringLiteral("kind"), featureKind(feature.kind));
            row.insert(QStringLiteral("value"), feature.value);
            row.insert(QStringLiteral("raw"), feature.raw);
            row.insert(QStringLiteral("raw_minimum"), feature.rawMinimum);
            row.insert(QStringLiteral("raw_maximum"), feature.rawMaximum);
            row.insert(QStringLiteral("tone"), feature.toneNumber);
            row.insert(QStringLiteral("editable"), feature.editable);
            row.insert(QStringLiteral("active"), feature.active);
            if (!feature.categoricalValue.empty())
                row.insert(QStringLiteral("categorical_value"), QString::fromStdString(feature.categoricalValue));
            featureRows.push_back(row);
        }
        record.insert(QStringLiteral("features"), featureRows);
        stream << QJsonDocument(record).toJson(QJsonDocument::Compact) << '\n';
    }
    QTextStream(stdout) << imported.entries.size() << " patches exported with schema "
                        << QString::fromUtf8(xp60studio::sounddna::PatchFeatureExtractor::kSchemaVersion.data(),
                                             static_cast<qsizetype>(xp60studio::sounddna::PatchFeatureExtractor::kSchemaVersion.size()))
                        << "\n";
    return 0;
}

#include "services/SnapshotStore.h"

#include "xp60/Xp60Device.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>

#include <algorithm>

namespace xp60studio::services {

namespace {

constexpr const char* kFormat = "xp60studio.instrument-snapshot/1";

QString manifestPathFor(const QString& sysExPath)
{
    const QFileInfo info(sysExPath);
    return info.dir().filePath(info.completeBaseName() + QStringLiteral(".json"));
}

qint64 toEpochSeconds(std::chrono::system_clock::time_point when)
{
    return std::chrono::duration_cast<std::chrono::seconds>(when.time_since_epoch()).count();
}

QString toQt(const std::string& text)
{
    return QString::fromStdString(text);
}

} // namespace

SnapshotSaveResult SnapshotStore::save(const library::InstrumentSnapshot& snapshot,
                                       const QString& path, bool overwrite)
{
    SnapshotSaveResult result;
    result.sysExPath = path;
    result.manifestPath = manifestPathFor(path);
    result.digest = snapshot.digest();
    result.byteCount = snapshot.byteCount();

    if (snapshot.isEmpty()) {
        // An empty file named like a backup is worse than no file: it looks
        // like protection and is none.
        result.error = QStringLiteral("The snapshot is empty; nothing was captured to save.");
        return result;
    }
    if (!overwrite && (QFile::exists(path) || QFile::exists(result.manifestPath))) {
        result.error = QStringLiteral("%1 already exists.").arg(path);
        return result;
    }

    const auto bytes = snapshot.toSysEx();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        result.error = file.errorString();
        return result;
    }
    const auto written = file.write(reinterpret_cast<const char*>(bytes.data()),
                                    static_cast<qint64>(bytes.size()));
    if (written != static_cast<qint64>(bytes.size()) || !file.commit()) {
        result.error = file.errorString();
        return result;
    }

    QJsonObject manifest;
    manifest[QStringLiteral("format")] = QString::fromLatin1(kFormat);
    manifest[QStringLiteral("digest")] = toQt(result.digest);
    manifest[QStringLiteral("byteCount")] = static_cast<qint64>(bytes.size());
    manifest[QStringLiteral("messageCount")] = static_cast<qint64>(snapshot.messageCount());
    manifest[QStringLiteral("label")] = toQt(snapshot.metadata().label);
    manifest[QStringLiteral("deviceName")] = toQt(snapshot.metadata().deviceName);
    manifest[QStringLiteral("appVersion")] = toQt(snapshot.metadata().appVersion);
    manifest[QStringLiteral("note")] = toQt(snapshot.metadata().note);
    manifest[QStringLiteral("capturedAt")] = toEpochSeconds(snapshot.metadata().capturedAt);
    manifest[QStringLiteral("summary")] = toQt(snapshot.summary());

    QJsonArray regions;
    for (const auto& region : snapshot.regions()) {
        QJsonObject entry;
        entry[QStringLiteral("address")] = toQt(region.begin.toHexString());
        entry[QStringLiteral("byteCount")] = static_cast<qint64>(region.byteCount);
        entry[QStringLiteral("messageCount")] = region.messageCount;
        regions.append(entry);
    }
    manifest[QStringLiteral("regions")] = regions;

    QSaveFile manifestFile(result.manifestPath);
    if (!manifestFile.open(QIODevice::WriteOnly)) {
        result.error = manifestFile.errorString();
        return result;
    }
    manifestFile.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
    if (!manifestFile.commit()) {
        result.error = manifestFile.errorString();
        return result;
    }

    result.ok = true;
    return result;
}

SnapshotLoadResult SnapshotStore::load(const QString& path)
{
    SnapshotLoadResult result;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = file.errorString();
        return result;
    }
    const QByteArray bytes = file.readAll();
    const roland::ByteVector data(reinterpret_cast<const roland::Byte*>(bytes.constData()),
                                  reinterpret_cast<const roland::Byte*>(bytes.constData())
                                      + bytes.size());

    library::SnapshotMetadata metadata;
    const QString manifestPath = manifestPathFor(path);
    QFile manifestFile(manifestPath);
    if (manifestFile.open(QIODevice::ReadOnly)) {
        const auto document = QJsonDocument::fromJson(manifestFile.readAll());
        if (document.isObject()) {
            const auto object = document.object();
            result.manifestFound = true;
            result.expectedDigest = object[QStringLiteral("digest")].toString().toStdString();
            metadata.label = object[QStringLiteral("label")].toString().toStdString();
            metadata.deviceName = object[QStringLiteral("deviceName")].toString().toStdString();
            metadata.appVersion = object[QStringLiteral("appVersion")].toString().toStdString();
            metadata.note = object[QStringLiteral("note")].toString().toStdString();
            metadata.capturedAt = std::chrono::system_clock::time_point{
                std::chrono::seconds{object[QStringLiteral("capturedAt")].toInteger()}};
            const auto format = object[QStringLiteral("format")].toString();
            if (format != QString::fromLatin1(kFormat)) {
                result.warnings << QStringLiteral(
                                       "The manifest says format \"%1\", which this build does not "
                                       "know. The .syx itself is still readable.")
                                       .arg(format);
            }
        }
    }
    if (!result.manifestFound) {
        result.warnings << QStringLiteral(
            "No snapshot manifest was found beside this file, so when it was taken and what from "
            "are unknown. The captured data is still usable.");
    }

    const std::vector<roland::RolandModelId> models{xp60::modelId()};
    auto snapshot = library::InstrumentSnapshot::fromSysEx(data, models, metadata);
    result.actualDigest = snapshot.digest();
    result.digestMatched = result.manifestFound && result.expectedDigest == result.actualDigest;
    if (result.manifestFound && !result.digestMatched) {
        // Reported, not enforced: the file may still be exactly what the user
        // needs. What must not happen is restoring it without saying this.
        result.warnings << QStringLiteral(
            "This file no longer matches the checksum recorded when it was saved. It has been "
            "changed or damaged since.");
    }
    if (!snapshot.isClean()) {
        result.warnings << QStringLiteral("%1 message(s) in this file could not be read; it is not "
                                          "a complete capture.")
                               .arg(snapshot.rejectedMessages());
    }
    if (snapshot.isEmpty()) {
        result.error = QStringLiteral("This file contains no Roland Data Set messages.");
        return result;
    }

    result.snapshot = std::move(snapshot);
    result.ok = true;
    return result;
}

std::vector<QString> SnapshotStore::list(const QString& directory)
{
    struct Found
    {
        QString path;
        qint64 capturedAt = 0;
    };
    std::vector<Found> found;

    QDir dir(directory);
    for (const auto& entry : dir.entryInfoList({QStringLiteral("*.syx")}, QDir::Files)) {
        const QString manifestPath = manifestPathFor(entry.absoluteFilePath());
        QFile manifestFile(manifestPath);
        if (!manifestFile.open(QIODevice::ReadOnly)) {
            // A .syx without a manifest is a .syx, not a snapshot.
            continue;
        }
        const auto document = QJsonDocument::fromJson(manifestFile.readAll());
        if (!document.isObject()
            || document.object()[QStringLiteral("format")].toString()
                != QString::fromLatin1(kFormat)) {
            continue;
        }
        found.push_back(
            Found{entry.absoluteFilePath(), document.object()[QStringLiteral("capturedAt")].toInteger()});
    }

    std::stable_sort(found.begin(), found.end(),
                     [](const Found& a, const Found& b) { return a.capturedAt > b.capturedAt; });
    std::vector<QString> paths;
    paths.reserve(found.size());
    for (const auto& entry : found) {
        paths.push_back(entry.path);
    }
    return paths;
}

} // namespace xp60studio::services

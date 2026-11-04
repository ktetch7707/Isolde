#include "settings/ProfileManager.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <QCoreApplication>
#include "home/ComponentSchema.h"

namespace {
bool validPolicy(const QVariant &value)
{
    if (!value.isValid()) return true; // Older profiles default to unrestricted editing.
    if (value.metaType().id() != QMetaType::QVariantMap) return false;
    const auto policy = value.toMap();
    for (auto it = policy.begin(); it != policy.end(); ++it) {
        if (it.key() == "editingAllowed" || it.key() == "installsAllowed") {
            if (it.value().metaType().id() != QMetaType::Bool) return false;
        } else if (it.key() == "restrictedDestinations") {
            if (it.value().metaType().id() != QMetaType::QVariantList && it.value().metaType().id() != QMetaType::QStringList) return false;
            if (it.value().metaType().id() == QMetaType::QVariantList)
                for (const auto &id : it.value().toList()) if (id.metaType().id() != QMetaType::QString) return false;
            const auto ids = it.value().toStringList();
            if (ids.size() > 64) return false;
            for (const auto &id : ids) if (!ComponentSchema::safeId(id) || id == "settings") return false;
        } else return false;
    }
    return true;
}
QVariantMap recoveryPolicy()
{
    return {{"editingAllowed", false}, {"installsAllowed", false}};
}
}

ProfileManager::ProfileManager(const QString &root, QObject *parent) : QObject(parent),
    m_root(root.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) : root)
{
    QFile file(QDir(m_root).filePath("profiles.json"));
    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly)) qFatal("Cannot read profiles; refusing to overwrite them");
        const auto doc = QJsonDocument::fromJson(file.readAll());
        if (!doc.isObject() || !doc.object().value("profiles").isArray()) qFatal("Invalid profiles file");
        m_profiles = doc.object().value("profiles").toArray().toVariantList();
        m_active = doc.object().value("active").toString("default");
    } else {
        m_profiles = {QVariantMap{{"id", "default"}, {"name", "Household"}}};
        if (!save()) qFatal("Cannot initialize profiles");
    }
    bool found = false;
    for (const auto &entry : m_profiles) {
        const auto id = entry.toMap().value("id").toString();
        if (id != "default" && (QUuid(id).isNull() || QUuid(id).toString(QUuid::WithoutBraces) != id)) qFatal("Invalid profile identifier");
        found |= id == m_active;
    }
    if (!found) qFatal("Active profile is missing");
    qApp->setProperty("isoldeProfile", m_active);
    publishPolicy();
}
QString ProfileManager::activeName() const {
    for (const auto &entry : m_profiles) if (entry.toMap().value("id") == m_active) return entry.toMap().value("name").toString();
    return {};
}
QString ProfileManager::layoutPath() const {
    return QDir(m_root).filePath(m_active == "default" ? "home-layout.json" : "profiles/" + m_active + "/home-layout.json");
}
QString ProfileManager::ambientGroup() {
    const auto id = qApp->property("isoldeProfile").toString();
    return id.isEmpty() || id == "default" ? "ambient" : "profiles/" + id + "/ambient";
}
bool ProfileManager::save() {
    QDir().mkpath(m_root);
    QSaveFile file(QDir(m_root).filePath("profiles.json"));
    const auto data = QJsonDocument(QJsonObject{{"active", m_active}, {"profiles", QJsonArray::fromVariantList(m_profiles)}}).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        m_error = "Could not save profiles. Check available storage."; emit changed(); return false;
    }
    m_error.clear(); emit changed(); return true;
}
bool ProfileManager::createProfile(const QString &name) {
    const auto clean = name.trimmed();
    if (clean.isEmpty() || clean.size() > 40) { m_error = "Use a name between 1 and 40 characters."; emit changed(); return false; }
    for (const auto &p : m_profiles) if (p.toMap().value("name").toString().compare(clean, Qt::CaseInsensitive) == 0) { m_error = "That name is already in use."; emit changed(); return false; }
    m_profiles.append(QVariantMap{{"id", QUuid::createUuid().toString(QUuid::WithoutBraces)}, {"name", clean}});
    if (save()) return true;
    m_profiles.removeLast(); emit changed(); return false;
}
bool ProfileManager::renameProfile(const QString &name) {
    const auto clean = name.trimmed();
    if (clean.isEmpty() || clean.size() > 40) { m_error = "Use a name between 1 and 40 characters."; emit changed(); return false; }
    for (const auto &p : m_profiles) if (p.toMap().value("id") != m_active && p.toMap().value("name").toString().compare(clean, Qt::CaseInsensitive) == 0) { m_error = "That name is already in use."; emit changed(); return false; }
    const auto previous = m_profiles;
    for (auto &p : m_profiles) { auto map = p.toMap(); if (map.value("id") == m_active) { map["name"] = clean; p = map; } }
    if (save()) return true;
    m_profiles = previous; emit changed(); return false;
}
bool ProfileManager::selectProfile(const QString &id) {
    if (id == m_active) return true;
    if (qApp->property("isoldeStudioDirty").toBool()) {
        m_error = tr("Apply or discard the Studio draft before switching profiles."); emit changed(); return false;
    }
    bool found = false;
    for (const auto &p : m_profiles) found |= p.toMap().value("id") == id;
    if (!found) return false;
    const auto previous = m_active;
    m_active = id;
    if (!save()) { m_active = previous; emit changed(); return false; }
    qApp->setProperty("isoldeProfile", m_active);
    publishPolicy();
    emit switchRequested(); return true;
}

QVariantMap ProfileManager::activePolicy() const
{
    for (const auto &value : m_profiles) {
        const auto profile = value.toMap();
        if (profile["id"] == m_active) return validPolicy(profile.value("policy")) ? profile.value("policy").toMap() : recoveryPolicy();
    }
    return {};
}
bool ProfileManager::editingAllowed() const { return activePolicy().value("editingAllowed", true).toBool(); }
bool ProfileManager::installsAllowed() const { return activePolicy().value("installsAllowed", true).toBool(); }
QStringList ProfileManager::restrictedDestinations() const { return activePolicy().value("restrictedDestinations").toStringList(); }
void ProfileManager::publishPolicy()
{
    qApp->setProperty("isoldeProfilePolicy", activePolicy());
    for (const auto &value : m_profiles) {
        const auto profile = value.toMap();
        if (profile["id"] == m_active && !validPolicy(profile.value("policy"))) {
            m_error = tr("This profile's household policy is invalid. Editing and installation are disabled. Change a policy control here to replace it; the saved file is kept until then.");
            emit changed(); break;
        }
    }
}
bool ProfileManager::allows(const QString &operation, const QString &destination)
{
    const auto policy = qApp->property("isoldeProfilePolicy").toMap();
    if (operation == "edit") return policy.value("editingAllowed", true).toBool();
    if (operation == "install") return policy.value("installsAllowed", true).toBool();
    if (operation == "destination") return !policy.value("restrictedDestinations").toStringList().contains(destination);
    return false;
}
bool ProfileManager::setPolicy(const QString &key, const QVariant &value)
{
    if (key == "editingAllowed" && !value.toBool() && qApp->property("isoldeStudioDirty").toBool()) {
        m_error = tr("Apply or discard the Studio draft before restricting editing."); emit changed(); return false;
    }
    const auto previous = m_profiles;
    for (auto &entry : m_profiles) {
        auto profile = entry.toMap();
        if (profile["id"] != m_active) continue;
        auto policy = validPolicy(profile.value("policy")) ? profile.value("policy").toMap() : recoveryPolicy(); policy[key] = value;
        profile["policy"] = policy; entry = profile;
    }
    if (!save()) { m_profiles = previous; emit changed(); return false; }
    publishPolicy(); emit changed(); return true;
}
bool ProfileManager::setEditingAllowed(bool allowed) { return setPolicy("editingAllowed", allowed); }
bool ProfileManager::setInstallsAllowed(bool allowed) { return setPolicy("installsAllowed", allowed); }
bool ProfileManager::setRestrictedDestinations(const QStringList &ids)
{
    if (ids.size() > 64) { m_error = tr("Restrict at most 64 destinations."); emit changed(); return false; }
    for (const auto &id : ids) {
        if (!ComponentSchema::safeId(id) || id == "settings") {
            m_error = tr("Choose valid destinations; Settings remains available for recovery."); emit changed(); return false;
        }
    }
    return setPolicy("restrictedDestinations", ids);
}

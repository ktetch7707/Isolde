#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class KodiImporter final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool backupAvailable READ backupAvailable NOTIFY changed)
    Q_PROPERTY(QString backupPath READ backupPath NOTIFY changed)
    Q_PROPERTY(bool importInProgress READ importInProgress NOTIFY changed)
    Q_PROPERTY(QString importStatus READ importStatus NOTIFY changed)
    Q_PROPERTY(QStringList conflicts READ conflicts NOTIFY changed)
    Q_PROPERTY(QString previewText READ previewText NOTIFY changed)
    Q_PROPERTY(bool canImport READ canImport NOTIFY changed)
    Q_PROPERTY(bool canRollback READ canRollback NOTIFY changed)
public:
    explicit KodiImporter(const QString &profile, QObject *parent = nullptr);
    ~KodiImporter() override;

    bool backupAvailable() const { return m_backupAvailable; }
    QString backupPath() const { return m_backupPath; }
    bool importInProgress() const { return m_importInProgress; }
    QString importStatus() const { return m_importStatus; }
    QStringList conflicts() const { return m_conflicts; }
    QString previewText() const { return m_previewText; }
    bool canImport() const { return m_canImport; }
    bool canRollback() const { return m_canRollback; }

    Q_INVOKABLE void scanExistingKodi();
    Q_INVOKABLE void createBackup();
    Q_INVOKABLE void previewImport();
    Q_INVOKABLE void import();
    Q_INVOKABLE void rollback();
    Q_INVOKABLE void clearConflicts();

signals:
    void changed();
    void backupCreated(const QString &path);
    void importCompleted(bool success, const QString &message);
    void rollbackCompleted(bool success, const QString &message);

private:
    QString kodiUserdataDir() const;
    QString backupDir() const;
    QString importStateFile() const;
    bool loadImportState();
    bool saveImportState();
    void resetState();
    QString readSetting(const QString &file, const QString &key) const;

    QString m_profile;
    bool m_backupAvailable = false;
    QString m_backupPath;
    bool m_importInProgress = false;
    QString m_importStatus = "Not started";
    QStringList m_conflicts;
    QString m_previewText;
    bool m_canImport = false;
    bool m_canRollback = false;
    QVariantMap m_scanResults;
};
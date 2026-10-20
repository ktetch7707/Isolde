#pragma once

#include "app/AppRegistry.h"

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QVariantList>

class PageModel;
class StudioSession;

class HomeLayout final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList items READ items NOTIFY layoutChanged)
    Q_PROPERTY(QVariantList components READ components NOTIFY layoutChanged)
    Q_PROPERTY(QVariantList categories READ categories NOTIFY layoutChanged)
    Q_PROPERTY(QVariantList sections READ sections NOTIFY layoutChanged)
    Q_PROPERTY(bool brandVisible READ brandVisible NOTIFY layoutChanged)
    Q_PROPERTY(QString backgroundStart READ backgroundStart NOTIFY layoutChanged)
    Q_PROPERTY(QString backgroundEnd READ backgroundEnd NOTIFY layoutChanged)
    Q_PROPERTY(double tileRoundness READ tileRoundness NOTIFY layoutChanged)
    Q_PROPERTY(QVariantMap appearance READ appearance NOTIFY layoutChanged)
    Q_PROPERTY(QVariantMap composition READ composition NOTIFY layoutChanged)
    Q_PROPERTY(QVariantMap grid READ grid NOTIFY layoutChanged)
    Q_PROPERTY(QVariantList placements READ placements NOTIFY layoutChanged)
    Q_PROPERTY(QVariantMap studioOptions READ studioOptions CONSTANT)
    Q_PROPERTY(QString homePageId READ homePageId NOTIFY layoutChanged)
    Q_PROPERTY(QVariantMap homeAppearance READ homeAppearance NOTIFY layoutChanged)

public:
    QString homePageId() const { return m_homePageId; }
    QVariantMap homeAppearance() const;
    bool load(const QString &defaultPath = {}, const QString &userPath = {});
    Q_INVOKABLE bool addAppTile(const QString &appId);
    Q_INVOKABLE bool addActionTile(const QString &name, const QString &actionType, const QString &target, const QString &categoryId = {}, const QString &openWith = {});
    Q_INVOKABLE bool removeAppTile(const QString &appId);
    Q_INVOKABLE bool moveItem(const QString &id, int offset);
    Q_INVOKABLE bool removeItem(const QString &id);
    Q_INVOKABLE bool addWidget(const QString &widgetType);
    Q_INVOKABLE bool setItemSize(const QString &id, const QString &size);
    Q_INVOKABLE bool setItemOpacity(const QString &id, double opacity);
    Q_INVOKABLE bool setTileOption(const QString &id, const QString &option, bool enabled);
    Q_INVOKABLE bool setTileAppearance(const QString &id, const QString &symbol, const QString &accent);
    Q_INVOKABLE bool setTileLogo(const QString &id, const QString &sourceUrl);
    Q_INVOKABLE bool clearTileLogo(const QString &id);
    Q_INVOKABLE QString addCategory(const QString &name);
    Q_INVOKABLE bool renameCategory(const QString &categoryId, const QString &name);
    Q_INVOKABLE bool removeCategory(const QString &categoryId);
    Q_INVOKABLE bool moveCategory(const QString &categoryId, int offset);
    Q_INVOKABLE bool assignItemCategory(const QString &itemId, const QString &categoryId);
    Q_INVOKABLE bool setBrandVisible(bool visible);
    Q_INVOKABLE bool setBackground(const QString &start, const QString &end);
    Q_INVOKABLE bool setTileRoundness(double roundness);
    Q_INVOKABLE bool applyTheme(const QVariantMap &theme, bool includeSuggestedArtwork = false);
    Q_INVOKABLE bool resetToDefaults();
    [[nodiscard]] QVariantList skins() const;
    Q_INVOKABLE QString saveSkin(const QString &name);
    Q_INVOKABLE bool applySkin(const QString &id);
    Q_INVOKABLE bool renameSkin(const QString &id, const QString &name);
    Q_INVOKABLE bool deleteSkin(const QString &id);
    QVariantList pages() const { return m_pages.toVariantList(); }
    bool isPage() const { return m_owner != nullptr; }
    Q_INVOKABLE QString createPage(const QString &name, const QString &preset = "blank");
    Q_INVOKABLE bool renamePage(const QString &id, const QString &name);
    Q_INVOKABLE bool removePage(const QString &id);
    Q_INVOKABLE QObject *pageModel(const QString &id);
    Q_INVOKABLE bool linkPage(const QString &id, bool asTab);
    Q_INVOKABLE QString addLibraryWidget(const QString &name, const QString &kind = "movies");
    Q_INVOKABLE bool setItemAction(const QString &id, const QVariantMap &action);
    Q_INVOKABLE bool setItemName(const QString &id, const QString &name);
    Q_INVOKABLE bool addButton();

    Q_PROPERTY(QString wallpaperSource READ wallpaperSource NOTIFY layoutChanged)
    Q_PROPERTY(QVariantList skins READ skins NOTIFY skinsChanged)
    Q_PROPERTY(QVariantList pages READ pages NOTIFY layoutChanged)
    Q_PROPERTY(bool isPage READ isPage CONSTANT)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)

public:
    explicit HomeLayout(AppRegistry *registry, QObject *parent = nullptr);
    [[nodiscard]] QVariantList items() const;
    [[nodiscard]] QVariantList components() const;
    void setCollections(const QVariantList &collections);
    void setCaptureInputsReady(bool ready);
    Q_INVOKABLE QVariantList sourceChoices(const QString &itemId) const;
    Q_INVOKABLE bool rebindSource(const QString &itemId, const QString &sourceId);
    [[nodiscard]] AppRegistry *registry() const { return m_registry; }
    [[nodiscard]] QVariantList categories() const;
    [[nodiscard]] QVariantList sections() const;
    [[nodiscard]] bool brandVisible() const;
    [[nodiscard]] QString backgroundStart() const;
    [[nodiscard]] QString backgroundEnd() const;
    [[nodiscard]] double tileRoundness() const;
    [[nodiscard]] QVariantMap appearance() const;
    Q_INVOKABLE bool setAppearance(const QVariantMap &changes);
    [[nodiscard]] QVariantMap composition() const { return m_composition; }
    [[nodiscard]] QVariantList placements() const;
    Q_INVOKABLE QVariantMap grid() const;
    [[nodiscard]] QVariantMap studioOptions() const;
    Q_INVOKABLE bool setGrid(const QVariantMap &value);
    Q_INVOKABLE bool setItemPlacement(const QString &id, const QVariantMap &placement);
    Q_INVOKABLE bool clearItemPlacement(const QString &id);
    Q_INVOKABLE bool duplicateItem(const QString &id);
    Q_INVOKABLE bool setItemPresentation(const QString &id, const QVariantMap &value);
    Q_INVOKABLE bool setSectionPresentation(const QString &id, const QVariantMap &value);
    Q_INVOKABLE QVariantList gridCollisions(const QString &itemId, const QVariantMap &placement) const;
    [[nodiscard]] QString wallpaperSource() const;
    Q_INVOKABLE bool setWallpaper(const QString &sourceUrl);
    Q_INVOKABLE bool setComposition(const QVariantMap &value);

    // T16 Studio bridge: committed bytes + atomic draft apply + focus recap.
    // committedDocument() exposes the current bytes for session.open();
    // applyStudioDraft() validates the candidate, swaps it in memory, and
    // persists through the existing atomic save() (temp → fsync → rename →
    // rollback), restoring the previous model on failure. focusOrigin()
    // restores the exact canvas origin after apply/recovery dialogs.
    Q_INVOKABLE QByteArray committedDocument() const;
    static bool validateStudioDocument(const QByteArray &data, QString *error = nullptr);
    Q_INVOKABLE QObject *beginStudio();
    Q_INVOKABLE bool applyStudioDraft(const QByteArray &draft,
                                      const QString &focusComponentId = {});
    Q_INVOKABLE QString focusOrigin() const { return m_focusOrigin; }
    Q_INVOKABLE void setFocusOrigin(const QString &id) { m_focusOrigin = id; }

    [[nodiscard]] QString errorString() const;
    void setError(const QString &message);

signals:
    void layoutChanged();
    void skinsChanged();
    void errorStringChanged();

private:
    friend class PageModel;
    friend class StudioSession;
    bool loadDocument(const QByteArray &data, bool userDocument);
    bool save();
    QByteArray documentData() const;
    QString skinDirectory() const;
    bool applySkinDocument(QJsonObject document, const QJsonObject &assets);
    bool storePage(const QString &id, const QJsonObject &document);
    void syncPages();
    QString pageKey(const QString &id) const;
    bool isPageId(const QString &id) const;
    QJsonArray m_pages;
    QString m_homePageId;
    QHash<QString, PageModel *> m_pageModels;
    HomeLayout *m_owner = nullptr;
    QString m_pageId;

    AppRegistry *m_registry;
    QJsonArray m_items;
    QJsonArray m_categories;
    bool m_brandVisible = false;
    QString m_backgroundStart = QStringLiteral("#111A32");
    QString m_backgroundEnd = QStringLiteral("#080C18");
    double m_tileRoundness = 0.18;
    QVariantMap m_appearance;
    QVariantMap m_composition;
    QString m_defaultPath;
    QString m_userPath;
    QString m_errorString;
    QVariantList m_collections;
    bool m_captureInputsReady = false;
    QByteArray m_legacyDocument;
    QString m_focusOrigin;
    bool m_draftOnly = false;
    StudioSession *m_studio = nullptr;

};

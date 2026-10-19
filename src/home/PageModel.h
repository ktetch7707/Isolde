#pragma once

#include <QObject>
#include <QJsonObject>
#include <QVariantMap>

class HomeLayout;

/**
 * @brief Page-scoped Home composition.
 *
 * A page model wraps a single page of the multi-page workspace. All page-level
 * composition state (composition, grid, placements, categories, presentation)
 * is scoped to the page and persisted through the owning HomeLayout
 * storePage()/loadDocument() pipeline, so a page export never leaks another
 * page data.
 *
 * Usage: const PageModel *pm = layout->pageModel(id);
 *        pm->composition(); pm->setComposition(map); pm->grid();
 */
class PageModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap composition READ composition WRITE setComposition NOTIFY compositionChanged)
    Q_PROPERTY(QVariantMap grid READ grid WRITE setGrid NOTIFY gridChanged)
    Q_PROPERTY(QVariantList placements READ placements NOTIFY placementsChanged)
    Q_PROPERTY(QVariantMap presentation READ presentation WRITE setPresentation NOTIFY presentationChanged)
    Q_PROPERTY(bool isPage READ isPage NOTIFY isPageChanged)

public:
    explicit PageModel(HomeLayout *owner, const QString &id, QObject *parent = nullptr);
    [[nodiscard]] HomeLayout *owner() const { return m_owner; }
    [[nodiscard]] QString id() const { return m_pageId; }
    [[nodiscard]] bool isPage() const { return m_pageId.startsWith(m_owner->pageKey(m_pageId)); }

    [[nodiscard]] QVariantMap composition() const;
    bool setComposition(const QVariantMap &value);
    [[nodiscard]] QVariantMap grid() const;
    bool setGrid(const QVariantMap &value);
    [[nodiscard]] QVariantList placements() const;
    [[nodiscard]] QVariantMap presentation() const;
    bool setPresentation(const QVariantMap &value);
    [[nodiscard]] QVariantList items() const;
    [[nodiscard]] QVariantList sections() const;
    [[nodiscard]] QString backgroundStart() const;
    [[nodiscard]] QString backgroundEnd() const;
    [[nodiscard]] QVariantMap appearance() const;
    Q_INVOKABLE bool setAppearance(const QVariantMap &changes);
    [[nodiscard]] QString wallpaperSource() const;
    [[nodiscard]] double tileRoundness() const;
    [[nodiscard]] bool brandVisible() const;
    Q_INVOKABLE bool addWidget(const QString &widgetType);
    Q_INVOKABLE bool addButton();
    Q_INVOKABLE bool setItemName(const QString &id, const QString &name);
    Q_INVOKABLE bool setItemAction(const QString &id, const QVariantMap &action);
    Q_INVOKABLE bool setItemPresentation(const QString &id, const QVariantMap &value);
    Q_INVOKABLE QString addLibraryWidget(const QString &name, const QString &kind = "movies");
    Q_INVOKABLE bool setItemPlacement(const QString &id, const QVariantMap &placement);
    Q_INVOKABLE bool clearItemPlacement(const QString &id);
    /** Page-scoped library sources (library sections + library widgets) for universal search. */
    Q_INVOKABLE QVariantList pageSearchSources() const;

    /** Load a page-scoped document (theme/composition/categories/components) into this model. */
    Q_INVOKABLE bool loadDocument(const QByteArray &data, bool userDocument = false);
    /** Persist this page's document through the owning HomeLayout storePage(). */
    Q_INVOKABLE bool storePage();
    [[nodiscard]] QString errorString() const;
    [[nodiscard]] QVariantMap document() const;

signals:
    void compositionChanged();
    void gridChanged();
    void placementsChanged();
    void presentationChanged();
    void isPageChanged();
    void documentLoaded();

private:
    HomeLayout *m_owner = nullptr;
    QString m_pageId;
    QJsonObject m_pageDocument;
    bool m_documentLoaded = false;

    [[nodiscard]] QVariantList resolveItems(const QJsonArray &components) const;
    [[nodiscard]] QVariantMap themeDefaults() const;
};

E /****************************************************************************
   ** $QT_BEGIN_HEADER**
   ** $QT_END_HEADER** ****************************************************************************/

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariantMap>
#include <QtTest/QtTest>
#include "src/sources/SourceManager.h"

    // Mock AppRegistry for testing
    class MockAppRegistry : public AppRegistry {
  public:
    MockAppRegistry() = default;
    ~MockAppRegistry() override = default;
};

class SourceManagerTests : public QObject {
    Q_OBJECT
  public:
    SourceManagerTests() = default;
    ~SourceManagerTests() override = default;

  private slots:
    void init() {
        QStandardPaths::setTestModeEnabled(true);
        m_registry = new MockAppRegistry;
        m_manager = new SourceManager(m_registry);
    }
    void cleanup() {
        delete m_manager;
        delete m_registry;
    }

    void test_constructor();
    void test_automatic_sources();
    void test_items_sections_sources();
    void test_error_string();
    void test_refresh();
    void test_policy();
    void test_visible_source_count();
    void test_hidden_sources();
    void test_set_automatic_sources();
    void test_set_policy();
    void test_source_action();

  private:
    MockAppRegistry* m_registry;
    SourceManager* m_manager;
};

void SourceManagerTests::init() {
    QStandardPaths::setTestModeEnabled(true);
    m_registry = new MockAppRegistry;
    m_manager = new SourceManager(m_registry);
}

void SourceManagerTests::cleanup() {
    delete m_manager;
    delete m_registry;
}

void SourceManagerTests::test_constructor() {
    QCOMPARE(m_manager->errorString(), QString());
    QCOMPARE(m_manager->revision(), 0);
    QCOMPARE(m_manager->visibleSourceCount(), 0);
    QCOMPARE(m_manager->automaticSourcesEnabled(), true);
}

void SourceManagerTests::test_automatic_sources() {
    QCOMPARE(m_manager->automaticSourcesEnabled(), true);

    m_manager->setAutomaticSourcesEnabled(false);
    QCOMPARE(m_manager->automaticSourcesEnabled(), false);

    m_manager->setAutomaticSourcesEnabled(true);
    QCOMPARE(m_manager->automaticSourcesEnabled(), true);
}

void SourceManagerTests::test_items_sections_sources() {
    QVariantList items = m_manager->items();
    QVariantList sections = m_manager->sections();
    QVariantList sources = m_manager->sources();

    // Initially empty
    QCOMPARE(items.size(), 0);
    QCOMPARE(sections.size(), 0);
    QCOMPARE(sources.size(), 0);
}

void SourceManagerTests::test_error_string() {
    QCOMPARE(m_manager->errorString(), QString());
}

void SourceManagerTests::test_refresh() {
    m_manager->refresh();
    // Should not crash; may be async
}

void SourceManagerTests::test_policy() {
    QVariantMap policy = m_manager->policy();
    // Policy should be valid QVariantMap
    QVERIFY(!policy.isNull());
}

void SourceManagerTests::test_visible_source_count() {
    QCOMPARE(m_manager->visibleSourceCount(), 0);
}

void SourceManagerTests::test_hidden_sources() {
    QVariantList hidden = m_manager->hiddenSources();
    QCOMPARE(hidden.size(), 0);
}

void SourceManagerTests::test_set_automatic_sources() {
    m_manager->setAutomaticSourcesEnabled(true);
    QCOMPARE(m_manager->automaticSourcesEnabled(), true);
}

void SourceManagerTests::test_set_policy() {
    QVariantMap changes = {{"key", "value"}};
    m_manager->setPolicy(changes);
    // Should not crash
}

void SourceManagerTests::test_source_action() {
    QVariantMap action = m_manager->sourceAction("nonexistent");
    // Should return empty or valid map
    QVERIFY(action.isNull() || !action.isEmpty());
}

QTEST_APPLESS_MAIN(SourceManagerTests)
#include "tst_source_manager.moc"
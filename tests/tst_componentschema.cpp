/****************************************************************************
** $QT_BEGIN_HEADER**
** $QT_END_HEADER**
****************************************************************************/

#include <QtTest/QtTest>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include "../src/home/ComponentSchema.h"

class ComponentSchemaTests : public QObject
{
    Q_OBJECT

private slots:
    void test_component_valid_components();
    void test_component_invalid_compositions();
    void test_component_grid_operations();
    void test_component_safe_id();
    void test_component_nesting_depth();
};

void ComponentSchemaTests::test_component_valid_components()
{
    // Test valid component creation
    QVariantMap comp = ComponentSchema::component({{"id", "test1"}, {"kind", "widget"}, {"widgetType", "button"}});
    QVERIFY(!comp.isEmpty());
    QCOMPARE(comp.value("id").toString(), QString("test1"));
    QCOMPARE(comp.value("kind").toString(), QString("widget"));
}

void ComponentSchemaTests::test_component_invalid_compositions()
{
    // Test invalid compositions are rejected
    QVariantMap invalid = ComponentSchema::component({{"id", ""}, {"kind", "widget"}});
    // Empty id should be invalid
    QVERIFY(ComponentSchema::safeId("").isEmpty() || !ComponentSchema::safeId(""));

    // Test malformed composition
    QVariantMap malformed;
    malformed["verticalScrolling"] = "true";
    QVERIFY(!ComponentSchema::validComposition(malformed));

    malformed["verticalScrolling"] = true;
    malformed["grid"] = QVariantMap{{"rows", 100}, {"columns", 4}, {"mode", "screen"}};
    QVERIFY(!ComponentSchema::validComposition(malformed));
}

void ComponentSchemaTests::test_component_grid_operations()
{
    // Test grid operations
    QJsonArray items;
    QJsonObject item;
    item["id"] = "test";
    item["source"] = "test";
    items.append(item);

    QVariantMap composition;
    composition["schemaVersion"] = 1;
    composition["enabled"] = true;
    composition["grid"] = QJsonObject{{"columns", 4}, {"rows", 3}, {"mode", "screen"}};

    // Test valid composition
    QVERIFY(ComponentSchema::validComposition(composition));

    // Test grid with invalid rows
    composition["grid"] = QJsonObject{{"rows", 100}, {"columns", 4}, {"mode", "screen"}};
    QVERIFY(!ComponentSchema::validComposition(composition));
}

void ComponentSchemaTests::test_component_safe_id()
{
    // Test safeId function
    QVERIFY(ComponentSchema::safeId("valid-id"));
    QVERIFY(ComponentSchema::safeId("valid_id"));
    QVERIFY(!ComponentSchema::safeId(""));
    QVERIFY(!ComponentSchema::safeId("with spaces"));
    QVERIFY(!ComponentSchema::safeId("with.dots"));
    QVERIFY(!ComponentSchema::safeId(".."));
}

void ComponentSchemaTests::test_component_nesting_depth()
{
    // Test max nesting depth
    // MaxComponentNestingDepth is 4
    QVERIFY(ComponentSchema::MaxComponentNestingDepth == 4);
}
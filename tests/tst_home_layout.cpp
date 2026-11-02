/****************************************************************************
** $QT_BEGIN_HEADER**
** $QT_END_HEADER**
****************************************************************************/

#include <QtTest/QtTest>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include "../src/home/HomeLayout.h"

class HomeLayoutTests : public QObject
{
    Q_OBJECT

private slots:
    void test_layout_creation();
    void test_layout_items_components();
    void test_layout_max_pages();
    void test_layout_root_page();
};

void HomeLayoutTests::test_layout_creation()
{
    // Create a test registry
    // Note: AppRegistry needs to be set up first
    QWidget *parent = nullptr;
    HomeLayout layout(nullptr);
    
    // Test default creation
    QVERIFY(layout.homePageId().isEmpty());
    QVERIFY(layout.rootPageVisible());
}

void HomeLayoutTests::test_layout_items_components()
{
    HomeLayout layout(nullptr);
    
    // Test initial items and components are empty
    QCOMPARE(layout.items().size(), 0);
    QCOMPARE(layout.components().size(), 0);
}

void HomeLayoutTests::test_layout_max_pages()
{
    HomeLayout layout(nullptr);
    
    // Test MaxPages constant
    QCOMPARE(HomeLayout::MaxPages, 64);
}

void HomeLayoutTests::test_layout_root_page()
{
    HomeLayout layout(nullptr);
    
    // Test root page name is empty initially
    QVERIFY(layout.rootPageName().isEmpty());
    QVERIFY(layout.rootPageVisible());
}
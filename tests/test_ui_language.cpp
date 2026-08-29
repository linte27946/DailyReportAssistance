#include <QtTest>
#include <QComboBox>
#include <QLabel>

#include "ui/UiLanguage.h"

class TestUiLanguage : public QObject {
    Q_OBJECT

private slots:
    void selectsRequestedLanguage()
    {
        UiLanguage::setLanguage("en");
        QCOMPARE(UiLanguage::language(), QString("en"));
        QCOMPARE(UiLanguage::text("Settings", "设置"), QString("Settings"));

        UiLanguage::setLanguage("zh-CN");
        QCOMPARE(UiLanguage::language(), QString("zh-CN"));
        QCOMPARE(UiLanguage::text("Settings", "设置"), QString("设置"));
    }

    void refreshesBoundWidgetsWithoutChangingStoredValues()
    {
        QLabel label;
        QComboBox combo;
        combo.addItem("", "en");
        combo.addItem("", "zh-CN");

        UiLanguage::setLanguage("en");
        UiLanguage::bindText(&label, "Reports", "报告中心");
        UiLanguage::bindComboItem(&combo, 0, "English", "English");
        UiLanguage::bindComboItem(&combo, 1, "Simplified Chinese", "简体中文");
        QCOMPARE(label.text(), QString("Reports"));
        QCOMPARE(combo.itemText(1), QString("Simplified Chinese"));

        combo.setCurrentIndex(1);
        UiLanguage::setLanguage("zh-CN");
        UiLanguage::apply(&label);
        UiLanguage::apply(&combo);
        QCOMPARE(label.text(), QString("报告中心"));
        QCOMPARE(combo.itemText(1), QString("简体中文"));
        QCOMPARE(combo.currentData().toString(), QString("zh-CN"));
    }
};

QTEST_MAIN(TestUiLanguage)
#include "test_ui_language.moc"

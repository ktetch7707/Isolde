#pragma once

#include <QObject>

class DisplaySettings final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int safeArea READ safeArea NOTIFY changed)
    Q_PROPERTY(int uiScale READ uiScale NOTIFY changed)
    Q_PROPERTY(bool reduceMotion READ reduceMotion NOTIFY changed)

public:
    explicit DisplaySettings(QObject *parent = nullptr);

    [[nodiscard]] int safeArea() const;
    [[nodiscard]] int uiScale() const;
    [[nodiscard]] bool reduceMotion() const;

    Q_INVOKABLE void adjustSafeArea(int delta);
    Q_INVOKABLE void adjustUiScale(int delta);
    Q_INVOKABLE void toggleReduceMotion();
    Q_INVOKABLE void reset();

signals:
    void changed();

private:
    void load();
    void save() const;
    static int clamp(int value, int minimum, int maximum);

    int m_safeArea = 4;
    int m_uiScale = 100;
    bool m_reduceMotion = false;
};

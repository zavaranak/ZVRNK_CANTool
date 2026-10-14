#pragma once

#include <QColor>
#include <QPalette>
#include <QString>

enum class ThemeMode
{
    System,
    Light,
    Dark,
};

struct ThemeTokens
{
    QColor page;
    QColor header;
    QColor headerText;
    QColor panel;
    QColor panelRaised;
    QColor inset;
    QColor text;
    QColor textMuted;
    QColor textSubtle;
    QColor border;
    QColor borderStrong;
    QColor accent;
    QColor accentHover;
    QColor accentPressed;
    QColor accentOn;
    QColor selection;
    QColor focusRing;
    QColor success;
    QColor successSurface;
    QColor warning;
    QColor warningSurface;
    QColor danger;
    QColor dangerSurface;
    QColor inactive;
    QColor inactiveSurface;
    QColor logSurface;
    QColor logText;

    static ThemeTokens light();
    static ThemeTokens dark();
    static ThemeTokens forMode(ThemeMode mode);

    QPalette palette() const;
    QString styleSheet() const;
};

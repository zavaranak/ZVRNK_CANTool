#pragma once

#include <QString>

enum class AppLanguage
{
    English,
    Vietnamese,
};

QString localize(AppLanguage language, const QString& source);

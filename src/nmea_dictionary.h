#pragma once

#include <QByteArray>
#include <QString>

namespace NmeaDictionary
{
// Decodes fields from add-in standards/NMEA2000_dictionary.csv.
// Returns an empty string when the schema is not available for the PGN.
QString decode(quint32 pgn, const QByteArray& payload);
QString path();
}

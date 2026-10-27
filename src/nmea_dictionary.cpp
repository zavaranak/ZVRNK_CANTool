#include "nmea_dictionary.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>

namespace {
struct Field { QString name, formula, unit, values, unavailable, endian; int start=0, length=0, bitOffset=0, bitLength=0; bool signedValue=false; };
QVector<Field> load(quint32 pgn) {
    QVector<Field> result;
    QFile file(NmeaDictionary::path());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return result;
    QTextStream stream(&file); stream.setEncoding(QStringConverter::Utf8);
    const auto csv = [](const QString& line) { QStringList out; QString cell; bool quoted=false; for (int i=0;i<line.size();++i) { const QChar c=line.at(i); if(c=='"'){ if(quoted&&i+1<line.size()&&line.at(i+1)=='"'){cell+='"';++i;} else quoted=!quoted; } else if(c==','&&!quoted){out<<cell;cell.clear();} else cell+=c; } out<<cell; return out; };
    stream.readLine();
    while (!stream.atEnd()) {
        const QStringList c=csv(stream.readLine()); if(c.size()<27 || c.value(2).toUInt()!=pgn) continue;
        Field f; f.name=c.value(9).trimmed(); f.start=c.value(10).toInt(); f.length=c.value(11).toInt(); f.bitOffset=c.value(12).toInt(); f.bitLength=c.value(13).toInt(); f.signedValue=c.value(14).compare("Yes",Qt::CaseInsensitive)==0; f.endian=c.value(15).trimmed(); f.formula=c.value(16).trimmed(); f.unit=c.value(17).trimmed(); f.unavailable=c.value(20).trimmed(); f.values=c.value(24).trimmed();
        if(f.name.isEmpty()||f.name.compare("Spare",Qt::CaseInsensitive)==0||f.name.compare("Reserved",Qt::CaseInsensitive)==0||f.name.compare("Data",Qt::CaseInsensitive)==0) continue;
        bool duplicate=false; for(const Field& old:result) if(old.name==f.name){duplicate=true;break;} if(!duplicate) result.push_back(f);
    }
    return result;
}
quint64 bits(const QByteArray& p, const Field& f) { quint64 v=0; for(int n=0;n<f.bitLength;++n){const int bit=f.start*8+f.bitOffset+n; if(bit/8<p.size() && (quint8(p.at(bit/8))&(1<<(bit%8)))) v|=(quint64(1)<<n);} return v; }
QString valueText(const Field& f, quint64 raw) {
    if(!f.unavailable.isEmpty() && f.unavailable.toULongLong()==raw) return "N/A";
    if(!f.values.isEmpty()){ for(const QString& item:f.values.split(';',Qt::SkipEmptyParts)){const int eq=item.indexOf('='); if(eq>0&&item.left(eq).trimmed().toULongLong()==raw)return item.mid(eq+1).trimmed();} }
    qint64 signedRaw=qint64(raw); if(f.signedValue&&f.bitLength>0&&f.bitLength<64&&(raw&(quint64(1)<<(f.bitLength-1)))) signedRaw=qint64(raw|(~quint64(0)<<f.bitLength)); double value=double(f.signedValue?signedRaw:qint64(raw)); const QRegularExpression mul(R"(x\s*\*\s*([0-9.+-]+))"); const auto m=mul.match(f.formula); if(m.hasMatch()) value*=m.captured(1).toDouble(); const QRegularExpression div(R"(/\s*([0-9.+-]+)\s*$)"); const auto d=div.match(f.formula); if(d.hasMatch()) value/=d.captured(1).toDouble();
    QString text=QString::number(value,'f',value==qRound(value)?0:3); if(!f.unit.isEmpty()) text+=" "+f.unit; return text;
}
}
namespace NmeaDictionary {
QString path(){ static QString cached; if(!cached.isEmpty())return cached; const QString file="add-in standards/NMEA2000_dictionary.csv"; const QStringList candidates={QCoreApplication::applicationDirPath()+"/"+file,QDir::currentPath()+"/"+file,QCoreApplication::applicationDirPath()+"/../../"+file,QStringLiteral("D:/Programming/Tools/ZVRNK_CANTool/")+file}; for(const QString& c:candidates)if(QFile::exists(c)){cached=QDir::toNativeSeparators(c);break;} return cached; }
QString decode(quint32 pgn,const QByteArray& payload){ static QHash<quint32,QVector<Field>> cache; if(!cache.contains(pgn))cache.insert(pgn,load(pgn)); const auto fields=cache.value(pgn); if(fields.isEmpty())return {}; QStringList out; for(const Field& f:fields){if(f.start+f.length>payload.size()||f.bitLength<=0)continue; out<<f.name+" = "+valueText(f,bits(payload,f));} return out.join("; "); }
}

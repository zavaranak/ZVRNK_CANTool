#include "nmea2000.h"

#include <QStringList>
#include <QtMath>

namespace {
quint8 b(const QByteArray& v, int i) { return i >= 0 && i < v.size() ? quint8(v.at(i)) : 0; }
quint16 u16(const QByteArray& v, int i) { return quint16(b(v, i)) | (quint16(b(v, i + 1)) << 8); }
quint32 u32(const QByteArray& v, int i) { return quint32(b(v, i)) | (quint32(b(v, i + 1)) << 8) | (quint32(b(v, i + 2)) << 16) | (quint32(b(v, i + 3)) << 24); }
QString fmt(quint32 p) { return QString("%1 / 0x%2").arg(p).arg(p, 5, 16, QLatin1Char('0')).toUpper(); }
QString hex(const QByteArray& v) { QStringList out; for (char c : v) out << QString::number(quint8(c), 16).rightJustified(2, QLatin1Char('0')).toUpper(); return out.join(' '); }
bool unavailable(quint32 v, int bits) { return v == (bits == 16 ? 0xFFFFU : bits == 32 ? 0xFFFFFFFFU : 0x7FFFFFFFU); }
}
namespace Nmea2000 {
QString pgnName(quint32 p) { switch (p) {
case 59392:return "ISO Acknowledgement"; case 59904:return "ISO Request"; case 60160:return "ISO Transport Protocol, Data Transfer"; case 60416:return "ISO Transport Protocol, Connection Management"; case 60928:return "ISO Address Claim"; case 126208:return "NMEA Group Function"; case 126464:return "PGN List"; case 126996:return "Product Information"; case 126998:return "Configuration Information"; case 127250:return "Vessel Heading"; case 127488:return "Engine Parameters, Rapid Update"; case 127489:return "Engine Parameters, Dynamic"; case 127505:return "Fluid Level"; case 128259:return "Speed, Water Referenced"; case 128267:return "Water Depth"; case 129025:return "Position, Rapid Update"; case 129026:return "COG & SOG, Rapid Update"; case 129029:return "GNSS Position Data"; case 129038:return "AIS Class A Position Report"; case 129039:return "AIS Class B Position Report"; case 129540:return "GNSS Sats in View"; case 130306:return "Wind Data"; case 130312:return "Temperature"; case 130313:return "Humidity"; case 130316:return "Temperature, Extended Range"; default:return "Unknown PGN"; } }
bool isFastPacketPgn(quint32 p) { switch (p) { case 127250:case 127488:case 127505:case 128259:case 128267:case 129025:case 129026:case 130306:case 130312:case 130313:case 130316:case 59392:case 59904:case 60160:case 60416:case 60928:return false; default:return p >= 126720; } }
QString decodePayload(quint32 p, const QByteArray& v) {
if (p==59904&&v.size()>=3)return QString("Requested PGN %1").arg(fmt(u32(v,0)&0xFFFFFFU));
if (p==59392&&v.size()>=8)return QString("Ack control 0x%1 for PGN %2").arg(b(v,0),2,16,QLatin1Char('0')).arg(fmt(u32(v,5)&0xFFFFFFU));
if (p==60928&&v.size()>=8){quint64 n=0;for(int x=7;x>=0;--x)n=(n<<8)|b(v,x);return QString("NAME 0x%1").arg(n,16,16,QLatin1Char('0')).toUpper();}
if (p==127250&&v.size()>=7&&!unavailable(u16(v,1),16))return QString("Heading %1 deg").arg(qRadiansToDegrees(u16(v,1)*0.0001),0,'f',2);
if (p==127488&&v.size()>=5)return QString("Engine %1: %2 rpm, boost %3 Pa").arg(b(v,0)).arg(u16(v,1)*0.25,0,'f',1).arg(u16(v,3)*100.0,0,'f',0);
if (p==128259&&v.size()>=5)return QString("Water %1 m/s, ground %2 m/s").arg(u16(v,1)*0.01,0,'f',2).arg(u16(v,3)*0.01,0,'f',2);
if (p==128267&&v.size()>=5&&!unavailable(u32(v,1),32))return QString("Depth %1 m").arg(u32(v,1)*0.01,0,'f',2);
if (p==129025&&v.size()>=8&&!unavailable(u32(v,0),32)&&!unavailable(u32(v,4),32))return QString("Lat %1 deg, Lon %2 deg").arg(qint32(u32(v,0))*1e-7,0,'f',7).arg(qint32(u32(v,4))*1e-7,0,'f',7);
if (p==129026&&v.size()>=6)return QString("COG %1 deg, SOG %2 m/s").arg(qRadiansToDegrees(u16(v,2)*0.0001),0,'f',2).arg(u16(v,4)*0.01,0,'f',2);
if (p==130306&&v.size()>=5)return QString("Wind %1 m/s at %2 deg").arg(u16(v,1)*0.01,0,'f',2).arg(qRadiansToDegrees(u16(v,3)*0.0001),0,'f',2);
if(p==126996||p==126998||p==129029||p==129540)return QString("%1 bytes reassembled").arg(v.size()); return QString("Payload %1").arg(hex(v)); }
Info Decoder::process(const RawFrame& f){Info i;i.canId=f.canId;if(!f.extended||f.canId>0x1FFFFFFFU){i.transport="IGNORED / STANDARD";return i;}const int pf=int((f.canId>>16)&0xFFU),ps=int((f.canId>>8)&0xFFU);i.valid=true;i.priority=int((f.canId>>26)&7U);i.source=int(f.canId&0xFFU);i.destination=pf<240?ps:255;i.pgn=(f.canId>>8)&0x1FFFFU;if(pf<240)i.pgn&=0x1FF00U;i.pgnText=fmt(i.pgn);i.pgnName=pgnName(i.pgn);if(i.pgn==60416&&f.data.size()>=8&&b(f.data,0)==0x20){i.transport="ISO-TP BAM";return i;}if(i.pgn==60160){i.transport="ISO-TP DATA";i.payload=f.data.mid(1);i.decoded=decodePayload(i.pgn,i.payload);return i;}if(!isFastPacketPgn(i.pgn)){i.transport="SINGLE";i.payload=f.data;i.decoded=decodePayload(i.pgn,i.payload);return i;}if(f.data.size()<2){i.transport="FAST / INVALID";return i;}const quint8 h=b(f.data,0);const int seq=h>>5,index=h&0x1F;const QString key=QString("%1|%2|%3|%4").arg(i.pgn).arg(i.source).arg(i.destination).arg(seq);if(index==0){FastPacketState s;s.expectedLength=qMin(223,int(b(f.data,1)));s.nextFrame=1;s.sequence=seq;s.payload=f.data.mid(2,s.expectedLength);if(s.payload.size()>=s.expectedLength){i.transport=QString("FAST COMPLETE %1/%1").arg(s.expectedLength);i.payload=s.payload;i.decoded=decodePayload(i.pgn,i.payload);}else{i.transport=QString("FAST %1/%2").arg(s.payload.size()).arg(s.expectedLength);fastPackets_.insert(key,s);}return i;}auto it=fastPackets_.find(key);if(it==fastPackets_.end()||it->nextFrame!=index){i.transport=QString("FAST ORPHAN #%1").arg(index);if(it!=fastPackets_.end())fastPackets_.erase(it);return i;}FastPacketState&s=it.value();s.payload.append(f.data.mid(1));s.nextFrame=index+1;if(s.payload.size()>=s.expectedLength){i.transport=QString("FAST COMPLETE %1/%1").arg(s.expectedLength);i.payload=s.payload.left(s.expectedLength);i.decoded=decodePayload(i.pgn,i.payload);fastPackets_.erase(it);}else i.transport=QString("FAST %1/%2").arg(s.payload.size()).arg(s.expectedLength);return i;}
void Decoder::reset(){fastPackets_.clear();}
}

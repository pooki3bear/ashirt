#ifndef TYPE_STREAMS_H
#define TYPE_STREAMS_H

#include <QDataStream>

#include "models/tag.h"
#include "config/server_item.h"
#include "models/server_setting.h"
#include "forms/connections/connection_cell_data.h"

QDataStream& operator<<(QDataStream& out, const model::Tag& v) {
  out << v.tagName << v.id << v.serverTagId;
  return out;
}

QDataStream& operator>>(QDataStream& in, model::Tag& v) {
  in >> v.tagName;
  in >> v.id;
  in >> v.serverTagId;
  return in;
}

QDataStream& operator<<(QDataStream& out, const std::vector<model::Tag>& v) {
  out << int(v.size());
  for (auto tag : v) {
    out << tag;
  }
  return out;
}

QDataStream& operator>>(QDataStream& in, std::vector<model::Tag>& v) {
  int qty;
  in >> qty;
  v.reserve(qty);
  for(int i = 0; i < qty; i++) {
    model::Tag t;
    in >> t;
    v.push_back(t);
  }
  return in;
}

QDataStream& operator<<(QDataStream& out, const model::ServerSetting& v) {
  out << v.activeOperationName << v.activeOperationSlug;
  return out;
}

QDataStream& operator>>(QDataStream& in, model::ServerSetting& v) {
  in >> v.activeOperationName;
  in >> v.activeOperationSlug;
  return in;
}

QDataStream& operator<<(QDataStream& out, const QMap<QString, model::ServerSetting>& v) {
  int count = v.size();
  out << count;
  for (auto key : v.keys()) {
    out << key;
    out << v[key];
  }

  return out;
}

QDataStream& operator>>(QDataStream& in, QMap<QString, model::ServerSetting>& v) {
  int count;
  in >> count;

  QString name;
  model::ServerSetting serverSetting;
  for( int i = 0; i < count; i++) {
    in >> name;
    in >> serverSetting;
    v[name] = serverSetting;
  }
  return in;
}

QDataStream& operator<<(QDataStream& out, const ConnectionCellData& v) {
  out << v.originalData << v.cellType;
  return out;
}

QDataStream& operator>>(QDataStream& in, ConnectionCellData& v) {
  in >> v.originalData;
  in >> v.cellType;
  return in;
}

QDataStream& operator<<(QDataStream& out, const ServerItem& v) {
  auto copy = ServerItem(v);
  out << copy.getServerUuid() << copy.getId() << copy.serverName
      << copy.accessKey << copy.secretKey << copy.hostPath << copy.deleted;
  return out;
}

QDataStream& operator>>(QDataStream& in, ServerItem& v) {
  int id;
  QString uuid, name, accessKey, secretKey, hostPath;
  bool deleted;

  in >> uuid;
  in >> id;
  in >> name;
  in >> accessKey;
  in >> secretKey;
  in >> hostPath;
  in >> deleted;
  auto temp = ServerItem(id, uuid, name, accessKey, secretKey, hostPath, deleted);
  v = temp;
  return in;
}

class TypeStreams {
 public:
  static void registerTypes() {
    qRegisterMetaTypeStreamOperators<model::Tag>("Tag");
    qRegisterMetaTypeStreamOperators<std::vector<model::Tag>>("TagVector");
    qRegisterMetaTypeStreamOperators<model::ServerSetting>("ServerSetting");
    qRegisterMetaTypeStreamOperators<QMap<QString, model::ServerSetting>>("ServerSettingMap");
    qRegisterMetaTypeStreamOperators<ConnectionCellData>("ConnectionCell");
    qRegisterMetaTypeStreamOperators<ServerItem>("ServerItem");
  }
};

#endif // TYPE_STREAMS_H
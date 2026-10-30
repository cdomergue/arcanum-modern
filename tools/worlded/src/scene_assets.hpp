#pragma once
#include "worlded/assets.hpp"
#include <QPixmap>
#include <QPoint>
#include <QCache>

struct Sprite {
    QPixmap pixels;
    QPoint hotspot;
    std::string path;
};
struct CatalogEntry {
    worlded::GameObject object;
    QString label, source;
};
class SceneAssets {
public:
    explicit SceneAssets(std::shared_ptr<worlded::Installation> installation);
    Sprite sprite(std::uint32_t aid);
    worlded::GameObject effective(const worlded::GameObject& object);
    QString description(const worlded::GameObject& object);
    std::vector<CatalogEntry> catalog();
    std::map<std::uint32_t,QString> surfaces(bool roofs);
    bool available(std::uint32_t aid) {try{return installation_->contains(worlded::resource_name(resolve(aid)));}catch(const std::exception&){return false;}}
    const std::string& prototypePath(unsigned id) const { return prototype_paths_.at(id); }
    QStringList catalogErrors;
private:
    std::shared_ptr<worlded::Installation> installation_;
    std::map<std::string, std::map<unsigned, std::string>> tables_;
    std::map<unsigned, std::string> prototype_paths_;
    std::map<unsigned, worlded::GameObject> prototypes_;
    QCache<quint32, Sprite> cache_{128 * 1024}; // Costs in KiB.
    const std::map<unsigned, std::string>& table(const std::string& path);
    std::string message(const std::string& path, unsigned id);
    std::string resolve(std::uint32_t aid);
};

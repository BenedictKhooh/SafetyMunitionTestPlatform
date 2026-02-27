#include "EntityRepository.h"

bool EntityRepository::addEntity(const QString& name, const MeshEntity& entity) {
    // 如果同名实体已存在，则覆盖并返回 false 提示，或者你也可以选择拒绝添加
    bool exists = m_entities.find(name) != m_entities.end();
    m_entities[name] = entity;
    return !exists;
}

bool EntityRepository::deleteEntity(const QString& name) {
    auto it = m_entities.find(name);
    if (it != m_entities.end()) {
        m_entities.erase(it);
        return true; // 删除成功
    }
    return false; // 没找到
}

bool EntityRepository::hasEntity(const QString& name) const {
    return m_entities.find(name) != m_entities.end();
}

const MeshEntity* EntityRepository::getEntity(const QString& name) const {
    auto it = m_entities.find(name);
    if (it != m_entities.end()) {
        return &(it->second);
    }
    return nullptr;
}

const std::map<QString, MeshEntity>& EntityRepository::getAllEntities() const {
    return m_entities;
}

void EntityRepository::clearAll() {
    m_entities.clear();
}
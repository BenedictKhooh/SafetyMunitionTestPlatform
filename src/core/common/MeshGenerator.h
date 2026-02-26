#ifndef MESHGENERATOR_H
#define MESHGENERATOR_H

#include <QString>

/**
 * @brief 几何网格生成基类
 */
class MeshGenerator {
public:
    virtual ~MeshGenerator() = default;

    // 纯虚函数：每个具体的几何体类都要实现这个方法来返回自己的 Gmsh 脚本
    virtual QString generateGeoScript() const = 0;

    // 可选：返回几何体类型名称，用于日志记录
    virtual QString getTypeName() const = 0;
};

#endif // MESHGENERATOR_H
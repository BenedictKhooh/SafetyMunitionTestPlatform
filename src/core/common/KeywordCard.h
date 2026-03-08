#pragma once
#include <string>

// 基类：所有 K 文件块都必须继承它并实现 generate()
class KeywordCard {
public:
    virtual ~KeywordCard() = default;
    virtual std::string generate() const = 0;
};

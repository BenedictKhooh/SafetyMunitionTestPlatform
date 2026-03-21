#ifndef KEYWORDCARD_H
#define KEYWORDCARD_H

#include <string>

// ==========================================
// LS-DYNA 关键字卡片的纯虚基类 (接口类)
// ==========================================
struct KeywordCard {

    // 🌟 1. 虚析构函数 
    // 因为 LSDynaDeck 底层存的是基类指针 (std::shared_ptr<KeywordCard>)。
    // 只有把析构函数声明为 virtual，当指针被销毁时，才能正确调用到子类(如 MaterialCard)的析构函数，防止内存泄漏。
    virtual ~KeywordCard() = default;

    // 🌟 2. 纯虚函数
    // 结尾的 "= 0" 表示这是一个纯虚函数，KeywordCard 本身不提供实现。
    // 它强制所有继承它的子类，都必须自己写一个 to_string() 函数。
    virtual std::string to_string() const = 0;

};

#endif // KEYWORDCARD_H
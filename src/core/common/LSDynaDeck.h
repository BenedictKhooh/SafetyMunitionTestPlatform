#ifndef LSDYNADECK_H
#define LSDYNADECK_H

#include <vector>
#include <memory>
#include <string>
#include "src/core/common/KeywordCard.h" // 确保引入了你的多态基类

class LSDynaDeck {
private:
    // 🌟 核心容器：存放所有继承自 KeywordCard 的子类智能指针
    std::vector<std::shared_ptr<KeywordCard>> m_cards;

public:
    LSDynaDeck() = default;
    ~LSDynaDeck() = default;

    // 1. 添加卡片的方法
    void addCard(std::shared_ptr<KeywordCard> card) {
        if (card) {
            m_cards.push_back(card);
        }
    }

    // 2. 修复报错：清空容器的方法
    void clear() {
        m_cards.clear();
    }

    // 3. 修复报错：一键多态生成 K 文件文本的方法
    std::string generateDeck() const {
        std::string result = "";
        // 遍历容器中的每一张卡片，调用它们各自的 to_string() 方法
        for (const auto& card : m_cards) {
            result += card->to_string();
        }
        return result;
    }
};

#endif // LSDYNADECK_H
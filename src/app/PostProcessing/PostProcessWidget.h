/**
 * @file PostProcessWidget.h
 * @brief LS-DYNA 仿真结果后处理与历程曲线分析模块
 * @details 负责解析 LS-DYNA ASCII 格式输出文件（如 MATSUM, GLSTAT, NODOUT, ELOUT, SECFORC, SPCFORC），
 * 提取时程数据并利用 QCustomPlot 进行动态可视化渲染。
 */

#ifndef POSTPROCESSWIDGET_H
#define POSTPROCESSWIDGET_H

#include <QWidget>
#include <QMap>
#include <QVector>
#include <QString>
#include <QComboBox>

 // 前置声明以降低编译依赖
class QListWidget;
class QPushButton;
class QCustomPlot;

/**
 * @struct SensorData
 * @brief 仿真观测点（传感器）时程数据容器
 */
struct SensorData {
    QVector<double> time;   ///< 时间序列 [通常为 μs 或 ms]
    QVector<double> value;  ///< 物理量观测值序列 [能量/力/应力/位移等]
};

/**
 * @class PostProcessWidget
 * @brief 后处理可视化交互窗口类
 */
class PostProcessWidget : public QWidget {
    Q_OBJECT

public:
    explicit PostProcessWidget(QWidget* parent = nullptr);
    ~PostProcessWidget() = default;

private slots:
    /** @brief 触发文件选择与数据解析路由 */
    void handleLoadData();
    /** @brief 响应列表选择变化，刷新绘图区 */
    void handleSensorSelectionChanged();

private:
    /** @brief 构建模块化 UI 布局 */
    void setupUI();

    // ==========================================
    // 核心文本解析引擎 (基于正则状态机)
    // ==========================================

    /** @brief 解析系统全局统计文件 (GLSTAT - Global Statistics) */
    bool parseGlstat(const QString& filePath);

    /** @brief 解析部件动能/内能文件 (MATSUM - Material Summary) */
    bool parseMatsum(const QString& filePath);

    /** @brief 解析节点运动学历程文件 (NODOUT - Nodal Output) */
    bool parseNodout(const QString& filePath);

    /** @brief 解析单元应力/应变时程文件 (ELOUT - Element Output) */
    bool parseElout(const QString& filePath);

    /** @brief 解析横截面内力历程文件 (SECFORC - Section Forces) */
    bool parseSecforc(const QString& filePath);

    /** @brief 解析单点约束反力历程文件 (SPCFORC - SPC Forces) */
    bool parseSpcforc(const QString& filePath);

private:
    // UI 组件指针
    QComboBox* m_comboFileType;     ///< 文件类型选择器
    QListWidget* m_sensorList;      ///< 观测对象（传感器）列表
    QPushButton* m_btnLoadData;     ///< 加载数据按钮
    QPushButton* m_btnClearPlot;    ///< 清空画板按钮
    QCustomPlot* m_plotWidget;      ///< 曲线渲染核心组件

    // 数据内存池：Key 为观测名称，Value 为对应的时程序列
    QMap<QString, SensorData> m_simulationData;
};

#endif // POSTPROCESSWIDGET_H
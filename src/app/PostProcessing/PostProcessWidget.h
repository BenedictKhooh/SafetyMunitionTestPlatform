/**
 * @file PostProcessWidget.h
 * @brief LS-DYNA 仿真结果后处理与可视化模块头文件
 * @details 定义了用于解析、缓存及渲染 ASCII 格式仿真时程结果数据（如 MATSUM, GLSTAT, NODOUT）的图形用户界面组件。
 */

#ifndef POSTPROCESSWIDGET_H
#define POSTPROCESSWIDGET_H

#include <QWidget>
#include <QMap>
#include <QVector>
#include <QString>
#include <QComboBox> 

 // 前置声明以降低编译耦合度
class QListWidget;
class QPushButton;
class QCustomPlot;

/**
 * @struct SensorData
 * @brief 时程序列数据容器
 * @details 用于在内存中连续存储单一物理观测项的时间坐标与对应数值，直接适配 QCustomPlot 的底层渲染接口。
 */
struct SensorData {
    QVector<double> time;   ///< 时间轴离散坐标序列
    QVector<double> value;  ///< 观测物理量离散数值序列 (能量/速度/位移等)
};

/**
 * @class PostProcessWidget
 * @brief 后处理与可视化分析核心面板类
 * @details 负责构建主从分栏视图，提供本地结果文件的 I/O 交互、多模式正则解析路由，
 * 以及基于 QCustomPlot 的多轴自适应高频时程图表渲染功能。
 */
class PostProcessWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父级 QWidget 指针
     */
    explicit PostProcessWidget(QWidget* parent = nullptr);

    /**
     * @brief 默认析构函数
     */
    ~PostProcessWidget() = default;

private slots:
    /**
     * @brief 响应用户加载结果文件事件
     * @details 调出文件选择对话框，依据下拉框指定的解析策略（Strategy），将文本流导向对应的正则解析器。
     */
    void handleLoadData();

    /**
     * @brief 响应数据列勾选状态变更事件
     * @details 实时捕获用户的复选操作，重构渲染管线，进行智能物理单位推演，并触发图表重绘与坐标轴自适应缩放。
     */
    void handleSensorSelectionChanged();

private:
    /**
     * @brief 初始化图形用户界面布局与控件实例化
     */
    void setupUI();

    // ==========================================
    // 核心正则解析引擎家族
    // ==========================================

    /**
     * @brief 解析 MATSUM 文件 (材料/部件级能量统计)
     * @param filePath 目标 ASCII 文件的绝对路径
     * @return 若解析成功且提取到有效时程序列则返回 true，否则返回 false
     */
    bool parseMatsum(const QString& filePath);

    /**
     * @brief 解析 GLSTAT 文件 (全局系统级能量与平衡统计)
     * @param filePath 目标 ASCII 文件的绝对路径
     * @return 若解析成功且提取到有效时程序列则返回 true，否则返回 false
     */
    bool parseGlstat(const QString& filePath);

    /**
     * @brief 解析 NODOUT 文件 (离散节点运动学历程输出)
     * @param filePath 目标 ASCII 文件的绝对路径
     * @return 若解析成功且提取到有效时程序列则返回 true，否则返回 false
     */
    bool parseNodout(const QString& filePath);

private:
    // ==========================================
    // UI 控件指针域
    // ==========================================
    QComboBox* m_comboFileType;     ///< 文件格式选择与解析策略路由下拉框
    QListWidget* m_sensorList;      ///< 已提取数据项展示与多选交互列表
    QPushButton* m_btnLoadData;     ///< 触发文件 IO 的控制按钮
    QPushButton* m_btnClearPlot;    ///< 清理当前视图管线的控制按钮
    QCustomPlot* m_plotWidget;      ///< 第三方高性能时程图表渲染核心组件

    // ==========================================
    // 后处理数据上下文域
    // ==========================================
    /** * @brief 仿真结果核心缓存池
     * @details 以语义化字符串（如 "Part 1 - Kinetic Energy"）作为键，映射对应的离散时间序列数据。
     */
    QMap<QString, SensorData> m_simulationData;
};

#endif // POSTPROCESSWIDGET_H
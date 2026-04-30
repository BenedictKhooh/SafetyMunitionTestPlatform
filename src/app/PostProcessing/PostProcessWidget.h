/**
 * @file PostProcessWidget.h
 * @brief LS-DYNA 后处理自动化分析与网格化多参数可视化组件
 * @details 支持自动扫描 glstat, matsum, nodout, elout, rcforc 文件，
 * 并提供针对复杂 ASCII 格式（防粘连、防缺省）的鲁棒性解析。
 */

#ifndef POSTPROCESSWIDGET_H
#define POSTPROCESSWIDGET_H

#include <QWidget>
#include <QMap>
#include <QVector>
#include <QGridLayout>
#include <QComboBox>
#include <QStringList>
#include "src/app/PostProcessing/qcustomplot.h"

class PostProcessWidget : public QWidget {
    Q_OBJECT

public:
    explicit PostProcessWidget(QWidget* parent = nullptr);
    ~PostProcessWidget() = default;

    /**
     * @brief 核心接口：自动扫描指定目录并执行一键绘图
     * @param workingDir 求解器输出文件所在的绝对路径
     */
    void autoScanAndPlot(const QString& workingDir);

private slots:
    /** @brief 响应手动选择文件夹按钮 */
    void handleManualDirSelect();

    /** @brief 响应 NODOUT 节点 ID 或参数下拉框切换事件，重绘图表 */
    void updateNodoutPlot();

    /** @brief 响应 ELOUT 单元 ID 或参数下拉框切换事件，重绘图表 */
    void updateEloutPlot();

    void showPlotContextMenu(const QPoint& pos);
    void exportPlotToCSV();
    void exportPlotToImage();

private:
    /** @brief 初始化 UI 布局与 2x2 网格图表 */
    void setupUI();

    QCustomPlot* m_contextMenuPlot = nullptr;
    /**
     * @brief 创建并配置基础的带有可选单一下拉框的 QCustomPlot 实例 (用于 glstat, matsum 等)
     * @param id 内部映射 ID
     * @param title 图表显示的中文标题
     * @param row 网格行索引
     * @param col 网格列索引
     * @param comboOut 传出参数：如果需要下拉选择框，则传入二级指针接收创建的实例
     */
    void createGridPlot(const QString& id, const QString& title, int row, int col, QComboBox** comboOut = nullptr);

    /** @brief 清除所有图表中的曲线数据与内存缓存 */
    void clearDashboard();

    // ==========================================
    // 后处理文件解析引擎
    // ==========================================
    bool processGlstat(const QString& path); // 全局能量 (动能、内能)
    bool processMatsum(const QString& path); // 部件能量 (各 Part 动能、内能)
    bool processNodout(const QString& path); // 节点历程 (全参数：位移、速度、加速度)
    bool processElout(const QString& path);  // 单元历程 (全参数：应力张量、反应度等)
    bool processRcforc(const QString& path); // 接触反力 (界面合力)

private:
    // UI 布局组件
    QGridLayout* m_gridLayout;
    QMap<QString, QCustomPlot*> m_plotMap;  ///< ID 到图表控件的映射

    // NODOUT 专属下拉框控件 (双联动)
    QComboBox* m_comboNodoutId = nullptr;    ///< 节点 ID 选择下拉框
    QComboBox* m_comboNodoutParam = nullptr; ///< 节点参数选择下拉框

    // ELOUT 专属下拉框控件 (双联动)
    QComboBox* m_comboEloutId = nullptr;     ///< 单元 ID 选择下拉框
    QComboBox* m_comboEloutParam = nullptr;  ///< 单元参数选择下拉框

    // ==========================================
    // 全参数内存数据缓存
    // ==========================================
    // NODOUT 数据结构: NodeID -> (ParamName -> ValueArray)
    QMap<int, QVector<double>> m_nodoutTimeMap;
    QMap<int, QMap<QString, QVector<double>>> m_nodoutData;

    // ELOUT 数据结构: ElementID -> (ParamName -> ValueArray)
    QMap<int, QVector<double>> m_eloutTimeMap;
    QMap<int, QMap<QString, QVector<double>>> m_eloutData;

    QString m_currentWorkDir;
    QList<QColor> m_colorPalette;            ///< 预设的专业色盘
};

#endif // POSTPROCESSWIDGET_H
/**
 * @file PostProcessWidget.h
 * @brief LS-DYNA 后处理自动化分析与网格化可视化组件
 * @author Gemini Visual Tutor
 * @date 2024
 */

#ifndef POSTPROCESSWIDGET_H
#define POSTPROCESSWIDGET_H

#include <QWidget>
#include <QMap>
#include <QVector>
#include <QGridLayout>
#include <QTimer>
#include "src/app/PostProcessing/qcustomplot.h"

 /**
  * @struct PlotData
  * @brief 存储传感器时历曲线的基础数据结构
  */
struct PlotData {
    QVector<double> time;  ///< 时间序列 (单位: us)
    QVector<double> value; ///< 数值序列 (单位: 物理单位)
};

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

private:
    /** @brief 初始化 UI 布局与 2x2 网格图表 */
    void setupUI();

    /**
     * @brief 创建并配置单个 QCustomPlot 实例
     * @param id 内部映射 ID
     * @param title 图表显示的中文标题
     * @param row 网格行索引
     * @param col 网格列索引
     */
    void createGridPlot(const QString& id, const QString& title, int row, int col);

    /** @brief 清除所有图表中的曲线数据与缓存 */
    void clearDashboard();

    // ==========================================
    // 后处理文件解析引擎 (封装了真实文本解析逻辑)
    // ==========================================
    bool processGlstat(const QString& path); // 全局能量
    bool processMatsum(const QString& path); // 部件能量
    bool processNodout(const QString& path); // 节点历程
    bool processElout(const QString& path);  // 单元历程
    bool processRcforc(const QString& path); // 接触/界面力

private:
    // UI 布局组件
    QGridLayout* m_gridLayout;
    QMap<QString, QCustomPlot*> m_plotMap;  ///< ID 到图表控件的映射

    // 数据持久化
    QString m_currentWorkDir;
    QList<QColor> m_colorPalette;           ///< 预设的专业色盘
};

#endif // POSTPROCESSWIDGET_H
#ifndef POSTPROCESSWIDGET_H
#define POSTPROCESSWIDGET_H

#include <QWidget>
#include <QMap>
#include <QVector>
#include <QString>

// 前置声明，减少编译依赖
class QListWidget;
class QPushButton;
class QCustomPlot;

// 数据结构：存储单个传感器的历史曲线
struct SensorData {
    QVector<double> time;
    QVector<double> value;
};

class PostProcessWidget : public QWidget {
    Q_OBJECT
public:
    explicit PostProcessWidget(QWidget* parent = nullptr);
    ~PostProcessWidget() = default;

private slots:
    // 专属的交互槽函数
    void handleLoadData();
    void handleSensorSelectionChanged();

private:
    // 初始化 UI 布局
    void setupUI();

    // UI 控件指针
    QListWidget* m_sensorList;
    QPushButton* m_btnLoadData;
    QPushButton* m_btnClearPlot;
    QCustomPlot* m_plotWidget;

    // 后处理核心数据
    QMap<QString, SensorData> m_simulationData;
};

#endif // POSTPROCESSWIDGET_H
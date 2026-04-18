/**
 * @file PostProcessWidget.h
 * @brief LS-DYNA 仿真结果后处理与历程曲线分析模块
 * @version 3.0 (纯 Qt API 重构版，彻底解决中文路径与数据断崖 Bug)
 */

#ifndef POSTPROCESSWIDGET_H
#define POSTPROCESSWIDGET_H

#include <QWidget>
#include <QMap>
#include <QVector>
#include <QString>
#include <QComboBox>

class QListWidget;
class QPushButton;
class QCustomPlot;

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
    void handleLoadData();
    void handleSensorSelectionChanged();

private:
    void setupUI();

    // 针对您上传的 5 种真实格式的专属解析引擎
    bool parseGlstat(const QString& filePath);
    bool parseMatsum(const QString& filePath);
    bool parseNodout(const QString& filePath);
    bool parseElout(const QString& filePath);
    bool parseRcforc(const QString& filePath);
    bool parseSleout(const QString& filePath);

    // 预留接口
    bool parseSecforc(const QString& filePath);
    bool parseSpcforc(const QString& filePath);

private:
    QComboBox* m_comboFileType;
    QListWidget* m_sensorList;
    QPushButton* m_btnLoadData;
    QPushButton* m_btnClearPlot;
    QCustomPlot* m_plotWidget;

    QMap<QString, SensorData> m_simulationData;
};

#endif // POSTPROCESSWIDGET_H
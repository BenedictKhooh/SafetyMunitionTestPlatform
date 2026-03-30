#include "PostProcessWidget.h"
#include "qcustomplot.h" // 确保你已经把 QCustomPlot 的源码加入了工程
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QSplitter>

PostProcessWidget::PostProcessWidget(QWidget* parent) : QWidget(parent) {
    setupUI();

    // 绑定内部信号与槽，完全自洽，不依赖外部！
    connect(m_btnLoadData, &QPushButton::clicked, this, &PostProcessWidget::handleLoadData);
    connect(m_sensorList, &QListWidget::itemSelectionChanged, this, &PostProcessWidget::handleSensorSelectionChanged);
    connect(m_btnClearPlot, &QPushButton::clicked, m_plotWidget, [this]() {
        m_plotWidget->clearGraphs();
        m_plotWidget->replot();
        });
}

void PostProcessWidget::setupUI() {
    // 1. 创建左侧控制面板
    QWidget* leftPanel = new QWidget(this);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    m_btnLoadData = new QPushButton("加载仿真数据 (.csv/.txt)", this);
    m_btnClearPlot = new QPushButton("清空图表", this);
    m_sensorList = new QListWidget(this);
    m_sensorList->setSelectionMode(QAbstractItemView::ExtendedSelection); // 支持按住 Ctrl 多选！

    leftLayout->addWidget(m_btnLoadData);
    leftLayout->addWidget(m_btnClearPlot);
    leftLayout->addWidget(m_sensorList);

    // 2. 创建右侧绘图区
    m_plotWidget = new QCustomPlot(this);
    m_plotWidget->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);

    // 3. 使用 QSplitter 让用户可以自由拖拽左右比例
    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(leftPanel);
    splitter->addWidget(m_plotWidget);
    splitter->setStretchFactor(0, 1); // 左侧占 1 份
    splitter->setStretchFactor(1, 4); // 右侧图表占 4 份

    // 4. 设置主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(splitter);
}

void PostProcessWidget::handleLoadData() {
    QString fileName = QFileDialog::getOpenFileName(this, "打开仿真数据", "", "Data Files (*.txt *.csv *.out);;All Files (*)");
    if (fileName.isEmpty()) return;

    // 清空旧数据
    m_simulationData.clear();
    m_sensorList->clear();

    // ==========================================
    // 🌟 在这里接入你的文件解析代码 (读取 LS-DYNA 输出)
    // 假设解析出了 Node_101 和 Node_102
    // ==========================================

    // 演示用的假数据（测试绘图逻辑）
    SensorData dummyData1, dummyData2;
    for (int i = 0; i < 100; ++i) {
        dummyData1.time.append(i / 10.0);
        dummyData1.value.append(qSin(i / 10.0));
        dummyData2.time.append(i / 10.0);
        dummyData2.value.append(qCos(i / 10.0));
    }
    m_simulationData["Sensor_Node_101"] = dummyData1;
    m_simulationData["Sensor_Node_102"] = dummyData2;

    // 刷新列表
    for (const QString& sensorName : m_simulationData.keys()) {
        m_sensorList->addItem(sensorName);
    }
}

void PostProcessWidget::handleSensorSelectionChanged() {
    m_plotWidget->clearGraphs();

    QList<QListWidgetItem*> selectedItems = m_sensorList->selectedItems();
    if (selectedItems.isEmpty()) {
        m_plotWidget->replot();
        return;
    }

    // 颜色库，用于区分多条曲线
    QList<QColor> colors = { QColor(0, 114, 189), QColor(217, 83, 25), QColor(237, 177, 32), QColor(126, 47, 142) };
    int colorIdx = 0;

    // 遍历所有选中的传感器，画出对比图
    for (QListWidgetItem* item : selectedItems) {
        QString sensorName = item->text();
        if (!m_simulationData.contains(sensorName)) continue;

        SensorData data = m_simulationData[sensorName];

        QCPGraph* graph = m_plotWidget->addGraph();
        graph->setData(data.time, data.value);
        graph->setName(sensorName); // 设置图例名称

        QPen pen;
        pen.setColor(colors[colorIdx % colors.size()]);
        pen.setWidth(2);
        graph->setPen(pen);

        colorIdx++;
    }

    m_plotWidget->xAxis->setLabel("Time (s)");
    m_plotWidget->yAxis->setLabel("Observation Value");
    m_plotWidget->legend->setVisible(true); // 显示图例
    m_plotWidget->rescaleAxes();
    m_plotWidget->replot();
}
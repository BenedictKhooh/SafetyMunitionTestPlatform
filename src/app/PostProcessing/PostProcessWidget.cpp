/**
 * @file PostProcessWidget.cpp
 * @brief LS-DYNA 后处理可视化模块核心实现文件
 */

#include "PostProcessWidget.h"
#include "qcustomplot.h" // 第三方 QCustomPlot 组件依赖
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QSplitter>

 // 标准库依赖
#include <fstream>
#include <regex>
#include <string>
#include <algorithm>
#include <cmath>

PostProcessWidget::PostProcessWidget(QWidget* parent) : QWidget(parent) {
    setupUI();

    // 绑定内部交互信号与逻辑处理槽，保障组件的高度内聚性
    connect(m_btnLoadData, &QPushButton::clicked, this, &PostProcessWidget::handleLoadData);
    connect(m_sensorList, &QListWidget::itemSelectionChanged, this, &PostProcessWidget::handleSensorSelectionChanged);
    connect(m_btnClearPlot, &QPushButton::clicked, m_plotWidget, [this]() {
        m_plotWidget->clearGraphs();
        m_plotWidget->replot();
        });
}

void PostProcessWidget::setupUI() {
    // 1. 构建左侧控制与数据导航面板
    QWidget* leftPanel = new QWidget(this);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    // 实例化路由配置器与交互组件
    m_comboFileType = new QComboBox(this);
    m_comboFileType->addItems({ "MATSUM (实体/部件级能量)", "GLSTAT (全局系统能量)", "NODOUT (节点历程输出)" });

    m_btnLoadData = new QPushButton("加载仿真结果文件", this);
    m_btnClearPlot = new QPushButton("清空渲染图表", this);

    // 实例化数据列多选容器 (启用 ExtendedSelection 以支持快捷键批量拾取)
    m_sensorList = new QListWidget(this);
    m_sensorList->setSelectionMode(QAbstractItemView::ExtendedSelection);

    leftLayout->addWidget(m_comboFileType);
    leftLayout->addWidget(m_btnLoadData);
    leftLayout->addWidget(m_btnClearPlot);
    leftLayout->addWidget(m_sensorList);

    // 2. 构建右侧 QCustomPlot 绘图核心区，并启用高级视口交互特性
    m_plotWidget = new QCustomPlot(this);
    m_plotWidget->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);

    // 3. 构建伸缩分割器 (QSplitter)，优化不同分辨率下的屏占比分配
    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(leftPanel);
    splitter->addWidget(m_plotWidget);
    splitter->setStretchFactor(0, 1); // 左侧配置区初始比例权重: 20%
    splitter->setStretchFactor(1, 4); // 右侧渲染区初始比例权重: 80%

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(splitter);
}

void PostProcessWidget::handleLoadData() {
    QString filter = "LS-DYNA ASCII Files (*.*);;All Files (*)";
    QString fileName = QFileDialog::getOpenFileName(this, "载入 LS-DYNA 后处理 ASCII 数据", "", filter);
    if (fileName.isEmpty()) return;

    // 清理脏数据，初始化解析环境
    m_simulationData.clear();
    m_sensorList->clear();
    m_plotWidget->clearGraphs();

    bool isParseSuccess = false;
    QString selectedType = m_comboFileType->currentText();

    // ==========================================
    // 依据用户声明的数据类别，执行对应正则状态机
    // ==========================================
    if (selectedType.contains("MATSUM")) {
        isParseSuccess = parseMatsum(fileName);
    }
    else if (selectedType.contains("GLSTAT")) {
        isParseSuccess = parseGlstat(fileName);
    }
    else if (selectedType.contains("NODOUT")) {
        isParseSuccess = parseNodout(fileName);
    }

    // 后处理回调判定与状态分发
    if (isParseSuccess) {
        // 重构前端 UI 模型视图
        for (const QString& sensorName : m_simulationData.keys()) {
            m_sensorList->addItem(sensorName);
        }
        QMessageBox::information(this, "I/O 解析成功",
            QString("底层数据提取完成，当前内存池驻留时程序列总数: %1").arg(m_simulationData.size()));
    }
    else {
        QMessageBox::warning(this, "I/O 异常",
            "文件流读取失败或正则状态机未捕获目标特征集。\n请核实验证文件类型是否与所选策略映射相符。");
    }
}

// =====================================================================
// LS-DYNA ASCII 文本正则解析引擎实现域
// =====================================================================

bool PostProcessWidget::parseMatsum(const QString& filePath) {
    std::ifstream file(filePath.toLocal8Bit().constData());
    if (!file.is_open()) return false;

    std::string line;
    // 定义核心状态捕获模式
    std::regex timeRegex(R"(time\s*=\s*([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?))");
    // 捕获簇语义: [1]=实体标识符(PartID), [2]=内能(InternalEnergy), [3]=动能(KineticEnergy)
    std::regex matRegex(R"(mat\.#=\s*(\d+)\s+inten=\s*([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?)\s+kinen=\s*([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?))");

    double currentTime = 0.0;
    std::smatch match;
    bool hasValidExtraction = false;

    while (std::getline(file, line)) {
        // 强制转为小写以增强正则引擎的系统兼容性
        std::string lowerLine = line;
        std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);

        if (std::regex_search(lowerLine, match, timeRegex)) {
            currentTime = std::stod(match[1].str());
        }
        else if (std::regex_search(lowerLine, match, matRegex)) {
            QString partId = QString::fromStdString(match[1].str());
            double internalEnergy = std::stod(match[2].str());
            double kineticEnergy = std::stod(match[3].str());

            QString ieKey = QString("Part %1 - Internal Energy").arg(partId);
            QString keKey = QString("Part %1 - Kinetic Energy").arg(partId);

            m_simulationData[ieKey].time.append(currentTime);
            m_simulationData[ieKey].value.append(internalEnergy);

            m_simulationData[keKey].time.append(currentTime);
            m_simulationData[keKey].value.append(kineticEnergy);

            hasValidExtraction = true;
        }
    }
    return hasValidExtraction;
}

bool PostProcessWidget::parseGlstat(const QString& filePath) {
    std::ifstream file(filePath.toLocal8Bit().constData());
    if (!file.is_open()) return false;

    std::string line;
    std::regex timeRegex(R"(time\s*=\s*([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?))");
    std::regex keRegex(R"(kinetic energy\s+([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?))");
    std::regex ieRegex(R"(internal energy\s+([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?))");
    std::regex teRegex(R"(total energy\s+([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?))");

    double currentTime = 0.0;
    std::smatch match;
    bool hasValidExtraction = false;

    while (std::getline(file, line)) {
        std::string lowerLine = line;
        std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);

        if (std::regex_search(lowerLine, match, timeRegex)) {
            currentTime = std::stod(match[1].str());
        }
        else if (std::regex_search(lowerLine, match, keRegex)) {
            m_simulationData["Global - Kinetic Energy"].time.append(currentTime);
            m_simulationData["Global - Kinetic Energy"].value.append(std::stod(match[1].str()));
            hasValidExtraction = true;
        }
        else if (std::regex_search(lowerLine, match, ieRegex)) {
            m_simulationData["Global - Internal Energy"].time.append(currentTime);
            m_simulationData["Global - Internal Energy"].value.append(std::stod(match[1].str()));
        }
        else if (std::regex_search(lowerLine, match, teRegex)) {
            m_simulationData["Global - Total Energy"].time.append(currentTime);
            m_simulationData["Global - Total Energy"].value.append(std::stod(match[1].str()));
        }
    }
    return hasValidExtraction;
}

bool PostProcessWidget::parseNodout(const QString& filePath) {
    std::ifstream file(filePath.toLocal8Bit().constData());
    if (!file.is_open()) return false;

    std::string line;
    std::regex timeRegex(R"(time\s*=\s*([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?))");
    // 解析序列阵列: 节点ID(1) X-Disp(2) Y-Disp(3) Z-Disp(4) X-Vel(5) Y-Vel(6) Z-Vel(7) 
    std::regex nodeRegex(R"(^\s*(\d+)\s+([+-]?\S+)\s+([+-]?\S+)\s+([+-]?\S+)\s+([+-]?\S+)\s+([+-]?\S+)\s+([+-]?\S+))");

    double currentTime = 0.0;
    std::smatch match;
    bool hasValidExtraction = false;

    while (std::getline(file, line)) {
        std::string lowerLine = line;
        std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);

        if (std::regex_search(lowerLine, match, timeRegex)) {
            currentTime = std::stod(match[1].str());
        }
        else if (std::regex_search(lowerLine, match, nodeRegex)) {
            QString nodeId = QString::fromStdString(match[1].str());
            double vx = std::stod(match[5].str());
            double vy = std::stod(match[6].str());
            double vz = std::stod(match[7].str());

            // 物理场二次计算: 构造局部坐标系下的合速度标量
            double v_mag = std::sqrt(vx * vx + vy * vy + vz * vz);

            QString keyDispX = QString("Node %1 - Disp X").arg(nodeId);
            QString keyVelMag = QString("Node %1 - Velocity Mag").arg(nodeId);

            m_simulationData[keyDispX].time.append(currentTime);
            m_simulationData[keyDispX].value.append(std::stod(match[2].str()));

            m_simulationData[keyVelMag].time.append(currentTime);
            m_simulationData[keyVelMag].value.append(v_mag);

            hasValidExtraction = true;
        }
    }
    return hasValidExtraction;
}

// =====================================================================
// 图表渲染管线与坐标映射域
// =====================================================================

void PostProcessWidget::handleSensorSelectionChanged() {
    // 强制卸载并清理渲染器中的遗留图表实例
    m_plotWidget->clearGraphs();

    QList<QListWidgetItem*> selectedItems = m_sensorList->selectedItems();
    if (selectedItems.isEmpty()) {
        m_plotWidget->replot();
        return;
    }

    // 初始化科研级高对比度离散色带
    QList<QColor> colors = { QColor(0, 114, 189), QColor(217, 83, 25), QColor(237, 177, 32), QColor(126, 47, 142), QColor(119, 172, 48) };
    int colorIdx = 0;

    QStringList yAxisCategorySet; // 维护 Y 轴的复合物理语义描述

    // 迭代用户选集，重组 QCustomPlot 的数据拓扑
    for (QListWidgetItem* item : selectedItems) {
        QString sensorName = item->text();
        if (!m_simulationData.contains(sensorName)) continue;

        SensorData data = m_simulationData[sensorName];

        QCPGraph* graph = m_plotWidget->addGraph();
        graph->setData(data.time, data.value);

        // ==========================================
        // 智能物理单位推演算法 (适配 g-cm-μs 兵器工业体系)
        // ==========================================
        QString unitStr = "";
        QString axisCategory = "";

        if (sensorName.contains("Energy", Qt::CaseInsensitive)) {
            unitStr = "[10^5 J]"; // 能量绝对单位映射
            axisCategory = QString("Energy %1").arg(unitStr);
        }
        else if (sensorName.contains("Velocity", Qt::CaseInsensitive)) {
            unitStr = "[cm/μs]";  // 运动学速度单位映射
            axisCategory = QString("Velocity %1").arg(unitStr);
        }
        else if (sensorName.contains("Disp", Qt::CaseInsensitive)) {
            unitStr = "[cm]";     // 运动学位移单位映射
            axisCategory = QString("Displacement %1").arg(unitStr);
        }

        // 避免轴标签内出现冗余的重复物理类别描述
        if (!axisCategory.isEmpty() && !yAxisCategorySet.contains(axisCategory)) {
            yAxisCategorySet << axisCategory;
        }

        // 装载包含物理单位图例标识符
        graph->setName(QString("%1 %2").arg(sensorName).arg(unitStr));

        // 曲线视觉属性映射
        QPen pen;
        pen.setColor(colors[colorIdx % colors.size()]);
        pen.setWidth(2);
        graph->setPen(pen);

        colorIdx++;
    }

    // 同步视图投影轴的语义标签
    m_plotWidget->xAxis->setLabel("Time [μs]");
    if (yAxisCategorySet.isEmpty()) {
        m_plotWidget->yAxis->setLabel("Numerical Value (Unspecified Unit)");
    }
    else {
        m_plotWidget->yAxis->setLabel(yAxisCategorySet.join("  |  "));
    }

    // 激活图例组件并执行视图自适应重算与重绘
    m_plotWidget->legend->setVisible(true);
    m_plotWidget->rescaleAxes();
    m_plotWidget->replot();
}
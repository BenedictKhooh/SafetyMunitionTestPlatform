/**
 * @file PostProcessWidget.cpp
 * @brief 后处理可视化模块核心实现文件
 * @details 包含了所有 LS-DYNA ASCII 文件的正则解析实现与智能单位换算图表渲染逻辑。
 */

#include "PostProcessWidget.h"
#include "qcustomplot.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QSplitter>
#include <fstream>
#include <regex>
#include <string>
#include <algorithm>
#include <cmath>
#include <stdexcept>

 // =====================================================================
 // 安全数据转换工具域 (防止 std::stod 遇到 "****" 或 "NaN" 导致程序崩溃)
 // =====================================================================
namespace {
    inline double safeStod(const std::string& str, double defaultVal = 0.0) {
        try {
            // 尝试去除首尾多余空格
            std::string s = str;
            s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
            s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());

            if (s.empty() || s.find("***") != std::string::npos) {
                return defaultVal; // LS-DYNA 输出溢出
            }
            return std::stod(s);
        }
        catch (const std::invalid_argument&) {
            return defaultVal; // 非数字字符
        }
        catch (const std::out_of_range&) {
            return defaultVal; // 越界溢出
        }
    }
}

PostProcessWidget::PostProcessWidget(QWidget* parent) : QWidget(parent) {
    setupUI();

    // 绑定交互信号与槽函数
    connect(m_btnLoadData, &QPushButton::clicked, this, &PostProcessWidget::handleLoadData);
    connect(m_sensorList, &QListWidget::itemSelectionChanged, this, &PostProcessWidget::handleSensorSelectionChanged);
    connect(m_btnClearPlot, &QPushButton::clicked, m_plotWidget, [this]() {
        m_plotWidget->clearGraphs();
        m_plotWidget->replot();
        });
}

void PostProcessWidget::setupUI() {
    QWidget* leftPanel = new QWidget(this);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    // 初始化文件类型下拉框
    m_comboFileType = new QComboBox(this);
    m_comboFileType->addItems({
        "GLSTAT (系统全局统计)",
        "MATSUM (部件能量/功)",
        "NODOUT (节点运动历程)",
        "ELOUT (单元应力历程)",
        "SECFORC (截面内力)",
        "SPCFORC (约束反力)"
        });

    m_btnLoadData = new QPushButton("载入数据文件", this);
    m_btnClearPlot = new QPushButton("清空渲染图表", this);

    m_sensorList = new QListWidget(this);
    m_sensorList->setSelectionMode(QAbstractItemView::ExtendedSelection); // 支持多选进行多曲线对比

    leftLayout->addWidget(m_comboFileType);
    leftLayout->addWidget(m_btnLoadData);
    leftLayout->addWidget(m_btnClearPlot);
    leftLayout->addWidget(m_sensorList);

    // 初始化图表区
    m_plotWidget = new QCustomPlot(this);
    m_plotWidget->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);

    // 布局切分器
    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(leftPanel);
    splitter->addWidget(m_plotWidget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 4);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(splitter);
}

void PostProcessWidget::handleLoadData() {
    QString filter = "LS-DYNA ASCII Files (*.*);;All Files (*)";
    QString fileName = QFileDialog::getOpenFileName(this, "载入 LS-DYNA 后处理文件", "", filter);
    if (fileName.isEmpty()) return;

    // 加载新数据前清空内存池与界面
    m_simulationData.clear();
    m_sensorList->clear();
    m_plotWidget->clearGraphs();

    bool success = false;
    QString selectedType = m_comboFileType->currentText();

    // ==========================================
    // 依据用户选择，路由至对应的解析引擎
    // ==========================================
    if (selectedType.contains("GLSTAT")) {
        success = parseGlstat(fileName);
    }
    else if (selectedType.contains("MATSUM")) {
        success = parseMatsum(fileName);
    }
    else if (selectedType.contains("NODOUT")) {
        success = parseNodout(fileName);
    }
    else if (selectedType.contains("ELOUT")) {
        success = parseElout(fileName);
    }
    else if (selectedType.contains("SECFORC")) {
        success = parseSecforc(fileName);
    }
    else if (selectedType.contains("SPCFORC")) {
        success = parseSpcforc(fileName);
    }

    // 解析结果反馈
    if (success) {
        for (auto it = m_simulationData.begin(); it != m_simulationData.end(); ++it) {
            m_sensorList->addItem(it.key());
        }
        QMessageBox::information(this, "解析成功", QString("底层数据提取完成，当前载入时程序列数: %1").arg(m_simulationData.size()));
    }
    else {
        QMessageBox::warning(this, "解析异常", "文件流读取失败或未捕获目标特征集。\n请确认所选文件类型与下拉框选项一致。");
    }
}

// =====================================================================
// LS-DYNA ASCII 解析引擎实现域
// =====================================================================

bool PostProcessWidget::parseGlstat(const QString& filePath) {
    std::ifstream file(filePath.toLocal8Bit().constData());
    if (!file.is_open()) return false;

    std::string line;
    std::regex timeRegex(R"(time\s*=\s*([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?))");
    std::regex kinRegex(R"(kinetic energy\s+([\d\.E+-]+))");
    std::regex intRegex(R"(internal energy\s+([\d\.E+-]+))");

    double currentTime = 0.0;
    std::smatch match;
    bool found = false;

    while (std::getline(file, line)) {
        std::string lowerLine = line;
        std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);

        if (std::regex_search(lowerLine, match, timeRegex)) {
            currentTime = safeStod(match[1].str());
        }
        else if (std::regex_search(lowerLine, match, kinRegex)) {
            m_simulationData["Global Kinetic Energy"].time.append(currentTime);
            m_simulationData["Global Kinetic Energy"].value.append(safeStod(match[1].str()));
            found = true;
        }
        else if (std::regex_search(lowerLine, match, intRegex)) {
            m_simulationData["Global Internal Energy"].time.append(currentTime);
            m_simulationData["Global Internal Energy"].value.append(safeStod(match[1].str()));
            found = true;
        }
    }
    return found;
}

bool PostProcessWidget::parseMatsum(const QString& filePath) {
    std::ifstream file(filePath.toLocal8Bit().constData());
    if (!file.is_open()) return false;

    std::string line;
    std::regex timeRegex(R"(time\s*=\s*([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?))");
    std::regex partRegex(R"(part id\s+(\d+))");
    std::regex kinRegex(R"(kinetic energy\s+([\d\.E+-]+))");
    std::regex intRegex(R"(internal energy\s+([\d\.E+-]+))");

    double currentTime = 0.0;
    QString currentPart = "Unknown";
    std::smatch match;
    bool found = false;

    while (std::getline(file, line)) {
        std::string lowerLine = line;
        std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);

        if (std::regex_search(lowerLine, match, timeRegex)) {
            currentTime = safeStod(match[1].str());
        }
        else if (std::regex_search(lowerLine, match, partRegex)) {
            currentPart = QString::fromStdString(match[1].str());
        }
        else if (std::regex_search(lowerLine, match, kinRegex)) {
            QString key = QString("Part %1 - Kinetic Energy").arg(currentPart);
            m_simulationData[key].time.append(currentTime);
            m_simulationData[key].value.append(safeStod(match[1].str()));
            found = true;
        }
        else if (std::regex_search(lowerLine, match, intRegex)) {
            QString key = QString("Part %1 - Internal Energy").arg(currentPart);
            m_simulationData[key].time.append(currentTime);
            m_simulationData[key].value.append(safeStod(match[1].str()));
            found = true;
        }
    }
    return found;
}

bool PostProcessWidget::parseNodout(const QString& filePath) {
    std::ifstream file(filePath.toLocal8Bit().constData());
    if (!file.is_open()) return false;

    std::string line;
    std::regex timeRegex(R"(time\s*=\s*([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?))");
    std::regex nodeRegex(R"(^\s*(\d+)\s+([+-]?\S+)\s+([+-]?\S+)\s+([+-]?\S+))");

    double currentTime = 0.0;
    std::smatch match;
    bool found = false;

    while (std::getline(file, line)) {
        std::string lowerLine = line;
        std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);

        if (std::regex_search(lowerLine, match, timeRegex)) {
            currentTime = safeStod(match[1].str());
        }
        else if (std::regex_search(lowerLine, match, nodeRegex)) {
            QString nodeId = QString::fromStdString(match[1].str());

            double dz = safeStod(match[4].str());
            QString keyZ = QString("Node %1 - Disp Z").arg(nodeId);
            m_simulationData[keyZ].time.append(currentTime);
            m_simulationData[keyZ].value.append(dz);

            double dx = safeStod(match[2].str());
            double dy = safeStod(match[3].str());
            double mag = std::sqrt(dx * dx + dy * dy + dz * dz);

            QString keyMag = QString("Node %1 - Disp Mag").arg(nodeId);
            m_simulationData[keyMag].time.append(currentTime);
            m_simulationData[keyMag].value.append(mag);

            found = true;
        }
    }
    return found;
}

bool PostProcessWidget::parseElout(const QString& filePath) {
    std::ifstream file(filePath.toLocal8Bit().constData());
    if (!file.is_open()) return false;

    std::string line;
    std::regex timeRegex(R"(time\s*=\s*([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?))");
    std::regex valRegex(R"(^\s*(\d+)\s+([+-]?\S+)\s+([+-]?\S+)\s+([+-]?\S+)\s+([+-]?\S+)\s+([+-]?\S+)\s+([+-]?\S+)\s+([+-]?\S+))");

    double currentTime = 0.0;
    std::smatch match;
    bool found = false;

    while (std::getline(file, line)) {
        std::string lowerLine = line;
        std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);

        if (std::regex_search(lowerLine, match, timeRegex)) {
            currentTime = safeStod(match[1].str());
        }
        else if (std::regex_search(lowerLine, match, valRegex)) {
            QString id = QString::fromStdString(match[1].str());

            QString keyEff = QString("Element %1 - Eff. Stress (vM)").arg(id);
            m_simulationData[keyEff].time.append(currentTime);
            m_simulationData[keyEff].value.append(safeStod(match[8].str()));

            QString keyX = QString("Element %1 - Stress X").arg(id);
            m_simulationData[keyX].time.append(currentTime);
            m_simulationData[keyX].value.append(safeStod(match[2].str()));

            found = true;
        }
    }
    return found;
}

bool PostProcessWidget::parseSecforc(const QString& filePath) {
    std::ifstream file(filePath.toLocal8Bit().constData());
    if (!file.is_open()) return false;

    std::string line;
    std::regex timeRegex(R"(time\s*=\s*([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?))");
    std::regex valRegex(R"(^\s*(\d+)\s+([+-]?\S+)\s+([+-]?\S+)\s+([+-]?\S+)\s+([+-]?\S+)\s+([+-]?\S+)\s+([+-]?\S+))");

    double currentTime = 0.0;
    std::smatch match;
    bool found = false;

    while (std::getline(file, line)) {
        std::string lowerLine = line;
        std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);

        if (std::regex_search(lowerLine, match, timeRegex)) {
            currentTime = safeStod(match[1].str());
        }
        else if (std::regex_search(lowerLine, match, valRegex)) {
            QString id = QString::fromStdString(match[1].str());

            double fx = safeStod(match[2].str());
            double fy = safeStod(match[3].str());
            double fz = safeStod(match[4].str());
            double f_mag = std::sqrt(fx * fx + fy * fy + fz * fz);

            QString keyZ = QString("Section %1 - Force Z").arg(id);
            m_simulationData[keyZ].time.append(currentTime);
            m_simulationData[keyZ].value.append(fz);

            QString keyMag = QString("Section %1 - Force Mag").arg(id);
            m_simulationData[keyMag].time.append(currentTime);
            m_simulationData[keyMag].value.append(f_mag);

            found = true;
        }
    }
    return found;
}

bool PostProcessWidget::parseSpcforc(const QString& filePath) {
    std::ifstream file(filePath.toLocal8Bit().constData());
    if (!file.is_open()) return false;

    std::string line;
    std::regex timeRegex(R"(time\s*=\s*([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?))");
    std::regex valRegex(R"(^\s*(\d+)\s+([+-]?\S+)\s+([+-]?\S+)\s+([+-]?\S+))");

    double currentTime = 0.0;
    std::smatch match;
    bool found = false;

    while (std::getline(file, line)) {
        std::string lowerLine = line;
        std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);

        if (std::regex_search(lowerLine, match, timeRegex)) {
            currentTime = safeStod(match[1].str());
        }
        else if (std::regex_search(lowerLine, match, valRegex)) {
            QString id = QString::fromStdString(match[1].str());

            double fx = safeStod(match[2].str());
            double fy = safeStod(match[3].str());
            double fz = safeStod(match[4].str());
            double f_mag = std::sqrt(fx * fx + fy * fy + fz * fz);

            QString keyZ = QString("SPC Node %1 - Reaction Z").arg(id);
            m_simulationData[keyZ].time.append(currentTime);
            m_simulationData[keyZ].value.append(fz);

            QString keyMag = QString("SPC Node %1 - Reaction Mag").arg(id);
            m_simulationData[keyMag].time.append(currentTime);
            m_simulationData[keyMag].value.append(f_mag);

            found = true;
        }
    }
    return found;
}

// =====================================================================
// 图表渲染管线与坐标轴映射域
// =====================================================================

void PostProcessWidget::handleSensorSelectionChanged() {
    m_plotWidget->clearGraphs();

    QList<QListWidgetItem*> selectedItems = m_sensorList->selectedItems();
    if (selectedItems.isEmpty()) {
        m_plotWidget->replot();
        return;
    }

    // 预设配色板
    QList<QColor> palette = {
        QColor(0, 114, 189), QColor(217, 83, 25), QColor(237, 177, 32),
        QColor(126, 47, 142), QColor(119, 172, 48), QColor(77, 190, 238)
    };
    int colorIdx = 0;

    QStringList yAxisCategorySet;

    for (QListWidgetItem* item : selectedItems) {
        QString sensorName = item->text();
        if (!m_simulationData.contains(sensorName)) continue;

        SensorData data = m_simulationData[sensorName];

        QCPGraph* graph = m_plotWidget->addGraph();
        graph->setData(data.time, data.value);

        // ==========================================
        // 智能物理单位推演算法
        // ==========================================
        QString unitStr = "";
        QString axisCategory = "";

        if (sensorName.contains("Energy", Qt::CaseInsensitive)) {
            unitStr = "[10^5 J]";
            axisCategory = QString("Energy %1").arg(unitStr);
        }
        else if (sensorName.contains("Velocity", Qt::CaseInsensitive)) {
            unitStr = "[cm/μs]";
            axisCategory = QString("Velocity %1").arg(unitStr);
        }
        else if (sensorName.contains("Disp", Qt::CaseInsensitive)) {
            unitStr = "[cm]";
            axisCategory = QString("Displacement %1").arg(unitStr);
        }
        else if (sensorName.contains("Stress", Qt::CaseInsensitive)) {
            unitStr = "[Mbar/100GPa]";
            axisCategory = QString("Stress %1").arg(unitStr);
        }
        else if (sensorName.contains("Force", Qt::CaseInsensitive) || sensorName.contains("Reaction", Qt::CaseInsensitive)) {
            unitStr = "[10^7 Dyne]";
            axisCategory = QString("Force %1").arg(unitStr);
        }

        if (!axisCategory.isEmpty() && !yAxisCategorySet.contains(axisCategory)) {
            yAxisCategorySet << axisCategory;
        }

        // 图例显示带上单位
        graph->setName(QString("%1 %2").arg(sensorName).arg(unitStr));

        // 设置画笔
        QPen pen;
        pen.setColor(palette[colorIdx % palette.size()]);
        pen.setWidth(2);
        graph->setPen(pen);

        colorIdx++;
    }

    // 设置坐标轴标签
    m_plotWidget->xAxis->setLabel("Time [μs]");
    if (yAxisCategorySet.isEmpty()) {
        m_plotWidget->yAxis->setLabel("Numerical Value (Unspecified Unit)");
    }
    else {
        m_plotWidget->yAxis->setLabel(yAxisCategorySet.join("  |  "));
    }

    // 应用设置并重绘
    m_plotWidget->legend->setVisible(true);
    m_plotWidget->rescaleAxes();
    m_plotWidget->replot();
}
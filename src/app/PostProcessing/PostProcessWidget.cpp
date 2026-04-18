/**
 * @file PostProcessWidget.cpp
 * @brief 基于纯 Qt API 重构的 LS-DYNA 结果解析器
 * @details 彻底免疫中文路径打不开、C++ Locale 小数点识别错乱等系统级 Bug。
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
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <cmath>

namespace {
    /**
     * @brief [黑科技] 强健的 Qt 数值提取器
     * @details 无视 LS-DYNA 列宽粘连 (如 "-1.31E-01-1.43E-03")，无视 "***" 乱码。
     */
    inline QVector<double> extractNumbersRobust(const QString& line) {
        QVector<double> numbers;
        // 匹配科学计数法、常规浮点数、整数，或连续星号(溢出)
        static QRegularExpression re("([-+]?(?:\\d+\\.?\\d*|\\.\\d+)(?:[eE][-+]?\\d+)?|\\*{3,})");
        QRegularExpressionMatchIterator i = re.globalMatch(line);

        while (i.hasNext()) {
            QRegularExpressionMatch match = i.next();
            QString matchStr = match.captured(1);
            if (matchStr.contains("***")) {
                numbers.push_back(0.0);
            }
            else {
                // Qt的 toDouble 默认绑定 C-Locale，永不因系统语言设置报错
                numbers.push_back(matchStr.toDouble());
            }
        }
        return numbers;
    }
}

PostProcessWidget::PostProcessWidget(QWidget* parent) : QWidget(parent) {
    setupUI();

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

    m_comboFileType = new QComboBox(this);
    m_comboFileType->addItems({
        "GLSTAT (全局系统能量)",
        "NODOUT (节点运动历程)",
        "ELOUT (单元应力历程)",
        "RCFORC (接触面反力)",
        "SLEOUT (接触面能量)",
        "MATSUM (部件能量)",
        "SECFORC (截面内力)",
        "SPCFORC (约束反力)"
        });

    m_btnLoadData = new QPushButton("载入数据文件", this);
    m_btnClearPlot = new QPushButton("清空渲染图表", this);

    m_sensorList = new QListWidget(this);
    m_sensorList->setSelectionMode(QAbstractItemView::ExtendedSelection);

    leftLayout->addWidget(m_comboFileType);
    leftLayout->addWidget(m_btnLoadData);
    leftLayout->addWidget(m_btnClearPlot);
    leftLayout->addWidget(m_sensorList);

    m_plotWidget = new QCustomPlot(this);
    m_plotWidget->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);

    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(leftPanel);
    splitter->addWidget(m_plotWidget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 4);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(splitter);
}

void PostProcessWidget::handleLoadData() {
    QString fileName = QFileDialog::getOpenFileName(this, "载入 LS-DYNA 后处理文件", "", "All Files (*)");
    if (fileName.isEmpty()) return;

    m_simulationData.clear();
    m_sensorList->clear();
    m_plotWidget->clearGraphs();

    bool success = false;
    QString type = m_comboFileType->currentText();

    if (type.contains("GLSTAT"))      success = parseGlstat(fileName);
    else if (type.contains("NODOUT")) success = parseNodout(fileName);
    else if (type.contains("ELOUT"))  success = parseElout(fileName);
    else if (type.contains("RCFORC")) success = parseRcforc(fileName);
    else if (type.contains("SLEOUT")) success = parseSleout(fileName);
    else if (type.contains("MATSUM")) success = parseMatsum(fileName);
    else if (type.contains("SECFORC"))success = parseSecforc(fileName);
    else if (type.contains("SPCFORC"))success = parseSpcforc(fileName);

    if (success) {
        for (auto it = m_simulationData.begin(); it != m_simulationData.end(); ++it) {
            m_sensorList->addItem(it.key());
        }
        QMessageBox::information(this, "解析成功", QString("成功载入时程序列数: %1").arg(m_simulationData.size()));
    }
    else {
        QMessageBox::warning(this, "解析异常", "读取失败。请检查文件类型是否匹配，或文件是否已被损坏。");
    }
}

// =====================================================================
// 专属解析引擎 (基于 Qt QFile 彻底解决路径/换行符乱码问题)
// =====================================================================

bool PostProcessWidget::parseGlstat(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    QTextStream in(&file);
    double currentTime = 0.0;
    bool found = false;

    while (!in.atEnd()) {
        QString line = in.readLine();
        QString lowerLine = line.toLower();

        QVector<double> nums = extractNumbersRobust(line);
        if (nums.isEmpty()) continue;

        if (lowerLine.contains("time") && !lowerLine.contains("step") && !lowerLine.contains("zone")) {
            currentTime = nums[0];
        }
        else if (lowerLine.contains("kinetic energy") && !lowerLine.contains("eroded")) {
            m_simulationData["Global Kinetic Energy"].time.append(currentTime);
            m_simulationData["Global Kinetic Energy"].value.append(nums[0]);
            found = true;
        }
        else if (lowerLine.contains("internal energy") && !lowerLine.contains("eroded")) {
            m_simulationData["Global Internal Energy"].time.append(currentTime);
            m_simulationData["Global Internal Energy"].value.append(nums[0]);
            found = true;
        }
    }
    return found;
}

/**
 * @brief 解析部件/材料能量文件 (MATSUM)
 * @details 针对 LS-DYNA 紧凑缩写格式 (mat.#=, inten=, kinen=) 进行了精准适配。
 * @param filePath 文件绝对路径
 * @return 提取到有效数据返回 true，否则返回 false
 */
bool PostProcessWidget::parseMatsum(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    QTextStream in(&file);
    double currentTime = 0.0;
    bool found = false;

    while (!in.atEnd()) {
        QString line = in.readLine();
        QString lowerLine = line.toLower();

        // 使用数值提取器剥离所有文字和符号
        QVector<double> nums = extractNumbersRobust(line);
        if (nums.isEmpty()) continue;

        // 1. 匹配时间戳行 (例如 "time =   0.0000E+00")
        if (lowerLine.contains("time") && lowerLine.contains("=") && !lowerLine.contains("step")) {
            currentTime = nums[0];
        }
        // 2. 匹配材料能量行 (例如 "mat.#=    1             inten=   3.4176E+01     kinen=   0.0000E+00 ...")
        else if (lowerLine.contains("mat.#=")) {
            // 确保至少提取到了 ID、内能(inten)、动能(kinen) 三个核心数据
            if (nums.size() >= 3) {
                int matId = qRound(nums[0]);
                double internalEnergy = nums[1];
                double kineticEnergy = nums[2];

                // 记录当前 Part 的内能
                QString intKey = QString("Part %1 - Internal Energy").arg(matId);
                m_simulationData[intKey].time.append(currentTime);
                m_simulationData[intKey].value.append(internalEnergy);

                // 记录当前 Part 的动能
                QString kinKey = QString("Part %1 - Kinetic Energy").arg(matId);
                m_simulationData[kinKey].time.append(currentTime);
                m_simulationData[kinKey].value.append(kineticEnergy);

                found = true;
            }
        }
    }
    return found;
}
bool PostProcessWidget::parseNodout(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    QTextStream in(&file);
    // 匹配 ( at time 0.0000000E+00 )
    static QRegularExpression timeRegex("\\(\\s*at time\\s+([-+]?\\d*\\.?\\d+(?:[eE][-+]?\\d+)?)\\s*\\)");

    double currentTime = 0.0;
    bool found = false;

    while (!in.atEnd()) {
        QString line = in.readLine();
        QString lowerLine = line.toLower();

        QRegularExpressionMatch match = timeRegex.match(line);
        if (match.hasMatch()) {
            currentTime = match.captured(1).toDouble();
        }
        else {
            QVector<double> nums = extractNumbersRobust(line);
            if (nums.size() >= 4 && !lowerLine.contains("nodal") && !lowerLine.contains("disp")) {
                int id = qRound(nums[0]);
                if (id > 0 && qAbs(nums[0] - id) < 1e-6) {
                    double dx = nums[1], dy = nums[2], dz = nums[3];
                    double mag = std::sqrt(dx * dx + dy * dy + dz * dz);

                    m_simulationData[QString("Node %1 - Disp Z").arg(id)].time.append(currentTime);
                    m_simulationData[QString("Node %1 - Disp Z").arg(id)].value.append(dz);

                    m_simulationData[QString("Node %1 - Disp Mag").arg(id)].time.append(currentTime);
                    m_simulationData[QString("Node %1 - Disp Mag").arg(id)].value.append(mag);
                    found = true;
                }
            }
        }
    }
    return found;
}

bool PostProcessWidget::parseElout(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    QTextStream in(&file);
    static QRegularExpression timeRegex("\\(\\s*at time\\s+([-+]?\\d*\\.?\\d+(?:[eE][-+]?\\d+)?)\\s*\\)");
    static QRegularExpression idRegex("^\\s*(\\d+)-\\s*\\d+");

    double currentTime = 0.0;
    int currentElemId = -1;
    bool found = false;

    while (!in.atEnd()) {
        QString line = in.readLine();
        QString lowerLine = line.toLower();

        QRegularExpressionMatch timeMatch = timeRegex.match(line);
        QRegularExpressionMatch idMatch = idRegex.match(line);

        if (timeMatch.hasMatch()) {
            currentTime = timeMatch.captured(1).toDouble();
        }
        else if (idMatch.hasMatch()) {
            currentElemId = idMatch.captured(1).toInt();
        }
        else if (currentElemId != -1 && (lowerLine.contains("elastic") || lowerLine.contains("plastic"))) {
            QVector<double> nums = extractNumbersRobust(line);
            if (nums.size() >= 9) {
                double stressX = nums[1];
                double effStress = nums[8];

                m_simulationData[QString("Element %1 - Eff. Stress").arg(currentElemId)].time.append(currentTime);
                m_simulationData[QString("Element %1 - Eff. Stress").arg(currentElemId)].value.append(effStress);

                m_simulationData[QString("Element %1 - Stress X").arg(currentElemId)].time.append(currentTime);
                m_simulationData[QString("Element %1 - Stress X").arg(currentElemId)].value.append(stressX);

                found = true;
                currentElemId = -1; // 归位
            }
        }
    }
    return found;
}

bool PostProcessWidget::parseRcforc(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    QTextStream in(&file);
    bool found = false;

    while (!in.atEnd()) {
        QString line = in.readLine();
        QString lowerLine = line.toLower();

        if (lowerLine.contains("surfa") || lowerLine.contains("surfb")) {
            QVector<double> nums = extractNumbersRobust(line);
            if (nums.size() >= 5) {
                int id = qRound(nums[0]);
                double t = nums[1];
                double fx = nums[2], fy = nums[3], fz = nums[4];
                double mag = std::sqrt(fx * fx + fy * fy + fz * fz);

                QString side = lowerLine.contains("surfa") ? "Master" : "Slave";

                m_simulationData[QString("Contact %1 - %2 Force Z").arg(id).arg(side)].time.append(t);
                m_simulationData[QString("Contact %1 - %2 Force Z").arg(id).arg(side)].value.append(fz);

                m_simulationData[QString("Contact %1 - %2 Force Mag").arg(id).arg(side)].time.append(t);
                m_simulationData[QString("Contact %1 - %2 Force Mag").arg(id).arg(side)].value.append(mag);
                found = true;
            }
        }
    }
    return found;
}

bool PostProcessWidget::parseSleout(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    QTextStream in(&file);
    static QRegularExpression timeRegex("time=\\s*([-+]?\\d*\\.?\\d+(?:[eE][-+]?\\d+)?)");

    double currentTime = 0.0;
    bool found = false;

    while (!in.atEnd()) {
        QString line = in.readLine();
        QString lowerLine = line.toLower();

        QRegularExpressionMatch match = timeRegex.match(line);
        if (match.hasMatch()) {
            currentTime = match.captured(1).toDouble();
        }
        else {
            QVector<double> nums = extractNumbersRobust(line);
            if (nums.size() >= 3 && !lowerLine.contains("summary") && !lowerLine.contains("surfa")) {
                int id = qRound(nums[0]);
                if (id > 0 && qAbs(nums[0] - id) < 1e-6) {
                    double slaveEng = nums[1];
                    double masterEng = nums[2];

                    m_simulationData[QString("Contact %1 - Slave Energy").arg(id)].time.append(currentTime);
                    m_simulationData[QString("Contact %1 - Slave Energy").arg(id)].value.append(slaveEng);
                    found = true;
                }
            }
        }
    }
    return found;
}

bool PostProcessWidget::parseSecforc(const QString& filePath) { return false; }
bool PostProcessWidget::parseSpcforc(const QString& filePath) { return false; }

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

        QString unitStr = "";
        QString axisCategory = "";

        if (sensorName.contains("Energy", Qt::CaseInsensitive)) {
            unitStr = "[10^5 J]";
            axisCategory = QString("Energy %1").arg(unitStr);
        }
        else if (sensorName.contains("Disp", Qt::CaseInsensitive)) {
            unitStr = "[cm]";
            axisCategory = QString("Displacement %1").arg(unitStr);
        }
        else if (sensorName.contains("Stress", Qt::CaseInsensitive)) {
            unitStr = "[Mbar]";
            axisCategory = QString("Stress %1").arg(unitStr);
        }
        else if (sensorName.contains("Force", Qt::CaseInsensitive)) {
            unitStr = "[10^7 Dyne]";
            axisCategory = QString("Force %1").arg(unitStr);
        }

        if (!axisCategory.isEmpty() && !yAxisCategorySet.contains(axisCategory)) {
            yAxisCategorySet << axisCategory;
        }

        graph->setName(QString("%1 %2").arg(sensorName).arg(unitStr));

        QPen pen;
        pen.setColor(palette[colorIdx % palette.size()]);
        pen.setWidth(2);
        graph->setPen(pen);
        colorIdx++;
    }

    m_plotWidget->xAxis->setLabel("Time [μs]");
    if (yAxisCategorySet.isEmpty()) m_plotWidget->yAxis->setLabel("Value");
    else m_plotWidget->yAxis->setLabel(yAxisCategorySet.join("  |  "));

    m_plotWidget->legend->setVisible(true);
    m_plotWidget->rescaleAxes();
    m_plotWidget->replot();
}
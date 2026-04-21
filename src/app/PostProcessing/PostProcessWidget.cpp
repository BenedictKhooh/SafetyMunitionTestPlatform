/**
 * @file PostProcessWidget.cpp
 * @brief LS-DYNA 后处理自动化分析与网格化可视化组件实现
 * @author
 * @date 2024
 */

#include "PostProcessWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QDir>
#include <QTextStream>
#include <QLabel>
#include <QDebug>
#include <cmath>
#include <QRegExp>

 /**
  * @brief 构造函数：初始化色盘并构建界面
  * @param parent 父窗口指针
  */
PostProcessWidget::PostProcessWidget(QWidget* parent) : QWidget(parent) {
    // 初始化专业工程绘图色盘 (MATLAB 标准配色)
    m_colorPalette << QColor(0, 114, 189) << QColor(217, 83, 25)
        << QColor(237, 177, 32) << QColor(126, 47, 142)
        << QColor(119, 172, 48) << QColor(77, 190, 238);
    setupUI();
}

/**
 * @brief 构建 UI 界面，采用 2x2 网格化布局实现全景监控
 */
void PostProcessWidget::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // 1. 顶部自动化工具栏
    QHBoxLayout* toolBar = new QHBoxLayout();
    QPushButton* btnManualLoad = new QPushButton("手动扫描工作目录");
    btnManualLoad->setMinimumHeight(35);
    btnManualLoad->setStyleSheet("font-weight: bold; background-color: #34495E; color: white;");

    toolBar->addWidget(btnManualLoad);
    toolBar->addStretch();
    mainLayout->addLayout(toolBar);

    // 2. 核心网格监控区 (2x2 Grid)
    QWidget* gridContainer = new QWidget();
    m_gridLayout = new QGridLayout(gridContainer);
    m_gridLayout->setSpacing(15);

    // 初始化四个核心维度的图表
    createGridPlot("glstat", "全局系统能量 (GLSTAT)", 0, 0);
    createGridPlot("matsum", "部件能量分布 (MATSUM)", 0, 1);
    createGridPlot("nodout", "关键节点运动历程 (NODOUT)", 1, 0);
    createGridPlot("elout", "单元应力与接触力 (ELOUT/RCFORC)", 1, 1);

    mainLayout->addWidget(gridContainer);

    // 3. 信号槽绑定
    connect(btnManualLoad, &QPushButton::clicked, this, &PostProcessWidget::handleManualDirSelect);
}

/**
 * @brief 创建并配置单个 QCustomPlot 实例
 * @param id 图表唯一标识符
 * @param title 图表标题
 * @param row 网格所在行
 * @param col 网格所在列
 */
void PostProcessWidget::createGridPlot(const QString& id, const QString& title, int row, int col) {
    QCustomPlot* plot = new QCustomPlot();
    plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);

    // 设置深色风格辅助线
    plot->xAxis->grid()->setPen(QPen(QColor(200, 200, 200), 1, Qt::DotLine));
    plot->yAxis->grid()->setPen(QPen(QColor(200, 200, 200), 1, Qt::DotLine));

    // 添加标题栏
    plot->plotLayout()->insertRow(0);
    QCPTextElement* titleElement = new QCPTextElement(plot, title, QFont("Microsoft YaHei", 10, QFont::Bold));
    plot->plotLayout()->addElement(0, 0, titleElement);

    plot->xAxis->setLabel("时间 (Time) [us]");
    plot->legend->setVisible(true);
    plot->legend->setFont(QFont("Consolas", 8));
    plot->legend->setBrush(QBrush(QColor(255, 255, 255, 150)));

    m_plotMap[id] = plot;
    m_gridLayout->addWidget(plot, row, col);
}

/**
 * @brief 响应手动选择文件夹操作
 */
void PostProcessWidget::handleManualDirSelect() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择 LS-DYNA 求解器输出目录");
    if (!dir.isEmpty()) {
        autoScanAndPlot(dir);
    }
}

/**
 * @brief 清空仪表盘所有图表
 */
void PostProcessWidget::clearDashboard() {
    for (QCustomPlot* p : m_plotMap.values()) {
        p->clearGraphs();
        p->replot();
    }
}

/**
 * @brief 执行自动化扫描：自动寻找目录下的文件并分流解析
 * @param workingDir 目标工作目录路径
 */
void PostProcessWidget::autoScanAndPlot(const QString& workingDir) {
    if (workingDir.isEmpty()) return;

    clearDashboard();
    m_currentWorkDir = workingDir;
    QDir dir(workingDir);

    // 任务队列：定义文件名与对应的解析器映射
    QMap<QString, QString> taskQueue;
    taskQueue["glstat"] = "glstat";
    taskQueue["matsum"] = "matsum";
    taskQueue["nodout"] = "nodout";
    taskQueue["elout"] = "elout";
    taskQueue["rcforc"] = "rcforc";

    // 遍历匹配存在的文件并调用对应的解析器
    for (auto it = taskQueue.begin(); it != taskQueue.end(); ++it) {
        QString filePath = dir.absoluteFilePath(it.key());
        if (dir.exists(it.key())) {
            QString type = it.value();
            if (type == "glstat") processGlstat(filePath);
            else if (type == "matsum") processMatsum(filePath);
            else if (type == "nodout") processNodout(filePath);
            else if (type == "elout")  processElout(filePath);
            else if (type == "rcforc") processRcforc(filePath);
        }
    }

    // 统一自适应视口并重绘
    for (QCustomPlot* p : m_plotMap.values()) {
        p->rescaleAxes();
        p->replot();
    }
}

// ============================================================================
// 后处理文件解析引擎具体实现 (基于有限状态机与正则分割)
// ============================================================================

/**
 * @brief 解析 GLSTAT 文件 (提取全局动能、内能)
 */
bool PostProcessWidget::processGlstat(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    QVector<double> time, ke, ie;
    double currentTime = 0.0;
    QTextStream in(&file);

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.startsWith("time", Qt::CaseInsensitive)) {
            currentTime = line.split(QRegExp("\\s+"), QString::SkipEmptyParts).last().toDouble();
        }
        else if (line.startsWith("kinetic energy", Qt::CaseInsensitive)) {
            time.append(currentTime);
            ke.append(line.split(QRegExp("\\s+"), QString::SkipEmptyParts).last().toDouble());
        }
        else if (line.startsWith("internal energy", Qt::CaseInsensitive)) {
            ie.append(line.split(QRegExp("\\s+"), QString::SkipEmptyParts).last().toDouble());
        }
    }
    file.close();

    QCustomPlot* p = m_plotMap["glstat"];
    if (!p || time.isEmpty()) return false;
    p->yAxis->setLabel("系统能量 [10^5 J]");

    p->addGraph(); p->graph()->setData(time, ke); p->graph()->setName("全局动能 (KE)");
    p->graph()->setPen(QPen(m_colorPalette[0 % m_colorPalette.size()], 2));

    p->addGraph(); p->graph()->setData(time, ie); p->graph()->setName("全局内能 (IE)");
    p->graph()->setPen(QPen(m_colorPalette[1 % m_colorPalette.size()], 2));

    return true;
}

/**
 * @brief 解析 MATSUM 文件 (提取各个 Part 的内能与动能)
 */
bool PostProcessWidget::processMatsum(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    QMap<int, QVector<double>> timeMap, ieMap, keMap;
    double currentTime = 0.0;
    QTextStream in(&file);

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.startsWith("time =", Qt::CaseInsensitive)) {
            currentTime = line.split(QRegExp("\\s+|="), QString::SkipEmptyParts).last().toDouble();
        }
        else if (line.startsWith("mat.#=", Qt::CaseInsensitive)) {
            QStringList parts = line.replace("=", " ").split(QRegExp("\\s+"), QString::SkipEmptyParts);
            if (parts.size() >= 6) {
                int partId = parts[1].toInt();
                timeMap[partId].append(currentTime);
                ieMap[partId].append(parts[3].toDouble()); // inten
                keMap[partId].append(parts[5].toDouble()); // kinen
            }
        }
    }
    file.close();

    QCustomPlot* p = m_plotMap["matsum"];
    if (!p || timeMap.isEmpty()) return false;
    p->yAxis->setLabel("部件能量 [10^5 J]");

    int colorIdx = 0, count = 0;
    for (int pid : ieMap.keys()) {
        if (count++ > 3) break; // 最多绘制 4 个 Part 以防止界面卡顿

        p->addGraph(); p->graph()->setData(timeMap[pid], ieMap[pid]);
        p->graph()->setName(QString("Part %1 内能").arg(pid));
        p->graph()->setPen(QPen(m_colorPalette[colorIdx % m_colorPalette.size()], 2));

        p->addGraph(); p->graph()->setData(timeMap[pid], keMap[pid]);
        p->graph()->setName(QString("Part %1 动能").arg(pid));
        p->graph()->setPen(QPen(m_colorPalette[colorIdx % m_colorPalette.size()], 2, Qt::DashLine));
        colorIdx++;
    }
    return true;
}

/**
 * @brief 解析 NODOUT 文件 (提取关键节点的运动速度)
 */
bool PostProcessWidget::processNodout(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    QMap<int, QVector<double>> timeMap, velMap;
    double currentTime = 0.0;
    bool isDataBlock = false;
    QTextStream in(&file);

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();

        if (line.contains("n o d a l") && line.contains("at time")) {
            QString timeStr = line.section("time", -1).remove(")").trimmed();
            currentTime = timeStr.toDouble();
            isDataBlock = false;
        }
        else if (line.startsWith("nodal point")) {
            isDataBlock = true;
        }
        else if (line.isEmpty() || line.startsWith("legend")) {
            isDataBlock = false;
        }

        if (isDataBlock && !line.isEmpty() && line[0].isDigit()) {
            QStringList parts = line.split(QRegExp("\\s+"), QString::SkipEmptyParts);
            if (parts.size() >= 7) {
                int nodeId = parts[0].toInt();
                double vx = parts[4].toDouble();
                double vy = parts[5].toDouble();
                double vz = parts[6].toDouble();
                double velMag = std::sqrt(vx * vx + vy * vy + vz * vz);

                timeMap[nodeId].append(currentTime);
                velMap[nodeId].append(velMag);
            }
        }
    }
    file.close();

    QCustomPlot* p = m_plotMap["nodout"];
    if (!p || timeMap.isEmpty()) return false;
    p->yAxis->setLabel("节点合速度 [cm/us]");

    int colorIdx = 0, count = 0;
    for (int nid : velMap.keys()) {
        if (count++ > 4) break; // 最多绘制 5 个节点
        p->addGraph(); p->graph()->setData(timeMap[nid], velMap[nid]);
        p->graph()->setName(QString("Node %1 速度").arg(nid));
        p->graph()->setPen(QPen(m_colorPalette[colorIdx++ % m_colorPalette.size()], 2));
    }
    return true;
}

/**
 * @brief 解析 ELOUT 文件 (提取关键单元的 Von-Mises 应力)
 */
bool PostProcessWidget::processElout(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    QMap<int, QVector<double>> timeMap, stressMap;
    double currentTime = 0.0;
    int currentElemId = -1;
    QTextStream in(&file);

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();

        if (line.contains("e l e m e n t") && line.contains("at time")) {
            QString timeStr = line.section("time", -1).remove(")").trimmed();
            currentTime = timeStr.toDouble();
        }
        else if (line.contains("-") && line[0].isDigit()) {
            QStringList parts = line.split("-", QString::SkipEmptyParts);
            if (!parts.isEmpty()) currentElemId = parts[0].simplified().toInt();
        }
        else if ((line.contains("elastic") || line.contains("plastic")) && currentElemId != -1) {
            QStringList parts = line.split(QRegExp("\\s+"), QString::SkipEmptyParts);
            if (parts.size() >= 9) {
                double effectiveStress = parts[8].toDouble(); // yield/effsg 位于第 9 列
                timeMap[currentElemId].append(currentTime);
                stressMap[currentElemId].append(effectiveStress);
            }
            currentElemId = -1;
        }
    }
    file.close();

    QCustomPlot* p = m_plotMap["elout"];
    if (!p || timeMap.isEmpty()) return false;
    p->yAxis->setLabel("应力/接触力 [Mbar]");

    int colorIdx = 0, count = 0;
    for (int eid : stressMap.keys()) {
        if (count++ > 3) break; // 最多绘制 4 个单元
        p->addGraph(); p->graph()->setData(timeMap[eid], stressMap[eid]);
        p->graph()->setName(QString("Elem %1 V-M应力").arg(eid));
        p->graph()->setPen(QPen(m_colorPalette[colorIdx++ % m_colorPalette.size()], 2));
    }
    return true;
}

/**
 * @brief 解析 RCFORC 文件 (提取主从面的接触合力)
 */
bool PostProcessWidget::processRcforc(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    QMap<int, QVector<double>> timeMap, forceMap;
    QTextStream in(&file);

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();

        if (line.startsWith("SURFA", Qt::CaseInsensitive)) {
            QStringList parts = line.split(QRegExp("\\s+"), QString::SkipEmptyParts);
            if (parts.size() >= 10) {
                int interfaceId = parts[1].toInt();
                double time = parts[3].toDouble();
                double fx = parts[5].toDouble();
                double fy = parts[7].toDouble();
                double fz = parts[9].toDouble();

                double resultantForce = std::sqrt(fx * fx + fy * fy + fz * fz);
                timeMap[interfaceId].append(time);
                forceMap[interfaceId].append(resultantForce);
            }
        }
    }
    file.close();

    // 共享右下角的 elout 图表面板
    QCustomPlot* p = m_plotMap["elout"];
    if (!p || timeMap.isEmpty()) return false;

    int colorIdx = 4; // 避开前面应力使用的颜色
    for (int iid : forceMap.keys()) {
        p->addGraph(); p->graph()->setData(timeMap[iid], forceMap[iid]);
        p->graph()->setName(QString("接触界面 %1 合力").arg(iid));
        p->graph()->setPen(QPen(m_colorPalette[colorIdx++ % m_colorPalette.size()], 2, Qt::DotLine)); // 接触力使用虚线
    }
    return true;
}
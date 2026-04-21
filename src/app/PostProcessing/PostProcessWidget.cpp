/**
 * @file PostProcessWidget.cpp
 * @brief LS-DYNA 后处理自动化分析与网格化多参数可视化组件实现
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
    QPushButton* btnManualLoad = new QPushButton("手动扫描工作目录并自动分析");
    btnManualLoad->setMinimumHeight(35);
    btnManualLoad->setStyleSheet("font-weight: bold; background-color: #34495E; color: white;");

    toolBar->addWidget(btnManualLoad);
    toolBar->addStretch();
    mainLayout->addLayout(toolBar);

    // 2. 核心网格监控区 (2x2 Grid)
    QWidget* gridContainer = new QWidget();
    m_gridLayout = new QGridLayout(gridContainer);
    m_gridLayout->setSpacing(15);

    // 初始化四个核心维度的图表 (附带对应的参数切换下拉框)
    createGridPlot("glstat", "全局系统能量 (GLSTAT)", 0, 0);
    createGridPlot("matsum", "部件能量分布 (MATSUM)", 0, 1);
    createGridPlot("nodout", "节点运动历程 (NODOUT)", 1, 0, &m_comboNodout);
    createGridPlot("elout", "单元与接触历程 (ELOUT/RCFORC)", 1, 1, &m_comboElout);

    mainLayout->addWidget(gridContainer);

    // 3. 信号槽绑定
    connect(btnManualLoad, &QPushButton::clicked, this, &PostProcessWidget::handleManualDirSelect);

    // 绑定下拉框切换事件重绘图表
    if (m_comboNodout) connect(m_comboNodout, &QComboBox::currentTextChanged, this, &PostProcessWidget::updateNodoutPlot);
    if (m_comboElout)  connect(m_comboElout, &QComboBox::currentTextChanged, this, &PostProcessWidget::updateEloutPlot);
}

/**
 * @brief 采用 Widget 嵌套布局，将标题、下拉框和图表优雅组合
 */
void PostProcessWidget::createGridPlot(const QString& id, const QString& title, int row, int col, QComboBox** comboOut) {
    QWidget* cellWidget = new QWidget();
    QVBoxLayout* cellLayout = new QVBoxLayout(cellWidget);
    cellLayout->setContentsMargins(0, 0, 0, 0);

    // 顶部标题与下拉框区域
    QHBoxLayout* headerLayout = new QHBoxLayout();
    QLabel* lblTitle = new QLabel(title);
    lblTitle->setFont(QFont("Microsoft YaHei", 10, QFont::Bold));
    headerLayout->addWidget(lblTitle);

    if (comboOut != nullptr) {
        *comboOut = new QComboBox();
        (*comboOut)->setMinimumWidth(180);
        headerLayout->addStretch();
        headerLayout->addWidget(*comboOut);
    }
    cellLayout->addLayout(headerLayout);

    // QCustomPlot 图表区域
    QCustomPlot* plot = new QCustomPlot();
    plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
    plot->xAxis->grid()->setPen(QPen(QColor(200, 200, 200), 1, Qt::DotLine));
    plot->yAxis->grid()->setPen(QPen(QColor(200, 200, 200), 1, Qt::DotLine));
    plot->xAxis->setLabel("时间 (Time) [us]");
    plot->legend->setVisible(true);
    plot->legend->setFont(QFont("Consolas", 8));
    plot->legend->setBrush(QBrush(QColor(255, 255, 255, 150)));

    cellLayout->addWidget(plot, 1);

    m_plotMap[id] = plot;
    m_gridLayout->addWidget(cellWidget, row, col);
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
 * @brief 清空仪表盘所有图表及内存缓存
 */
void PostProcessWidget::clearDashboard() {
    for (QCustomPlot* p : m_plotMap.values()) {
        p->clearGraphs();
        p->replot();
    }
    // 清空字典内存缓存
    m_nodoutData.clear(); m_nodoutTimeMap.clear();
    m_eloutData.clear();  m_eloutTimeMap.clear();

    // 静默清空下拉框
    if (m_comboNodout) { m_comboNodout->blockSignals(true); m_comboNodout->clear(); m_comboNodout->blockSignals(false); }
    if (m_comboElout) { m_comboElout->blockSignals(true); m_comboElout->clear(); m_comboElout->blockSignals(false); }
}

/**
 * @brief 执行自动化扫描：自动寻找目录下的文件并分流解析
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
        if (count++ > 3) break; // 防止线过多导致卡顿，最多画 4 个部件

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
 * @brief 解析 NODOUT 文件的所有列 (包含对 Fortran 负号粘连的完美修复)
 */
bool PostProcessWidget::processNodout(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    double currentTime = 0.0;
    bool isDataBlock = false;
    QTextStream in(&file);

    QStringList params = { "x-disp (X位移)", "y-disp (Y位移)", "z-disp (Z位移)",
                          "x-vel (X速度)", "y-vel (Y速度)", "z-vel (Z速度)",
                          "x-accl (X加速度)", "y-accl (Y加速度)", "z-accl (Z加速度)",
                          "res-vel (合速度)" };

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();

        if (line.contains("n o d a l") && line.contains("at time")) {
            currentTime = line.section("time", -1).remove(")").trimmed().toDouble();
            isDataBlock = false;
        }
        else if (line.startsWith("nodal point")) isDataBlock = true;
        else if (line.isEmpty() || line.startsWith("legend")) isDataBlock = false;

        if (isDataBlock && !line.isEmpty() && line[0].isDigit()) {

            // 🌟 核心修复：分离 Fortran 格式黏连的负数 (例如 -1.2E-01-1.3E-02)
            QString cleanLine = line;
            cleanLine.replace("E-", "E_").replace("e-", "e_"); // 保护科学计数法符号
            cleanLine.replace("-", " -");                      // 强制分割负数
            cleanLine.replace("E_", "E-").replace("e_", "e-"); // 还原

            QStringList parts = cleanLine.split(QRegExp("\\s+"), QString::SkipEmptyParts);

            if (parts.size() >= 10) {
                int nodeId = parts[0].toInt();
                m_nodoutTimeMap[nodeId].append(currentTime);

                m_nodoutData[nodeId]["x-disp (X位移)"].append(parts[1].toDouble());
                m_nodoutData[nodeId]["y-disp (Y位移)"].append(parts[2].toDouble());
                m_nodoutData[nodeId]["z-disp (Z位移)"].append(parts[3].toDouble());

                double vx = parts[4].toDouble(), vy = parts[5].toDouble(), vz = parts[6].toDouble();
                m_nodoutData[nodeId]["x-vel (X速度)"].append(vx);
                m_nodoutData[nodeId]["y-vel (Y速度)"].append(vy);
                m_nodoutData[nodeId]["z-vel (Z速度)"].append(vz);
                m_nodoutData[nodeId]["res-vel (合速度)"].append(std::sqrt(vx * vx + vy * vy + vz * vz));

                m_nodoutData[nodeId]["x-accl (X加速度)"].append(parts[7].toDouble());
                m_nodoutData[nodeId]["y-accl (Y加速度)"].append(parts[8].toDouble());
                m_nodoutData[nodeId]["z-accl (Z加速度)"].append(parts[9].toDouble());
            }
        }
    }
    file.close();

    // 解析完毕，初始化下拉框并触发初次绘图
    if (m_comboNodout && !m_nodoutData.isEmpty()) {
        m_comboNodout->blockSignals(true);
        m_comboNodout->clear();
        m_comboNodout->addItems(params);
        m_comboNodout->setCurrentIndex(9); // 默认选合速度
        m_comboNodout->blockSignals(false);
        updateNodoutPlot();
    }
    return true;
}

/**
 * @brief 解析 ELOUT 文件的所有应力列 (包含静水压力与塑性屈服等全参数读取)
 */
bool PostProcessWidget::processElout(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    double currentTime = 0.0;
    int currentElemId = -1;
    QTextStream in(&file);

    // 🌟 扩充参数列表，加入衍生计算的静水压力和原生的塑性屈服函数
    QStringList params = { "sig-xx (X正应力)", "sig-yy (Y正应力)", "sig-zz (Z正应力)",
                          "sig-xy (XY剪应力)", "sig-yz (YZ剪应力)", "sig-zx (ZX剪应力)",
                          "effsg (Von-Mises 等效应力)", "pressure (静水压力)", "yield (屈服函数/塑性应变)" };

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();

        if (line.contains("e l e m e n t") && line.contains("at time")) {
            currentTime = line.section("time", -1).remove(")").trimmed().toDouble();
        }
        // 兼容无横杠的 Element ID 行
        else if (!line.isEmpty() && line[0].isDigit() &&
            !line.contains("elastic", Qt::CaseInsensitive) &&
            !line.contains("plastic", Qt::CaseInsensitive) &&
            !line.contains("failed", Qt::CaseInsensitive)) {
            QString firstToken = line.split(QRegExp("\\s+|-"), QString::SkipEmptyParts).first();
            currentElemId = firstToken.toInt();
        }
        else if ((line.contains("elastic") || line.contains("plastic") || line.contains("failed")) && currentElemId != -1) {

            // 解决多个负数连在一起没有空格的 Fortran 经典粘连问题
            QString cleanLine = line;
            cleanLine.replace("E-", "E_").replace("e-", "e_");
            cleanLine.replace("-", " -");
            cleanLine.replace("E_", "E-").replace("e_", "e-");

            QStringList parts = cleanLine.split(QRegExp("\\s+"), QString::SkipEmptyParts);

            if (parts.size() >= 9) {
                double sig_xx = parts[2].toDouble();
                double sig_yy = parts[3].toDouble();
                double sig_zz = parts[4].toDouble();

                // 🌟 核心扩展：物理机制 - 根据三个主应力分量计算静水压力 (Pressure)
                // LS-DYNA 惯例中，拉伸为正，压缩为负。静水压力 P = - (σx + σy + σz) / 3
                double pressure = -(sig_xx + sig_yy + sig_zz) / 3.0;

                m_eloutTimeMap[currentElemId].append(currentTime);
                m_eloutData[currentElemId]["sig-xx (X正应力)"].append(sig_xx);
                m_eloutData[currentElemId]["sig-yy (Y正应力)"].append(sig_yy);
                m_eloutData[currentElemId]["sig-zz (Z正应力)"].append(sig_zz);
                m_eloutData[currentElemId]["sig-xy (XY剪应力)"].append(parts[5].toDouble());
                m_eloutData[currentElemId]["sig-yz (YZ剪应力)"].append(parts[6].toDouble());
                m_eloutData[currentElemId]["sig-zx (ZX剪应力)"].append(parts[7].toDouble());
                m_eloutData[currentElemId]["effsg (Von-Mises 等效应力)"].append(parts[8].toDouble());

                // 记录推导计算得到的静水压力
                m_eloutData[currentElemId]["pressure (静水压力)"].append(pressure);

                // 🌟 提取第 10 列的 yield/eff. plastic strain (如果有输出的话，部分材料模型可能不输出第10列)
                double yield_val = (parts.size() >= 10) ? parts[9].toDouble() : 0.0;
                m_eloutData[currentElemId]["yield (屈服函数/塑性应变)"].append(yield_val);
            }
            currentElemId = -1; // 读完后复位
        }
    }
    file.close();

    if (m_comboElout && !m_eloutData.isEmpty()) {
        m_comboElout->blockSignals(true);
        m_comboElout->clear();
        m_comboElout->addItems(params);
        m_comboElout->setCurrentIndex(6); // 默认依然选 Von-Mises 等效应力
        m_comboElout->blockSignals(false);
        updateEloutPlot();
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

    // 默认绘制到右下角 (elout 所在) 的图表
    QCustomPlot* p = m_plotMap["elout"];
    if (!p || timeMap.isEmpty()) return false;

    int colorIdx = 4; // 避开应力图的常用色
    for (int iid : forceMap.keys()) {
        p->addGraph(); p->graph()->setData(timeMap[iid], forceMap[iid]);
        p->graph()->setName(QString("接触界面 %1 合力").arg(iid));
        p->graph()->setPen(QPen(m_colorPalette[colorIdx++ % m_colorPalette.size()], 2, Qt::DotLine)); // 接触力使用虚线
    }
    return true;
}

// ============================================================================
// 下拉框动态切换重绘槽函数
// ============================================================================

/**
 * @brief 根据用户在下拉框的选择，动态重绘 NODOUT 曲线
 */
void PostProcessWidget::updateNodoutPlot() {
    if (!m_comboNodout || m_nodoutData.isEmpty()) return;
    QString selectedParam = m_comboNodout->currentText();

    QCustomPlot* p = m_plotMap["nodout"];
    p->clearGraphs();

    // 动态调整 Y 轴单位标签
    if (selectedParam.contains("位移")) p->yAxis->setLabel(selectedParam + " [cm]");
    else if (selectedParam.contains("速度")) p->yAxis->setLabel(selectedParam + " [cm/us]");
    else if (selectedParam.contains("加速度")) p->yAxis->setLabel(selectedParam + " [cm/us^2]");
    else p->yAxis->setLabel(selectedParam);

    int colorIdx = 0, count = 0;
    for (int nid : m_nodoutData.keys()) {
        if (count++ > 4) break; // 最多画 5 个节点以保持界面流畅
        p->addGraph();
        p->graph()->setData(m_nodoutTimeMap[nid], m_nodoutData[nid][selectedParam]);
        p->graph()->setName(QString("Node %1").arg(nid));
        p->graph()->setPen(QPen(m_colorPalette[colorIdx++ % m_colorPalette.size()], 2));
    }
    p->rescaleAxes();
    p->replot();
}

/**
 * @brief 根据用户在下拉框的选择，动态重绘 ELOUT 曲线
 */
void PostProcessWidget::updateEloutPlot() {
    if (!m_comboElout || m_eloutData.isEmpty()) return;
    QString selectedParam = m_comboElout->currentText();

    QCustomPlot* p = m_plotMap["elout"];
    p->clearGraphs();
    p->yAxis->setLabel(selectedParam + " [Mbar]");

    int colorIdx = 0, count = 0;
    for (int eid : m_eloutData.keys()) {
        if (count++ > 4) break; // 最多画 5 个单元以保持界面流畅
        p->addGraph();
        p->graph()->setData(m_eloutTimeMap[eid], m_eloutData[eid][selectedParam]);
        p->graph()->setName(QString("Elem %1").arg(eid));
        p->graph()->setPen(QPen(m_colorPalette[colorIdx++ % m_colorPalette.size()], 2));
    }

    // 由于 elout 图表与 rcforc 共享，重绘参数时可能需要把 rcforc 曲线重新加回来
    // 这里为了逻辑纯粹，切换应力时仅绘制应力数据

    p->rescaleAxes();
    p->replot();
}
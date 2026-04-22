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
 * @brief 解析 ELOUT 文件的核心引擎 (支持提取应力、压力、屈服及反应度全参数)
 * @param path elout 结果文件的绝对路径
 * @return bool 解析成功返回 true，文件打开失败返回 false
 * * @details 该解析器采用了基于状态机 (State Machine) 的按行读取策略。
 * 针对 LS-DYNA 的输出陷阱进行了三重防御：
 * 1. [区块隔离]：屏蔽 s t r a i n (应变) 块的干扰。
 * 2. [负号粘连]：修复 Fortran 科学计数法格式化导致的数字粘连。
 * 3. [缺省防御]：通过特征匹配 (小数点) 识别历史变量是否真正输出。若 K 文件未开启
 * NEIPH 导致历史变量丢失，将自动补零防报错，并防止吞噬下一单元的 ID。
 */
 /**
  * @brief 解析 ELOUT 文件的核心引擎 (支持提取应力、压力、屈服及反应度全参数)
  * @details 适配单元 ID 格式 "ID- PARTID" 及其对应的 stress 和 histry 数据块
  */
bool PostProcessWidget::processElout(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    double currentTime = 0.0;
    int currentElemId = -1;

    // 状态定义：NONE-无, STRESS-应力块, HISTORY-历史变量块
    enum BlockType { NONE, STRESS, HISTORY } currentBlock = NONE;

    QTextStream in(&file);
    QStringList params = {
        "sig-xx (X正应力)", "sig-yy (Y正应力)", "sig-zz (Z正应力)",
        "sig-xy (XY剪应力)", "sig-yz (YZ剪应力)", "sig-zx (ZX剪应力)",
        "effsg (Von-Mises 等效应力)", "pressure (静水压力)", "yield (屈服函数/塑性应变)",
        "reaction_degree (反应度/燃烧分数)"
    };

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        // 1. 捕获时间步更新及数据块类型 (处理带空格的关键字)
        if (line.contains("e l e m e n t") && line.contains("at time")) {
            currentTime = line.section("time", -1).remove(")").trimmed().toDouble();

            if (line.contains("s t r e s s")) {
                currentBlock = STRESS;
            }
            else if (line.contains("h i s t r y")) {
                currentBlock = HISTORY;
            }
            else {
                currentBlock = NONE;
            }

            currentElemId = -1; // 换块时重置单元ID
            continue;
        }

        if (currentBlock == NONE) continue;

        // 2. 捕获单元 ID (匹配格式如 "34-       1")
        if (line.contains("-") && line.indexOf("-") > 0 && line.at(line.indexOf("-") - 1).isDigit()) {
            QString idStr = line.split("-", QString::SkipEmptyParts).first().trimmed();
            currentElemId = idStr.toInt();
            continue;
        }

        // 3. 提取数值数据行 (包含 elastic, plastic 或 failed)
        if (currentElemId != -1 && (line.contains("elastic") || line.contains("plastic") || line.contains("failed"))) {

            // 修复 Fortran 科学计数法格式粘连
            QString cleanLine = line;
            cleanLine.replace("E-", "E_").replace("e-", "e_");
            cleanLine.replace("-", " -");
            cleanLine.replace("E_", "E-").replace("e_", "e-");

            QStringList parts = cleanLine.split(QRegExp("\\s+"), QString::SkipEmptyParts);

            // 标准数据行应至少有 10 列 (0:ipt, 1:state, 2-9:data)
            if (parts.size() < 10) continue;

            if (currentBlock == STRESS) {
                double sig_xx = parts[2].toDouble();
                double sig_yy = parts[3].toDouble();
                double sig_zz = parts[4].toDouble();
                double pressure = -(sig_xx + sig_yy + sig_zz) / 3.0;

                // 在应力块记录时间步（通常应力块先出现）
                if (!m_eloutTimeMap[currentElemId].contains(currentTime)) {
                    m_eloutTimeMap[currentElemId].append(currentTime);
                }

                m_eloutData[currentElemId]["sig-xx (X正应力)"].append(sig_xx);
                m_eloutData[currentElemId]["sig-yy (Y正应力)"].append(sig_yy);
                m_eloutData[currentElemId]["sig-zz (Z正应力)"].append(sig_zz);
                m_eloutData[currentElemId]["sig-xy (XY剪应力)"].append(parts[5].toDouble());
                m_eloutData[currentElemId]["sig-yz (YZ剪应力)"].append(parts[6].toDouble());
                m_eloutData[currentElemId]["sig-zx (ZX剪应力)"].append(parts[7].toDouble());
                m_eloutData[currentElemId]["effsg (Von-Mises 等效应力)"].append(parts[8].toDouble());
                m_eloutData[currentElemId]["pressure (静水压力)"].append(pressure);
                m_eloutData[currentElemId]["yield (屈服函数/塑性应变)"].append(parts[9].toDouble());
            }
            else if (currentBlock == HISTORY) {
                // 提取 history 8 (对应 parts 的第 10 列，索引为 9)
                double burn_fraction = parts[9].toDouble();

                // 物理限幅：反应度 [0.0, 1.0]
                if (burn_fraction < 0.0) burn_fraction = 0.0;
                if (burn_fraction > 1.0) burn_fraction = 1.0;

                m_eloutData[currentElemId]["reaction_degree (反应度/燃烧分数)"].append(burn_fraction);
            }
        }
    }
    file.close();

    // 更新 UI 下拉框
    if (m_comboElout && !m_eloutData.isEmpty()) {
        m_comboElout->blockSignals(true);
        m_comboElout->clear();
        m_comboElout->addItems(params);
        m_comboElout->setCurrentIndex(9); // 默认选择反应度
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
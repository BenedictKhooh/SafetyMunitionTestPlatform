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
 * @details 布局逻辑如下：
 * - 1. 顶部工具栏：提供手动选择工作目录并触发全量分析的按钮。
 * - 2. 中央网格区 (QGridLayout)：
 * - [0,0] GLSTAT：展示全局系统能量（动能、内能）。
 * - [0,1] MATSUM：展示不同材料部件的能量分布。
 * - [1,0] NODOUT：展示节点运动历程，定制化实现“节点ID”与“参数”双联动控制。
 * - [1,1] ELOUT ：展示单元反应度及力学历程，定制化实现“单元ID”与“参数”双联动控制。
 */
void PostProcessWidget::setupUI() {
    // 创建主垂直布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // ---------------------------------------------------------
    // 1. 顶部自动化工具栏
    // ---------------------------------------------------------
    QHBoxLayout* toolBar = new QHBoxLayout();
    QPushButton* btnManualLoad = new QPushButton("手动扫描工作目录并自动分析");
    btnManualLoad->setMinimumHeight(35);
    // 设置专业深色调风格
    btnManualLoad->setStyleSheet(
        "QPushButton {"
        "   font-weight: bold; background-color: #34495E; color: white; "
        "   border-radius: 4px; padding: 0 15px;"
        "}"
        "QPushButton:hover { background-color: #2C3E50; }"
    );

    toolBar->addWidget(btnManualLoad);
    toolBar->addStretch();
    mainLayout->addLayout(toolBar);

    // ---------------------------------------------------------
    // 2. 核心网格监控区 (2x2 Grid)
    // ---------------------------------------------------------
    QWidget* gridContainer = new QWidget();
    m_gridLayout = new QGridLayout(gridContainer);
    m_gridLayout->setSpacing(15);

    // 初始化前两个标准图表 (全局能量与部件能量)
    createGridPlot("glstat", "全局系统能量 (GLSTAT)", 0, 0);
    createGridPlot("matsum", "部件能量分布 (MATSUM)", 0, 1);

    // ---------------------------------------------------------
    // 3. NODOUT 节点历程定制布局 (双下拉框控制)
    // ---------------------------------------------------------
    QWidget* nodoutCell = new QWidget();
    QVBoxLayout* nodoutLayout = new QVBoxLayout(nodoutCell);
    nodoutLayout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout* nodoutHeader = new QHBoxLayout();
    QLabel* lblNodout = new QLabel("节点运动历程 (NODOUT)");
    lblNodout->setFont(QFont("Microsoft YaHei", 10, QFont::Bold));
    nodoutHeader->addWidget(lblNodout);
    nodoutHeader->addStretch();

    // 创建节点 ID 选择下拉框
    nodoutHeader->addWidget(new QLabel("节点ID:"));
    m_comboNodoutId = new QComboBox();
    m_comboNodoutId->setMinimumWidth(100);
    m_comboNodoutId->setToolTip("选择特定节点ID或全选");
    nodoutHeader->addWidget(m_comboNodoutId);

    // 创建节点参数选择下拉框
    nodoutHeader->addWidget(new QLabel(" 参数:"));
    m_comboNodoutParam = new QComboBox();
    m_comboNodoutParam->setMinimumWidth(150);
    nodoutHeader->addWidget(m_comboNodoutParam);

    nodoutLayout->addLayout(nodoutHeader);

    QCustomPlot* nodoutPlot = new QCustomPlot();
    nodoutPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
    nodoutPlot->xAxis->setLabel("时间 (Time) [us]");
    nodoutPlot->legend->setVisible(true);
    nodoutPlot->legend->setFont(QFont("Consolas", 8));
    nodoutPlot->legend->setBrush(QBrush(QColor(255, 255, 255, 150)));
    nodoutPlot->xAxis->grid()->setPen(QPen(QColor(200, 200, 200), 1, Qt::DotLine));
    nodoutPlot->yAxis->grid()->setPen(QPen(QColor(200, 200, 200), 1, Qt::DotLine));

    nodoutLayout->addWidget(nodoutPlot, 1);
    m_plotMap["nodout"] = nodoutPlot;
    m_gridLayout->addWidget(nodoutCell, 1, 0);

    // ---------------------------------------------------------
    // 4. ELOUT 单元历程定制布局 (双下拉框控制)
    // ---------------------------------------------------------
    QWidget* eloutCell = new QWidget();
    QVBoxLayout* eloutLayout = new QVBoxLayout(eloutCell);
    eloutLayout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout* eloutHeader = new QHBoxLayout();
    QLabel* lblElout = new QLabel("单元反应度/历程 (ELOUT)");
    lblElout->setFont(QFont("Microsoft YaHei", 10, QFont::Bold));
    eloutHeader->addWidget(lblElout);
    eloutHeader->addStretch();

    // 创建单元 ID 选择下拉框
    eloutHeader->addWidget(new QLabel("单元ID:"));
    m_comboEloutId = new QComboBox();
    m_comboEloutId->setMinimumWidth(100);
    m_comboEloutId->setToolTip("选择特定单元ID或全选");
    eloutHeader->addWidget(m_comboEloutId);

    // 创建单元参数选择下拉框
    eloutHeader->addWidget(new QLabel(" 参数:"));
    m_comboEloutParam = new QComboBox();
    m_comboEloutParam->setMinimumWidth(150);
    eloutHeader->addWidget(m_comboEloutParam);

    eloutLayout->addLayout(eloutHeader);

    QCustomPlot* eloutPlot = new QCustomPlot();
    eloutPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
    eloutPlot->xAxis->setLabel("时间 (Time) [us]");
    eloutPlot->legend->setVisible(true);
    eloutPlot->legend->setFont(QFont("Consolas", 8));
    eloutPlot->legend->setBrush(QBrush(QColor(255, 255, 255, 150)));
    eloutPlot->xAxis->grid()->setPen(QPen(QColor(200, 200, 200), 1, Qt::DotLine));
    eloutPlot->yAxis->grid()->setPen(QPen(QColor(200, 200, 200), 1, Qt::DotLine));

    eloutLayout->addWidget(eloutPlot, 1);
    m_plotMap["elout"] = eloutPlot;
    m_gridLayout->addWidget(eloutCell, 1, 1);

    mainLayout->addWidget(gridContainer);

    // ---------------------------------------------------------
    // 5. 信号槽绑定
    // ---------------------------------------------------------
    // 目录扫描按钮
    connect(btnManualLoad, &QPushButton::clicked, this, &PostProcessWidget::handleManualDirSelect);

    // NODOUT 双联动控制 (ID 改变或参数改变均触发重绘)
    connect(m_comboNodoutId, &QComboBox::currentTextChanged, this, &PostProcessWidget::updateNodoutPlot);
    connect(m_comboNodoutParam, &QComboBox::currentTextChanged, this, &PostProcessWidget::updateNodoutPlot);

    // ELOUT 双联动控制 (ID 改变或参数改变均触发重绘)
    connect(m_comboEloutId, &QComboBox::currentTextChanged, this, &PostProcessWidget::updateEloutPlot);
    connect(m_comboEloutParam, &QComboBox::currentTextChanged, this, &PostProcessWidget::updateEloutPlot);
}

/**
 * @brief 采用 Widget 嵌套布局，将标题、下拉框和图表优雅组合 (仅用于基础图表)
 */
void PostProcessWidget::createGridPlot(const QString& id, const QString& title, int row, int col, QComboBox** comboOut) {
    QWidget* cellWidget = new QWidget();
    QVBoxLayout* cellLayout = new QVBoxLayout(cellWidget);
    cellLayout->setContentsMargins(0, 0, 0, 0);

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
 * @brief 清除仪表盘所有图表、内存缓存及 UI 控件状态
 */
void PostProcessWidget::clearDashboard() {
    // 1. 重置所有图表显示
    for (QCustomPlot* p : m_plotMap.values()) {
        if (p) {
            p->clearGraphs();
            p->replot();
        }
    }

    // 2. 清空底层内存缓存
    m_nodoutData.clear();
    m_nodoutTimeMap.clear();
    m_eloutData.clear();
    m_eloutTimeMap.clear();

    // 3. 重置 NODOUT 相关的双下拉框
    if (m_comboNodoutId) {
        m_comboNodoutId->blockSignals(true);
        m_comboNodoutId->clear();
        m_comboNodoutId->blockSignals(false);
    }
    if (m_comboNodoutParam) {
        m_comboNodoutParam->blockSignals(true);
        m_comboNodoutParam->clear();
        m_comboNodoutParam->blockSignals(false);
    }

    // 4. 重置 ELOUT 相关的双下拉框
    if (m_comboEloutId) {
        m_comboEloutId->blockSignals(true);
        m_comboEloutId->clear();
        m_comboEloutId->blockSignals(false);
    }
    if (m_comboEloutParam) {
        m_comboEloutParam->blockSignals(true);
        m_comboEloutParam->clear();
        m_comboEloutParam->blockSignals(false);
    }

    qDebug() << "[清理完毕] 后处理仪表盘已完成重置。";
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

    m_nodoutData.clear();
    m_nodoutTimeMap.clear();

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

    // ---------------------------------------------------------
    // 解析完毕，UI 联动与下拉框刷新 (NODOUT)
    // ---------------------------------------------------------
    // 1. 刷新节点 ID 下拉框
    if (m_comboNodoutId) {
        m_comboNodoutId->blockSignals(true);
        m_comboNodoutId->clear();
        m_comboNodoutId->addItem("全画 (All)");

        QList<int> ids = m_nodoutData.keys();
        qSort(ids.begin(), ids.end());

        for (int id : ids) {
            m_comboNodoutId->addItem(QString::number(id));
        }
        m_comboNodoutId->setCurrentIndex(0);
        m_comboNodoutId->blockSignals(false);
    }

    // 2. 刷新节点参数下拉框
    if (m_comboNodoutParam) {
        m_comboNodoutParam->blockSignals(true);
        m_comboNodoutParam->clear();
        m_comboNodoutParam->addItems(params);
        m_comboNodoutParam->setCurrentIndex(9); // 默认选合速度
        m_comboNodoutParam->blockSignals(false);

        updateNodoutPlot();
    }
    return true;
}

/**
 * @brief 解析 ELOUT 文件的核心引擎
 * @param path elout 文件的绝对路径
 * @return bool 解析成功返回 true，文件打开失败返回 false
 * * @details 算法特性：
 * 1. 状态机：自动切换 STRESS (应力) 和 HISTORY (历史变量) 解析模式。
 * 2. 鲁棒性：支持 "ID- PartID" 格式的单元识别，并修复 Fortran 负号粘连问题。
 * 3. 反应度提取：精准定位 History Block 中的第 10 列（索引 9）作为炸药反应度。
 * 4. UI 联动：解析完成后自动刷新单元 ID 下拉框，支持“全画(All)”与单选模式。
 */
bool PostProcessWidget::processElout(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "[ELOUT解析失败] 无法打开文件:" << path;
        return false;
    }

    // 内存数据预清理
    m_eloutData.clear();
    m_eloutTimeMap.clear();

    QTextStream in(&file);
    double currentTime = 0.0;
    int currentElemId = -1;

    // 定义解析状态机状态
    enum BlockType { NONE, STRESS, HISTORY } currentBlock = NONE;

    // 参数列表定义 (与下拉框对应)
    QStringList params = {
        "reaction_degree (反应度)",
        "effsg (等效应力)",
        "yield (屈服函数/塑性应变)",
        "sig-xx (X正应力)",
        "sig-yy (Y正应力)",
        "sig-zz (Z正应力)"
    };

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        // ---------------------------------------------------------
        // 阶段 1: 块头识别 (Time Step & Block Type)
        // 适配格式: e l e m e n t  s t r e s s / h i s t r y ...
        // ---------------------------------------------------------
        if (line.contains("e l e m e n t") && line.contains("at time")) {
            // 使用正则表达式提取科学计数法时间
            QRegExp timeRegex("at time\\s+([\\d\\.\\+\\-E]+)");
            if (timeRegex.indexIn(line) != -1) {
                currentTime = timeRegex.cap(1).toDouble();
            }

            // 判断当前进入的是应力块还是历史变量块
            if (line.contains("s t r e s s")) {
                currentBlock = STRESS;
            }
            else if (line.contains("h i s t r y") || line.contains("h i s t o r y")) {
                currentBlock = HISTORY;
            }
            else {
                currentBlock = NONE;
            }

            currentElemId = -1; // 每个时间步/块开始时重置当前单元
            continue;
        }

        if (currentBlock == NONE) continue;

        // ---------------------------------------------------------
        // 阶段 2: 单元 ID 识别
        // 适配格式: "34-      1" 或 "230556-      1"
        // ---------------------------------------------------------
        if (line.contains("-")) {
            QString firstToken = line.split("-").first().trimmed();
            bool ok;
            int id = firstToken.toInt(&ok);
            if (ok) {
                currentElemId = id;
                continue;
            }
        }

        // ---------------------------------------------------------
        // 阶段 3: 数值数据行解析 (包含状态关键字: elastic, plastic, failed)
        // ---------------------------------------------------------
        if (currentElemId != -1 && (line.contains("elastic") || line.contains("plastic") || line.contains("failed"))) {

            // 解决 Fortran 格式下的数字粘连问题 (例如: -1.234-5.678)
            QString cleanLine = line;
            cleanLine.replace("E-", "E_").replace("e-", "e_"); // 保护指数项
            cleanLine.replace("-", " -");                      // 强制在负号前加空格
            cleanLine.replace("E_", "E-").replace("e_", "e-"); // 还原指数项

            // 按空格分割字符串
            QStringList parts = cleanLine.split(QRegExp("\\s+"), QString::SkipEmptyParts);

            // 标准行格式：[0]ipt [1]state [2]data1 ... [9]data8
            if (parts.size() < 10) continue;

            // 根据当前状态分流数据
            if (currentBlock == STRESS) {
                // 在应力块中记录时间戳 (作为基准)
                if (!m_eloutTimeMap[currentElemId].contains(currentTime)) {
                    m_eloutTimeMap[currentElemId].append(currentTime);
                }

                m_eloutData[currentElemId]["sig-xx (X正应力)"].append(parts[2].toDouble());
                m_eloutData[currentElemId]["sig-yy (Y正应力)"].append(parts[3].toDouble());
                m_eloutData[currentElemId]["sig-zz (Z正应力)"].append(parts[4].toDouble());

                // 索引 8 对应 effsg (等效应力)
                m_eloutData[currentElemId]["effsg (等效应力)"].append(parts[8].toDouble());

                // 索引 9 对应 yield function (屈服函数)
                m_eloutData[currentElemId]["yield (屈服函数/塑性应变)"].append(parts[9].toDouble());
            }
            else if (currentBlock == HISTORY) {
                // 核心需求：提取第 8 个历史变量 (对应索引 9)
                double reaction = parts[9].toDouble();

                // 反应度物理限幅 [0, 1]
                if (reaction < 0.0) reaction = 0.0;
                if (reaction > 1.0) reaction = 1.0;

                m_eloutData[currentElemId]["reaction_degree (反应度)"].append(reaction);
            }
        }
    }
    file.close();

    // ---------------------------------------------------------
    // 阶段 4: UI 联动与下拉框刷新 (ELOUT)
    // ---------------------------------------------------------

    // 1. 刷新单元 ID 下拉框
    if (m_comboEloutId) {
        m_comboEloutId->blockSignals(true);
        m_comboEloutId->clear();
        m_comboEloutId->addItem("全画 (All)"); // 添加默认全选选项

        QList<int> ids = m_eloutData.keys();
        qSort(ids.begin(), ids.end()); // ID 升序排列

        for (int id : ids) {
            m_comboEloutId->addItem(QString::number(id));
        }
        m_comboEloutId->setCurrentIndex(0); // 默认选中"全画"
        m_comboEloutId->blockSignals(false);
    }

    // 2. 刷新参数下拉框
    if (m_comboEloutParam) {
        m_comboEloutParam->blockSignals(true);
        m_comboEloutParam->clear();
        m_comboEloutParam->addItems(params);
        m_comboEloutParam->setCurrentIndex(0); // 默认选中反应度
        m_comboEloutParam->blockSignals(false);

        updateEloutPlot();
    }

    qDebug() << "[ELOUT解析完成] 共发现单元数量:" << m_eloutData.size();
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
 * @details 绘图逻辑分支：
 * - 1. 全量模式 (All)：遍历全部有效节点绘制历史曲线。
 * - 2. 单选模式：提取所选指定节点绘制历史曲线。
 */
void PostProcessWidget::updateNodoutPlot() {
    // 基础有效性校验
    if (!m_comboNodoutParam || !m_comboNodoutId || m_nodoutData.isEmpty()) {
        return;
    }

    // 获取当前 UI 选择状态
    QString selectedParam = m_comboNodoutParam->currentText();
    QString selectedIdStr = m_comboNodoutId->currentText();

    // 定位目标图表
    QCustomPlot* p = m_plotMap["nodout"];
    if (!p) return;

    // 清除已有曲线
    p->clearGraphs();

    // 动态调整 Y 轴单位标签
    if (selectedParam.contains("位移")) p->yAxis->setLabel(selectedParam + " [cm]");
    else if (selectedParam.contains("速度")) p->yAxis->setLabel(selectedParam + " [cm/us]");
    else if (selectedParam.contains("加速度")) p->yAxis->setLabel(selectedParam + " [cm/us^2]");
    else p->yAxis->setLabel(selectedParam);

    int colorIdx = 0;

    // 分支 A: 全量绘图模式 (All)
    if (selectedIdStr == "全画 (All)") {
        for (int nid : m_nodoutData.keys()) {
            if (!m_nodoutData[nid].contains(selectedParam)) continue;

            p->addGraph();
            p->graph()->setData(m_nodoutTimeMap[nid], m_nodoutData[nid][selectedParam]);
            p->graph()->setName(QString("Node %1").arg(nid));

            QColor lineColor = m_colorPalette[colorIdx % m_colorPalette.size()];
            p->graph()->setPen(QPen(lineColor, 2));
            colorIdx++;
        }
    }
    // 分支 B: 单节点查看模式
    else {
        int targetId = selectedIdStr.toInt();
        if (m_nodoutData.contains(targetId) && m_nodoutData[targetId].contains(selectedParam)) {
            p->addGraph();
            p->graph()->setData(m_nodoutTimeMap[targetId], m_nodoutData[targetId][selectedParam]);
            p->graph()->setName(QString("Node %1").arg(targetId));

            p->graph()->setPen(QPen(m_colorPalette[0], 2.5));
        }
    }

    // 执行坐标轴自适应并刷新
    p->rescaleAxes();
    p->replot();
}

/**
 * @brief 根据用户在下拉框的选择，动态重绘 ELOUT 曲线
 * @details 绘图逻辑分支：
 * - 1. 全量模式 (All)：遍历全部有效单元绘制历史曲线。
 * - 2. 单选模式：提取所选指定单元绘制历史曲线。
 */
void PostProcessWidget::updateEloutPlot() {
    // 基础有效性校验
    if (!m_comboEloutParam || !m_comboEloutId || m_eloutData.isEmpty()) {
        return;
    }

    // 获取当前 UI 选择状态
    QString selectedParam = m_comboEloutParam->currentText();
    QString selectedIdStr = m_comboEloutId->currentText();

    // 定位目标图表
    QCustomPlot* p = m_plotMap["elout"];
    if (!p) return;

    // 清除已有曲线
    p->clearGraphs();
    p->yAxis->setLabel(selectedParam);

    int colorIdx = 0;

    // 分支 A: 全量绘图模式 (All)
    if (selectedIdStr == "全画 (All)") {
        for (int eid : m_eloutData.keys()) {
            if (!m_eloutData[eid].contains(selectedParam)) continue;

            p->addGraph();
            p->graph()->setData(m_eloutTimeMap[eid], m_eloutData[eid][selectedParam]);
            p->graph()->setName(QString("Elem %1").arg(eid));

            QColor lineColor = m_colorPalette[colorIdx % m_colorPalette.size()];
            p->graph()->setPen(QPen(lineColor, 2));
            colorIdx++;
        }
    }
    // 分支 B: 单单元查看模式
    else {
        int targetId = selectedIdStr.toInt();
        if (m_eloutData.contains(targetId) && m_eloutData[targetId].contains(selectedParam)) {
            p->addGraph();
            p->graph()->setData(m_eloutTimeMap[targetId], m_eloutData[targetId][selectedParam]);
            p->graph()->setName(QString("Elem %1").arg(targetId));

            p->graph()->setPen(QPen(m_colorPalette[0], 2.5));
        }
    }

    // 执行坐标轴自适应并刷新
    p->rescaleAxes();
    p->replot();
}
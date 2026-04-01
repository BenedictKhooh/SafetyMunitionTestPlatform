#include <QMainWindow>
#include "mainwindow.h"
#include "gmsh.h"
#include "glwidget.h"
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QDebug>
#include <QFileInfo>
#include "MeshUtils.h"

#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QMessageBox>

//JSON file processors
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
//

#include <QMenu>
#include <QDialog>
#include <QTextEdit>
#include <set>

#include <fstream>
#include <string>
#include <cmath>
#include <QEventLoop>
#include <regex>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    // ==========================================
    // 引入多工作区
    // ==========================================
    mainModeTab = new QTabWidget(this);
    setCentralWidget(mainModeTab); 

    mainModeTab->setStyleSheet("QTabBar::tab { height: 0px; width: 0px; padding: 0px; margin: 0px; border: none; }");

    // --- Tab 1: 前处理工作区 ---
    glWidget = new GLWidget(this);
    glWidget->setRepository(&m_repository); // ptr to the entities repo
    mainModeTab->addTab(glWidget, "前处理与建模 (Pre-Processing)");

    // --- Tab 2: 求解器工作区 ---
    postProcessWidget = new QWidget();
    setupPostProcessUI(); // 在这里面把后处理控件都加到 postProcessWidget 上
    mainModeTab->addTab(postProcessWidget, "求解与仿真试验方案设计 (Solve & Post)");

    PostProcessWidget* postProcessTab = new PostProcessWidget(this);
    mainModeTab->addTab(postProcessTab, "后处理可视化");


    // ==========================================
    // 2. 原有的初始化逻辑 (保持不变)
    // ==========================================
    startMeshing();
    // 连接绘图完成信号
    connect(glWidget, &GLWidget::drawingComplete, this, &MainWindow::onDrawingComplete);

    // 初始化菜单栏、工具栏、状态栏、停靠窗口
    createMenuBar();
    createToolBars();
    createStatusBar();
    createDockWidgets();
    createCommandLine();
    createSimulationSetupDock();

    setWindowTitle("SafetyMunitionTestPlatform");
    resize(1024, 768);

    // clear()
    clean();

    m_meshManager = new MeshManager(this);

    // 核心：当网格文件准备好后，自动触发 parseMeshFile 进行解析和渲染
    connect(m_meshManager, &MeshManager::meshReady, this, &MainWindow::parseMeshFile);

    // 错误处理：如果 Gmsh 报错，打印到日志
    connect(m_meshManager, &MeshManager::errorOccurred, this, [this](QString msg) {
        logCommand("Error", msg);
        });

    // 初始化默认工作目录为当前程序运行的目录
    m_workingDirectory = QDir::currentPath();


    // ==========================================
    // 3. 新增：后处理进程与工作区智能联动
    // ==========================================

    // 初始化求解器后台进程
    m_solverProcess = new QProcess(this);
    connect(m_solverProcess, &QProcess::readyReadStandardOutput, this, &MainWindow::readSolverOutput);
    connect(m_solverProcess, &QProcess::readyReadStandardError, this, &MainWindow::readSolverOutput);
    connect(m_solverProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &MainWindow::handleSolverFinished);

    // 只要检测到用户切换了 Tab 页面，自动隐藏/显示外围的 Dock 和工具栏
    connect(mainModeTab, &QTabWidget::currentChanged, this, [this](int index) {
        bool isPreProcess = (index == 0); // 只有在第一页时，才显示前处理面板

        // 自动遍历并控制所有停靠窗口 (Docks) 的显示状态
        for (QDockWidget* dock : this->findChildren<QDockWidget*>()) {
            dock->setVisible(isPreProcess);
        }
        });

    // =========================================================
    // 进程管理器初始化与信号绑定
    // =========================================================
    m_batchProcess = new QProcess(this);

    // 配置进程通道：将标准错误输出 (stderr) 合并至标准输出 (stdout)，以便统一读取
    m_batchProcess->setProcessChannelMode(QProcess::MergedChannels);

    // 绑定标准输出就绪信号，实现日志流的实时捕获
    connect(m_batchProcess, &QProcess::readyReadStandardOutput, this, &MainWindow::handleBatchProcessOutput);

    // 绑定进程终止信号，实现任务生命周期的状态监控
    connect(m_batchProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this, &MainWindow::handleBatchProcessFinished);

}

MainWindow::~MainWindow() {}

void MainWindow::createMenuBar() {
    menuBar = new QMenuBar(this);
    setMenuBar(menuBar);

    // 文件菜单
    QMenu *fileMenu = menuBar->addMenu("File");
    QAction *newAction = fileMenu->addAction("New");
    QAction *openAction = fileMenu->addAction("Open");
    QAction *saveAction = fileMenu->addAction("Save");
    QAction *exitAction = fileMenu->addAction("Exit");

    // 编辑菜单
    QMenu *editMenu = menuBar->addMenu("Edit");
    QAction *cutAction = editMenu->addAction("Cut");
    QAction *copyAction = editMenu->addAction("Copy");
    QAction *pasteAction = editMenu->addAction("Paste");

    // 视图菜单
    QMenu *viewMenu = menuBar->addMenu("View");
    QAction *zoomInAction = viewMenu->addAction("Zoom In");
    QAction *zoomOutAction = viewMenu->addAction("Zoom Out");
    QAction *resetViewAction = viewMenu->addAction("Reset View");

    // 连接信号和槽
    connect(exitAction, &QAction::triggered, this, &QApplication::quit);
    connect(zoomInAction, &QAction::triggered, glWidget, [this]() { glWidget->scaleFactor *= 1.1; glWidget->update(); });
    connect(zoomOutAction, &QAction::triggered, glWidget, [this]() { glWidget->scaleFactor /= 1.1; glWidget->update(); });
    connect(resetViewAction, &QAction::triggered, glWidget, [this]() {
        glWidget->rotationX = glWidget->rotationY = glWidget->rotationZ = 0;
        glWidget->scaleFactor = 1.0;
        glWidget->translation = QVector3D(0, 0, 0);
        glWidget->update();
    });

    QAction* setWorkDirAct = new QAction(tr("设置工作目录 (Set Working Directory)..."), this);
    setWorkDirAct->setStatusTip(tr("设置所有导出文件和仿真数据的保存目录"));
    connect(setWorkDirAct, &QAction::triggered, this, &MainWindow::onSetWorkingDirectory);

    fileMenu->addAction(setWorkDirAct);
    fileMenu->addSeparator();
}

void MainWindow::createToolBars() {
    // 1. 创建全新的全局模式切换工具栏
    QToolBar* modeToolBar = addToolBar("工作模式 (Mode)");
    modeToolBar->setMovable(false); // 固定在顶部，不让用户乱拖破坏布局

    // 2. 创建两个切换动作 (Action)
    QAction* preModeAct = new QAction("前处理与建模", this);
    preModeAct->setCheckable(true);
    preModeAct->setChecked(true); // 默认启动时选中前处理

    QAction* postModeAct = new QAction("求解与仿真试验方案设计", this);
    postModeAct->setCheckable(true);

    QAction* vizModeAct = new QAction("后处理可视化分析", this);
    vizModeAct->setCheckable(true);

    // 3. 把它们加入互斥组 (ActionGroup)，保证一次只能按下一个
    QActionGroup* modeGroup = new QActionGroup(this);
    modeGroup->addAction(preModeAct);
    modeGroup->addAction(postModeAct);
    modeGroup->addAction(vizModeAct);
    modeGroup->setExclusive(true);

    // 4. 将动作添加到工具栏
    modeToolBar->addAction(preModeAct);
    modeToolBar->addSeparator();
    modeToolBar->addAction(postModeAct);
    modeToolBar->addAction(vizModeAct);

    // 5. 绑定点击事件，通过点击工具栏按钮，在底层悄悄切换 Tab 页面
    connect(preModeAct, &QAction::triggered, this, [this]() {
        mainModeTab->setCurrentIndex(0);
        });
    connect(postModeAct, &QAction::triggered, this, [this]() {
        mainModeTab->setCurrentIndex(1);
        });
    connect(vizModeAct, &QAction::triggered, this, [this]() {
        mainModeTab->setCurrentIndex(2);
        });

    modeToolBar->setStyleSheet(
        "QToolBar {"
        "   background-color: #F8F9FA;"
        "   border-bottom: 1px solid #D0D0D0;"
        "   padding: 4px;"
        "}"
        "QToolButton {"
        "   font-size: 11pt;"
        "   font-weight: bold;"
        "   padding: 6px 20px;"
        "   margin: 0px 5px;"
        "   border-radius: 4px;"
        "   color: #555555;"
        "   background-color: transparent;"
        "}"
        "QToolButton:checked {"
        "   background-color: #0055A4;"   
        "   color: white;"                
        "}"
        "QToolButton:hover:!checked {"
        "   background-color: #E2E6EA;"
        "}"
    );
}

void MainWindow::createStatusBar() {
    statusBar = new QStatusBar(this);
    setStatusBar(statusBar);
    //QLabel *coordLabel = new QLabel("X: 0, Y: 0, Z: 0", this);
    //statusBar->addPermanentWidget(coordLabel);
    statusBar->showMessage("Ready");
}

void MainWindow::createDockWidgets() {
    // ==========================================
    // 1. 命令历史停靠窗口 (保留在底部不变)
    // ==========================================
    commandDock = new QDockWidget("Command History", this);
    commandDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    commandHistoryEdit = new QTextEdit();  // 初始化 commandHistoryEdit
    commandHistoryEdit->setReadOnly(true);
    commandDock->setWidget(commandHistoryEdit);
    addDockWidget(Qt::BottomDockWidgetArea, commandDock);

    // ==========================================
    // 2. 实体树停靠窗口 (Substances)
    // ==========================================
    substanceDock = new QDockWidget("Substances", this);
    substanceDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    substanceTree = new QTreeWidget();
    substanceTree->setHeaderLabel("substanceTree");
    substanceDock->setWidget(substanceTree);
    substanceDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    substanceTree->setContextMenuPolicy(Qt::CustomContextMenu);

    // 🌟 核心修改 1：原本是 LeftDockWidgetArea，现在移到【右侧】
    addDockWidget(Qt::RightDockWidgetArea, substanceDock);

    // 绑定右键菜单信号
    connect(substanceTree, &QTreeWidget::customContextMenuRequested,
        this, &MainWindow::onSubstanceTreeContextMenu);

    // ==========================================
    // 3. 实体生成器停靠窗口 (Entity Generator)
    // ==========================================
    QDockWidget* generatorDock = new QDockWidget(tr("Entity Generator"), this);
    generatorDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    generatorDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    // 创建标签页控件作为 Dock 的核心组件
    QTabWidget* tabWidget = new QTabWidget(generatorDock);

    // 将三个分类作为不同的 Tab 添加进去
    setupGeneratorTab(tabWidget, tr("破片 (Fragment)"),
        { "Cube", "Sphere", "Hemisphere", "TriangularPrism", "PentagonalPrism", "HexagonalPrism", "Fragment Simulating Projectile"}, m_fragmentUI);

    setupGeneratorTab(tabWidget, tr("壳体 (Shell)"),
        { "CylindricalShell", "OpenCylindricalShell", "Frustum", "HalfCylindricalShell" }, m_shellUI);

    setupGeneratorTab(tabWidget, tr("装药 (Charge)"),
        { "Cylinder", "HalfCylinder", "Cube", "Sphere" }, m_chargeUI);

    // 将 TabWidget 设置为 Dock 的主体，并添加到右侧
    generatorDock->setWidget(tabWidget);
    addDockWidget(Qt::RightDockWidgetArea, generatorDock);

    // 🌟 核心修改 2：把右侧的 Substance Tree 和 Entity Generator 合并成标签页组！
    tabifyDockWidget(substanceDock, generatorDock);
    substanceDock->raise(); // 默认让 Substance Tree 显示在前面
}

void MainWindow::createCommandLine() {
    QDockWidget *commandLineDock = new QDockWidget("Command Line", this);
    commandLineDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    commandLine = new CommandLine();
    connect(commandLine, &CommandLine::commandEntered, this, &MainWindow::onCommandEntered);
    commandLineDock->setWidget(commandLine);
    addDockWidget(Qt::BottomDockWidgetArea, commandLineDock);
}

void MainWindow::onCommandEntered(const QString &command) {
    
    logCommand(command);

    QStringList args = command.split(" ", Qt::SkipEmptyParts);
    if (args.isEmpty()) return;

    QString cmd = args[0].toLower();
    
    if (cmd == "cube") {
     
        if (args.size() < 9) {
            logCommand(command, "用法: box [name] [lx] [ly] [lz] [ms] [cx] [cy] [cz]");
            return;
        }

        QString name = args[1];
        double lx = args[2].toDouble();
        double ly = args[3].toDouble();
        double lz = args[4].toDouble();
        double ms = args[5].toDouble();
        double cx = args[6].toDouble();
        double cy = args[7].toDouble();
        double cz = args[8].toDouble();

        // 1. 实例化立方体生成器
        CubeGenerator cubeGen;

        // 2. 设置参数（长度、宽度、高度、网格大小、中心点坐标）
        cubeGen.setParameters(lx, ly, lz, ms, cx, cy, cz);

        // 3. 调用管理器进行构建与加载
        // MeshManager 会获取 boxGen 的 .geo 脚本并交给 Gmsh 处理
        m_meshManager->buildAndLoad(cubeGen, name);

        logCommand(command, QString("Box (%1x%2x%3) generation task sent to manager...").arg(lx).arg(ly).arg(lz));
     }
    
    else if (cmd == "zoom") {
        if (args.size() > 1 && args[1] == "in") {
            glWidget->m_zoom *= 1.5f;
            logCommand(command, "Zoomed in.");
        } else if (args.size() > 1 && args[1] == "out") {
            glWidget->m_zoom /= 1.5f;
            logCommand(command, "Zoomed out.");
        } else {
            logCommand(command, "Error: Invalid zoom argument. Use 'zoom in' or 'zoom out'.");
        }
    } else if (cmd == "reset") {
        //glWidget->resetView();
		startMeshing();
        logCommand(command, "View reset to default.");
    } else if (cmd == "clear") {

        // 1. 清空数据仓库 (核心：源头清理)
            m_repository.clearAll();

        // 2. 清理 UI 界面上的对象树
        substanceTree->clear();

        // 3. 通知 GLWidget 重新绘图
        // 由于 GLWidget 持有仓库指针，当它 update 时发现仓库为空，自然就什么都不画了
        glWidget->update();
    } 
    else if (cmd == "help") {
        showHelp();
    } 
    else if (cmd == "cylinder") {
    
		//generateCylindricalMesh(3.8, 10.0, 0.1);
        if (args.size() < 8) {
            logCommand(command, "Argument: cylinder [name] [radius] [ms] [height] [x] [y] [z]");
            return;
        }

        QString name = args[1];
        double r = args[2].toDouble();
        double ms = args[3].toDouble();
		double height = args[4].toDouble();
        double cx = args[5].toDouble();
        double cy = args[6].toDouble();
        double cz = args[7].toDouble();

        // 实例化并设置参数
        CylinderGenerator cylinderGen;
        cylinderGen.setParameters(r, ms, height, cx, cy, cz);

        // 调用管理器（MeshManager 会处理 buildAndLoad 逻辑）
        m_meshManager->buildAndLoad(cylinderGen, name);

        logCommand(command, "Sphere generation task sent to manager...");
    
        //glWidget->update();
    }

    else if (cmd == "half_cylinder") {
        // 预期命令格式: half_cylinder [名称] [半径] [网格尺寸] [高度] [x] [y] [z]
        if (args.size() < 8) {
            logCommand(command, "Argument: half_cylinder [name] [radius] [ms] [height] [x] [y] [z]");
            return;
        }

        // 1. 解析参数
        QString name = args[1];
        double r = args[2].toDouble();
        double ms = args[3].toDouble();
        double height = args[4].toDouble();
        double cx = args[5].toDouble();
        double cy = args[6].toDouble();
        double cz = args[7].toDouble();

        // 2. 实例化生成器并设置参数
        // 确保你已经在 mainwindow.h 中包含了 #include "src/core/generation/HalfCylinderGenerator.h"
        HalfCylinderGenerator halfCylinderGen;
        halfCylinderGen.setParameters(r, height, ms, cx, cy, cz);

        // 3. 调用管理器执行生成、保存和解析流程
        // MeshManager 会调用 generator.generateGeoScript() 并通过 Gmsh 产生 .msh 文件
        m_meshManager->buildAndLoad(halfCylinderGen, name);

        logCommand(command, QString("Half-Cylinder '%1' generation task sent to manager...").arg(name));
    }

    else if (cmd == "cylindrical_shell") {
        
        // args[0] 是 "cylindrical_shell"，所以 args.size() 至少需要 10
        if (args.size() < 10) {
            logCommand(command, "用法: cylindrical_shell [name] [radius] [height] [lid] [wall] [ms] [x] [y] [z]");
            return;
        }

        QString name = args[1];
        double r = args[2].toDouble();      // 内径
        double h = args[3].toDouble();      // 总高度
        double lid = args[4].toDouble();    // 端盖厚度
        double wall = args[5].toDouble();   // 侧壁厚度
        double ms = args[6].toDouble();     // 网格大小

        // 中心点坐标 (目前 Generator 内部逻辑以 0,0,0 为底面中心，
        double cx = args[7].toDouble();
        double cy = args[8].toDouble();
        double cz = args[9].toDouble();

        // 1. 实例化圆柱壳体生成器
        CylindricalShellGenerator shellGen;

        // 2. 设置参数
        // 注意：这里调用的参数顺序需对应类中 setParameters 的定义
        shellGen.setParameters(r, h, lid, wall, ms, cx, cy, cz);

        // 3. 调用管理器进行构建与加载
        m_meshManager->buildAndLoad(shellGen, name);

        logCommand(command, QString("Cylindrical Shell (%1) generation task sent to manager...").arg(name));
    }

    else if (cmd == "open_cylindrical_shell") {

        // args[0] 是 "open_cylindrical_shell"
        // 参数顺序: name, radius, wall, h_base, h_wall, ms, x, y, z
        if (args.size() < 10) {
            logCommand(command, "用法: open_cylindrical_shell [name] [radius] [wall] [h_base] [h_wall] [ms] [x] [y] [z]");
            return;
        }

        QString name = args[1];
        double r = args[2].toDouble();      // 内径
        double wall = args[3].toDouble();   // 侧壁厚度
        double h_base = args[4].toDouble(); // 底部厚度
        double h_wall = args[5].toDouble(); // 侧壁高度
        double ms = args[6].toDouble();     // 网格大小

        // 中心点坐标预留（如需平移，需在Generator脚本中添加Translate逻辑）
        double cx = args[7].toDouble();
        double cy = args[8].toDouble();
        double cz = args[9].toDouble();

        // 1. 实例化开口圆柱壳体生成器 (有底无盖) [cite: 86, 107]
        OpenCylindricalShellGenerator openShellGen;

        // 2. 设置参数 
        // 对应封装类中的: setParameters(radius, wall, h_base, h_wall, meshSize) 
        openShellGen.setParameters(r, wall, h_base, h_wall, ms, cx, cy, cz);

        // 3. 调用管理器进行构建与加载
        m_meshManager->buildAndLoad(openShellGen, name);

        logCommand(command, QString("Open Cylindrical Shell (有底无盖: %1) generation task sent to manager...").arg(name));
        }

    else if (cmd == "sphere") {
    
        if (args.size() < 7) {
            logCommand(command, "用法: sphere [name] [radius] [ms] [x] [y] [z]");
            return;
        }

        QString name = args[1];
        double r = args[2].toDouble();
        double ms = args[3].toDouble();
        double cx = args[4].toDouble();
        double cy = args[5].toDouble();
        double cz = args[6].toDouble();

        // 实例化并设置参数
        SphereGenerator sphereGen;
        sphereGen.setParameters(r, ms, cx, cy, cz);

        // 调用管理器（MeshManager 会处理 buildAndLoad 逻辑）
        m_meshManager->buildAndLoad(sphereGen, name);

        logCommand(command, "Sphere generation task sent to manager...");
    }

    else if (cmd == "hemisphere") {
        // 参数检查: hemisphere [name] [radius] [ms] [cx] [cy] [cz]
        // args[0] 是 "hemisphere"，所以 args.size() 至少需要 7
        if (args.size() < 7) {
            logCommand(command, "用法: hemisphere [name] [radius] [ms] [cx] [cy] [cz]");
            return;
        }

        QString name = args[1];
        double r = args[2].toDouble();  // 半径
        double ms = args[3].toDouble();  // 网格大小
        double cx = args[4].toDouble();  // 中心 X (底面圆心)
        double cy = args[5].toDouble();  // 中心 Y
        double cz = args[6].toDouble();  // 中心 Z

        // 1. 实例化半球形生成器 (使用 O-Grid 结构化算法)
        HemisphereGenerator hemiGen;

        // 2. 设置参数
        hemiGen.setParameters(r, ms, cx, cy, cz);

        // 3. 调用管理器进行构建与加载
        // MeshManager 会调用 hemiGen.generateGeoScript() 并驱动 Gmsh 渲染
        m_meshManager->buildAndLoad(hemiGen, name);

        logCommand(command, QString("Hemisphere (R=%1, MS=%2) task sent to manager...").arg(r).arg(ms));

        // 如果需要立即刷新界面
        // glWidget->update();
        }

    else if (cmd == "frustum") {
        // frustum [name] [rBase] [rTop] [height] [ms] [x] [y] [z]
        if (args.size() < 9) {
            logCommand(command, "用法: frustum [name] [rBase] [rTop] [height] [ms] [x] [y] [z]");
            return;
        }

        QString name = args[1];
        double rb = args[2].toDouble();
        double rt = args[3].toDouble();
        double h = args[4].toDouble();
        double ms = args[5].toDouble();
        double cx = args[6].toDouble();
        double cy = args[7].toDouble();
        double cz = args[8].toDouble();

        FrustumGenerator frustumGen;
        frustumGen.setParameters(rb, rt, h, ms, cx, cy, cz);

        m_meshManager->buildAndLoad(frustumGen, name);
        logCommand(command, QString("Frustum (%1) task sent to manager...").arg(name));
        }

    else if (cmd == "save" || cmd == "export") {
        if (args.size() < 2) {
            logCommand(command, "用法: save [filename.k]");
            return;
        }
        QString fileName = args[1];
        if (!fileName.endsWith(".k")) fileName += ".k";

        exportToKFile(fileName);
        }

      else {
        logCommand("Error: Unknown command. Type 'help' for a list of commands.");
    }
    glWidget->update();
}

void MainWindow::onDrawingComplete() {
    statusBar->showMessage("Drawing completed.");
}

void MainWindow::logCommand(const QString &command, const QString &response) {
    commandHistoryEdit->append("> " + command);
    if (!response.isEmpty()) {
        commandHistoryEdit->append(" " + response);
    }
    commandHistoryEdit->ensureCursorVisible();
    statusBar->showMessage("Executed: " + command);
}


void MainWindow::showHelp() {
    if (commandLine) {  // 检查是否为空指针
        QString helpText = commandLine->getHelpText();
        commandHistoryEdit->append(helpText);
        logCommand("help", helpText);
    }
}

void MainWindow::DrawLine(QStringList args) {

    if (args.size() == 8) {
        
        QString line_name = args[1];
        float x_start = args[2].toFloat();
        float y_start = args[3].toFloat();
        float z_start = args[4].toFloat();
        float x_end = args[5].toFloat();
        float y_end = args[6].toFloat();
        float z_end = args[7].toFloat();

        Vector3 start = { x_start, y_start, z_start };
        Vector3 end = { x_end, y_end, z_end };

        drawables_lines.push_back(GeoLine(start, end));
        glWidget->setDrawableLines(drawables_lines);

        QTreeWidgetItem* instance_generate = new QTreeWidgetItem(substanceTree);
        instance_generate->setText(0, line_name);

        glWidget->update();
    }
    else {
        logCommand(QString("Error: Missing argument for 'line' command."));
    }
}

void MainWindow::clean() {

}

void MainWindow::startMeshing() {

    QString geoContent = "Point(1) = {0, 0, 0, 0.1}; Point(2) = {1, 0, 0, 0.1}; Line(1) = {1, 2};";

    QFile file("input.geo");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(geoContent.toUtf8());
        file.close();
    }

    // 2. 像在命令行里输入命令一样启动 gmsh.exe
    QProcess gmsh;
    QStringList args;
    args << "input.geo" << "-1" << "-o" << "output.msh"; // -1 代表生成1D网格，-o 输出文件名
    gmsh.start("gmsh.exe", args);
    if ( !gmsh.waitForFinished() ) {
		qDebug() << "Gmsh 进程启动失败或超时！";
    }

    //qDebug() << "网格划分完成！文件已保存在 output.msh";
}

void MainWindow::parseMeshFile(QString fileName) {

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    QString line;
    std::vector<MeshPoint> nodes; nodes.push_back(MeshPoint());//占位
	std::vector<Hexahedron> hexes;

    QString entityName = QFileInfo(fileName).baseName();
    std::map<int, int> gmshToLocalIdx;

    while (!in.atEnd()) {
        line = in.readLine().trimmed();

        // 找节点数据块
        if (line == "$Nodes") {
            int numNodes = in.readLine().trimmed().toInt();
            for (int i = 0; i < numNodes; ++i) {
                QStringList data = in.readLine().split(" ", Qt::SkipEmptyParts);
                if (data.size() >= 4) {
                    MeshPoint node;
                    int gmshId = data[0].toInt(); // 🌟 提取出 Gmsh 赋予的原始 ID

                    node.pos.setX( data[1].toDouble() );
                    node.pos.setY(data[2].toDouble());
                    node.pos.setZ(data[3].toDouble());
					nodes.push_back(node);

                    gmshToLocalIdx[gmshId] = nodes.size() - 1;
                }
            }
        }

        if (line == "$Elements") {
        
            int numElements = in.readLine().trimmed().toInt();
            for (int i = 0; i < numElements; i++) {
                QStringList data = in.readLine().split(" ", QString::SkipEmptyParts);
                if (data.size() < 3) continue;

                int elementType = data[1].toInt();

                if (elementType == 5) {
                
                    Hexahedron hex;
					//hex.id = data[0].toInt();
					int numTags = data[2].toInt();
                    for (int j = 0; j < 8; j++) {
                    
                        int gmshNodeId = data[3 + numTags + j].toInt();
                        // 核心修复：通过映射表，将外部 Gmsh ID 转换成你内部的真实数组下标
                        hex[j] = gmshToLocalIdx[gmshNodeId];
                       // hex[j] = data[3 +numTags+ j].toInt();
                    }
					hexes.push_back(hex);
                }
            }
        }
    }

    std::vector<MeshPoint> points;
    for (auto& n : nodes)
        points.push_back(n);

    /*DrawCommand cmd;
    cmd.type = DrawCommand::Points;
    cmd.pointsCmd.points = points;
    cmd.pointsCmd.size = 2.5f;*/

    //glWidget->clearDrawCommands();
    /*glWidget->submitDrawCommand(cmd);*/

    std::vector<Vector3> wireLines = buildHexWireframe(nodes, hexes);
   // auto wireLines = buildHexWireframe( nodes, hexes );

    MeshEntity newEntity;
    newEntity.name = entityName;

    // ==========================================
    // 🌟 核心拦截：如果仓库中已有这个实体（说明是刚才按钮预注册的），完美继承其所有物理属性！
    // ==========================================
    MeshEntity* existingEntity = m_repository.getMutableEntity(entityName);
    if (existingEntity) {
        newEntity.type = existingEntity->type;
        newEntity.category = existingEntity->category; // 继承我们刚填进去的“装药/破片”！
        newEntity.geoParams = existingEntity->geoParams;
    }
    else {
        newEntity.type = "Empty";
        newEntity.category = "未分类";
    }
    // ==========================================

    newEntity.nodes = points;
    newEntity.hexes = hexes;
    newEntity.wireLines = wireLines;

    m_repository.addEntity(entityName, newEntity);

    redrawAllEntities(); // 这句话内部会自动刷新 Substance Tree！

    file.close();
}

void MainWindow::redrawAllEntities() {
    // 1. 清空 GLWidget 里的旧命令
    glWidget->clearDrawCommands();

    //// 2. 遍历仓库里的所有实体
    //const auto& allEntities = m_repository.getAllEntities();
    //for (const auto& pair : allEntities) {
    //    const MeshEntity& entity = pair.second;

    //    // 提交点云渲染命令 (如果你需要)
    //    DrawCommand ptCmd;
    //    ptCmd.type = DrawCommand::Points;
    //    ptCmd.pointsCmd.points = entity.nodes;
    //    ptCmd.pointsCmd.size = 2.5f;
    //    glWidget->submitDrawCommand(ptCmd);

    //    // 提交线框渲染命令
    //    DrawCommand lineCmd;
    //    lineCmd.type = DrawCommand::Lines;
    //    lineCmd.linesCmd.points = entity.wireLines;
    //    lineCmd.linesCmd.width = 2.0f;
    //    glWidget->submitDrawCommand(lineCmd);
    //}

    // 3. 触发显卡刷新
    glWidget->update();
    // 4. Update Substance Tree
    updateSubstanceTree();
}

void MainWindow::clearAllEntities() {
    m_repository.clearAll();      // 清空账本
    glWidget->clearDrawCommands(); // 清空显卡指令
    glWidget->update();           // 刷新画面
    qDebug() << "Cleared all entities";
}

void MainWindow::updateSubstanceTree() {
    if (!substanceTree) return;

    substanceTree->clear();
    // 确保有两列，一列显示名称，一列显示信息
    substanceTree->setHeaderLabels(QStringList() << "Entity / Boundary" << "Info");

    const auto& allEntities = m_repository.getAllEntities();

    // 遍历仓库中的所有实体
    for (auto it = allEntities.begin(); it != allEntities.end(); ++it) {
        QString entityName = it->first;
        const MeshEntity& entity = it->second;

        // 1. 创建实体项 (父节点)，挂载在 substanceTree 上
        QTreeWidgetItem* entityItem = new QTreeWidgetItem(substanceTree);
        entityItem->setText(0, entityName);
        QString catStr = entity.category.isEmpty() ? "未分类" : entity.category;
        entityItem->setText(1, QString("[%1] Nodes: %2").arg(catStr).arg(entity.nodes.size()));

        // --- [新增边界子节点逻辑开始] ---
        // 2. 遍历该实体所拥有的边界，作为小项挂载到 entityItem 下面
        for (const auto& boundary : entity.boundaries) {
            // 关键：传入 entityItem 作为父级，这样它就会变成可展开的子项
            QTreeWidgetItem* boundaryItem = new QTreeWidgetItem(entityItem);

            // 为了视觉上区分，可以加个前缀或图标
            boundaryItem->setText(0, QString("[Boundary] %1").arg(QString::fromStdString(boundary.name)));
            boundaryItem->setText(1, QString("Size: %1").arg(boundary.nodeIndices.size()));

            boundaryItem->setForeground(0, QBrush(QColor(80, 120, 200)));
        }

        if (!entity.boundaries.empty()) {
            entityItem->setExpanded(true);
        }
    }

    // 每次新建实体，更新一次下拉菜单
    m_simSetupUI.entitySelector->clear();
    for (const auto& pair : m_repository.getAllEntities()) {
        m_simSetupUI.entitySelector->addItem(pair.first); // pair.first 是实体名字，如 Cube_1
    }

    if (m_simSetupUI.entitySelector != nullptr) {
        updateAllEntitySelectors();
    }
}

void MainWindow::onSubstanceTreeContextMenu(const QPoint& pos) {
    QTreeWidgetItem* item = substanceTree->itemAt(pos);
    // 假设只有顶层实体可以操作 (没有父节点的 Item)
    if (!item || item->parent() != nullptr) return;

    QString entityName = item->text(0);
    QMenu menu(this);

    QAction* viewAct = menu.addAction("查看关键字卡片 (View Keyword Card)");
    connect(viewAct, &QAction::triggered, this, [this, entityName]() { handleViewEntityKeyword(entityName); });

    menu.addSeparator();
    // 1. 删除
    QAction* delAct = menu.addAction("删除实体 (Delete)");
    connect(delAct, &QAction::triggered, this, [this, entityName]() { handleDeleteEntity(entityName); });

    menu.addSeparator(); // 加条分割线

    QMenu* symMenu = menu.addMenu("添加对称边界 (Symmetry)");

    QAction* symXAct = symMenu->addAction("X 平面对称 (Fix X)");
    connect(symXAct, &QAction::triggered, this, [this, entityName]() { handleApplySymmetryBoundary(entityName, 'X'); });

    QAction* symYAct = symMenu->addAction("Y 平面对称 (Fix Y)");
    connect(symYAct, &QAction::triggered, this, [this, entityName]() { handleApplySymmetryBoundary(entityName, 'Y'); });

    QAction* symZAct = symMenu->addAction("Z 平面对称 (Fix Z)");
    connect(symZAct, &QAction::triggered, this, [this, entityName]() { handleApplySymmetryBoundary(entityName, 'Z'); });

    menu.addSeparator(); // 加条分割线

    // 2. 平移
    QAction* transAct = menu.addAction("平移 (Translate)");
    connect(transAct, &QAction::triggered, this, [this, entityName]() { handleTranslateEntity(entityName); });

    // 3. 缩放
    QAction* scaleAct = menu.addAction("缩放 (Scale)");
    connect(scaleAct, &QAction::triggered, this, [this, entityName]() { handleScaleEntity(entityName); });

    // 4. 旋转
    QAction* rotateAct = menu.addAction("旋转 (Rotate)");
    connect(rotateAct, &QAction::triggered, this, [this, entityName]() { handleRotateEntity(entityName); });

    menu.exec(substanceTree->viewport()->mapToGlobal(pos));
}

void MainWindow::handleDeleteEntity(QString name) {
    // 1. 从实体仓库(Repository)中移除数据
    if (m_repository.deleteEntity(name)) {

        m_symmetryRules.erase(
            std::remove_if(m_symmetryRules.begin(), m_symmetryRules.end(),
                [&](const SymmetryRule& rule) { return rule.entityName == name; }),
            m_symmetryRules.end()
        );

        if (m_entityParts.contains(name)) {
            // 获取这个 Part 卡片的智能指针
            auto partPtr = m_entityParts[name];

            // 从 LS-DYNA 总牌组中移出这张卡片
            m_deck.removeCard(partPtr.get());

            // 从实体名称映射字典中抹除
            m_entityParts.remove(name);
        }

        // 2. 刷新 3D 画面 (清空指令 -> 重新遍历剩余实体提交给 GLWidget)
        redrawAllEntities();

        // 3. 刷新左侧列表 (清空树节点 -> 重新遍历剩余实体插入 Tree)
        updateSubstanceTree();

        qDebug() << "Entity deleted and UI refreshed: " << name;
    }
}

void MainWindow::exportToKFile(const QString& fileName) {
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        logCommand("Export", "Failed to open file: " + fileName);
        return;
    }

    QTextStream out(&file);
    out << "*KEYWORD\n";

    const auto& allEntities = m_repository.getAllEntities(); // 获取仓库中所有实体

    int globalNodeId = 1;  // 全局节点计数器
    int globalElemId = 1;  // 全局单元计数器
    std::vector<int> globalSensorIds; // 收集所有传感器的全局 ID
    std::set<int> globalSensorElemIds;

    for (auto it = allEntities.begin(); it != allEntities.end(); ++it) {
        const MeshEntity& entity = it->second;
        QString entityName = it->first; // 获取实体名字

        int realPartId = getOrCreatePart(entityName)->pid;

        // 映射表：局部向量索引 -> 全局文件节点 ID
        std::unordered_map<int, int> localToGlobal;

        out << "$ #################################################\n";
        out << "$ Entity: " << entity.name << "\n";
        out << "$ #################################################\n";

        // 1. 导出节点 (从索引 1 开始，跳过占位的 nodes[0])
        out << "*NODE\n";
        for (size_t i = 1; i < entity.nodes.size(); ++i) {
            localToGlobal[static_cast<int>(i)] = globalNodeId;

            // 格式：ID, X, Y, Z
            out << globalNodeId << ", "
                << entity.nodes[i].pos.x() << ", "
                << entity.nodes[i].pos.y() << ", "
                << entity.nodes[i].pos.z() << "\n";

            if (m_sensorNodes.contains(entityName) && m_sensorNodes[entityName].contains(i)) {
                globalSensorIds.push_back(globalNodeId);
            }

            globalNodeId++;
        }

        // 2. 导出六面体单元
        out << "*ELEMENT_SOLID\n";
        for (const auto& hex : entity.hexes) {
            // 解决报错的关键：直接使用 hex[j] 访问 std::array 元素
            out << QString("%1").arg(globalElemId, 8)
                << QString("%1").arg(realPartId, 8);            
            bool isSensorElement = false;

            for (int j = 0; j < 8; ++j) {
                int localIdx = hex[j]; // 获取存储在 array 中的局部节点索引

                out << QString("%1").arg(localToGlobal[localIdx], 8);

                if (m_sensorNodes.contains(entityName) && m_sensorNodes[entityName].contains(localIdx)) {
                    isSensorElement = true;
                }
            }
            out << "\n";

            if (isSensorElement) {
                globalSensorElemIds.insert(globalElemId);
            }
            globalElemId++;
        }

    }

    if (!globalSensorIds.empty()) {
        // 1. 指定观测点全局 ID
        out << "*DATABASE_HISTORY_NODE\n";
        out << "$#    id1       id2       id3       id4       id5       id6       id7       id8\n";

        // LS-DYNA 要求每行最多 8 个 ID，宽度为 10，自动换行
        for (size_t i = 0; i < globalSensorIds.size(); ++i) {
            out << QString("%1").arg(globalSensorIds[i], 10, 10, QChar(' '));
            if ((i + 1) % 8 == 0) out << "\n";
        }
        if (globalSensorIds.size() % 8 != 0) out << "\n";

        // 2. 指定节点数据的输出时间步长 (这里默认0.005，越小数据点越密，曲线越平滑)
        out << "*DATABASE_NODOUT\n";
        out << "$#      dt      lcdt      beam     npltc    psetid\n";
        out << "     0.005         0         0         0         0\n";

        out << "*DATABASE_HISTORY_SOLID\n";
        out << "$#    id1       id2       id3       id4       id5       id6       id7       id8\n";
        int count = 0;
        for (int elemId : globalSensorElemIds) {
            out << QString("%1").arg(elemId, 10, 10, QChar(' '));
            count++;
            if (count % 8 == 0) out << "\n";
        }
        if (count % 8 != 0) out << "\n";

        out << "*DATABASE_ELOUT\n"; // Element Output 单元输出卡片
        out << "     0.005         0         0         0         0\n";
    }

    // ==============================================================
    //
    // ==============================================================
    if (!m_symmetryRules.empty()) {
        static int currentSetId = 2000;

        for (const auto& rule : m_symmetryRules) {
             
            const MeshEntity* entity = m_repository.getEntity(rule.entityName);
            if (!entity) continue;

            std::set<int> localNodes = getSymmetryPlaneNodes(*entity, rule.axis);

            // 计算全局节点偏移量 (与上面导出 *NODE 时的逻辑绝对同步)
            int nodeOffset = 0;
            for (auto it = allEntities.begin(); it != allEntities.end(); ++it) {
                if (it->first == rule.entityName) break;
                int realNodeCount = it->second.nodes.empty() ? 0 : (it->second.nodes.size() - 1);
                nodeOffset += realNodeCount;
            }

            std::vector<int> globalNodes;
            for (int localIdx : localNodes) {
                if (localIdx == 0) continue; // 避开 nodes[0] 占位符
                globalNodes.push_back(nodeOffset + localIdx);
            }

            if (globalNodes.empty()) continue;

            // 实例化对象，使用对象自带的 to_string 方法直接输出到文件
            int setId = currentSetId++;
            std::string title = QString("%1_Symmetry_%2").arg(rule.entityName).arg(rule.axis).toStdString();

            SetNodeListCard setCard(setId, title);
            for (int nid : globalNodes) setCard.addNode(nid);

            SpcSetCard spcCard;
            int tx = (rule.axis == 'X') ? 1 : 0;
            int ty = (rule.axis == 'Y') ? 1 : 0;
            int tz = (rule.axis == 'Z') ? 1 : 0;
            spcCard.addConstraint(setId, tx, ty, tz, 0, 0, 0);

            out << QString::fromStdString(setCard.to_string());
            out << QString::fromStdString(spcCard.to_string());
        }
    }
    // ==============================================================

    out << "*END\n";
    file.close();
    logCommand("Export", "Saved entities to " + fileName);
}


// 辅助函数：创建独立且不可关闭的 Dock 窗口
void MainWindow::setupGeneratorTab(QTabWidget* tabWidget, const QString& title, const QStringList& shapes, GeneratorUI& ui) {
    // 1. 创建一个普通的 QWidget 作为单独的标签页
    QWidget* tab = new QWidget(tabWidget);
    QVBoxLayout* mainLayout = new QVBoxLayout(tab);

    // 2. 形状选择下拉框
    mainLayout->addWidget(new QLabel("Select Shape:"));
    ui.shapeComboBox = new QComboBox(tab);
    ui.shapeComboBox->addItems(shapes);
    mainLayout->addWidget(ui.shapeComboBox);

    // 3. 动态参数区域
    mainLayout->addSpacing(10);
    mainLayout->addWidget(new QLabel("Parameters:"));
    ui.paramContainer = new QWidget(tab);
    ui.paramLayout = new QVBoxLayout(ui.paramContainer);
    ui.paramLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(ui.paramContainer);

    mainLayout->addStretch(1);

    // 4. 生成按钮
    QPushButton* generateBtn = new QPushButton("Generate " + title, tab);
    mainLayout->addWidget(generateBtn);

    // 5. 将配置好的页面添加到 TabWidget 中
    tabWidget->addTab(tab, title);

    // 6. 信号槽连接
    connect(ui.shapeComboBox, &QComboBox::currentTextChanged, this, [this, &ui](const QString& text) {
        handleShapeTypeChanged(ui, text);
        });
    connect(generateBtn, &QPushButton::clicked, this, [this, &ui]() {
        handleGenerateButtonClicked(ui);
        });

    // 初始化显示
    handleShapeTypeChanged(ui, ui.shapeComboBox->currentText());
}

// 辅助函数：向特定的 UI 结构体中添加输入框
void MainWindow::addNumParamToUI(GeneratorUI& ui, const QString& labelText, const QString& key, double defaultValue, const QString& unit) {
    QHBoxLayout* row = new QHBoxLayout();
    row->addWidget(new QLabel(labelText));
    QDoubleSpinBox* sb = new QDoubleSpinBox(this);
    sb->setRange(-9999.0, 9999.0);
    sb->setValue(defaultValue);

    // 🌟 新增：如果传入了单位，就把它作为后缀显示在数字后面
    if (!unit.isEmpty()) {
        sb->setSuffix(unit);
    }

    row->addWidget(sb);
    ui.paramLayout->addLayout(row);
    ui.paramInputs[key] = sb;
}

// 槽函数重构：处理任意一个窗口的下拉菜单变化
void MainWindow::handleShapeTypeChanged(GeneratorUI& ui, const QString& text) {
    if (!ui.paramLayout || !ui.paramContainer) return;

    // 清理该窗口旧的参数输入框
    QList<QWidget*> children = ui.paramContainer->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget* child : children) {
        child->hide();
        child->deleteLater();
    }
    ui.paramInputs.clear();

    // 重新添加实体名称输入框
    QHBoxLayout* nameRow = new QHBoxLayout();
    nameRow->addWidget(new QLabel("Entity Name:"));
    ui.nameInput = new QLineEdit(this);
    ui.nameInput->setText(text.toLower() + "_1");
    nameRow->addWidget(ui.nameInput);
    ui.paramLayout->addLayout(nameRow);

    // 根据选中的形状生成参数
    if (text == "Cube") {
        addNumParamToUI(ui, "Length (LX):", "lx", 10.0, " cm");
        addNumParamToUI(ui, "Width (LY):", "ly", 10.0, " cm");
        addNumParamToUI(ui, "Height (LZ):", "lz", 10.0, " cm");
        addNumParamToUI(ui, "Mesh Size:", "ms", 1.0, " cm");
        addNumParamToUI(ui, "Center X:", "cx", 0.0, " cm");
        addNumParamToUI(ui, "Center Y:", "cy", 0.0, " cm");
        addNumParamToUI(ui, "Center Z:", "cz", 0.0, " cm");
    }
    else if (text == "Cylinder") {
        addNumParamToUI(ui, "Radius (R):", "r", 4.0, " cm");
        addNumParamToUI(ui, "Height (H):", "h", 8.0, " cm");
        addNumParamToUI(ui, "Mesh Size:", "ms", 0.1, " cm");
        addNumParamToUI(ui, "Center X:", "cx", 0.0, " cm");
        addNumParamToUI(ui, "Center Y:", "cy", 0.0, " cm");
        addNumParamToUI(ui, "Center Z:", "cz", 0.6, " cm");
    }
    else if (text == "CylindricalShell") {
        addNumParamToUI(ui, "Inner Radius:", "r", 4.0, " cm");
        addNumParamToUI(ui, "Total Height:", "h", 9.2, " cm");
        addNumParamToUI(ui, "Lid Thick:", "lid", 0.6, " cm");
        addNumParamToUI(ui, "Wall Thick:", "wall", 0.6, " cm");
        addNumParamToUI(ui, "Mesh Size:", "ms", 0.1, " cm");
        addNumParamToUI(ui, "Center X:", "cx", 0.0, " cm");
        addNumParamToUI(ui, "Center Y:", "cy", 0.0, " cm");
        addNumParamToUI(ui, "Center Z:", "cz", 0.0, " cm");
    }
    else if (text == "OpenCylindricalShell") {
        addNumParamToUI(ui, "Radius:", "r", 5.0, " cm");
        addNumParamToUI(ui, "Wall Thick:", "wall", 1.0, " cm");
        addNumParamToUI(ui, "Base Height:", "h_base", 1.0, " cm");
        addNumParamToUI(ui, "Wall Height:", "h_wall", 10.0, " cm");
        addNumParamToUI(ui, "Mesh Size:", "ms", 1.0, " cm");
        addNumParamToUI(ui, "Center X:", "cx", 0.0, " cm");
        addNumParamToUI(ui, "Center Y:", "cy", 0.0, " cm");
        addNumParamToUI(ui, "Center Z:", "cz", 0.0, " cm");
    }
    else if (text == "Sphere" || text == "Hemisphere") {
        addNumParamToUI(ui, "Radius:", "r", 0.38, " cm");
        addNumParamToUI(ui, "Mesh Size:", "ms", 0.02, " cm");
        addNumParamToUI(ui, "Center X:", "cx", 6.0, " cm");
        addNumParamToUI(ui, "Center Y:", "cy", 0.0, " cm");
        addNumParamToUI(ui, "Center Z:", "cz", 4.0, " cm");
    }
    else if (text == "Frustum") {
        addNumParamToUI(ui, "Base R:", "rb", 6.0, " cm");
        addNumParamToUI(ui, "Top R:", "rt", 3.0, " cm");
        addNumParamToUI(ui, "Height:", "h", 10.0, " cm");
        addNumParamToUI(ui, "Mesh Size:", "ms", 1.0, " cm");
        addNumParamToUI(ui, "Center X:", "cx", 0.0, " cm");
        addNumParamToUI(ui, "Center Y:", "cy", 0.0, " cm");
        addNumParamToUI(ui, "Center Z:", "cz", 0.0, " cm");
    }
    else if (text == "HalfCylinder") {
        addNumParamToUI(ui, "Radius:", "r", 5.0, " cm");
        addNumParamToUI(ui, "Height:", "h", 15.0, " cm");
        addNumParamToUI(ui, "Mesh Size:", "ms", 1.0, " cm");
        addNumParamToUI(ui, "Center X:", "cx", 0.0, " cm");
        addNumParamToUI(ui, "Center Y:", "cy", 0.0, " cm");
        addNumParamToUI(ui, "Center Z:", "cz", 0.0, " cm");
    }
    else if (text == "TriangularPrism" || text == "PentagonalPrism" || text == "HexagonalPrism") {
        // 这三种多边形棱柱的参数是一模一样的
        addNumParamToUI(ui, "Radius (外接圆半径 R):", "r", 5.0, " cm");
        addNumParamToUI(ui, "Height (高度 H):", "h", 10.0, " cm");
        addNumParamToUI(ui, "Mesh Size:", "ms", 1.0, " cm");
        addNumParamToUI(ui, "Center X:", "cx", 0.0, " cm");
        addNumParamToUI(ui, "Center Y:", "cy", 0.0, " cm");
        addNumParamToUI(ui, "Center Z:", "cz", 0.0, " cm");
    }
    else if (text == "HalfCylindricalShell") {
        // 半圆柱壳需要内径、高度、端盖和壁厚
        addNumParamToUI(ui, "Inner Radius (内径 R):", "r", 4.0, " cm");
        addNumParamToUI(ui, "Total Height (总高 H):", "h", 9.2, " cm");
        addNumParamToUI(ui, "Lid Thick (端盖厚度):", "lid", 0.6, " cm");
        addNumParamToUI(ui, "Wall Thick (壁厚):", "wall", 0.6, " cm");
        addNumParamToUI(ui, "Mesh Size:", "ms", 0.1, " cm");
        addNumParamToUI(ui, "Center X:", "cx", 0.0, " cm");
        addNumParamToUI(ui, "Center Y:", "cy", 0.0, " cm");
        addNumParamToUI(ui, "Center Z:", "cz", 0.0, " cm");
    }
    else if (text == "Fragment Simulating Projectile") {
        addNumParamToUI(ui, "身管半径 (R):", "r", 5.0, " cm");
        addNumParamToUI(ui, "身管高度 (Hb):", "hb", 10.0, " cm");
        addNumParamToUI(ui, "头部高度 (Hn):", "hn", 4.0, " cm");
        addNumParamToUI(ui, "头部顶端半径 (Rt):", "rt", 2.0, " cm");
        addNumParamToUI(ui, "Mesh Size:", "ms", 0.6, " cm");
        addNumParamToUI(ui, "Center X:", "cx", 0.0, " cm");
        addNumParamToUI(ui, "Center Y:", "cy", 0.0, " cm");
        addNumParamToUI(ui, "Center Z:", "cz", 0.0, " cm");
    }
}

// 槽函数重构：处理任意一个窗口的生成按钮点击
void MainWindow::handleGenerateButtonClicked(GeneratorUI& ui) {
    if (!ui.nameInput || ui.shapeComboBox->currentText().isEmpty()) return;

    QString type = ui.shapeComboBox->currentText();
    QString name = ui.nameInput->text();

    // ==========================================
    // 🌟 核心防丢机制：在异步调用生成器前，先预注册实体！
    // ==========================================
    MeshEntity preEntity;
    preEntity.name = name;
    preEntity.type = type;

    // 智能推断物理类别
    if (&ui == &m_fragmentUI) preEntity.category = "破片";
    else if (&ui == &m_shellUI) preEntity.category = "壳体";
    else if (&ui == &m_chargeUI) preEntity.category = "装药";
    else preEntity.category = "未分类";

    // 备份参数字典
    auto val = [&](QString key) {
        return ui.paramInputs.contains(key) ? ui.paramInputs[key]->value() : 0.0;
        };
    for (auto it = ui.paramInputs.begin(); it != ui.paramInputs.end(); ++it) {
        preEntity.geoParams[it.key()] = it.value()->value();
    }

    // 提前将带有物理语义的空壳实体塞入仓库，锁定属性！
    m_repository.addEntity(name, preEntity);
    // ==========================================

    // 再调用异步底层生成逻辑
    if (type == "Cube") {
        CubeGenerator gen;
        gen.setParameters(val("lx"), val("ly"), val("lz"), val("ms"), val("cx"), val("cy"), val("cz"));
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "Cylinder") {
        CylinderGenerator gen;
        gen.setParameters(val("r"), val("ms"), val("h"), val("cx"), val("cy"), val("cz"));
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "CylindricalShell") {
        CylindricalShellGenerator gen;
        gen.setParameters(val("r"), val("h"), val("lid"), val("wall"), val("ms"), val("cx"), val("cy"), val("cz"));
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "OpenCylindricalShell") {
        OpenCylindricalShellGenerator gen;
        gen.setParameters(val("r"), val("wall"), val("h_base"), val("h_wall"), val("ms"), val("cx"), val("cy"), val("cz"));
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "Sphere") {
        SphereGenerator gen;
        gen.setParameters(val("r"), val("ms"), val("cx"), val("cy"), val("cz"));
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "Hemisphere") {
        HemisphereGenerator gen;
        gen.setParameters(val("r"), val("ms"), val("cx"), val("cy"), val("cz"));
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "Frustum") {
        FrustumGenerator gen;
        gen.setParameters(val("rb"), val("rt"), val("h"), val("ms"), val("cx"), val("cy"), val("cz"));
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "HalfCylinder") {
        HalfCylinderGenerator gen;
        gen.setParameters(val("r"), val("h"), val("ms"), val("cx"), val("cy"), val("cz"));
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "TriangularPrism") {
        TriangularPrismGenerator gen;
        gen.setParameters(val("r"), val("h"), val("ms"), val("cx"), val("cy"), val("cz"));
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "PentagonalPrism") {
        PentagonalPrismGenerator gen;
        gen.setParameters(val("r"), val("h"), val("ms"), val("cx"), val("cy"), val("cz"));
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "HexagonalPrism") {
        HexagonalPrismGenerator gen;
        gen.setParameters(val("r"), val("h"), val("ms"), val("cx"), val("cy"), val("cz"));
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "HalfCylindricalShell") {
        HalfCylindricalShellGenerator gen;
        gen.setParameters(val("r"), val("h"), val("lid"), val("wall"), val("ms"), val("cx"), val("cy"), val("cz"));
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "Fragment Simulating Projectile") {
        FSPGenerator gen;
        gen.setParameters(val("r"), val("hb"), val("hn"), val("rt"), val("ms"), val("cx"), val("cy"), val("cz"));
        m_meshManager->buildAndLoad(gen, name);
    }

    logCommand("GUI Generate (" + type + "): " + name);
}

// ==========================================
// 1. 平移操作 (Translate)
// ==========================================
void MainWindow::handleTranslateEntity(const QString& entityName) {
    QDialog dialog(this);
    dialog.setWindowTitle("平移实体 - " + entityName);
    QFormLayout form(&dialog);

    QDoubleSpinBox* dx = new QDoubleSpinBox(&dialog); dx->setRange(-99999, 99999); dx->setDecimals(3);
    QDoubleSpinBox* dy = new QDoubleSpinBox(&dialog); dy->setRange(-99999, 99999); dy->setDecimals(3);
    QDoubleSpinBox* dz = new QDoubleSpinBox(&dialog); dz->setRange(-99999, 99999); dz->setDecimals(3);

    form.addRow("X位移 (dx):", dx);
    form.addRow("Y位移 (dy):", dy);
    form.addRow("Z位移 (dz):", dz);

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form.addRow(&buttonBox);
    connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        QMatrix4x4 mat;
        mat.setToIdentity();
        mat.translate(dx->value(), dy->value(), dz->value());

        applyTransformation(entityName, mat);
        logCommand("Transformation", "Translated " + entityName);
    }
}

// ==========================================
// 2. 缩放操作 (Scale)
// ==========================================
void MainWindow::handleScaleEntity(const QString& entityName) {
    QDialog dialog(this);
    dialog.setWindowTitle("缩放实体 - " + entityName);
    QFormLayout form(&dialog);

    // 缩放因子默认值为 1.0
    QDoubleSpinBox* sx = new QDoubleSpinBox(&dialog); sx->setRange(0.001, 9999); sx->setValue(1.0); sx->setDecimals(3);
    QDoubleSpinBox* sy = new QDoubleSpinBox(&dialog); sy->setRange(0.001, 9999); sy->setValue(1.0); sy->setDecimals(3);
    QDoubleSpinBox* sz = new QDoubleSpinBox(&dialog); sz->setRange(0.001, 9999); sz->setValue(1.0); sz->setDecimals(3);

    form.addRow("X向缩放:", sx);
    form.addRow("Y向缩放:", sy);
    form.addRow("Z向缩放:", sz);

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form.addRow(&buttonBox);
    connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        MeshEntity* entity = m_repository.getMutableEntity(entityName);
        if (!entity || entity->nodes.empty()) return;

        // 【关键】必须以实体的几何中心为基准进行缩放
        QVector3D center(0, 0, 0);
        for (const auto& node : entity->nodes) {
            center += QVector3D(node.pos.x(), node.pos.y(), node.pos.z());
        }
        center /= float(entity->nodes.size());

        QMatrix4x4 mat;
        mat.setToIdentity();
        mat.translate(center); // 3. 将原点移回其实际位置
        mat.scale(sx->value(), sy->value(), sz->value()); // 2. 缩放
        mat.translate(-center); // 1. 将实体中心平移到坐标原点

        applyTransformation(entityName, mat);
        logCommand("Transformation", "Scaled " + entityName);
    }
}

// ==========================================
// 3. 旋转操作 (Rotate)
// ==========================================
void MainWindow::handleRotateEntity(const QString& entityName) {
    QDialog dialog(this);
    dialog.setWindowTitle("旋转实体 - " + entityName);
    QFormLayout form(&dialog);

    QDoubleSpinBox* cx = new QDoubleSpinBox(&dialog); cx->setRange(-9999, 9999); cx->setDecimals(3);
    QDoubleSpinBox* cy = new QDoubleSpinBox(&dialog); cy->setRange(-9999, 9999); cy->setDecimals(3);
    QDoubleSpinBox* cz = new QDoubleSpinBox(&dialog); cz->setRange(-9999, 9999); cz->setDecimals(3);

    QDoubleSpinBox* ax = new QDoubleSpinBox(&dialog); ax->setRange(-1.0, 1.0); ax->setValue(0.0);
    QDoubleSpinBox* ay = new QDoubleSpinBox(&dialog); ay->setRange(-1.0, 1.0); ay->setValue(0.0);
    QDoubleSpinBox* az = new QDoubleSpinBox(&dialog); az->setRange(-1.0, 1.0); az->setValue(1.0);

    QDoubleSpinBox* angle = new QDoubleSpinBox(&dialog); angle->setRange(-360.0, 360.0); angle->setValue(90.0);

    form.addRow("旋转中心 X:", cx);
    form.addRow("旋转中心 Y:", cy);
    form.addRow("旋转中心 Z:", cz);
    form.addRow("旋转轴向量 X:", ax);
    form.addRow("旋转轴向量 Y:", ay);
    form.addRow("旋转轴向量 Z:", az);
    form.addRow("旋转角度 (度):", angle);

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form.addRow(&buttonBox);
    connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        QVector3D center(cx->value(), cy->value(), cz->value());
        QVector3D axis(ax->value(), ay->value(), az->value());

        QMatrix4x4 mat;
        mat.setToIdentity();
        mat.translate(center);  // 3. 移回旋转中心点
        mat.rotate(angle->value(), axis); // 2. 绕原点的对应轴旋转
        mat.translate(-center); // 1. 将旋转中心点移到原点

        applyTransformation(entityName, mat);
        logCommand("Transformation", "Rotated " + entityName);
    }
}

// ==========================================
// 4. 矩阵变换底层执行 (Apply)
// ==========================================
void MainWindow::applyTransformation(const QString& entityName, const QMatrix4x4& mat) {
    MeshEntity* entity = m_repository.getMutableEntity(entityName);
    if (!entity) return;

    // 1. 变换所有节点的坐标
    for (auto& node : entity->nodes) {
        QVector3D oldPos(node.pos.x(), node.pos.y(), node.pos.z());
        QVector3D newPos = mat * oldPos; // 矩阵乘法直接完成变换

        // 重新赋值（请根据你自定义 Vector 的方法名修改此处，比如 setX 或者直接赋值）
        node.pos = QVector3D(newPos.x(), newPos.y(), newPos.z());
    }

    // 2. 变换实体线框/边界线（如果在渲染层你用 wireLines 保存了骨架线）
    for (auto& linePos : entity->wireLines) {
        QVector3D oldPos(linePos.x(), linePos.y(), linePos.z());
        QVector3D newPos = mat * oldPos;

        linePos = QVector3D(newPos.x(), newPos.y(), newPos.z());
    }

    // 3. 强制重绘 3D 界面
    glWidget->update();
}

void MainWindow::createSimulationSetupDock() {

    QDockWidget* setupDock = new QDockWidget(tr("Simulation Setup (物理与求解设置)"), this);
    setupDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setupDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    m_simSetupUI.mainTab = new QTabWidget(setupDock);

    // ==========================================
    // Tab 1: 材料与状态方程 (Materials & EOS)
    // ==========================================
    QWidget* matTab = new QWidget();
    QVBoxLayout* matLayout = new QVBoxLayout(matTab);

    // 实体选择
    matLayout->addWidget(new QLabel("目标实体 (Target Entity):"));
    m_simSetupUI.entitySelector = new QComboBox();
    matLayout->addWidget(m_simSetupUI.entitySelector);

    // 材料库选择
    matLayout->addWidget(new QLabel("材料本构 (Material Model):"));
    m_simSetupUI.materialSelector = new QComboBox();
    m_simSetupUI.materialSelector->addItems({ "*MAT_PIECEWISE_LINEAR_PLASTICITY", "*MAT_JOHNSON_COOK", "*MAT_HIGH_EXPLOSIVE_BURN", "*MAT_ELASTIC_PLASTIC_HYDRO","*MAT_NULL" });
    matLayout->addWidget(m_simSetupUI.materialSelector);

    // EOS 库选择 (初始隐藏)
    QLabel* eosLabel = new QLabel("状态方程 (Equation of State):");
    m_simSetupUI.eosSelector = new QComboBox();

    m_simSetupUI.eosSelector->addItems({
            "None",
            "*EOS_GRUNEISEN",
            "*EOS_JWL",
            "*EOS_LINEAR_POLYNOMIAL",
            "*EOS_TILLOTSON",
            "*EOS_IGNITION_AND_GROWTH_OF_REACTION_IN_HE"
        });    

    matLayout->addWidget(eosLabel);
    matLayout->addWidget(m_simSetupUI.eosSelector);

    // 动态参数容器
    m_simSetupUI.matParamContainer = new QWidget();
    m_simSetupUI.matParamLayout = new QFormLayout(m_simSetupUI.matParamContainer);
    QScrollArea* scrollArea = new QScrollArea(); //滚动条
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(m_simSetupUI.matParamContainer);
    matLayout->addWidget(scrollArea);

    m_simSetupUI.mainTab->addTab(matTab, "材料(Material)");

    m_simSetupUI.btnAddMaterial = new QPushButton("添加材料与侵蚀设定 (Add Material)");
    matLayout->addWidget(m_simSetupUI.btnAddMaterial);

	// 材料预设下拉菜单
    m_simSetupUI.presetSelector = new QComboBox();
    
    matLayout->addWidget(new QLabel("选择材料预设 (自动填充参数):"));
    matLayout->addWidget(m_simSetupUI.presetSelector);

    loadMaterialPresetsFromJson();

    connect(m_simSetupUI.presetSelector, &QComboBox::currentTextChanged, this, &MainWindow::handlePresetChanged);
    // ==========================================
    // Tab 2: 求解控制 (Control)
    // ==========================================
    QWidget* ctrlTab = new QWidget();
    QFormLayout* ctrlLayout = new QFormLayout(ctrlTab);

    m_simSetupUI.endtimeInput = new QDoubleSpinBox();
    m_simSetupUI.endtimeInput->setRange(0, 99999); m_simSetupUI.endtimeInput->setValue(1.0);
    ctrlLayout->addRow("结束时间 (ENDTIM):", m_simSetupUI.endtimeInput);
    m_simSetupUI.endtimeInput->setSuffix(" μs");

    m_simSetupUI.d3plotFreqInput = new QDoubleSpinBox();
    m_simSetupUI.d3plotFreqInput->setRange(0, 9999); m_simSetupUI.d3plotFreqInput->setValue(0.01);

    
    ctrlLayout->addRow("D3PLOT 步长 (DT):", m_simSetupUI.d3plotFreqInput);
    m_simSetupUI.d3plotFreqInput->setSuffix(" μs");

    m_simSetupUI.mainTab->addTab(ctrlTab, "控制(Control)");
    m_simSetupUI.btnAddControl = new QPushButton("应用全局控制 (Add Control)");
	ctrlLayout->addWidget(m_simSetupUI.btnAddControl);
    // ==========================================
    // 底部应用按钮
    // ==========================================
    QWidget* mainContainer = new QWidget();
    QVBoxLayout* mainVLayout = new QVBoxLayout(mainContainer);
    mainVLayout->addWidget(m_simSetupUI.mainTab);

    // ==========================================
    // 底部：实时观察面板与导出区域
    // ==========================================

    // 1. 上半部分放入你的 TabWidget
    mainVLayout->addWidget(m_simSetupUI.mainTab);

    // 2. 下半部分放入实时观察列表
    mainVLayout->addWidget(new QLabel("已添加的仿真参数 (Active Cards):"));
    m_simSetupUI.setupSummaryList = new QListWidget();
    m_simSetupUI.setupSummaryList->setMaximumHeight(150);
    mainVLayout->addWidget(m_simSetupUI.setupSummaryList);

    m_simSetupUI.setupSummaryList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_simSetupUI.setupSummaryList, &QListWidget::customContextMenuRequested, this, &MainWindow::showSummaryContextMenu);

    // 3. 底部操作按钮
    QHBoxLayout* bottomBtnLayout = new QHBoxLayout();
    m_simSetupUI.btnClearSummary = new QPushButton("清空 (Clear)");
    m_simSetupUI.btnExportKFile = new QPushButton("导出工况.k文件(Export .k File)");
    m_simSetupUI.btnExportKFile->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;"); // 导出按钮搞个醒目的绿色

    bottomBtnLayout->addWidget(m_simSetupUI.btnClearSummary);
    bottomBtnLayout->addWidget(m_simSetupUI.btnExportKFile);
    mainVLayout->addLayout(bottomBtnLayout);


    // ==========================================
    // Tab 材料标签页的底部加入“侵蚀(Erosion)设置”
    // ==========================================
    m_simSetupUI.erosionGroup = new QGroupBox("单元侵蚀准则 (*MAT_ADD_EROSION)");
    m_simSetupUI.erosionGroup->setCheckable(true);
    m_simSetupUI.erosionGroup->setChecked(false); // 默认不开启
    QFormLayout* erosionLayout = new QFormLayout(m_simSetupUI.erosionGroup);
    m_simSetupUI.erosionMxeps = new QDoubleSpinBox();
    m_simSetupUI.erosionMxeps->setRange(0.0, 10.0);
    m_simSetupUI.erosionMxeps->setValue(2.0); // 默认失效应变 2.0
    erosionLayout->addRow("最大失效应变 (MXEPS):", m_simSetupUI.erosionMxeps);
    matLayout->addWidget(m_simSetupUI.erosionGroup); // 加到原来的 matLayout 底部

    // ==========================================
    // Tab: 初始条件 (Initial Conditions)
    // ==========================================
    QWidget* icTab = new QWidget();
    QFormLayout* icLayout = new QFormLayout(icTab);

    m_simSetupUI.icEntitySelector = new QComboBox();
    icLayout->addRow("目标实体 (Part):", m_simSetupUI.icEntitySelector);

    m_simSetupUI.icVx = new QDoubleSpinBox(); m_simSetupUI.icVx->setRange(-99999, 99999);
    m_simSetupUI.icVx->setSuffix(" cm/μs");
    m_simSetupUI.icVy = new QDoubleSpinBox(); m_simSetupUI.icVy->setRange(-99999, 99999);
    m_simSetupUI.icVy->setSuffix(" cm/μs");
    m_simSetupUI.icVz = new QDoubleSpinBox(); m_simSetupUI.icVz->setRange(-99999, 99999);
    m_simSetupUI.icVz->setSuffix(" cm/μs");
    icLayout->addRow("X 向初始速度 (Vx):", m_simSetupUI.icVx);
    icLayout->addRow("Y 向初始速度 (Vy):", m_simSetupUI.icVy);
    icLayout->addRow("Z 向初始速度 (Vz):", m_simSetupUI.icVz);

    m_simSetupUI.mainTab->addTab(icTab, "初始条件(IC)");

    m_simSetupUI.btnAddIC = new QPushButton("添加初始速度 (Add IC)");
    icLayout->addWidget(m_simSetupUI.btnAddIC);
    // ==========================================
    // Tab: 接触定义 (Contact)
    // ==========================================
    QWidget* contactTab = new QWidget();
    QFormLayout* contactLayout = new QFormLayout(contactTab);

    m_simSetupUI.contactTypeSelector = new QComboBox();
    m_simSetupUI.contactTypeSelector->addItems({ "*CONTACT_ERODING_SURFACE_TO_SURFACE", "*CONTACT_AUTOMATIC_SINGLE_SURFACE", "*CONTACT_TIED_SURFACE_TO_SURFACE" });
    contactLayout->addRow("接触类型:", m_simSetupUI.contactTypeSelector);

    m_simSetupUI.contactMasterSelector = new QComboBox();
    m_simSetupUI.contactSlaveSelector = new QComboBox();
    contactLayout->addRow("主面实体 (Master):", m_simSetupUI.contactMasterSelector);
    contactLayout->addRow("从面实体 (Slave):", m_simSetupUI.contactSlaveSelector);

    m_simSetupUI.contactFs = new QDoubleSpinBox(); m_simSetupUI.contactFs->setRange(0, 1); m_simSetupUI.contactFs->setValue(0.0);
    m_simSetupUI.contactFd = new QDoubleSpinBox(); m_simSetupUI.contactFd->setRange(0, 1); m_simSetupUI.contactFd->setValue(0.0);
    contactLayout->addRow("静摩擦系数 (FS):", m_simSetupUI.contactFs);
    contactLayout->addRow("动摩擦系数 (FD):", m_simSetupUI.contactFd);

    m_simSetupUI.mainTab->addTab(contactTab, "接触(Contact)");

    m_simSetupUI.btnAddContact = new QPushButton("添加接触定义 (Add Contact)");
    contactLayout->addWidget(m_simSetupUI.btnAddContact);
    // ==========================================
    // Tab: 截面与算法 (Section)
    // ==========================================
    QWidget* sectionTab = new QWidget();
    QFormLayout* sectionLayout = new QFormLayout(sectionTab);

    m_simSetupUI.sectionEntitySelector = new QComboBox();
    sectionLayout->addRow("目标实体 (Part):", m_simSetupUI.sectionEntitySelector);

    m_simSetupUI.sectionTypeSelector = new QComboBox();
    m_simSetupUI.sectionTypeSelector->addItems({ "*SECTION_SOLID", "*SECTION_SHELL" });
    sectionLayout->addRow("单元类型:", m_simSetupUI.sectionTypeSelector);

    m_simSetupUI.sectionElformSelector = new QComboBox();
    m_simSetupUI.sectionElformSelector->addItems({ "1 - 单点积分 (快, 需控制沙漏)", "2 - 全积分 (慢, 精确)", "0 - 默认" });
    m_simSetupUI.sectionElformSelector->setCurrentIndex(0);
    sectionLayout->addRow("单元算法 (ELFORM):", m_simSetupUI.sectionElformSelector);

    m_simSetupUI.mainTab->addTab(sectionTab, "截面(Section)");

    m_simSetupUI.btnAddSection = new QPushButton("添加截面属性 (Add Section)");
    sectionLayout->addWidget(m_simSetupUI.btnAddSection);
    // ==========================================
    // Tab: 控制面板 (Control) 增加高级选项
    // ==========================================

    m_simSetupUI.jobTitleInput = new QLineEdit("Fragment_ignition");
    ctrlLayout->insertRow(0, "项目名称 (TITLE):", m_simSetupUI.jobTitleInput);

    m_simSetupUI.tssfacInput = new QDoubleSpinBox();
    m_simSetupUI.tssfacInput->setRange(0.1, 1.0); m_simSetupUI.tssfacInput->setValue(0.9); m_simSetupUI.tssfacInput->setSingleStep(0.1);
    ctrlLayout->insertRow(1, "时间步缩放 (TSSFAC):", m_simSetupUI.tssfacInput);

    // ==========================================
    // [重写] Tab: 传感器与观测点 (Sensors) - 统一界面
    // ==========================================
    QWidget* sensorTab = new QWidget();
    QVBoxLayout* sensorMainLayout = new QVBoxLayout(sensorTab);

    QFormLayout* sensorLayout = new QFormLayout();

    // 1. 实体选择
    m_simSetupUI.sensorEntitySelector = new QComboBox();
    sensorLayout->addRow("目标实体 (Target):", m_simSetupUI.sensorEntitySelector);

    // 2. 模式切换
    m_simSetupUI.sensorModeSelector = new QComboBox();
    m_simSetupUI.sensorModeSelector->addItems({ "单点观测 (Single Point)", "线性阵列观测 (Line Array)" });
    sensorLayout->addRow("观测模式 (Mode):", m_simSetupUI.sensorModeSelector);

    // 3. 起点 / 单点坐标
    m_simSetupUI.sensorStartX = new QDoubleSpinBox(); m_simSetupUI.sensorStartX->setRange(-99999, 99999);
    m_simSetupUI.sensorStartY = new QDoubleSpinBox(); m_simSetupUI.sensorStartY->setRange(-99999, 99999);
    m_simSetupUI.sensorStartZ = new QDoubleSpinBox(); m_simSetupUI.sensorStartZ->setRange(-99999, 99999);

    QHBoxLayout* startLayout = new QHBoxLayout();
    startLayout->addWidget(m_simSetupUI.sensorStartX); startLayout->addWidget(m_simSetupUI.sensorStartY); startLayout->addWidget(m_simSetupUI.sensorStartZ);
    sensorLayout->addRow("位置/起点 (X, Y, Z):", startLayout);

    // 4. 阵列专属折叠面板 (默认隐藏)
    m_simSetupUI.sensorArrayContainer = new QWidget();
    QFormLayout* arrayLayout = new QFormLayout(m_simSetupUI.sensorArrayContainer);
    arrayLayout->setContentsMargins(0, 0, 0, 0); // 取消边距让它看起来无缝衔接

    m_simSetupUI.sensorEndX = new QDoubleSpinBox(); m_simSetupUI.sensorEndX->setRange(-99999, 99999);
    m_simSetupUI.sensorEndY = new QDoubleSpinBox(); m_simSetupUI.sensorEndY->setRange(-99999, 99999);
    m_simSetupUI.sensorEndZ = new QDoubleSpinBox(); m_simSetupUI.sensorEndZ->setRange(-99999, 99999);

    QHBoxLayout* endLayout = new QHBoxLayout();
    endLayout->addWidget(m_simSetupUI.sensorEndX); endLayout->addWidget(m_simSetupUI.sensorEndY); endLayout->addWidget(m_simSetupUI.sensorEndZ);
    arrayLayout->addRow("终点坐标 (X, Y, Z):", endLayout);

    m_simSetupUI.sensorNumPoints = new QSpinBox();
    m_simSetupUI.sensorNumPoints->setRange(2, 1000);
    m_simSetupUI.sensorNumPoints->setValue(10);
    arrayLayout->addRow("测点数量 (N):", m_simSetupUI.sensorNumPoints);

    sensorLayout->addRow("", m_simSetupUI.sensorArrayContainer);
    m_simSetupUI.sensorArrayContainer->setVisible(false); // 初始状态为单点，隐藏该容器

    // 🌟 核心折叠逻辑：根据下拉框的值，动态显示/隐藏下半部分
    connect(m_simSetupUI.sensorModeSelector, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        m_simSetupUI.sensorArrayContainer->setVisible(text.contains("Line Array"));
        });

    sensorMainLayout->addLayout(sensorLayout);

    // 5. 统一提交按钮
    m_simSetupUI.btnAddSensor = new QPushButton("添加观测点 (Add Sensor)");
    sensorMainLayout->addWidget(m_simSetupUI.btnAddSensor);
    sensorMainLayout->addStretch();

    m_simSetupUI.mainTab->addTab(sensorTab, "测点(Sensors)");

    // 绑定信号
    connect(m_simSetupUI.btnAddSensor, &QPushButton::clicked, this, &MainWindow::handleAddSensor);

    // 连接所有信号
    connect(m_simSetupUI.materialSelector, &QComboBox::currentTextChanged, this, [this](const QString& matType) {
        m_simSetupUI.eosSelector->blockSignals(true); // 防止自动匹配时重复触发绘制
        if (matType == "*MAT_JOHNSON_COOK") m_simSetupUI.eosSelector->setCurrentText("*EOS_GRUNEISEN");
        else if (matType == "*MAT_HIGH_EXPLOSIVE_BURN") m_simSetupUI.eosSelector->setCurrentText("*EOS_JWL");
        else if (matType == "*MAT_NULL") m_simSetupUI.eosSelector->setCurrentText("*EOS_LINEAR_POLYNOMIAL");
        else if (matType == "*MAT_ELASTIC_PLASTIC_HYDRO") m_simSetupUI.eosSelector->setCurrentText("*EOS_IGNITION_AND_GROWTH_OF_REACTION_IN_HE");
        else m_simSetupUI.eosSelector->setCurrentText("None");
        m_simSetupUI.eosSelector->blockSignals(false);

        handleMaterialTypeChanged(matType); // 绘制输入框
        });

    connect(m_simSetupUI.btnAddMaterial, &QPushButton::clicked, this, &MainWindow::handleAddMaterial);
    connect(m_simSetupUI.btnAddContact, &QPushButton::clicked, this, &MainWindow::handleAddContact);
    connect(m_simSetupUI.btnAddIC, &QPushButton::clicked, this, &MainWindow::handleAddIC);
    connect(m_simSetupUI.btnAddSection, &QPushButton::clicked, this, &MainWindow::handleAddSection);
    connect(m_simSetupUI.btnClearSummary, &QPushButton::clicked, this, &MainWindow::handleClearSummary);
    connect(m_simSetupUI.btnAddControl, &QPushButton::clicked, this, &MainWindow::handleAddGlobalControl);
    // 这里的 btnExportKFile 连接到你之前写的 handleApplySimulationSettings
    connect(m_simSetupUI.btnExportKFile, &QPushButton::clicked, this, &MainWindow::handleApplySimulationSettings);

    // ==========================================
    //全局控件随窗口自适应缩放 (Fluid Layout)
    // ==========================================

    // 1. 批量解除【所有】输入控件和按钮的宽度锁定
    QList<QWidget*> allWidgets = mainContainer->findChildren<QWidget*>();
    for (QWidget* w : allWidgets) {
        // 利用 qobject_cast 过滤出所有的下拉框、数字框、文本框和按钮
        if (qobject_cast<QComboBox*>(w) ||
            qobject_cast<QDoubleSpinBox*>(w) ||
            qobject_cast<QSpinBox*>(w) ||
            qobject_cast<QLineEdit*>(w) ||
            qobject_cast<QPushButton*>(w)) {

            w->setMinimumWidth(40); // 允许压缩到底线 40 像素
            // 🌟 核心：横向策略设为 Expanding (自动跟随窗口拉伸/压缩)
            // 纵向策略设为 Fixed (高度保持原生尺寸不变)
            w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        }
    }

    // 2. 加入透明滚动区 (QScrollArea) 
    QScrollArea* mainScrollArea = new QScrollArea();
    mainScrollArea->setWidget(mainContainer);
    mainScrollArea->setWidgetResizable(true);       // 必须开启：内部面板跟随外框灵活伸缩
    mainScrollArea->setFrameShape(QFrame::NoFrame); // 去除自带的凹陷边框

    mainScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // 3. 将被滚动区保护的面板放入 Dock 中
    setupDock->setWidget(mainScrollArea);

    addDockWidget(Qt::LeftDockWidgetArea, setupDock);

    // 触发一次初始化
    handleMaterialTypeChanged(m_simSetupUI.materialSelector->currentText());
}

void MainWindow::handleMaterialTypeChanged(const QString& matType) {
    // 1. 清空旧表单
    QLayoutItem* item;
    while ((item = m_simSetupUI.matParamLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    m_simSetupUI.currentMatInputs.clear();

    // 辅助 Lambda
    auto addParam = [&](const QString& label, const QString& key, double defaultVal, const QString& unit = "") {
        QDoubleSpinBox* box = new QDoubleSpinBox();
        box->setRange(-999999, 999999);
        box->setDecimals(5);
        box->setValue(defaultVal);

        if (!unit.isEmpty()) {
            box->setSuffix(unit);
        }

        m_simSetupUI.matParamLayout->addRow(label, box);
        m_simSetupUI.currentMatInputs[key] = box;
        };

    // 2. 先画出纯材料 (MAT) 的参数
    if (matType == "*MAT_JOHNSON_COOK") {
        addParam("密度 (RO):", "ro", 7.83e-6, " g/cm³");
        addParam("剪切模量 (G):", "g", 77.0, " Mbar");
        addParam("屈服强度 (A):", "a", 0.792, " Mbar");
        addParam("硬化常数 (B):", "b", 0.510, " Mbar");
        addParam("硬化指数 (N):", "n", 0.26);
        addParam("应变率常数 (C):", "c", 0.014);
        addParam("软化指数 (M):", "m", 1.03);
        addParam("熔点 (TMELT):", "tmelt", 1793, " K");
    }
    else if (matType == "*MAT_HIGH_EXPLOSIVE_BURN") {
        addParam("密度 (RO):", "ro", 1.63e-6);
        addParam("爆速 (D):", "d", 6.93);
        addParam("CJ 压力 (PCJ):", "pcj", 21.0);
    }
    else if (matType == "*MAT_NULL") {
        addParam("密度 (RO):", "ro", 1.0e-6);
        addParam("压力截断 (PC):", "pc", -1.0e-6);
        addParam("动力粘度 (MU):", "mu", 0.0);
    }
    else if (matType == "*MAT_ELASTIC_PLASTIC_HYDRO") {
        addParam("密度 (RO):", "ro", 1.712);
        addParam("剪切模量 (G):", "g", 0.0354);
        addParam("屈服强度 (SIGY):", "sigy", 2.0e-4);
        addParam("压力截断 (PC):", "pc", -9.0);
    }

    // 3. 接着读取当前 EOS 选了什么，并在下方画出具体的 EOS 参数！
    QString eosType = m_simSetupUI.eosSelector->currentText();

    if (eosType == "*EOS_GRUNEISEN") {
        addParam("[GRUNEISEN] C (截距):", "gr_c", 0.0);
        addParam("[GRUNEISEN] S1 (斜率):", "gr_s1", 0.0);
        addParam("[GRUNEISEN] GAMAO:", "gr_gamao", 0.0);
    }
    else if (eosType == "*EOS_JWL") {
        addParam("[JWL] A:", "jwl_a", 373.77, " Mbar");
        addParam("[JWL] B:", "jwl_b", 3.747, " Mbar");
        addParam("[JWL] R1:", "jwl_r1", 4.15);
        addParam("[JWL] R2:", "jwl_r2", 0.90);
        addParam("[JWL] OMEGA:", "jwl_omega", 0.35);
        addParam("[JWL] E0:", "jwl_e0", 0.06, " Mbar");
    }
    else if (eosType == "*EOS_LINEAR_POLYNOMIAL") {
        addParam("[LINEAR] C0:", "lp_c0", 0.0);
        addParam("[LINEAR] C1:", "lp_c1", 0.0);
        addParam("[LINEAR] C2:", "lp_c2", 0.0);
        addParam("[LINEAR] C3:", "lp_c3", 0.0);
        addParam("[LINEAR] C4:", "lp_c4", 0.4);
        addParam("[LINEAR] C5:", "lp_c5", 0.4);
        addParam("[LINEAR] C6:", "lp_c6", 0.0);
        addParam("[LINEAR] E0:", "lp_e0", 2.5e-6);
        addParam("[LINEAR] V0:", "lp_v0", 1.0);
    }
    else if (eosType == "*EOS_TILLOTSON") {
        addParam("[TILLOTSON] A:", "til_a", 0.0);
        addParam("[TILLOTSON] B:", "til_b", 0.0);
        addParam("[TILLOTSON] OMEGA:", "til_omega", 0.0);
        addParam("[TILLOTSON] E0:", "til_e0", 0.0);
        addParam("[TILLOTSON] V0:", "til_v0", 1.0);
        addParam("[TILLOTSON] ALPHA:", "til_alpha", 0.0);
        addParam("[TILLOTSON] BETA:", "til_beta", 0.0);
    }
    else if (eosType == "*EOS_IGNITION_AND_GROWTH_OF_REACTION_IN_HE") {
        // 第一行
        addParam("[I&G] A:", "ig_a", 5.242);
        addParam("[I&G] B:", "ig_b", 0.07678);
        addParam("[I&G] XP1:", "ig_xp1", 2.84999);
        addParam("[I&G] XP2:", "ig_xp2", 1.1);
        addParam("[I&G] FRER:", "ig_frer", 0.667);
        addParam("[I&G] G:", "ig_g", 5.0e-6);
        addParam("[I&G] R1:", "ig_r1", 778.1);

        // 第二行
        addParam("[I&G] R2:", "ig_r2", -0.05031);
        addParam("[I&G] R3:", "ig_r3", 2.223e-5);
        addParam("[I&G] R5:", "ig_r5", 11.3);
        addParam("[I&G] R6:", "ig_r6", 1.13);
        addParam("[I&G] FMXIG:", "ig_fmxig", 0.022);
        addParam("[I&G] FREQ:", "ig_freq", 4.0);
        addParam("[I&G] GROW1:", "ig_grow1", 120.0);
        addParam("[I&G] EM:", "ig_em", 2.0);

        // 第三行
        addParam("[I&G] AR1:", "ig_ar1", 0.333);
        addParam("[I&G] ES1:", "ig_es1", 0.667);
        addParam("[I&G] CVP:", "ig_cvp", 10.0);
        addParam("[I&G] CVR:", "ig_cvr", 24.78);
        addParam("[I&G] EETAL:", "ig_eetal", 7.0);
        addParam("[I&G] CCRIT:", "ig_ccrit", 0.0367);
        addParam("[I&G] ENQ:", "ig_enq", 0.085);
        addParam("[I&G] TMP0:", "ig_tmp0", 298.0);

        // 第四行
        addParam("[I&G] GROW2:", "ig_grow2", 1000.0);
        addParam("[I&G] AR2:", "ig_ar2", 1.0);
        addParam("[I&G] ES2:", "ig_es2", 0.222);
        addParam("[I&G] EN:", "ig_en", 3.0);
        addParam("[I&G] FMXGR:", "ig_fmxgr", 0.7);
        addParam("[I&G] FMNGR:", "ig_fmngr", 0.0);
    }
}

void MainWindow::handleApplySimulationSettings() {
    if (m_workingDirectory.isEmpty()) {
        QMessageBox::warning(this, "警告", "请先设置工作目录！");
        return;
    }

    for (int i = 0; i < m_simSetupUI.entitySelector->count(); ++i) {
        getOrCreatePart(m_simSetupUI.entitySelector->itemText(i));
    }

    QString jobTitle = m_simSetupUI.jobTitleInput->text();
    if (jobTitle.isEmpty()) jobTitle = "Simulation_Job";

    QString meshFileName = jobTitle + "_mesh.k";
    QString controlFileName = jobTitle + "_control.k";

    // 1. 导出几何网格
    exportToKFile(QDir(m_workingDirectory).filePath(meshFileName));

    // 2. 导出控制文件
    QFile controlFile(QDir(m_workingDirectory).filePath(controlFileName));
    if (controlFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&controlFile);

        // 写入固定头部
        out << "*KEYWORD MEMORY=399999999\n";
        out << "*TITLE\n" << jobTitle << "\n";
        out << "*INCLUDE\n" << meshFileName << "\n"; // Include网格

        if (m_globalControlCard != nullptr) {
            out << QString::fromStdString(m_globalControlCard->to_string());
        }

        out << QString::fromStdString(m_deck.generateDeck());

        out << "*END\n";
        controlFile.close();
    }

    QMessageBox::information(this, "导出成功", "仿真文件打包成功！控制文件与网格文件已分离。");
}

// ==========================================
// 设置工作目录
// ==========================================
void MainWindow::onSetWorkingDirectory() {
    // 弹出文件夹选择对话框
    QString dir = QFileDialog::getExistingDirectory(this,
        tr("选择工作目录 (Select Working Directory)"),
        m_workingDirectory,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    // 如果用户没有点击取消
    if (!dir.isEmpty()) {
        m_workingDirectory = dir; // 更新路径

        // 在底部的命令行/日志窗口打印提示
        logCommand("System", "工作目录已设置为: " + m_workingDirectory);

     }
}

void MainWindow::updateAllEntitySelectors() {
    // 提前保存当前选择的文本，防止刷新后用户的选择丢失
    QString curMat = m_simSetupUI.entitySelector->currentText();
    QString curIc = m_simSetupUI.icEntitySelector->currentText();
    QString curMaster = m_simSetupUI.contactMasterSelector->currentText();
    QString curSlave = m_simSetupUI.contactSlaveSelector->currentText();
    QString curSec = m_simSetupUI.sectionEntitySelector->currentText();

    // 清空所有框
    m_simSetupUI.entitySelector->clear();
    m_simSetupUI.icEntitySelector->clear();
    m_simSetupUI.contactMasterSelector->clear();
    m_simSetupUI.contactSlaveSelector->clear();
    m_simSetupUI.sectionEntitySelector->clear();
    m_simSetupUI.sensorEntitySelector->clear();

    // 加载所有实体
    const auto& allEntities = m_repository.getAllEntities();
    for (const auto& pair : allEntities) {
        QString name = pair.first;

        m_simSetupUI.entitySelector->addItem(name);
        m_simSetupUI.icEntitySelector->addItem(name);
        m_simSetupUI.contactMasterSelector->addItem(name);
        m_simSetupUI.contactSlaveSelector->addItem(name);
        m_simSetupUI.sectionEntitySelector->addItem(name);
        m_simSetupUI.sensorEntitySelector->addItem(name);
    }

    // 尝试恢复之前的选择
    m_simSetupUI.entitySelector->setCurrentText(curMat);
    m_simSetupUI.icEntitySelector->setCurrentText(curIc);
    m_simSetupUI.contactMasterSelector->setCurrentText(curMaster);
    m_simSetupUI.contactSlaveSelector->setCurrentText(curSlave);
    m_simSetupUI.sectionEntitySelector->setCurrentText(curSec);

    QString curSensor = m_simSetupUI.sensorEntitySelector->currentText();
    m_simSetupUI.sensorEntitySelector->setCurrentText(curSensor);
}

void MainWindow::handleAddMaterial() {
    QString target = m_simSetupUI.entitySelector->currentText();
    if (target.isEmpty()) return;

    auto part = getOrCreatePart(target);
    QString matType = m_simSetupUI.materialSelector->currentText().remove("*MAT_");
    QString eosTypeStr = m_simSetupUI.eosSelector->currentText();

    // 1. 将前端动态输入全部抓取为字典
    std::map<std::string, double> paramDict;
    for (auto it = m_simSetupUI.currentMatInputs.begin(); it != m_simSetupUI.currentMatInputs.end(); ++it) {
        paramDict[it.key().toStdString()] = it.value()->value();
    }
    if (m_simSetupUI.erosionGroup->isChecked()) {
        paramDict["mxeps"] = m_simSetupUI.erosionMxeps->value();
    }

    // 2. 实例化材料卡片并存入 Deck
    auto matCard = std::make_shared<MaterialCard>(part->pid, matType.toStdString(), paramDict);
    m_deck.addCard(matCard);
    part->mid = part->pid;

    // 3. 动态解析并挂载 EOS 卡片
    std::shared_ptr<EOSCard> eosCard = nullptr;
    if (eosTypeStr != "None") {
        QString cleanEos = eosTypeStr;
        cleanEos.remove("*EOS_"); // 变成 "GRUNEISEN", "JWL" 等

        eosCard = std::make_shared<EOSCard>(part->pid, cleanEos.toStdString(), paramDict);
        m_deck.addCard(eosCard);
        part->eosid = part->pid; // 绑定到实体
    }
    else {
        part->eosid = 0; // 无 EOS
    }

    // 4. 生成右侧列表文字
    QString summary = eosCard ? QString("[材料+EOS] 实体:%1 | %2 + %3").arg(target).arg(matType).arg(eosTypeStr.remove("*EOS_"))
        : QString("[材料] 实体:%1 | %2").arg(target).arg(matType);

    // 5. 【核心修改】将卡片指针地址悄悄绑定到列表项 (UserRole隐藏域)
    QListWidgetItem* item = new QListWidgetItem(summary);

    // 绑定 MAT 卡片地址
    item->setData(Qt::UserRole, QVariant::fromValue(reinterpret_cast<quintptr>(matCard.get())));

    // 如果有 EOS，把 EOS 卡片地址绑在备用域 UserRole + 2
    if (eosCard) {
        item->setData(Qt::UserRole + 2, QVariant::fromValue(reinterpret_cast<quintptr>(eosCard.get())));
    }
    item->setData(Qt::UserRole + 1, "CARD"); // 标记类型为实体卡片

    m_simSetupUI.setupSummaryList->addItem(item);
}

void MainWindow::handleAddContact() {
    QString type = m_simSetupUI.contactTypeSelector->currentText().remove("*CONTACT_");
    QString master = m_simSetupUI.contactMasterSelector->currentText();
    QString slave = m_simSetupUI.contactSlaveSelector->currentText();

    if (master.isEmpty() || slave.isEmpty()) return;

    // 🌟 1. 获取主从面实体的 PID
    auto masterPart = getOrCreatePart(master);
    auto slavePart = getOrCreatePart(slave);

    // 🌟 2. 真正实例化接触卡片，并存入后台管家！
    auto contactCard = std::make_shared<ContactCard>(
        type.toStdString(),
        slavePart->pid,
        masterPart->pid,
        m_simSetupUI.contactFs->value(),
        m_simSetupUI.contactFd->value()
    );
    m_deck.addCard(contactCard);

    // 🌟 3. 在 UI 上显示，并绑定真实的卡片指针
    QString summary = QString("[接触] %1 | 主面: %2 | 从面: %3").arg(type).arg(master).arg(slave);
    QListWidgetItem* item = new QListWidgetItem(summary);

    // 绑定真实数据，导出时才能找到它
    item->setData(Qt::UserRole, QVariant::fromValue(reinterpret_cast<quintptr>(contactCard.get())));
    item->setData(Qt::UserRole + 1, "CARD");

    m_simSetupUI.setupSummaryList->addItem(item);
}

void MainWindow::handleAddIC() {
    QString target = m_simSetupUI.icEntitySelector->currentText();
    if (target.isEmpty()) return;

    auto part = getOrCreatePart(target); // 获取实体对应的 Part 指针

    // 1. 实例化初始速度卡片
    auto icCard = std::make_shared<InitialVelocityGenerationCard>(
        part->pid,
        m_simSetupUI.icVx->value(),
        m_simSetupUI.icVy->value(),
        m_simSetupUI.icVz->value()
    );
    m_deck.addCard(icCard);

    // 2. 生成右侧列表文字
    QString summary = QString("[初始速度] 实体: %1 | V=(%2, %3, %4)")
        .arg(target).arg(m_simSetupUI.icVx->value()).arg(m_simSetupUI.icVy->value()).arg(m_simSetupUI.icVz->value());

    // 3. 【核心修改】将卡片指针地址绑定到列表项
    QListWidgetItem* item = new QListWidgetItem(summary);
    item->setData(Qt::UserRole, QVariant::fromValue(reinterpret_cast<quintptr>(icCard.get())));
    item->setData(Qt::UserRole + 1, "CARD");

    m_simSetupUI.setupSummaryList->addItem(item);
}

void MainWindow::handleAddSection() {
    QString target = m_simSetupUI.sectionEntitySelector->currentText();
    if (target.isEmpty()) return;

    // 1. 获取目标实体的底层指针
    auto part = getOrCreatePart(target);

    // 2. 获取截面类型和算法
    QString type = m_simSetupUI.sectionTypeSelector->currentText().remove("*SECTION_");
    QString elformStr = m_simSetupUI.sectionElformSelector->currentText();
    int elform = elformStr.split(" ").first().toInt();

    // 3. 实例化截面卡片
    auto secCard = std::make_shared<SectionCard>(part->pid, type.toStdString(), elform);
    m_deck.addCard(secCard);
    part->secid = part->pid; // 绑定实体

    // 4. 生成右侧列表文字
    QString summary = QString("[截面] 实体: %1 | 类型: %2 | 算法: ELFORM=%3").arg(target).arg(type).arg(elform);

    // 5. 【核心修改】将卡片指针地址绑定到列表项
    QListWidgetItem* item = new QListWidgetItem(summary);
    item->setData(Qt::UserRole, QVariant::fromValue(reinterpret_cast<quintptr>(secCard.get())));
    item->setData(Qt::UserRole + 1, "CARD");

    m_simSetupUI.setupSummaryList->addItem(item);
}

void MainWindow::handleClearSummary() {
    // 1. 清空前端显示列表
    m_simSetupUI.setupSummaryList->clear();

    // 2. 清空实体指针映射字典
    m_entityParts.clear();

    m_deck.clear();
}

// 在 mainwindow.cpp 空白处添加
std::shared_ptr<PartCard> MainWindow::getOrCreatePart(const QString& entityName) {
    if (!m_entityParts.contains(entityName)) {
        
        // 直接使用当前已分配实体的总数 + 1 作为全局唯一 PID。
        
        int pid = m_entityParts.size() + 1;

        auto part = std::make_shared<PartCard>(pid, entityName.toStdString());
        m_deck.addCard(part);
        m_entityParts[entityName] = part;
    }
    return m_entityParts[entityName];
}

void MainWindow::loadMaterialPresetsFromJson() {
    m_simSetupUI.presetSelector->clear();
    m_simSetupUI.presetSelector->addItem("None (自定义)");

    // 🌟 智能路径查找逻辑：适配不同的运行环境和编译目录
    QString jsonPath;
    QString relativePath = "/src/core/materials/material_repo.json";

    // 1. 尝试直接在当前工作目录下寻找 (适用于在工程根目录直接运行)
    QFile file("." + relativePath);

    if (!file.exists()) {
        // 2. 尝试在 exe 所在目录往上一层找 (适用于常见的 build/ 目录)
        file.setFileName(QCoreApplication::applicationDirPath() + "/.." + relativePath);
    }
    if (!file.exists()) {
        // 3. 尝试在 exe 所在目录往上两层找 (适用于 build/debug/ 目录)
        file.setFileName(QCoreApplication::applicationDirPath() + "/../.." + relativePath);
    }

    // 如果还是打不开，报错并在命令行提示
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        logCommand("System Error", "未找到 material_repo.json，请检查 src/core/materials/ 路径！材料预设功能暂时禁用。");
        return;
    }

    // 读取并解析 JSON 文件
    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        logCommand("System Error", "material_repo.json 格式错误，无法解析！");
        return;
    }

    QJsonArray presetsArray = doc.object()["presets"].toArray();

    // 遍历 JSON 数组，将其存入字典并添加到下拉框
    for (int i = 0; i < presetsArray.size(); ++i) {
        QJsonObject obj = presetsArray[i].toObject();
        QString name = obj["name"].toString();

        MaterialPreset preset;
        preset.matType = obj["matType"].toString();
        preset.eosType = obj["eosType"].toString();
        preset.source = obj["source"].toString();

        QJsonObject paramsObj = obj["params"].toObject();
        for (auto it = paramsObj.begin(); it != paramsObj.end(); ++it) {
            preset.params[it.key()] = it.value().toDouble();
        }

        m_materialPresets[name] = preset;
        m_simSetupUI.presetSelector->addItem(name);
    }

    logCommand("System", QString("成功从 material_repo.json 加载 %1 种材料预设。").arg(m_materialPresets.size()));
}

void MainWindow::handlePresetChanged(const QString& presetName) {
    if (presetName == "None (自定义)" || !m_materialPresets.contains(presetName)) {
        return;
    }

    // 从我们解析好的内存字典中拿出预设数据
    const MaterialPreset& preset = m_materialPresets[presetName];

    // 1. 自动切换材料与EOS下拉框（屏蔽信号防死循环）
    m_simSetupUI.materialSelector->blockSignals(true);
    m_simSetupUI.eosSelector->blockSignals(true);

    m_simSetupUI.materialSelector->setCurrentText(preset.matType);
    m_simSetupUI.eosSelector->setCurrentText(preset.eosType);

    m_simSetupUI.materialSelector->blockSignals(false);
    m_simSetupUI.eosSelector->blockSignals(false);

    // 2. 强制触发一次界面重绘，生成对应材料的几十个空白输入框
    handleMaterialTypeChanged(preset.matType);

    // 3. ✨ 核心：无脑遍历匹配！根据 JSON 里的 Key，直接把值塞进对应 UI 输入框
    for (auto it = preset.params.begin(); it != preset.params.end(); ++it) {
        if (m_simSetupUI.currentMatInputs.contains(it.key())) {
            m_simSetupUI.currentMatInputs[it.key()]->setValue(it.value());
        }
    }

    logCommand("Material Preset", QString("已加载预设 [%1]. 数据来源文献: %2").arg(presetName).arg(preset.source));
}

void MainWindow::handleAddSensor() {
    QString target = m_simSetupUI.sensorEntitySelector->currentText();
    if (target.isEmpty()) return;

    MeshEntity* entity = m_repository.getMutableEntity(target);
    if (!entity || entity->nodes.size() < 2) return;

    // 获取当前模式是否为阵列
    bool isArray = m_simSetupUI.sensorModeSelector->currentText().contains("Line Array");

    // 提取通用起点
    double sx = m_simSetupUI.sensorStartX->value();
    double sy = m_simSetupUI.sensorStartY->value();
    double sz = m_simSetupUI.sensorStartZ->value();

    if (!isArray) {
        // -----------------------------
        // 分支 A: 单点测点逻辑
        // -----------------------------
        int closestIdx = -1;
        double min_dist = 1e9;

        for (size_t i = 1; i < entity->nodes.size(); ++i) {
            double dx = entity->nodes[i].pos.x() - sx;
            double dy = entity->nodes[i].pos.y() - sy;
            double dz = entity->nodes[i].pos.z() - sz;
            double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (dist < min_dist) { min_dist = dist; closestIdx = i; }
        }

        if (closestIdx != -1 && !m_sensorNodes[target].contains(closestIdx)) {
            m_sensorNodes[target].append(closestIdx);

            QString summary = QString("[测点] 实体:%1 | 单点:(%2,%3,%4) -> 吸附误差:%5")
                .arg(target).arg(sx).arg(sy).arg(sz).arg(min_dist, 0, 'f', 4);
            if (m_simSetupUI.setupSummaryList) m_simSetupUI.setupSummaryList->addItem(summary);
            logCommand("Sensor", summary);
        }
    }
    else {
        // -----------------------------
        // 分支 B: 线性阵列测点逻辑
        // -----------------------------
        double ex = m_simSetupUI.sensorEndX->value();
        double ey = m_simSetupUI.sensorEndY->value();
        double ez = m_simSetupUI.sensorEndZ->value();
        int count = m_simSetupUI.sensorNumPoints->value();
        int successCount = 0;

        for (int k = 0; k < count; ++k) {
            double t = static_cast<double>(k) / (count - 1);
            double tx = sx + t * (ex - sx);
            double ty = sy + t * (ey - sy);
            double tz = sz + t * (ez - sz);

            int closestIdx = -1;
            double min_dist = 1e9;
            for (size_t i = 1; i < entity->nodes.size(); ++i) {
                double dx = entity->nodes[i].pos.x() - tx;
                double dy = entity->nodes[i].pos.y() - ty;
                double dz = entity->nodes[i].pos.z() - tz;
                double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (dist < min_dist) { min_dist = dist; closestIdx = i; }
            }

            if (closestIdx != -1 && !m_sensorNodes[target].contains(closestIdx)) {
                m_sensorNodes[target].append(closestIdx);
                successCount++;
            }
        }

        QString summary = QString("[阵列测点] 实体:%1 | %2个点 | 从(%3,%4,%5)到(%6,%7,%8)")
            .arg(target).arg(successCount).arg(sx).arg(sy).arg(sz).arg(ex).arg(ey).arg(ez);
        if (m_simSetupUI.setupSummaryList) m_simSetupUI.setupSummaryList->addItem(summary);
        logCommand("Sensor", summary);
    }
}

/**
 * @brief 初始化“求解与仿真试验方案设计”工作区界面 (自动化综合控制台)
 * * @details 该函数负责构建后处理模块的 UI 布局，主要包含三个核心部分：
 * 1. 单次求解与控制台监控面板。
 * 2. 基于实体级别的网格收敛性智能批处理与研判面板（左侧占比 60%）。
 * 3. 基于升降法的起爆阈值寻优批处理及序列预览面板（右侧占比 40%）。
 * 注：自动化批处理界面的表格控件被设置为垂直方向自动扩展，以充分利用屏幕高度。
 */
void MainWindow::setupPostProcessUI() {
    // 1. 清理遗留布局，防止多次调用时发生控件重叠与内存泄漏
    if (postProcessWidget->layout() != nullptr) {
        QWidget().setLayout(postProcessWidget->layout());
    }

    QVBoxLayout* mainLayout = new QVBoxLayout(postProcessWidget);
    solveTaskTabs = new QTabWidget(postProcessWidget);

    // 2. 设置 TabWidget 样式，覆盖父级控件可能的隐藏属性 (width/height: 0)
    solveTaskTabs->setStyleSheet(
        "QTabBar::tab {"
        "  min-width: 180px; min-height: 35px; "
        "  padding: 5px; margin: 2px; "
        "  font-weight: bold; font-size: 13px; color: #111111; "
        "  background-color: #E0E0E0; border: 1px solid #A0A0A0; border-radius: 4px; "
        "}"
        "QTabBar::tab:selected { background-color: #FFFFFF; color: #0055A4; border-bottom: 2px solid #0055A4; }"
    );
    mainLayout->addWidget(solveTaskTabs);

    // =========================================================
    // Tab 1: 单次工况求解与监控面板
    // =========================================================
    QWidget* singleRunWidget = new QWidget();
    QVBoxLayout* singleLayout = new QVBoxLayout(singleRunWidget);

    // 1.1 求解器提交参数设置区
    QGroupBox* submitGroup = new QGroupBox("单次工况提交 (Single Job)");
    QFormLayout* submitLayout = new QFormLayout(submitGroup);

    kFilePathEdit = new QLineEdit();
    QPushButton* btnBrowseK = new QPushButton("浏览...");
    QHBoxLayout* kLayout = new QHBoxLayout();
    kLayout->addWidget(kFilePathEdit); kLayout->addWidget(btnBrowseK);
    submitLayout->addRow("控制文件 (.k):", kLayout);

    solverPathEdit = new QLineEdit();
    solverPathEdit->setPlaceholderText("D:\Program Files\ANSYS Inc\v241\ansys\bin\winx64\lsdyna_sp.exe");
    QPushButton* btnBrowseSolver = new QPushButton("浏览...");
    QHBoxLayout* solverLayout = new QHBoxLayout();
    solverLayout->addWidget(solverPathEdit); solverLayout->addWidget(btnBrowseSolver);
    submitLayout->addRow("求解器路径 (EXE):", solverLayout);

    cpuCoresSpin = new QSpinBox();
    cpuCoresSpin->setRange(1, 128); cpuCoresSpin->setValue(4);
    submitLayout->addRow("计算核心数 (NCPU):", cpuCoresSpin);

    btnRunSolver = new QPushButton("▶ 开始单次求解");
    btnRunSolver->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold; min-height: 35px;");
    btnStopSolver = new QPushButton("■ 终止计算");
    btnStopSolver->setStyleSheet("min-height: 35px;");
    btnStopSolver->setEnabled(false);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addWidget(btnRunSolver); btnLayout->addWidget(btnStopSolver);
    submitLayout->addRow("", btnLayout);
    singleLayout->addWidget(submitGroup);

    // 1.2 求解器进程输出监控区
    QGroupBox* monitorGroup = new QGroupBox("计算监控台 (Console)");
    QVBoxLayout* monitorLayout = new QVBoxLayout(monitorGroup);
    solverConsole = new QTextEdit();
    solverConsole->setReadOnly(true);
    solverConsole->setStyleSheet("background-color: #1E1E1E; color: #00FF00; font-family: Consolas;");
    monitorLayout->addWidget(solverConsole);
    singleLayout->addWidget(monitorGroup, 1);

    solveTaskTabs->addTab(singleRunWidget, "单次求解与监控");

    // =========================================================
    // Tab 2: 自动化批处理与分析面板 (左右自适应铺满布局)
    // =========================================================
    QScrollArea* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QWidget* batchWidget = new QWidget();
    QVBoxLayout* batchMainLayout = new QVBoxLayout(batchWidget);

    // 创建水平分割布局，实现 6:4 黄金比例排布
    QHBoxLayout* hSplitLayout = new QHBoxLayout();

    // ---------------------------------------------------------
    // 左半区 (模块 A): 实体级网格收敛性智能分析
    // ---------------------------------------------------------
    QGroupBox* meshConvergenceGroup = new QGroupBox("【任务 A】实体级网格收敛性智能分析");
    QVBoxLayout* meshConvLayout = new QVBoxLayout(meshConvergenceGroup);

    // A.1 实体级网格独立控制表
    QHBoxLayout* tableHeaderLayout = new QHBoxLayout();
    tableHeaderLayout->addWidget(new QLabel("当前物理实体网格控制参数："));
    QPushButton* btnRefreshTable = new QPushButton("🔄 刷新读取实体");
    btnRefreshTable->setStyleSheet("background-color: #f0f0f0; font-weight: bold; padding: 4px; min-height: 25px;");
    tableHeaderLayout->addStretch();
    tableHeaderLayout->addWidget(btnRefreshTable);
    meshConvLayout->addLayout(tableHeaderLayout);

    tableMeshSettings = new QTableWidget(0, 4);
    tableMeshSettings->setHorizontalHeaderLabels({ "物理实体名称", "分类语义", "基础网格尺寸(mm)", "迭代缩放因子" });
    tableMeshSettings->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tableMeshSettings->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding); // 允许垂直方向充分扩展
    tableMeshSettings->setMinimumHeight(150);
    meshConvLayout->addWidget(tableMeshSettings);

    // A.2 全局迭代参数控制区
    QFormLayout* globalMeshLayout = new QFormLayout();
    spinMeshSteps = new QSpinBox(); spinMeshSteps->setValue(3); spinMeshSteps->setRange(2, 10);
    globalMeshLayout->addRow("全局迭代细化总次数:", spinMeshSteps);

    comboTargetMetric = new QComboBox();
    comboTargetMetric->addItems({ "靶板总内能 (Internal Energy)", "系统总动能 (Kinetic Energy)", "弹体质心剩余速度" });
    globalMeshLayout->addRow("收敛判定全局指标:", comboTargetMetric);

    spinTolerance = new QDoubleSpinBox();
    spinTolerance->setRange(0.1, 20.0); spinTolerance->setValue(5.0); spinTolerance->setSuffix(" %");
    globalMeshLayout->addRow("容差阈值(收敛标准):", spinTolerance);
    meshConvLayout->addLayout(globalMeshLayout);

    // A.3 收敛性批处理执行按钮区
    QHBoxLayout* meshBtnLayout = new QHBoxLayout();
    QPushButton* btnGenerateMeshBatch = new QPushButton("① 一键生成网格收敛 .bat 脚本");
    btnGenerateMeshBatch->setStyleSheet("background-color: #008CBA; color: white; font-weight: bold; min-height: 35px;");
    QPushButton* btnAnalyzeConvergence = new QPushButton("② 读取结果生成收敛报告");
    btnAnalyzeConvergence->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold; min-height: 35px;");
    meshBtnLayout->addWidget(btnGenerateMeshBatch);
    meshBtnLayout->addWidget(btnAnalyzeConvergence);
    meshConvLayout->addLayout(meshBtnLayout);

    // 赋予左侧布局 6 的宽度伸缩因子
    hSplitLayout->addWidget(meshConvergenceGroup, 6);

    // ---------------------------------------------------------
    // 右半区 (模块 B): 起爆阈值升降法寻优参数设置
    // ---------------------------------------------------------
    QGroupBox* velSetupGroup = new QGroupBox("【任务 B】起爆阈值升降法寻优");
    QVBoxLayout* velContainerLayout = new QVBoxLayout(velSetupGroup);

    // B.1 速度序列预览表
    QHBoxLayout* velHeaderLayout = new QHBoxLayout();
    velHeaderLayout->addWidget(new QLabel("速度梯度测试工况序列预览："));
    QPushButton* btnPreviewVel = new QPushButton("🔄 预览序列");
    btnPreviewVel->setStyleSheet("background-color: #f0f0f0; font-weight: bold; padding: 4px; min-height: 25px;");
    velHeaderLayout->addStretch();
    velHeaderLayout->addWidget(btnPreviewVel);
    velContainerLayout->addLayout(velHeaderLayout);

    tableVelocitySequence = new QTableWidget(0, 3);
    tableVelocitySequence->setHorizontalHeaderLabels({ "工况序号", "撞击速度 (m/s)", "控制文件名" });
    tableVelocitySequence->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tableVelocitySequence->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding); // 允许垂直方向充分扩展
    tableVelocitySequence->setMinimumHeight(150);
    velContainerLayout->addWidget(tableVelocitySequence);

    // B.2 速度寻优参数设置区
    QFormLayout* velLayout = new QFormLayout();
    spinStartVelocity = new QDoubleSpinBox(); spinStartVelocity->setRange(0, 5000); spinStartVelocity->setValue(1000.0);
    spinEndVelocity = new QDoubleSpinBox(); spinEndVelocity->setRange(0, 5000); spinEndVelocity->setValue(2000.0);
    spinVelocityStep = new QDoubleSpinBox(); spinVelocityStep->setRange(10, 500); spinVelocityStep->setValue(100.0);
    velLayout->addRow("起始撞击速度 (m/s):", spinStartVelocity);
    velLayout->addRow("终止撞击速度 (m/s):", spinEndVelocity);
    velLayout->addRow("速度梯度步长 (m/s):", spinVelocityStep);
    velContainerLayout->addLayout(velLayout);

    // B.3 速度批处理执行按钮
    QPushButton* btnGenerateVelBatch = new QPushButton("一键生成速度梯度 .bat 脚本");
    btnGenerateVelBatch->setStyleSheet("background-color: #008CBA; color: white; font-weight: bold; min-height: 35px;");
    velContainerLayout->addWidget(btnGenerateVelBatch);

    // 赋予右侧布局 4 的宽度伸缩因子
    hSplitLayout->addWidget(velSetupGroup, 4);

    // 将水平分割布局加入批处理主容器
    batchMainLayout->addLayout(hSplitLayout);

    scrollArea->setWidget(batchWidget);
    solveTaskTabs->addTab(scrollArea, "自动化批处理与分析");

    // =========================================================
    // 全局通用模块: 结果目录与后处理入口
    // =========================================================
    QGroupBox* postGroup = new QGroupBox("结果目录与后处理入口");
    QHBoxLayout* postLayout = new QHBoxLayout(postGroup);

    btnOpenFolder = new QPushButton("📂 打开当前结果目录");
    btnOpenFolder->setStyleSheet("min-height: 35px;");
    btnLaunchD3plot = new QPushButton("📊 切换至三维曲线分析工作区");
    btnLaunchD3plot->setStyleSheet("min-height: 35px;");
    postLayout->addWidget(btnOpenFolder);
    postLayout->addWidget(btnLaunchD3plot);

    mainLayout->addWidget(postGroup);

    // =========================================================
    // 统一信号与槽绑定
    // =========================================================

    // 常规求解与后处理信号
    connect(btnBrowseK, &QPushButton::clicked, this, &MainWindow::browseKFile);
    connect(btnBrowseSolver, &QPushButton::clicked, this, &MainWindow::browseSolver);
    connect(btnRunSolver, &QPushButton::clicked, this, &MainWindow::startCalculation);
    connect(btnStopSolver, &QPushButton::clicked, this, &MainWindow::stopCalculation);
    connect(btnOpenFolder, &QPushButton::clicked, this, &MainWindow::openResultFolder);
    connect(btnLaunchD3plot, &QPushButton::clicked, this, &MainWindow::launchPostProcessor);

    // 自动化批处理专属信号
    connect(btnRefreshTable, &QPushButton::clicked, this, &MainWindow::handleRefreshEntityTable);
    connect(btnPreviewVel, &QPushButton::clicked, this, &MainWindow::handlePreviewVelocitySequence);
    connect(btnGenerateMeshBatch, &QPushButton::clicked, this, &MainWindow::handleGenerateMeshConvergenceBatch);
    connect(btnGenerateVelBatch, &QPushButton::clicked, this, &MainWindow::handleGenerateVelocityThresholdBatch);
    connect(btnAnalyzeConvergence, &QPushButton::clicked, this, &MainWindow::handleAnalyzeConvergence);
}


// ==========================================
// 逻辑实现：求解器控制与日志读取
// ==========================================
void MainWindow::startCalculation() {
    QString kFile = kFilePathEdit->text();
    QString solver = solverPathEdit->text();

    if (kFile.isEmpty() || solver.isEmpty()) {
        solverConsole->append("<b><font color='red'>[错误] 请先选择 .k 文件和求解器路径！</font></b>");
        return;
    }

    // 拼接 LS-DYNA 命令行参数： i=xxx.k ncpu=4 memory=100m
    QStringList arguments;
    arguments << QString("i=%1").arg(kFile);
    arguments << QString("ncpu=%1").arg(cpuCoresSpin->value());
    arguments << "memory=200m"; // 预设内存，可根据需求提取为UI输入

    // 设置工作目录为 .k 文件所在的目录，这样 d3plot 就会生成在那里
    QFileInfo kFileInfo(kFile);
    m_solverProcess->setWorkingDirectory(kFileInfo.absolutePath());

    solverConsole->clear();
    solverConsole->append(QString("<b><font color='yellow'>[系统] 正在启动求解器...</font></b>"));
    solverConsole->append(QString("执行命令: %1 %2").arg(solver).arg(arguments.join(" ")));
    solverConsole->append("--------------------------------------------------");

    // 启动进程
    m_solverProcess->start(solver, arguments);

    // 更新界面状态
    btnRunSolver->setEnabled(false);
    btnStopSolver->setEnabled(true);
}

void MainWindow::readSolverOutput() {
    // 读取来自 LS-DYNA 的控制台输出
    QByteArray output = m_solverProcess->readAllStandardOutput();
    QByteArray error = m_solverProcess->readAllStandardError();

    if (!output.isEmpty()) {
        solverConsole->insertPlainText(QString::fromLocal8Bit(output));
    }
    if (!error.isEmpty()) {
        solverConsole->insertPlainText(QString::fromLocal8Bit(error));
    }

    // 自动滚动到最底部
    QScrollBar* sb = solverConsole->verticalScrollBar();
    sb->setValue(sb->maximum());
}

void MainWindow::stopCalculation() {
    if (m_solverProcess->state() == QProcess::Running) {
        m_solverProcess->kill(); // 强制终止进程
        solverConsole->append("<b><font color='red'>[系统] 收到用户指令，计算已强行终止！</font></b>");
    }
}

void MainWindow::handleSolverFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    btnRunSolver->setEnabled(true);
    btnStopSolver->setEnabled(false);

    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        solverConsole->append("--------------------------------------------------");
        solverConsole->append("<b><font color='cyan'>[系统] LS-DYNA 求解正常完成 (Normal Termination)！</font></b>");
    }
    else {
        solverConsole->append("--------------------------------------------------");
        solverConsole->append("<b><font color='red'>[系统] 求解异常退出或被终止 (Error Termination)。</font></b>");
    }
}

// ==========================================
// 后处理界面：按钮槽函数实现
// ==========================================

// 1. 浏览选择 K 文件
void MainWindow::browseKFile() {
    // 弹出文件选择框，只过滤 .k 或 .key 文件
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "选择 LS-DYNA 控制文件",
        m_workingDirectory, // 默认打开之前的工作目录
        "LS-DYNA 文件 (*.k *.key);;所有文件 (*.*)"
    );

    if (!fileName.isEmpty()) {
        kFilePathEdit->setText(fileName);
        // 同步更新系统的工作目录为该文件所在的目录
        m_workingDirectory = QFileInfo(fileName).absolutePath();
        logCommand("System", "已加载控制文件: " + fileName);
    }
}

// 2. 浏览选择求解器
void MainWindow::browseSolver() {
    // 弹出文件选择框，只过滤 .exe 可执行文件
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "选择 LS-DYNA 求解器程序",
        "C:/", // 默认从 C 盘开始找
        "可执行文件 (*.exe)"
    );

    if (!fileName.isEmpty()) {
        solverPathEdit->setText(fileName);
        logCommand("System", "已手动配置求解器路径: " + fileName);
    }
}

// 3. 一键打开结果文件夹
void MainWindow::openResultFolder() {
    QString kFile = kFilePathEdit->text();
    if (kFile.isEmpty()) {
        // 如果用户还没选择文件，弹出警告并记录日志
        logCommand("Warning", "请先选择一个 .k 文件！");
        return;
    }

    // 提取 .k 文件所在的纯目录路径
    QString dirPath = QFileInfo(kFile).absolutePath();

    // 调用操作系统的资源管理器打开这个路径
    QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
    logCommand("System", "已打开结果所在目录: " + dirPath);
}

// 4. 加载 d3plot 后处理程序 (占位接口)
void MainWindow::launchPostProcessor() {
    // 目前处于开发阶段，使用信息弹窗占位
    QMessageBox::information(
        this,
        "后处理接口",
        "d3plot 可视化接口模块正在开发中...\n\n后续可以在这里唤起官方的 LS-PrePost 软件，或者集成我们自己的 OpenGL 后处理渲染器！"
    );
}

// ==========================================
// 右键菜单与预览功能 (全新增)
// ==========================================
void MainWindow::showSummaryContextMenu(const QPoint& pos) {
    QListWidgetItem* item = m_simSetupUI.setupSummaryList->itemAt(pos);
    if (!item) return;

    QMenu menu(this);
    QAction* viewAct = menu.addAction("查看关键字格式 (View K-File)");
    QAction* delAct = menu.addAction("删除该项 (Delete)");

    QAction* selected = menu.exec(m_simSetupUI.setupSummaryList->mapToGlobal(pos));
    if (!selected) return;

    // 提取隐藏在 item 中的卡片指针和类型
    QString type = item->data(Qt::UserRole + 1).toString();
    quintptr ptrVal = item->data(Qt::UserRole).value<quintptr>();
    KeywordCard* cardPtr = reinterpret_cast<KeywordCard*>(ptrVal);

    if (selected == viewAct) {
        if (type == "CARD" && cardPtr) {

            QDialog dialog(this);
            dialog.setWindowTitle("LS-DYNA 关键字预览");
            dialog.resize(1200, 800);

            QVBoxLayout layout(&dialog);
            QTextEdit textEdit;
            textEdit.setReadOnly(true);
            textEdit.setStyleSheet("background-color: #1E1E1E; color: #D4D4D4; font-family: Consolas; font-size: 11pt;");

            // 解析主卡片
            QString kText = QString::fromStdString(cardPtr->to_string());

            // 如果有绑定的 EOS 附加卡片，拼接在一起显示
            if (item->data(Qt::UserRole + 2).isValid()) {
                quintptr eosPtrVal = item->data(Qt::UserRole + 2).value<quintptr>();
                KeywordCard* eosCardPtr = reinterpret_cast<KeywordCard*>(eosPtrVal);
                if (eosCardPtr) {
                    kText += "\n" + QString::fromStdString(eosCardPtr->to_string());
                }
            }

            textEdit.setPlainText(kText);
            layout.addWidget(&textEdit);
            dialog.exec();
        }
        else {
            QMessageBox::information(this, "提示", "该项暂未生成标准卡片(如测点、接触等将在导出时动态生成)。");
        }
    }
    else if (selected == delAct) {
        if (type == "CARD" && cardPtr) {
 
            m_deck.removeCard(cardPtr);

            if (cardPtr == m_globalControlCard.get()) {
                m_globalControlCard = nullptr;
            }

            // 如果有绑定的 EOS 卡片，一并注销
            if (item->data(Qt::UserRole + 2).isValid()) {
                quintptr eosPtrVal = item->data(Qt::UserRole + 2).value<quintptr>();
                KeywordCard* eosCardPtr = reinterpret_cast<KeywordCard*>(eosPtrVal);
                if (eosCardPtr) {
                    m_deck.removeCard(eosCardPtr);
                }
            }
        }
        // 清理 UI 列表项
        delete item;
        logCommand("System", "已撤销指定的仿真参数配置。");
    }
}

void MainWindow::handleAddGlobalControl() {
    // 1. 获取界面上的值
    double endtim = m_simSetupUI.endtimeInput->value();
    double tssfac = m_simSetupUI.tssfacInput->value();
    double dt = m_simSetupUI.d3plotFreqInput->value();

    // 2. 【核心防漏水】如果列表里已经有全局控制了，先删掉旧的（保证控制卡片的唯一性）
    for (int i = m_simSetupUI.setupSummaryList->count() - 1; i >= 0; --i) {
        QListWidgetItem* existingItem = m_simSetupUI.setupSummaryList->item(i);
        if (existingItem->text().contains("[全局控制]")) {
            delete m_simSetupUI.setupSummaryList->takeItem(i);
        }
    }

    // 3. 实例化全局控制卡片（单独存在 m_globalControlCard 里，不放入 m_deck，以便排在 K 文件顶部）
    m_globalControlCard = std::make_shared<GlobalControlCard>(endtim, tssfac, dt);

    // 4. 生成右侧列表文字
    QString summary = QString("[全局控制] 结束时间:%1 | 步长缩放:%2 | D3PLOT:%3")
        .arg(endtim).arg(tssfac).arg(dt);

    // 5. 将卡片指针地址悄悄绑定到列表项 (UserRole隐藏域)，和材料卡片逻辑完全一致
    QListWidgetItem* item = new QListWidgetItem(summary);
    item->setData(Qt::UserRole, QVariant::fromValue(reinterpret_cast<quintptr>(m_globalControlCard.get())));
    item->setData(Qt::UserRole + 1, "CARD");

    // 强制插入到列表的最顶端（第 0 行）
    m_simSetupUI.setupSummaryList->insertItem(0, item);
    logCommand("System", "全局控制参数已生成并自动置顶。");
}

// ==========================================
// 实体树：查看实体对应的 Part 关键字卡片
// ==========================================
void MainWindow::handleViewEntityKeyword(const QString& entityName) {
    // 1. 尝试获取该实体对应的 PartCard
    // 注意：这里我们使用 contains 检查，而不是直接用 getOrCreatePart
    // 因为用户可能只是建了模型，还没给它分配材质等导致没触发生成 Part
    std::shared_ptr<PartCard> partCard = nullptr;
    if (m_entityParts.contains(entityName)) {
        partCard = m_entityParts[entityName];
    }
    else {
        // 如果还没有 Part 卡片（例如刚生成几何，还没点生成材料/截面等），则临时生成一个展示
        partCard = getOrCreatePart(entityName);
    }

    if (partCard) {
        
        QDialog dialog(this);
        dialog.setWindowTitle(QString("LS-DYNA 关键字预览 - %1").arg(entityName));
        dialog.resize(600, 400);

        QVBoxLayout layout(&dialog);
        QTextEdit textEdit;
        textEdit.setReadOnly(true);
        textEdit.setStyleSheet("background-color: #1E1E1E; color: #D4D4D4; font-family: Consolas; font-size: 11pt;");

        // 3. 解析主卡片 (*PART)
        QString kText = QString::fromStdString(partCard->to_string());

        // 提示信息
        kText.prepend(QString("$ 实体 [%1] 的 Part 卡片\n").arg(entityName));

        textEdit.setPlainText(kText);
        layout.addWidget(&textEdit);
        dialog.exec();
    }
    else {
        QMessageBox::warning(this, "警告", "无法获取该实体的关键字卡片。");
    }
}

// ==========================================
// 处理实体对称边界条件添加
// ==========================================
void MainWindow::handleApplySymmetryBoundary(const QString& entityName, char axis) {
    const MeshEntity* entity = m_repository.getEntity(entityName);
    if (!entity) return;

    // 1. 验证一下是否能抓到节点（提供即时反馈）
    std::set<int> localNodes = getSymmetryPlaneNodes(*entity, axis);
    if (localNodes.empty()) {
        QMessageBox::warning(this, "警告", QString("未能在 [%1] 的 %2 轴截面上找到任何节点！").arg(entityName).arg(axis));
        return;
    }

    // 2. 🌟 核心改变：不再立刻生成卡片塞进 m_deck！而是把规则记录到小本本上
    // 检查是否已经添加过同样的规则，防止重复
    bool exists = false;
    for (const auto& rule : m_symmetryRules) {
        if (rule.entityName == entityName && rule.axis == axis) {
            exists = true; break;
        }
    }

    if (!exists) {
        m_symmetryRules.push_back({ entityName, axis });
    }

    QMessageBox::information(this, "成功",
        QString("已记录对 [%1] 施加 %2 轴对称的规则（探测到 %3 个节点）。\n\n"
            "🌟 为防止实体增删导致 ID 错位，边界卡片将在您最终导出 K 文件时动态结算生成！")
        .arg(entityName).arg(axis).arg(localNodes.size()));
}

/**
 * @brief 基于新尺寸重构指定实体的网格 (支持异步阻塞等待)
 * * @details 该函数读取实体原有的几何参数与物理语义，利用对应的几何生成器重新划分网格。
 * 通过引入局部事件循环 (QEventLoop) 机制，实现对底层异步网格生成过程的同步等待。
 * 此举确保了批处理任务在当前网格生成完毕并成功入库后，再继续向下执行，
 * 从而避免了批量生成时因底层 QProcess 异步调用过快而导致的竞态条件 (Race Condition)。
 * * @param entity 指向需要重构网格的实体指针 (MeshEntity*)
 * @param newMeshSize 目标细化/粗化后的新网格尺寸 (mm)
 */
void MainWindow::remeshEntityWithNewSize(MeshEntity* entity, double newMeshSize) {
    if (!entity || entity->geoParams.isEmpty()) return;

    // 1. 备份该实体的分类语义与初始几何参数字典
    QString name = entity->name;
    QString type = entity->type;
    QString category = entity->category;
    QMap<QString, double> p = entity->geoParams;

    // 2. 创建局部事件循环，用于挂起当前作用域，等待底层异步网格生成完毕
    QEventLoop loop;
    QMetaObject::Connection conn1 = connect(m_meshManager, &MeshManager::meshReady, &loop, &QEventLoop::quit);
    QMetaObject::Connection conn2 = connect(m_meshManager, &MeshManager::errorOccurred, &loop, &QEventLoop::quit);

    bool isValid = true;

    // 3. 根据实体几何类型装配相应的生成器，并注入更新后的网格尺寸参数
    if (type == "Cube") {
        CubeGenerator gen;
        gen.setParameters(p["lx"], p["ly"], p["lz"], newMeshSize, p["cx"], p["cy"], p["cz"]);
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "Cylinder") {
        CylinderGenerator gen;
        gen.setParameters(p["r"], newMeshSize, p["h"], p["cx"], p["cy"], p["cz"]);
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "CylindricalShell") {
        CylindricalShellGenerator gen;
        gen.setParameters(p["r"], p["h"], p["lid"], p["wall"], newMeshSize, p["cx"], p["cy"], p["cz"]);
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "OpenCylindricalShell") {
        OpenCylindricalShellGenerator gen;
        gen.setParameters(p["r"], p["wall"], p["h_base"], p["h_wall"], newMeshSize, p["cx"], p["cy"], p["cz"]);
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "Sphere") {
        SphereGenerator gen;
        gen.setParameters(p["r"], newMeshSize, p["cx"], p["cy"], p["cz"]);
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "Hemisphere") {
        HemisphereGenerator gen;
        gen.setParameters(p["r"], newMeshSize, p["cx"], p["cy"], p["cz"]);
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "Frustum") {
        FrustumGenerator gen;
        gen.setParameters(p["rb"], p["rt"], p["h"], newMeshSize, p["cx"], p["cy"], p["cz"]);
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "HalfCylinder") {
        HalfCylinderGenerator gen;
        gen.setParameters(p["r"], p["h"], newMeshSize, p["cx"], p["cy"], p["cz"]);
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "TriangularPrism") {
        TriangularPrismGenerator gen;
        gen.setParameters(p["r"], p["h"], newMeshSize, p["cx"], p["cy"], p["cz"]);
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "PentagonalPrism") {
        PentagonalPrismGenerator gen;
        gen.setParameters(p["r"], p["h"], newMeshSize, p["cx"], p["cy"], p["cz"]);
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "HexagonalPrism") {
        HexagonalPrismGenerator gen;
        gen.setParameters(p["r"], p["h"], newMeshSize, p["cx"], p["cy"], p["cz"]);
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "HalfCylindricalShell") {
        HalfCylindricalShellGenerator gen;
        gen.setParameters(p["r"], p["h"], p["lid"], p["wall"], newMeshSize, p["cx"], p["cy"], p["cz"]);
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "Fragment Simulating Projectile") {
        FSPGenerator gen;
        gen.setParameters(p["r"], p["hb"], p["hn"], p["rt"], newMeshSize, p["cx"], p["cy"], p["cz"]);
        m_meshManager->buildAndLoad(gen, name);
    }
    else {
        isValid = false;
    }

    // 4. 阻塞当前线程的线性执行，直到接收到网格生成完成或错误信号
    if (isValid) {
        loop.exec();
    }

    // 5. 断开信号连接，防止内存泄漏或重复触发
    disconnect(conn1);
    disconnect(conn2);

    // 6. 从仓库中获取重构后的新实体，并恢复其原有的物理分类与几何参数记录
    MeshEntity* newEntity = m_repository.getMutableEntity(name);
    if (newEntity) {
        newEntity->type = type;
        newEntity->category = category;
        newEntity->geoParams = p;
    }
}

/**
 * @brief 生成网格收敛性批处理任务脚本及控制文件，并调度后台进程执行
 * @details 根据实体级网格控制表中的参数，自动迭代计算各实体在不同收敛步下的网格尺寸，
 * 重新划分网格并导出独立的控制文件 (.k)。随后生成 Windows 批处理脚本 (.bat)
 * 并通过内部的 QProcess 管理器将其提交至操作系统后台队列静默执行。
 */
void MainWindow::handleGenerateMeshConvergenceBatch() {
    // 1. 前置条件检查
    if (m_workingDirectory.isEmpty()) {
        QMessageBox::warning(this, "路径缺失", "请先在菜单栏设置有效的工作目录。");
        return;
    }
    if (tableMeshSettings->rowCount() == 0) {
        QMessageBox::warning(this, "数据缺失", "当前实体控制表为空，请先刷新并读取物理实体。");
        return;
    }

    int steps = spinMeshSteps->value();

    // 2. 缓存界面表格中的实体网格控制参数
    struct EntityMeshSetting { QString name; double baseSize; double factor; };
    std::vector<EntityMeshSetting> settings;

    for (int r = 0; r < tableMeshSettings->rowCount(); ++r) {
        EntityMeshSetting s;
        s.name = tableMeshSettings->item(r, 0)->text();
        s.baseSize = qobject_cast<QDoubleSpinBox*>(tableMeshSettings->cellWidget(r, 2))->value();
        s.factor = qobject_cast<QDoubleSpinBox*>(tableMeshSettings->cellWidget(r, 3))->value();
        settings.push_back(s);
    }

    // 3. 初始化批处理脚本文件流
    QString batFilePath = QDir(m_workingDirectory).filePath("run_mesh_convergence.bat");
    QFile batFile(batFilePath);
    if (!batFile.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream batStream(&batFile);

    QString solverPath = solverPathEdit->text().isEmpty() ? "D:\Program Files\ANSYS Inc\v241\ansys\bin\winx64\lsdyna_sp.exe" : solverPathEdit->text();
    batStream << "@echo off\nset DYNA_PATH=\"" << solverPath << "\"\n\n";

    QProgressDialog progress("正在生成多梯度网格控制文件队列...", "取消", 0, steps, this);
    progress.setWindowModality(Qt::WindowModal);

    // 4. 执行网格迭代重构与主控文件组装
    for (int i = 0; i < steps; ++i) {
        progress.setValue(i);
        if (progress.wasCanceled()) break;

        QString stepInfo = QString("Step %1 -> ").arg(i + 1);

        // 4.1 遍历并重构所有实体的网格
        for (const auto& s : settings) {
            MeshEntity* mutableEntity = m_repository.getMutableEntity(s.name);
            double currentSize = s.baseSize * std::pow(s.factor, i);
            remeshEntityWithNewSize(mutableEntity, currentSize);

            stepInfo += QString("[%1: %2mm] ").arg(s.name).arg(currentSize, 0, 'f', 1);
        }

        QString jobName = QString("MeshConv_Step%1").arg(i + 1);

        // 4.2 导出当前收敛步的纯几何网格数据 (Nodes & Elements)
        QString meshFileName = jobName + "_mesh.k";
        exportToKFile(QDir(m_workingDirectory).filePath(meshFileName));

        // 4.3 构建当前收敛步的 LS-DYNA 主控文件 (Master Control Deck)
        QString controlFileName = jobName + "_run.k";
        QFile controlFile(QDir(m_workingDirectory).filePath(controlFileName));

        if (controlFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&controlFile);

            // 写入文件头声明
            out << "*KEYWORD\n*TITLE\n" << jobName << "\n";

            // 采用 INCLUDE 语法挂载对应的网格文件，保持主文件结构清晰
            out << "*INCLUDE\n" << meshFileName << "\n";

            // 写入全局控制参数卡片 (如 *CONTROL_TERMINATION, *CONTROL_ENERGY 等)
            if (m_globalControlCard != nullptr) {
                out << QString::fromStdString(m_globalControlCard->to_string());
            }

            // 写入卡片管线中的所有物理属性定义 (材料、部件、接触、边界条件等)
            out << QString::fromStdString(m_deck.generateDeck());

            out << "*END\n";
            controlFile.close();
        }

        // 4.4 写入批处理执行队列 (注：此时传入求解器的是组装后的 controlFileName)
        batStream << "echo Running " << stepInfo << "\n";
        batStream << "%DYNA_PATH% I=" << controlFileName << " NCPU=" << cpuCoresSpin->value() << " MEMORY=2000m\n\n";
    }

    // 注：移除 pause 指令以防止后台进程发生死锁挂起
    batStream << "echo All Mesh Convergence Jobs Finished!\n";
    batFile.close();
    progress.setValue(steps);
    glWidget->update();

    // =========================================================
    // 5. 批处理进程自动调度与执行
    // =========================================================
    if (m_batchProcess->state() == QProcess::Running) {
        QMessageBox::warning(this, "资源冲突", "后台计算引擎正在运行中，请等待当前任务队列结束后再提交新任务。");
        return;
    }

    // 切换视图至单次求解与监控面板
    if (solveTaskTabs) {
        solveTaskTabs->setCurrentIndex(0);
    }

    // 初始化监控台状态
    solverConsole->clear();
    solverConsole->append("========================================");
    solverConsole->append("[系统提示] 开始执行实体级网格收敛性批处理队列...");
    solverConsole->append("[系统提示] 当前工作目录: " + m_workingDirectory);
    solverConsole->append("========================================\n");

    // 配置运行环境并拉起系统命令解释器静默执行批处理脚本
    m_batchProcess->setWorkingDirectory(m_workingDirectory);
    m_batchProcess->start("cmd.exe", QStringList() << "/c" << batFilePath);
}

/**
 * @brief 生成起爆阈值升降法寻优批处理任务脚本及控制文件，并调度后台进程执行
 * @details 提取用户界面的起爆速度区间与步长，自动为每个速度梯度生成对应的
 * LS-DYNA 控制卡片及主文件 (.k)。组装批处理执行队列，并通过 QProcess 唤醒系统底层执行。
 */
void MainWindow::handleGenerateVelocityThresholdBatch() {
    // 1. 前置条件检查
    if (m_workingDirectory.isEmpty()) {
        QMessageBox::warning(this, "路径缺失", "请先在菜单栏设置有效的工作目录。");
        return;
    }

    double vStart = spinStartVelocity->value();
    double vEnd = spinEndVelocity->value();
    double vStep = spinVelocityStep->value();

    if (vStep <= 0 || vStart > vEnd) {
        QMessageBox::warning(this, "参数错误", "速度步长必须大于 0，且起始速度不能高于终止速度。");
        return;
    }

    // 2. 初始化批处理脚本文件流
    QString batFilePath = QDir(m_workingDirectory).filePath("run_velocity_optimization.bat");
    QFile batFile(batFilePath);
    if (!batFile.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream batStream(&batFile);

    QString solverPath = solverPathEdit->text().isEmpty() ? "D:\Program Files\ANSYS Inc\v241\ansys\bin\winx64\lsdyna_sp.exe" : solverPathEdit->text();
    batStream << "@echo off\nset DYNA_PATH=\"" << solverPath << "\"\n\n";

    // 预先将当前的公共网格结构导出至主文件中复用，以减少存储占用
    QString sharedMeshFileName = "shared_mesh_geometry.k";
    exportToKFile(QDir(m_workingDirectory).filePath(sharedMeshFileName));

    // 3. 循环遍历速度区间，生成独立控制文件
    for (double currentVel = vStart; currentVel <= vEnd; currentVel += vStep) {
        QString jobName = QString("VelocityOpt_V%1").arg(currentVel);
        QString controlFileName = jobName + "_control.k";

        // 更新初始条件卡片中的撞击速度向量
        // (注：需确保 m_initialConditionsCard 在其他模块已正确初始化，此处修改指定方向的速度)
        // 伪代码示例：m_initialConditionsCard->setVelocity(currentVel); 
        // 需保留您原有的速度修改逻辑

        QFile controlFile(QDir(m_workingDirectory).filePath(controlFileName));
        if (controlFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&controlFile);
            out << "*KEYWORD\n*TITLE\n" << jobName << "\n";

            // 采用 INCLUDE 语法复用外部的几何网格文件
            out << "*INCLUDE\n" << sharedMeshFileName << "\n";

            if (m_globalControlCard != nullptr) {
                out << QString::fromStdString(m_globalControlCard->to_string());
            }
            out << QString::fromStdString(m_deck.generateDeck());
            out << "*END\n";
            controlFile.close();
        }

        // 写入该工况的执行指令至批处理队列
        batStream << "echo Running Detonation Test V = " << currentVel << " m/s\n";
        batStream << "%DYNA_PATH% I=" << controlFileName << " NCPU=" << cpuCoresSpin->value() << " MEMORY=2000m\n\n";
    }

    // 注：移除 pause 指令以避免后台阻塞
    batStream << "echo All Threshold Jobs Finished!\n";
    batFile.close();

    // =========================================================
    // 4. 批处理进程自动调度与执行
    // =========================================================
    if (m_batchProcess->state() == QProcess::Running) {
        QMessageBox::warning(this, "资源冲突", "后台计算引擎正在运行中，请等待当前任务队列结束后再提交新任务。");
        return;
    }

    // 切换视图至单次求解与监控面板
    if (solveTaskTabs) {
        solveTaskTabs->setCurrentIndex(0);
    }

    // 初始化监控台状态
    solverConsole->clear();
    solverConsole->append("========================================");
    solverConsole->append("[系统提示] 开始执行起爆阈值寻优批处理队列...");
    solverConsole->append("[系统提示] 当前工作目录: " + m_workingDirectory);
    solverConsole->append("========================================\n");

    // 配置运行环境并拉起系统命令解释器静默执行批处理脚本
    m_batchProcess->setWorkingDirectory(m_workingDirectory);
    m_batchProcess->start("cmd.exe", QStringList() << "/c" << batFilePath);
}

/**
 * @brief 自动化解析求解结果并生成网格收敛性分析报告
 * @details 该模块负责读取 LS-DYNA 批处理计算所产生的 ASCII 结果文件 (如 glstat, nodout)。
 * 依托预编译的正则表达式引擎提取各工况最终时刻的能量或运动学标量。
 * 基于 L2 范数或直接差分计算相邻迭代步之间的相对误差，并与用户设定的容差阈值进行比对，
 * 最终输出具有工程指导意义的最优网格尺寸配置。
 */
void MainWindow::handleAnalyzeConvergence() {
    // 1. 前置条件与工作空间校验
    if (m_workingDirectory.isEmpty()) {
        QMessageBox::warning(this, "路径缺失", "请先在菜单栏设置有效的工作目录。");
        return;
    }
    if (tableMeshSettings->rowCount() == 0) {
        QMessageBox::warning(this, "数据缺失", "当前实体控制表为空，请先刷新并读取物理实体。");
        return;
    }

    int steps = spinMeshSteps->value();
    double tolerance = spinTolerance->value() / 100.0;
    int metricIndex = comboTargetMetric->currentIndex(); // 0: 内能, 1: 动能, 2: 剩余速度

    std::vector<int> stepList;
    std::vector<double> targetValues;

    QProgressDialog progress("正在对后台计算结果文件进行正则解析...", "取消", 0, steps, this);
    progress.setWindowModality(Qt::WindowModal);

    // 2. 预编译正则表达式以提升大规模 ASCII 文件的解析性能
    // 匹配格式例: "internal energy  0.1234E+04" (支持可选的科学计数法)
    std::regex internalRegex(R"(internal energy\s+([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?))");
    std::regex kineticRegex(R"(kinetic energy\s+([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?))");

    // 匹配 nodout 中目标节点的速度记录行
    // 匹配格式例: " 1  0.0  0.0  0.0  1.2E+2  0.0  0.0" (假定追踪质心节点 ID 为 1)
    std::regex velocityRegex(R"(^\s*1\s+(?:[+-]?\S+\s+){3}([+-]?\S+)\s+([+-]?\S+)\s+([+-]?\S+))");

    // 3. 循环遍历并解析每一个收敛步的物理计算结果
    for (int i = 0; i < steps; ++i) {
        progress.setValue(i);
        if (progress.wasCanceled()) break;

        // 根据所选判据推断目标结果文件后缀 (全局统计使用 glstat, 节点输出使用 nodout)
        QString fileSuffix = (metricIndex == 2) ? "_nodout" : "_glstat";
        QString resultFileName = QDir(m_workingDirectory).filePath(
            QString("MeshConv_Step%1%2").arg(i + 1).arg(fileSuffix));

        double finalMetricValue = 0.0;
        std::ifstream file(resultFileName.toLocal8Bit().constData());

        // ---------------------------------------------------------
        // 核心 IO 解析区：逐行读取并进行正则特征匹配
        // ---------------------------------------------------------
        if (file.is_open()) {
            std::string line;
            std::smatch match;

            while (std::getline(file, line)) {
                // 将字符串转换为小写以增强对 LS-DYNA 不同版本输出格式的鲁棒性
                std::string lowerLine = line;
                std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);

                if (metricIndex == 0) { // [靶板总内能]
                    if (std::regex_search(lowerLine, match, internalRegex)) {
                        finalMetricValue = std::stod(match[1].str());
                    }
                }
                else if (metricIndex == 1) { // [系统总动能]
                    if (std::regex_search(lowerLine, match, kineticRegex)) {
                        finalMetricValue = std::stod(match[1].str());
                    }
                }
                else if (metricIndex == 2) { // [弹体质心剩余速度]
                    if (std::regex_search(lowerLine, match, velocityRegex)) {
                        double vx = std::stod(match[1].str());
                        double vy = std::stod(match[2].str());
                        double vz = std::stod(match[3].str());
                        // 计算合成速度标量 (L2 范数)
                        finalMetricValue = std::sqrt(vx * vx + vy * vy + vz * vz);
                    }
                }
            }
            file.close();
        }
        else {
            // IO 异常处理：抛出缺失文件路径，提示用户检查底层求解器状态
            QMessageBox::warning(this, "IO 解析异常",
                QString("无法打开第 %1 步的计算结果文件：\n%2\n\n请确认 LS-DYNA 批处理是否已经全部计算完毕，"
                    "且已在 K 文件中正确配置了 *DATABASE 卡片！")
                .arg(i + 1).arg(resultFileName));
            return;
        }

        stepList.push_back(i + 1);
        targetValues.push_back(finalMetricValue);
    }
    progress.setValue(steps);

    // 4. 收敛性研判与相对误差计算
    QString report = "【实体级网格收敛性综合分析报告】\n\n";
    bool isConverged = false;
    int optimalStep = 1;

    for (size_t i = 0; i < targetValues.size(); ++i) {
        report += QString("迭代第 %1 步: 观测极值 = %2\n").arg(stepList[i]).arg(targetValues[i], 0, 'e', 4);

        if (i > 0) {
            // 计算相对误差：E = |V_new - V_old| / V_old (采用 1e-9 防止除零异常)
            double error = std::abs(targetValues[i] - targetValues[i - 1]) / (std::abs(targetValues[i - 1]) + 1e-9);
            report += QString("   -> 相对变化率: %1%\n").arg(error * 100.0, 0, 'f', 2);

            // 若当前误差落入设定的容差阈值区间，且为首次达标，则锁定最优收敛步
            if (error <= tolerance && !isConverged) {
                isConverged = true;
                optimalStep = stepList[i];
                report += QString("   ✅ [系统评估] 达到收敛标准！\n");
            }
        }
    }

    // 5. 组装最终结果与网格参数推荐方案
    if (isConverged) {
        report += QString("\n[结论] 网格在第 %1 步时已达到 %2% 的收敛标准。\n以下为该最优步对应的实体网格尺寸配置推荐方案：\n")
            .arg(optimalStep).arg(spinTolerance->value());
    }
    else {
        report += QString("\n[结论] 经历 %1 轮细化迭代后，观测指标的相对误差仍未降至 %2% 以下。\n请考虑提升细化总次数或排查模型应力奇异性。暂推荐最后一步配置：\n")
            .arg(steps).arg(spinTolerance->value());
        optimalStep = steps;
    }

    // 从实体控制表中反推最优步时各实体所分配的精确网格尺寸
    for (int r = 0; r < tableMeshSettings->rowCount(); ++r) {
        QString name = tableMeshSettings->item(r, 0)->text();
        double baseSize = qobject_cast<QDoubleSpinBox*>(tableMeshSettings->cellWidget(r, 2))->value();
        double factor = qobject_cast<QDoubleSpinBox*>(tableMeshSettings->cellWidget(r, 3))->value();

        // 推演公式: OptimalSize = BaseSize * (Factor ^ (OptimalStep - 1))
        double optimalSize = baseSize * std::pow(factor, optimalStep - 1);
        report += QString(" - 实体 [%1] 建议网格尺寸: %2 mm\n").arg(name).arg(optimalSize, 0, 'f', 2);
    }

    QMessageBox::information(this, "收敛性分析完成", report);
}

// ==============================================================
// 🌟 槽函数：读取现有物理实体，自动填充网格控制表
// ==============================================================
void MainWindow::handleRefreshEntityTable() {
    tableMeshSettings->setRowCount(0); // 清空表格

    // 获取当前所有已经生成的实体
    auto entities = m_repository.getEntities();
    for (auto it = entities.begin(); it != entities.end(); ++it) {
        int row = tableMeshSettings->rowCount();
        tableMeshSettings->insertRow(row);

        // 第 0 列: 实体名字 (只读)
        QTableWidgetItem* nameItem = new QTableWidgetItem(it->first);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        tableMeshSettings->setItem(row, 0, nameItem);

        // 第 1 列: 物理分类 (只读)
        QString cat = it->second.category.isEmpty() ? "未分类" : it->second.category;
        QTableWidgetItem* catItem = new QTableWidgetItem(cat);
        catItem->setFlags(catItem->flags() & ~Qt::ItemIsEditable);
        tableMeshSettings->setItem(row, 1, catItem);

        // 第 2 列: 智能读取实体原本的基础网格尺寸，作为默认初始值
        QDoubleSpinBox* spinBase = new QDoubleSpinBox();
        spinBase->setRange(0.0001, 1000.0); 
        spinBase->setDecimals(3);
        // 如果当时画图时存了网格参数 ms，就用当时的，否则默认 5.0
        double defaultMs = it->second.geoParams.contains("ms") ? it->second.geoParams["ms"] : 5.0;
        spinBase->setValue(defaultMs);
        tableMeshSettings->setCellWidget(row, 2, spinBase);

        // 第 3 列: 每个实体独立的缩放因子 (默认 0.8)
        QDoubleSpinBox* spinFactor = new QDoubleSpinBox();
        spinFactor->setRange(0.001, 1.0);
        spinFactor->setSingleStep(0.1);
        spinFactor->setDecimals(3);
        spinFactor->setValue(0.8);
        tableMeshSettings->setCellWidget(row, 3, spinFactor);
    }
}

// ==============================================================
// 🌟 槽函数：刷新并预览起爆速度工况序列
// ==============================================================
void MainWindow::handlePreviewVelocitySequence() {
    tableVelocitySequence->setRowCount(0); // 清空旧数据

    double vStart = spinStartVelocity->value();
    double vEnd = spinEndVelocity->value();
    double vStep = spinVelocityStep->value();

    if (vStep <= 0 || vStart > vEnd) {
        QMessageBox::warning(this, "参数错误", "速度步长必须大于0，且起始速度不能大于终止速度！");
        return;
    }

    int row = 0;
    for (double v = vStart; v <= vEnd; v += vStep) {
        tableVelocitySequence->insertRow(row);

        // 第 0 列: 序号
        QTableWidgetItem* idItem = new QTableWidgetItem(QString("Step %1").arg(row + 1));
        idItem->setTextAlignment(Qt::AlignCenter);
        idItem->setFlags(idItem->flags() & ~Qt::ItemIsEditable); // 设置为只读
        tableVelocitySequence->setItem(row, 0, idItem);

        // 第 1 列: 速度值
        QTableWidgetItem* vItem = new QTableWidgetItem(QString::number(v, 'f', 1));
        vItem->setTextAlignment(Qt::AlignCenter);
        vItem->setFlags(vItem->flags() & ~Qt::ItemIsEditable);
        tableVelocitySequence->setItem(row, 1, vItem);

        // 第 2 列: 对应的生成控制文件名
        QTableWidgetItem* nameItem = new QTableWidgetItem(QString("VelocityOpt_V%1_control.k").arg(v));
        nameItem->setTextAlignment(Qt::AlignCenter);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        tableVelocitySequence->setItem(row, 2, nameItem);

        row++;
    }
}

/**
 * @brief 实时读取后台批处理进程的输出流，并重定向至 GUI 控制台
 * @details 捕获 LS-DYNA 及底层 CMD 批处理脚本的标准输出流，
 * 采用本地字符集进行解码，并将解析后的日志文本追加至求解监控台 (solverConsole) 中。
 * 同时自动将滚动条置于最底端，确保最新日志始终处于可视区域。
 */
void MainWindow::handleBatchProcessOutput() {
    if (!m_batchProcess) return;

    // 读取当前缓冲区内所有可用的输出字节流
    QByteArray outputData = m_batchProcess->readAllStandardOutput();

    // 采用操作系统本地编码 (Windows 环境通常为 GBK) 进行解码，防止控制台出现乱码
    QString outputStr = QString::fromLocal8Bit(outputData);

    if (solverConsole) {
        solverConsole->append(outputStr);
        // 强制更新滚动条视图位置
        QScrollBar* scrollBar = solverConsole->verticalScrollBar();
        if (scrollBar) {
            scrollBar->setValue(scrollBar->maximum());
        }
    }
}

/**
 * @brief 监控后台批处理任务的终止状态并输出最终结论
 * @param exitCode 进程执行完毕后返回的退出码 (0 通常代表正常结束)
 * @param exitStatus 进程的退出状态 (标识正常退出或因崩溃退出)
 */
void MainWindow::handleBatchProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    if (!solverConsole) return;

    solverConsole->append("\n========================================");
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        solverConsole->append("[系统提示] 自动化批处理求解任务已全部正常执行完毕。");
    }
    else {
        solverConsole->append(QString("[系统警告] 批处理任务异常终止。退出码: %1").arg(exitCode));
    }
    solverConsole->append("========================================\n");
}
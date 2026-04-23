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
#include <QTimer>

#include "src/app/PostProcessing/qcustomplot.h"

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
    QMenu* fileMenu = menuBar->addMenu("File");
    QAction* newAction = fileMenu->addAction("New");
    QAction* openAction = fileMenu->addAction("Open");
    QAction* saveAction = fileMenu->addAction("Save");

    fileMenu->addSeparator();

    // 置工作目录
    QAction* setWorkDirAct = new QAction(tr("设置工作目录 (Set Working Directory)..."), this);
    setWorkDirAct->setStatusTip(tr("设置所有导出文件和仿真数据的保存目录"));
    connect(setWorkDirAct, &QAction::triggered, this, &MainWindow::onSetWorkingDirectory);
    fileMenu->addAction(setWorkDirAct);

    // 全局系统设置
    QAction* globalSettingsAct = new QAction(tr("全局设置 (Global Settings)..."), this);
    globalSettingsAct->setStatusTip(tr("配置 LS-DYNA 求解器路径与运行环境变量"));
    connect(globalSettingsAct, &QAction::triggered, this, &MainWindow::onGlobalSettings);
    fileMenu->addAction(globalSettingsAct);

    fileMenu->addSeparator();

    QAction* exitAction = fileMenu->addAction("Exit");

    // 编辑菜单
    QMenu* editMenu = menuBar->addMenu("Edit");
    QAction* cutAction = editMenu->addAction("Cut");
    QAction* copyAction = editMenu->addAction("Copy");
    QAction* pasteAction = editMenu->addAction("Paste");

    // 视图菜单
    QMenu* viewMenu = menuBar->addMenu("View");
    QAction* zoomInAction = viewMenu->addAction("Zoom In");
    QAction* zoomOutAction = viewMenu->addAction("Zoom Out");
    QAction* resetViewAction = viewMenu->addAction("Reset View");

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
    substanceTree->setMinimumWidth(320);

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
    tabWidget->setMinimumWidth(320);

    // 将三个分类作为不同的 Tab 添加进去
    setupGeneratorTab(tabWidget, tr("破片 (Fragment)"),
        { "Cube", "Sphere", "Hemisphere", "TriangularPrism", "PentagonalPrism", "HexagonalPrism","Frustum", "Fragment Simulating Projectile", "Bullet" }, m_fragmentUI);

    setupGeneratorTab(tabWidget, tr("壳体 (Shell)"),
        { "CylindricalShell", "OpenCylindricalShell", "HalfCylindricalShell","FrustumShell"}, m_shellUI);

    setupGeneratorTab(tabWidget, tr("装药 (Charge)"),
        { "Cylinder", "HalfCylinder", "Cube", "Sphere", "Frustum"}, m_chargeUI);

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

/**
 * @brief 从环境中彻底销毁指定实体及其关联属性
 * @param name 待销毁的物理实体标识名
 * @details 执行级联清理，将抹除该实体的拓扑数据、对称边界条件、
 * 已挂载的 *PART 关键字卡片以及附着其上的所有传感器探针缓存。
 */
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

        // =========================================================
        // 🌟 核心修复：同步移除附着在该幽灵实体身上的所有探测点
        // =========================================================
        m_sensorNodes.remove(name);

        // 2. 刷新 3D 画面 (清空指令 -> 重新遍历剩余实体提交给 GLWidget)
        redrawAllEntities();

        // 3. 刷新左侧列表 (清空树节点 -> 重新遍历剩余实体插入 Tree)
        updateSubstanceTree();

        qDebug() << "Entity deleted and UI refreshed: " << name;
    }
}

/**
 * @brief 导出 LS-DYNA 关键字文件 (*.k)
 * @param fileName 导出的目标文件绝对路径
 * @details 该函数负责将当前 UI 面板中配置的网格节点、实体单元、
 * 传感器探测点以及对称边界条件，组装并格式化输出为 LS-DYNA 求解器
 * 可识别的标准 ASCII 关键字文件。
 * * @note
 * - [自适应采样]：针对 NODOUT 与 ELOUT，系统会自动根据总计算时间 (EndTime)
 * 均分分配输出步长 (dtOut)，默认采样密度为 200 帧，避免输出文件过大或曲线失真。
 * - [微观数据提取]：若场景中存在观测点 (Sensors)，会自动强制开启实体单元的
 * 历史变量输出 (NEIPH=3)，用于后处理解析器提取炸药的反应度等微观参数。
 */
void MainWindow::exportToKFile(const QString& fileName) {
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        logCommand("Export", "Failed to open file: " + fileName);
        return;
    }

    QTextStream out(&file);
    out << "*KEYWORD\n";

    // ==============================================================
    // 0. 获取计算时间并计算自适应输出步长
    // ==============================================================
    // 获取用户在界面设定的总计算时长
    double endTime = m_simSetupUI.endtimeInput->value();

    // 自适应输出步长计算 (Target: 200 frames)
    double dtOut = (endTime > 0.0) ? (endTime / 200.0) : 0.005;

    // ==============================================================
    // 1. 实体网格数据获取与写入 (Nodes & Elements)
    // ==============================================================
    const auto& allEntities = m_repository.getAllEntities(); // 获取仓库中所有实体

    int globalNodeId = 1;  // 全局节点计数器
    int globalElemId = 1;  // 全局单元计数器
    std::vector<int> globalSensorIds; // 收集所有传感器的全局 ID
    std::set<int> globalSensorElemIds;
    QMap<QString, QSet<int>> matchedSensorNodes; // 记录已分配过单元的监测点节点

    for (auto it = allEntities.begin(); it != allEntities.end(); ++it) {
        const MeshEntity& entity = it->second;
        QString entityName = it->first; // 获取实体名字

        int realPartId = getOrCreatePart(entityName)->pid;

        // 映射表：局部向量索引 -> 全局文件节点 ID
        std::unordered_map<int, int> localToGlobal;

        out << "$ #################################################\n";
        out << "$ Entity: " << entity.name << "\n";
        out << "$ #################################################\n";

        // 1.1 导出节点 (从索引 1 开始，跳过占位的 nodes[0])
        out << "*NODE\n";
        for (size_t i = 1; i < entity.nodes.size(); ++i) {
            localToGlobal[static_cast<int>(i)] = globalNodeId;

            // 格式：ID, X, Y, Z
            out << globalNodeId << ", "
                << entity.nodes[i].pos.x() << ", "
                << entity.nodes[i].pos.y() << ", "
                << entity.nodes[i].pos.z() << "\n";

            // 收集当前实体中被选作传感器的节点 ID
            if (m_sensorNodes.contains(entityName) && m_sensorNodes[entityName].contains(i)) {
                globalSensorIds.push_back(globalNodeId);
            }

            globalNodeId++;
        }

        // 1.2 导出六面体单元
        out << "*ELEMENT_SOLID\n";
        for (const auto& hex : entity.hexes) {
            // 直接使用 hex[j] 访问 std::array 元素
            out << QString("%1").arg(globalElemId, 8)
                << QString("%1").arg(realPartId, 8);

            bool isSensorElement = false;

            for (int j = 0; j < 8; ++j) {
                int localIdx = hex[j]; // 获取存储在 array 中的局部节点索引
                out << QString("%1").arg(localToGlobal[localIdx], 8);

                // 判断当前单元是否包含传感器节点
                if (m_sensorNodes.contains(entityName) && m_sensorNodes[entityName].contains(localIdx)) {
                    if (!matchedSensorNodes[entityName].contains(localIdx)) {
                        matchedSensorNodes[entityName].insert(localIdx);
                        isSensorElement = true;
                    }
                }
            }
            out << "\n";

            // 如果单元包含传感器节点，则将其计入实体观测点集合
            if (isSensorElement) {
                globalSensorElemIds.insert(globalElemId);
            }
            globalElemId++;
        }
    }

    // ==============================================================
    // 2. 导出探针观测点及微观数据输出控制 (Sensors)
    // ==============================================================
    if (!globalSensorIds.empty()) {

        // 2.1 指定节点观测点全局 ID
        out << "*DATABASE_HISTORY_NODE\n";
        out << "$#    id1       id2       id3       id4       id5       id6       id7       id8\n";
        // LS-DYNA 要求每行最多 8 个 ID，宽度为 10，自动换行
        for (size_t i = 0; i < globalSensorIds.size(); ++i) {
            out << QString("%1").arg(globalSensorIds[i], 10, 10, QChar(' '));
            if ((i + 1) % 8 == 0) out << "\n";
        }
        if (globalSensorIds.size() % 8 != 0) out << "\n";

        // 2.2 指定节点历程数据 (NODOUT) 的输出时间步长 (使用自适应步长 dtOut)
        out << "*DATABASE_NODOUT\n";
        out << "$#      dt      lcdt      beam     npltc    psetid\n";
        out << QString("%1").arg(dtOut, 10, 'f', 5, ' ') << "         0         0         0         0\n";

        // 2.3 指定实体单元观测点全局 ID
        out << "*DATABASE_HISTORY_SOLID\n";
        out << "$#    id1       id2       id3       id4       id5       id6       id7       id8\n";
        int count = 0;
        for (int elemId : globalSensorElemIds) {
            out << QString("%1").arg(elemId, 10, 10, QChar(' '));
            count++;
            if (count % 8 == 0) out << "\n";
        }
        if (count % 8 != 0) out << "\n";

        // 2.4 指定单元历程数据 (ELOUT) 的输出时间步长 (使用自适应步长 dtOut)，并强制输出 8 个历史变量
        out << "*DATABASE_ELOUT\n";
        out << "$#      dt    binary      lcur     ioopt   option1   option2   option3   option4\n";
        out << QString("%1").arg(dtOut, 10, 'f', 5, ' ') << "         3         0         1         8         0         0         0\n";

    }

    // ==============================================================
    // 3. 导出对称边界条件
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

            // 实例化集合对象与约束对象，并转换为字符串输出
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

    out << "*END\n";
    file.close();
    logCommand("Export", "Saved entities to " + fileName);
}


/**
 * @brief 构建并初始化物理实体生成器的选项卡界面 (UI Setup)
 * * 该函数负责为不同类别的物理实体（如破片、壳体、装药等）动态生成输入面板，
 * 包含基础几何参数输入区，以及可选的“竖直方向”和“壁厚方向”高级局部网格细化控制区。
 * * @param tabWidget 目标父级选项卡控件 (QTabWidget)
 * @param title     选项卡标题 (如 "Fragment", "Shell")
 * @param shapes    该选项卡支持生成的形状列表 (如 "Cylinder", "Cube" 等)
 * @param ui        关联的 GeneratorUI 结构体引用，用于持久化存储该选项卡内的控件指针
 */
void MainWindow::setupGeneratorTab(QTabWidget* tabWidget, const QString& title, const QStringList& shapes, GeneratorUI& ui)
{
    // =========================================================================
    // 1. 基础布局与形状选择 (Base Layout & Shape Selection)
    // =========================================================================
    QWidget* tab = new QWidget(tabWidget);
    QVBoxLayout* mainLayout = new QVBoxLayout(tab);

    mainLayout->addWidget(new QLabel("Select Shape:", tab));
    ui.shapeComboBox = new QComboBox(tab);
    ui.shapeComboBox->addItems(shapes);
    mainLayout->addWidget(ui.shapeComboBox);

    mainLayout->addSpacing(10);

    // =========================================================================
    // 2. 动态基础参数区 (Dynamic Base Parameters)
    // =========================================================================
    mainLayout->addWidget(new QLabel("Parameters:", tab));
    ui.paramContainer = new QWidget(tab);
    ui.paramLayout = new QVBoxLayout(ui.paramContainer);
    ui.paramLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(ui.paramContainer);

    // =========================================================================
    // 3. 竖直方向(Z轴)局部网格细化控制区 (Vertical Mesh Refinement)
    // =========================================================================
    ui.chkVerticalRefinement = new QCheckBox("启用竖直方向局部网格细化", tab);
    mainLayout->addWidget(ui.chkVerticalRefinement);

    ui.refinementContainer = new QWidget(tab);
    QFormLayout* refineLayout = new QFormLayout(ui.refinementContainer);
    refineLayout->setContentsMargins(15, 0, 0, 0); // 左侧缩进以体现层级关系

    ui.spinZStart = new QDoubleSpinBox(tab);
    ui.spinZStart->setRange(0.0, 10000.0); ui.spinZStart->setDecimals(3);

    ui.spinZEnd = new QDoubleSpinBox(tab);
    ui.spinZEnd->setRange(0.0, 10000.0); ui.spinZEnd->setDecimals(3);

    ui.spinMsLocalZ = new QDoubleSpinBox(tab);
    ui.spinMsLocalZ->setRange(0.001, 100.0); ui.spinMsLocalZ->setDecimals(3);
    ui.spinMsLocalZ->setValue(0.1);

    refineLayout->addRow("起始高度 (Z_start):", ui.spinZStart);
    refineLayout->addRow("结束高度 (Z_end):", ui.spinZEnd);
    refineLayout->addRow("局部网格尺寸 (ms_z):", ui.spinMsLocalZ);

    mainLayout->addWidget(ui.refinementContainer);
    ui.refinementContainer->setVisible(false); // 默认隐藏

    connect(ui.chkVerticalRefinement, &QCheckBox::toggled, this, [&ui](bool checked) {
        ui.refinementContainer->setVisible(checked);
        });

    // =========================================================================
    // 4. 壁厚方向(径向)局部网格细化控制区 (Wall Thickness Refinement)
    // =========================================================================
    ui.chkWallRefinement = new QCheckBox("启用壁厚方向局部网格细化", tab);
    mainLayout->addWidget(ui.chkWallRefinement);

    ui.wallRefineContainer = new QWidget(tab);
    QFormLayout* wallLayout = new QFormLayout(ui.wallRefineContainer);
    wallLayout->setContentsMargins(15, 0, 0, 0);

    ui.spinMsWall = new QDoubleSpinBox(tab);
    ui.spinMsWall->setRange(0.001, 100.0); ui.spinMsWall->setDecimals(3);
    ui.spinMsWall->setValue(0.1);

    wallLayout->addRow("壁厚局部网格尺寸 (ms_wall):", ui.spinMsWall);
    mainLayout->addWidget(ui.wallRefineContainer);
    ui.wallRefineContainer->setVisible(false); // 默认隐藏

    connect(ui.chkWallRefinement, &QCheckBox::toggled, this, [&ui](bool checked) {
        ui.wallRefineContainer->setVisible(checked);
        });

    mainLayout->addStretch(1);

    // =========================================================================
    // 5. 触发按钮与核心交互绑定 (Triggers & Interactions)
    // =========================================================================
    QPushButton* generateBtn = new QPushButton("Generate " + title, tab);
    mainLayout->addWidget(generateBtn);
    tabWidget->addTab(tab, title);

    // 形状切换响应逻辑：动态刷新参数列表，并控制细化选项的显隐状态
    connect(ui.shapeComboBox, &QComboBox::currentTextChanged, this, [this, &ui](const QString& text) {
        handleShapeTypeChanged(ui, text);

        // 5.1 竖直细化可用性判定 (Z-axis refinement support)
        bool supportsZRefine = (text == "Cylinder" || text == "Frustum" ||
            text == "CylindricalShell" || text == "FrustumShell" ||
            text == "HalfCylindricalShell" || text == "OpenCylindricalShell");
        ui.chkVerticalRefinement->setVisible(supportsZRefine);
        if (!supportsZRefine) ui.chkVerticalRefinement->setChecked(false);

        // 5.2 壁厚细化可用性判定 (Wall thickness refinement support)
        bool supportsWallRefine = (text == "CylindricalShell" || text == "FrustumShell" ||
            text == "HalfCylindricalShell" || text == "OpenCylindricalShell");
        ui.chkWallRefinement->setVisible(supportsWallRefine);
        if (!supportsWallRefine) ui.chkWallRefinement->setChecked(false);
        });

    // 生成按钮响应逻辑：交由集中式构建函数处理
    connect(generateBtn, &QPushButton::clicked, this, [this, &ui]() {
        handleGenerateButtonClicked(ui);
        });

    // 初始化时主动触发一次形状刷新，构建默认(如 Cube)的参数输入框
    QString defaultShape = ui.shapeComboBox->currentText();
    handleShapeTypeChanged(ui, defaultShape);
    // 判定初始形状是否支持竖直细化
    bool supportsZRefine = (defaultShape == "Cylinder" || defaultShape == "Frustum" ||
        defaultShape == "CylindricalShell" || defaultShape == "FrustumShell" ||
        defaultShape == "HalfCylindricalShell" || defaultShape == "OpenCylindricalShell");
    ui.chkVerticalRefinement->setVisible(supportsZRefine);
    if (!supportsZRefine) {
        ui.chkVerticalRefinement->setChecked(false);
        ui.refinementContainer->setVisible(false); // 同步隐藏子面板
    }

    // 判定初始形状是否支持壁厚细化
    bool supportsWallRefine = (defaultShape == "CylindricalShell" || defaultShape == "FrustumShell" ||
        defaultShape == "HalfCylindricalShell" || defaultShape == "OpenCylindricalShell");
    ui.chkWallRefinement->setVisible(supportsWallRefine);
    if (!supportsWallRefine) {
        ui.chkWallRefinement->setChecked(false);
        ui.wallRefineContainer->setVisible(false); // 同步隐藏子面板
    }
}
// 辅助函数：向特定的 UI 结构体中添加输入框
void MainWindow::addNumParamToUI(GeneratorUI& ui, const QString& labelText, const QString& key, double defaultValue, const QString& unit) {
    QHBoxLayout* row = new QHBoxLayout();
    row->addWidget(new QLabel(labelText));
    QDoubleSpinBox* sb = new QDoubleSpinBox(this);
    sb->setRange(-9999.0, 9999.0);
    sb->setValue(defaultValue);
    sb->setDecimals(4);

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
        addNumParamToUI(ui, "Length (LX):", "lx", 0.54, " cm");
        addNumParamToUI(ui, "Width (LY):", "ly", 0.54, " cm");
        addNumParamToUI(ui, "Height (LZ):", "lz", 0.54, " cm");
        addNumParamToUI(ui, "Mesh Size:", "ms", 0.02, " cm");
        addNumParamToUI(ui, "Center X:", "cx", 5.0, " cm");
        addNumParamToUI(ui, "Center Y:", "cy", 0.0, " cm");
        addNumParamToUI(ui, "Center Z:", "cz", 4.0, " cm");
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
        addNumParamToUI(ui, "Base R:", "rb", 4.0, " cm");
        addNumParamToUI(ui, "Top R:", "rt", 3.0, " cm");
        addNumParamToUI(ui, "Height:", "h", 8.0, " cm");
        addNumParamToUI(ui, "Mesh Size:", "ms", 0.08, " cm");
        addNumParamToUI(ui, "Center X:", "cx", 0.0, " cm");
        addNumParamToUI(ui, "Center Y:", "cy", 0.0, " cm");
        addNumParamToUI(ui, "Center Z:", "cz", 0.6, " cm");
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
        addNumParamToUI(ui, "Radius (外接圆半径 R):", "r", 0.38, " cm");
        addNumParamToUI(ui, "Height (高度 H):", "h", 0.76, " cm");
        addNumParamToUI(ui, "Mesh Size:", "ms", 0.02, " cm");
        addNumParamToUI(ui, "Center X:", "cx", 6, " cm");
        addNumParamToUI(ui, "Center Y:", "cy", 0.0, " cm");
        addNumParamToUI(ui, "Center Z:", "cz", 5, " cm");
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
    else if (text == "FrustumShell") {
        addNumParamToUI(ui, "Base Inner R:", "rb", 4.0, " cm");
        addNumParamToUI(ui, "Top Inner R:", "rt", 2.0, " cm");
        addNumParamToUI(ui, "Wall Thick:", "wall", 0.6, " cm");
        addNumParamToUI(ui, "Cap Thick:", "hcap", 0.6, " cm");
        addNumParamToUI(ui, "Void Height:", "hvoid", 8.0, " cm");
        addNumParamToUI(ui, "Mesh Size:", "ms", 0.08, " cm");
        addNumParamToUI(ui, "Center X:", "cx", 0.0, " cm");
        addNumParamToUI(ui, "Center Y:", "cy", 0.0, " cm");
        addNumParamToUI(ui, "Center Z:", "cz", 0.0, " cm");
    }
    else if (text == "Bullet") {
        addNumParamToUI(ui, "Caliber (口径):", "caliber", 0.762, " cm");
        addNumParamToUI(ui, "Jacket Thick (被甲厚度):", "jacketThickness", 0.06, " cm");
        addNumParamToUI(ui, "Cyl Length (圆柱段长):", "cylinderLength", 1.2, " cm");
        addNumParamToUI(ui, "Nose Length (弹头长):", "noseLength", 1.6, " cm");
        addNumParamToUI(ui, "Tip Dia (尖端平切直径):", "tipDiameter", 0.1, " cm");
        addNumParamToUI(ui, "Mesh Size (网格尺寸):", "ms", 0.05, " cm");
        addNumParamToUI(ui, "Center X:", "cx", 0.0, " cm");
        addNumParamToUI(ui, "Center Y:", "cy", 0.0, " cm");
        addNumParamToUI(ui, "Center Z:", "cz", 0.0, " cm");
        }
}

/**
 * @brief 处理网格生成请求的事件槽 (Generation Dispatcher)
 * * 该函数负责从指定的 GeneratorUI 中提取用户输入的几何参数和模型元数据。
 * 根据所选形状及其是否启用了“局部网格细化”(竖直或壁厚方向)，实例化对应的网格生成器 (MeshGenerator)，
 * 并交由底层 MeshManager 执行异步网格划分与文件导出。
 * * @param ui 触发生成请求的选项卡 UI 结构体引用
 */
void MainWindow::handleGenerateButtonClicked(GeneratorUI& ui)
{
    // =========================================================================
    // 1. 输入合法性校验
    // =========================================================================
    if (!ui.nameInput || ui.shapeComboBox->currentText().isEmpty()) {
        return;
    }

    QString type = ui.shapeComboBox->currentText();
    QString name = ui.nameInput->text();

    // =========================================================================
    // 2. 构建实体元数据与预注册 (Entity Metadata Registration)
    // =========================================================================
    MeshEntity preEntity;
    preEntity.name = name;
    preEntity.type = type;

    // 依据发起调用的 UI 归属划分实体分类
    if (&ui == &m_fragmentUI) preEntity.category = "破片";
    else if (&ui == &m_shellUI) preEntity.category = "壳体";
    else if (&ui == &m_chargeUI) preEntity.category = "装药";
    else preEntity.category = "未分类";

    // 提取所有基础几何参数字典的 Lambda 辅助函数
    auto val = [&](QString key) {
        return ui.paramInputs.contains(key) ? ui.paramInputs[key]->value() : 0.0;
        };

    // 备份参数字典到实体的 geoParams 中
    for (auto it = ui.paramInputs.begin(); it != ui.paramInputs.end(); ++it) {
        preEntity.geoParams[it.key()] = it.value()->value();
    }

    // 核心防丢机制：提前将带有物理语义的空壳实体塞入仓库锁定属性
    m_repository.addEntity(name, preEntity);

    // =========================================================================
    // 3. 提取高级网格控制参数 (Refinement Parameter Extraction)
    // =========================================================================

    // 3.1 竖直方向局部细化 (Z-axis Refinement)
    bool isZRefined = false;
    double zStart = 0.0, zEnd = 0.0, msLocalZ = 0.0;
    if (ui.chkVerticalRefinement && ui.chkVerticalRefinement->isChecked()) {
        isZRefined = true;
        zStart = ui.spinZStart->value();
        zEnd = ui.spinZEnd->value();
        msLocalZ = ui.spinMsLocalZ->value();
    }

    // 3.2 壁厚方向细化 (Wall Thickness Refinement)
    // 默认壁厚网格尺寸与全局基础网格尺寸 (ms) 保持一致
    double msWall = val("ms");
    if (ui.chkWallRefinement && ui.chkWallRefinement->isChecked()) {
        isZRefined = true; // 只要开启了任意一种细化，统一调度到 Refined 生成器中
        msWall = ui.spinMsWall->value();
    }

    // =========================================================================
    // 4. 多态生成器调度分发 (Generator Instantiation & Dispatch)
    // =========================================================================

    if (type == "Cube") {
        CubeGenerator gen;
        gen.setParameters(val("lx"), val("ly"), val("lz"), val("ms"), val("cx"), val("cy"), val("cz"));
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "Cylinder") {
        if (isZRefined) {
            RefinedCylinderGenerator gen;
            gen.setParameters(val("r"), val("ms"), val("h"), val("cx"), val("cy"), val("cz"),
                zStart, zEnd, msLocalZ);
            m_meshManager->buildAndLoad(gen, name);
        }
        else {
            CylinderGenerator gen;
            gen.setParameters(val("r"), val("ms"), val("h"), val("cx"), val("cy"), val("cz"));
            m_meshManager->buildAndLoad(gen, name);
        }
    }
    else if (type == "Frustum") {
        if (isZRefined) {
            RefinedFrustumGenerator gen;
            gen.setParameters(val("rb"), val("rt"), val("h"), val("ms"), val("cx"), val("cy"), val("cz"),
                zStart, zEnd, msLocalZ);
            m_meshManager->buildAndLoad(gen, name);
        }
        else {
            FrustumGenerator gen;
            gen.setParameters(val("rb"), val("rt"), val("h"), val("ms"), val("cx"), val("cy"), val("cz"));
            m_meshManager->buildAndLoad(gen, name);
        }
    }
    else if (type == "CylindricalShell") {
        if (isZRefined) {
            RefinedCylindricalShellGenerator gen;
            double hVoid = val("h") - 2.0 * val("lid"); // 换算空腔高度
            // 注入 Z 轴与壁厚的双重细化参数
            gen.setParameters(val("r"), val("wall"), val("lid"), hVoid, val("ms"),
                val("cx"), val("cy"), val("cz"),
                zStart, zEnd, msLocalZ, msWall);
            m_meshManager->buildAndLoad(gen, name);
        }
        else {
            CylindricalShellGenerator gen;
            gen.setParameters(val("r"), val("h"), val("lid"), val("wall"), val("ms"), val("cx"), val("cy"), val("cz"));
            m_meshManager->buildAndLoad(gen, name);
        }
    }
    else if (type == "FrustumShell") {
        if (isZRefined) {
            RefinedFrustumShellGenerator gen;
            // 传入了全新的 msWall 参数进行厚度控制
            gen.setParameters(val("rb"), val("rt"), val("wall"), val("hcap"), val("hvoid"), val("ms"),
                val("cx"), val("cy"), val("cz"),
                zStart, zEnd, msLocalZ, msWall);
            m_meshManager->buildAndLoad(gen, name);
        }
        else {
            FrustumShellGenerator gen;
            gen.setParameters(val("rb"), val("rt"), val("wall"), val("hcap"), val("hvoid"), val("ms"), val("cx"), val("cy"), val("cz"));
            m_meshManager->buildAndLoad(gen, name);
        }
    }
    else if (type == "OpenCylindricalShell") {
        if (isZRefined) {
            RefinedOpenCylindricalShellGenerator gen;
            gen.setParameters(val("r"), val("wall"), val("h_base"), val("h_wall"), val("ms"),
                val("cx"), val("cy"), val("cz"),
                zStart, zEnd, msLocalZ, msWall);
            m_meshManager->buildAndLoad(gen, name);
        }
        else {
            OpenCylindricalShellGenerator gen;
            gen.setParameters(val("r"), val("wall"), val("h_base"), val("h_wall"), val("ms"), val("cx"), val("cy"), val("cz"));
            m_meshManager->buildAndLoad(gen, name);
        }
    }
    else if (type == "HalfCylindricalShell") {
        if (isZRefined) {
            RefinedHalfCylindricalShellGenerator gen;
            gen.setParameters(val("r"), val("h"), val("lid"), val("wall"), val("ms"),
                val("cx"), val("cy"), val("cz"),
                zStart, zEnd, msLocalZ, msWall);
            m_meshManager->buildAndLoad(gen, name);
        }
        else {
            HalfCylindricalShellGenerator gen;
            gen.setParameters(val("r"), val("h"), val("lid"), val("wall"), val("ms"), val("cx"), val("cy"), val("cz"));
            m_meshManager->buildAndLoad(gen, name);
        }
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
    else if (type == "Fragment Simulating Projectile") {
        FSPGenerator gen;
        gen.setParameters(val("r"), val("hb"), val("hn"), val("rt"), val("ms"), val("cx"), val("cy"), val("cz"));
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "Bullet") {
        BulletGenerator gen;
        // 严格按照封装类 setParameters 的参数顺序传入
        gen.setParameters(
            val("caliber"),
            val("jacketThickness"),
            val("cylinderLength"),
            val("noseLength"),
            val("ms"),
            val("cx"), val("cy"), val("cz"),
            val("tipDiameter")
        );
        m_meshManager->buildAndLoad(gen, name);
        }

    // =========================================================================
    // 5. 记录用户操作日志
    // =========================================================================
    logCommand(QString("GUI Generate [%1]: %2 (Refined: %3)").arg(type, name, isZRefined ? "Yes" : "No"));
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
    m_simSetupUI.endtimeInput->setRange(0, 99999); m_simSetupUI.endtimeInput->setValue(20.0);
    ctrlLayout->addRow("结束时间 (ENDTIM):", m_simSetupUI.endtimeInput);
    m_simSetupUI.endtimeInput->setSuffix(" μs");

    m_simSetupUI.d3plotFreqInput = new QDoubleSpinBox();
    m_simSetupUI.d3plotFreqInput->setRange(0, 9999); m_simSetupUI.d3plotFreqInput->setValue(0.50);

    
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
    m_simSetupUI.icVx->setDecimals(5);
    m_simSetupUI.icVy = new QDoubleSpinBox(); m_simSetupUI.icVy->setRange(-99999, 99999);
    m_simSetupUI.icVy->setSuffix(" cm/μs");
    m_simSetupUI.icVy->setDecimals(5);
    m_simSetupUI.icVz = new QDoubleSpinBox(); m_simSetupUI.icVz->setRange(-99999, 99999);
    m_simSetupUI.icVz->setSuffix(" cm/μs");
    m_simSetupUI.icVz->setDecimals(5);

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

    m_simSetupUI.contactFs = new QDoubleSpinBox(); m_simSetupUI.contactFs->setRange(0, 1); m_simSetupUI.contactFs->setValue(0.10);
    m_simSetupUI.contactFd = new QDoubleSpinBox(); m_simSetupUI.contactFd->setRange(0, 1); m_simSetupUI.contactFd->setValue(0.10);
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
    m_simSetupUI.tssfacInput->setRange(0.1, 1.0); m_simSetupUI.tssfacInput->setValue(0.67); m_simSetupUI.tssfacInput->setSingleStep(0.1);
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
    mainScrollArea->setMinimumWidth(380);

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
        box->setDecimals(10);
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

    m_sensorNodes.clear();

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

/**
 * @brief 处理添加传感器观测点的请求
 * @details 依据用户设定的空间坐标及目标实体，自动搜索欧氏距离最近的网格节点并吸附。
 * 针对单点与阵列模式分别处理，吸附成功后将数据注册至 m_sensorNodes 内存字典，
 * 并将对应的目标实体名称与节点集合通过 UserRole 绑定至 UI 列表项中，以便后续精准撤销。
 */
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

            // 🌟 核心修复：将实体名与节点ID绑定到列表项的后台数据中
            QListWidgetItem* item = new QListWidgetItem(summary);
            item->setData(Qt::UserRole + 1, "SENSOR");
            item->setData(Qt::UserRole + 3, target); // 绑定目标实体名称
            item->setData(Qt::UserRole + 4, QVariantList() << closestIdx); // 绑定节点ID列表

            if (m_simSetupUI.setupSummaryList) m_simSetupUI.setupSummaryList->addItem(item);
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

        QVariantList addedNodes; // 用于记录本次成功吸附的所有节点

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
                addedNodes.append(closestIdx);
                successCount++;
            }
        }

        QString summary = QString("[阵列测点] 实体:%1 | %2个点 | 从(%3,%4,%5)到(%6,%7,%8)")
            .arg(target).arg(successCount).arg(sx).arg(sy).arg(sz).arg(ex).arg(ey).arg(ez);

        // 🌟 核心修复：绑定实体名与阵列节点集合
        QListWidgetItem* item = new QListWidgetItem(summary);
        item->setData(Qt::UserRole + 1, "SENSOR");
        item->setData(Qt::UserRole + 3, target);
        item->setData(Qt::UserRole + 4, addedNodes);

        if (m_simSetupUI.setupSummaryList) m_simSetupUI.setupSummaryList->addItem(item);
        logCommand("Sensor", summary);
    }
}

/**
 * @brief 初始化“求解与仿真试验方案设计”工作区界面 (自动化综合控制台)
 * @details 该函数负责构建后处理模块的 UI 布局，主要包含三个核心部分：
 * 1. 单次求解与控制台监控面板
 * 2. 基于实体级别的网格收敛性智能批处理与研判面板
 * 3. 基于升降法的起爆阈值寻优批处理及序列预览面板
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

    QHBoxLayout* mainHLayout = new QHBoxLayout(singleRunWidget);

    QVBoxLayout* leftVLayout = new QVBoxLayout();

    // ---------------------------------------------------------
    // 左侧上半部分：单次工况提交
    // ---------------------------------------------------------
    QGroupBox* submitGroup = new QGroupBox("单次工况提交 (Single Job)");
    QFormLayout* submitLayout = new QFormLayout(submitGroup);

    kFilePathEdit = new QLineEdit();
    QPushButton* btnBrowseK = new QPushButton("浏览...");
    QHBoxLayout* kLayout = new QHBoxLayout();
    kLayout->addWidget(kFilePathEdit); kLayout->addWidget(btnBrowseK);
    submitLayout->addRow("控制文件 (.k):", kLayout);

    solverPathEdit = new QLineEdit();
    solverPathEdit->setPlaceholderText("D:\\Program Files\\ANSYS Inc\\v241\\ansys\\bin\\winx64\\lsdyna_sp.exe");
    solverPathEdit->setText("D:\\Program Files\\ANSYS Inc\\v241\\ansys\\bin\\winx64\\lsdyna_sp.exe");
    QPushButton* btnBrowseSolver = new QPushButton("浏览...");
    QHBoxLayout* solverLayout = new QHBoxLayout();
    solverLayout->addWidget(solverPathEdit); solverLayout->addWidget(btnBrowseSolver);
    submitLayout->addRow("求解器路径 (EXE):", solverLayout);

    cpuCoresSpin = new QSpinBox();
    cpuCoresSpin->setRange(1, 128); cpuCoresSpin->setValue(8);
    submitLayout->addRow("计算核心数 (NCPU):", cpuCoresSpin);

    btnRunSolver = new QPushButton("▶ 开始单次求解");
    btnRunSolver->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold; min-height: 35px;");
    btnStopSolver = new QPushButton("■ 终止计算");
    btnStopSolver->setStyleSheet("min-height: 35px;");
    btnStopSolver->setEnabled(false);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addWidget(btnRunSolver); btnLayout->addWidget(btnStopSolver);
    submitLayout->addRow("", btnLayout);

    leftVLayout->addWidget(submitGroup); // 把提交框塞入左侧垂直布局的上半截

    // ---------------------------------------------------------
    // 左侧下半部分：求解器进程输出监控区
    // ---------------------------------------------------------
    QGroupBox* monitorGroup = new QGroupBox("计算监控台 (Console)");
    QVBoxLayout* monitorLayout = new QVBoxLayout(monitorGroup);
    solverConsole = new QTextEdit();
    solverConsole->setReadOnly(true);
    solverConsole->setStyleSheet("background-color: #1E1E1E; color: #00FF00; font-family: Consolas;");
    monitorLayout->addWidget(solverConsole);

    leftVLayout->addWidget(monitorGroup, 1); // 把控制台塞入左侧垂直布局，系数1表示让它撑满下方所有空间

    
    mainHLayout->addLayout(leftVLayout, 1);

    // ---------------------------------------------------------
    // 右侧全屏部分：实时监控图表
    setupSolverPlotUI(mainHLayout);

    // 设置左右宽度的黄金分割比例 (1 : 2)
    // 索引0是左侧布局，索引1是刚刚被 setupSolverPlotUI 放进去的图表
    mainHLayout->setStretch(0, 1);
    mainHLayout->setStretch(1, 2); // 让右边图表占据双倍的宽度，视野更宽阔

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
    QGroupBox* meshConvergenceGroup = new QGroupBox("实体级网格收敛性分析");
    QVBoxLayout* meshConvLayout = new QVBoxLayout(meshConvergenceGroup);

    // A.1 实体级网格独立控制表
    QHBoxLayout* tableHeaderLayout = new QHBoxLayout();
    tableHeaderLayout->addWidget(new QLabel("当前物理实体网格控制参数："));
    QPushButton* btnRefreshTable = new QPushButton("刷新读取实体");
    btnRefreshTable->setStyleSheet("background-color: #f0f0f0; font-weight: bold; padding: 4px; min-height: 25px;");
    tableHeaderLayout->addStretch();
    tableHeaderLayout->addWidget(btnRefreshTable);
    meshConvLayout->addLayout(tableHeaderLayout);

    tableMeshSettings = new QTableWidget(0, 4);
    tableMeshSettings->setHorizontalHeaderLabels({ "物理实体名称", "实体类型", "基础网格尺寸(mm)", "网格缩放因子" });
    tableMeshSettings->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tableMeshSettings->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
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
    QPushButton* btnGenerateMeshBatch = new QPushButton("生成网格收敛批处理脚本");
    btnGenerateMeshBatch->setStyleSheet("background-color: #008CBA; color: white; font-weight: bold; min-height: 35px;");
    btnSubmitExistingBat = new QPushButton("提交计算现有批处理文件");
    btnSubmitExistingBat->setStyleSheet("background-color: #FF9800; color: white; font-weight: bold; min-height: 35px;");
    QPushButton* btnAnalyzeConvergence = new QPushButton("读取网格收敛性结果");
    btnAnalyzeConvergence->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold; min-height: 35px;");
    btnStopMeshBatch = new QPushButton("停止当前计算");
    btnStopMeshBatch->setStyleSheet("background-color: #f44336; color: white; font-weight: bold; min-height: 35px;");

    meshBtnLayout->addWidget(btnGenerateMeshBatch);
    meshBtnLayout->addWidget(btnSubmitExistingBat);
    meshBtnLayout->addWidget(btnAnalyzeConvergence);
    meshBtnLayout->addWidget(btnStopMeshBatch);
    meshConvLayout->addLayout(meshBtnLayout);

    hSplitLayout->addWidget(meshConvergenceGroup, 6);

    meshMonitorConsole = new QTextEdit();
    meshMonitorConsole->setReadOnly(true);
    meshMonitorConsole->setPlaceholderText("等待任务提交... 实时输出将显示在此");
    meshMonitorConsole->setStyleSheet("background-color: #1e1e1e; color: #d4d4d4; font-family: 'Consolas'; font-size: 10pt;");
    meshMonitorConsole->setMinimumHeight(120);
    meshConvLayout->addWidget(meshMonitorConsole);

    QGroupBox* meshMonitorGroup = new QGroupBox("计算过程实时特征监控");
    QVBoxLayout* meshMonitorLayout = new QVBoxLayout(meshMonitorGroup);

    meshConvergencePlot = new QCustomPlot();
    meshConvergencePlot->setMinimumHeight(250);
    meshConvergencePlot->addGraph();
    meshConvergencePlot->graph(0)->setPen(QPen(Qt::cyan, 2));
    meshConvergencePlot->xAxis->setLabel("时间 (Time)");
    meshConvergencePlot->yAxis->setLabel("能量值");
    meshConvergencePlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    meshMonitorLayout->addWidget(meshConvergencePlot);

    meshConvLayout->addWidget(meshMonitorGroup);

    // ---------------------------------------------------------
    // 右半区 (模块 B): 起爆阈值闭环寻优操作面板
    // ---------------------------------------------------------
    QGroupBox* velSetupGroup = new QGroupBox("起爆阈值升降法批处理分析");
    QVBoxLayout* velContainerLayout = new QVBoxLayout(velSetupGroup);

    // B.1 实体级初速读取控制区
    QHBoxLayout* velHeaderLayout = new QHBoxLayout();
    velHeaderLayout->addWidget(new QLabel("当前物理模型预设初速："));
    btnRefreshVelocity = new QPushButton("刷新读取初速");
    btnRefreshVelocity->setStyleSheet("background-color: #f0f0f0; font-weight: bold; padding: 4px; min-height: 25px;");
    velHeaderLayout->addStretch();
    velHeaderLayout->addWidget(btnRefreshVelocity);
    velContainerLayout->addLayout(velHeaderLayout);

    // B.2 参数配置表单布局 
    QFormLayout* formLayout = new QFormLayout();

    spinStartVelocity = new QDoubleSpinBox();
    spinStartVelocity->setRange(-10.0, 10.0);
    spinStartVelocity->setDecimals(4);
    spinStartVelocity->setValue(0.08);
    spinStartVelocity->setSuffix(" cm/μs");

    spinVelocityStep = new QDoubleSpinBox();
    spinVelocityStep->setRange(0.0001, 1.0);
    spinVelocityStep->setDecimals(4);
    spinVelocityStep->setValue(0.005);
    spinVelocityStep->setSuffix(" cm/μs");

    spinMaxSteps = new QSpinBox();
    spinMaxSteps->setRange(1, 100);
    spinMaxSteps->setValue(20);

    formLayout->addRow("初始测试撞击速度 (V0):", spinStartVelocity);
    formLayout->addRow("升降法速度变分步长 (ΔV):", spinVelocityStep);
    formLayout->addRow("最大有效测试总次数 (N):", spinMaxSteps);

    velContainerLayout->addLayout(formLayout);

    // B.3 动作控制总线布局 
    QHBoxLayout* thresholdBtnLayout = new QHBoxLayout();

    btnGenerateThreshold = new QPushButton("生成基础模型");
    btnSubmitThreshold = new QPushButton("提交并启动");
    btnTerminateProcess = new QPushButton("终止进程");
    btnSkipStep = new QPushButton("跳过当前计算工况");

    btnGenerateThreshold->setStyleSheet("background-color: #008CBA; color: white; font-weight: bold; min-height: 35px;");
    btnSubmitThreshold->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold; min-height: 35px;");
    btnTerminateProcess->setStyleSheet("background-color: #d9534f; color: white; font-weight: bold; min-height: 35px;");
    btnSkipStep->setStyleSheet("background-color: #f0ad4e; color: white; font-weight: bold; min-height: 35px;");

    thresholdBtnLayout->addWidget(btnGenerateThreshold);
    thresholdBtnLayout->addWidget(btnSubmitThreshold);
    thresholdBtnLayout->addWidget(btnSkipStep);
    thresholdBtnLayout->addWidget(btnTerminateProcess);

    velContainerLayout->addLayout(thresholdBtnLayout);

    // B.4 专属集成式控制台 (Console)
    QLabel* consoleLabel = new QLabel("当前寻优工况实时监控:");
    consoleLabel->setStyleSheet("font-weight: bold; color: #555; margin-top: 10px;");
    velContainerLayout->addWidget(consoleLabel);

    thresholdConsole = new QTextEdit();
    thresholdConsole->setReadOnly(true);
    thresholdConsole->setStyleSheet("background-color: #1e1e1e; color: #d4d4d4; font-family: 'Consolas', 'Monaco', monospace; font-size: 10pt;");
    velContainerLayout->addWidget(thresholdConsole, 1);

    hSplitLayout->addWidget(velSetupGroup, 4);

    batchMainLayout->addLayout(hSplitLayout);

    scrollArea->setWidget(batchWidget);
    solveTaskTabs->addTab(scrollArea, "自动化批处理与分析");

    // =========================================================
    // 统一信号与槽绑定
    // =========================================================

    // 常规求解与后处理信号
    connect(btnBrowseK, &QPushButton::clicked, this, &MainWindow::browseKFile);
    connect(btnBrowseSolver, &QPushButton::clicked, this, &MainWindow::browseSolver);
    connect(btnRunSolver, &QPushButton::clicked, this, &MainWindow::startCalculation);
    connect(btnStopSolver, &QPushButton::clicked, this, &MainWindow::stopCalculation);

    // 自动化批处理专属信号
    connect(btnRefreshTable, &QPushButton::clicked, this, &MainWindow::handleRefreshEntityTable);
    connect(btnGenerateMeshBatch, &QPushButton::clicked, this, &MainWindow::handleGenerateMeshConvergenceBatch);
    connect(btnAnalyzeConvergence, &QPushButton::clicked, this, &MainWindow::handleAnalyzeConvergence);

    connect(btnGenerateThreshold, &QPushButton::clicked, this, &MainWindow::handleGenerateThresholdFiles);
    connect(btnSubmitThreshold, &QPushButton::clicked, this, &MainWindow::handleSubmitThresholdTask);
    connect(btnTerminateProcess, &QPushButton::clicked, this, &MainWindow::handleTerminateProcess);
    connect(btnRefreshVelocity, &QPushButton::clicked, this, &MainWindow::handleRefreshInitialVelocity);

    //绑定跳过按钮的槽函数
    connect(btnSkipStep, &QPushButton::clicked, this, &MainWindow::handleSkipCurrentStep);

    connect(btnSubmitExistingBat, &QPushButton::clicked, this, &MainWindow::handleRunExistingBat);
    connect(comboTargetMetric, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onMeshMonitorMetricChanged);
    m_meshMonitorTimer = new QTimer(this);
    connect(m_meshMonitorTimer, &QTimer::timeout, this, &MainWindow::updateMeshConvergencePlot);
    connect(btnStopMeshBatch, &QPushButton::clicked, this, &MainWindow::handleStopMeshBatch);
}


/**
 * @brief 启动单次求解计算任务
 * * 该函数用于配置并异步启动 LS-DYNA 求解器进程。
 * 核心流程包括：验证输入路径、继承并注入环境变量(解决 DLL 缺失问题)、
 * 切换工作目录(规避路径空格问题)，以及封装启动参数。
 */
void MainWindow::startCalculation() {
    QString kFile = kFilePathEdit->text();
    QString solver = solverPathEdit->text();

    if (kFile.isEmpty() || solver.isEmpty()) {
        solverConsole->append("<b><font color='red'>[错误] 请先选择关键字(.k)文件和求解器可执行文件路径！</font></b>");
        return;
    }

    QFileInfo kFileInfo(kFile);

    m_lastSolverGlstatPos = 0;
    m_lastSolverNodoutPos = 0;
    m_lastSolverEloutPos = 0;
    m_rtGlstatTime.clear(); m_rtGlstatKe.clear(); m_rtGlstatIe.clear();
    m_rtNodoutTimeMap.clear(); m_rtNodoutData.clear();
    m_rtEloutTimeMap.clear(); m_rtEloutData.clear();
    m_rtNodoutTime = 0.0; m_rtNodoutIsDataBlock = false;
    m_rtEloutTime = 0.0; m_rtEloutId = -1; m_rtEloutBlock = 0;

    if (m_solverPlotTimer) m_solverPlotTimer->start(1000);

    // 1. 配置求解器运行所需的系统环境变量
    // 获取当前系统环境，并将求解器所在目录及用户自定义依赖目录(m_dynaEnvPath)前置追加至 PATH，
    // 以防止求解器因子进程无法定位 Fortran/MPI 等动态链接库而发生异常退出。
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString solverDir = QFileInfo(solver).absolutePath();

    QString newPath = QDir::toNativeSeparators(solverDir) + ";";
    if (!m_dynaEnvPath.isEmpty()) {
        newPath += QDir::toNativeSeparators(m_dynaEnvPath) + ";";
    }
    newPath += env.value("PATH");
    env.insert("PATH", newPath);

    m_solverProcess->setProcessEnvironment(env);

    // 合并标准输出(stdout)与标准错误(stderr)通道，便于 UI 终端统一截获与监控日志
    m_solverProcess->setProcessChannelMode(QProcess::MergedChannels);

    // 2. 配置进程工作目录与命令行参数
    // 将工作目录切换至输入文件所在层级，命令行参数仅传递纯文件名。
    // 此举可有效规避 LS-DYNA 引擎处理带空格绝对路径时产生的 IO 解析异常。
    m_solverProcess->setWorkingDirectory(kFileInfo.absolutePath());

    QStringList arguments;
    arguments << QString("I=%1").arg(kFileInfo.fileName());
    arguments << QString("NCPU=%1").arg(cpuCoresSpin->value());
    arguments << "MEMORY=200m"; // 预设内存参数，可根据需要调整

    // 3. 更新 UI 监控终端信息
    solverConsole->clear();
    solverConsole->append(QString("<b><font color='yellow'>[系统] 正在初始化求解进程...</font></b>"));
    solverConsole->append(QString("工作目录: %1").arg(kFileInfo.absolutePath()));
    solverConsole->append(QString("执行指令: %1 %2").arg(solver).arg(arguments.join(" ")));
    solverConsole->append("--------------------------------------------------");

    // 4. 启动计算进程并执行状态校验
    m_solverProcess->start(solver, arguments);

    // 阻塞当前线程最多 3000ms，以验证底层的系统调用是否真实拉起目标进程
    if (!m_solverProcess->waitForStarted(3000)) {
        solverConsole->append(QString("<b><font color='red'>[致命错误] 求解进程拉起失败: %1</font></b>")
            .arg(m_solverProcess->errorString()));
        return;
    }

    // 5. 刷新界面相关控件的状态
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
    if (m_solverPlotTimer) m_solverPlotTimer->stop();
}

void MainWindow::handleSolverFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    btnRunSolver->setEnabled(true);
    btnStopSolver->setEnabled(false);

    if (m_solverPlotTimer) m_solverPlotTimer->stop();
    updateSolverPlot();

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

/**
 * @brief 右键呼出仿真参数摘要列表的上下文菜单
 * @details 支持用户查看特定的卡片关键字，或执行精准撤销操作。
 * 在删除时，自动根据隐藏于项内部的 UserRole 标识判断类型：若是 CARD 则从 Deck 注销，
 * 若是 SENSOR 则从 m_sensorNodes 内存字典中定点剔除对应的节点。
 */
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
        // =========================================================
        // 🌟 核心修复：精准清理内存字典中残留的幽灵传感器数据
        // =========================================================
        else if (type == "SENSOR") {
            QString targetEntity = item->data(Qt::UserRole + 3).toString();
            QVariantList nodesToRemove = item->data(Qt::UserRole + 4).toList();

            if (m_sensorNodes.contains(targetEntity)) {
                // 遍历当时添加的所有节点，逐一移除
                for (const QVariant& nodeVar : nodesToRemove) {
                    m_sensorNodes[targetEntity].removeOne(nodeVar.toInt());
                }
                // 若该实体下已被完全清空，顺手销毁此实体的 Key 以保持内存洁净
                if (m_sensorNodes[targetEntity].isEmpty()) {
                    m_sensorNodes.remove(targetEntity);
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
    else if (type == "Bullet") {
        BulletGenerator gen;
        gen.setParameters(
            p["caliber"],
            p["jacketThickness"],
            p["cylinderLength"],
            p["noseLength"],
            newMeshSize,
            p["cx"], p["cy"], p["cz"],
            p["tipDiameter"]
        );
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
 * @brief 生成网格收敛性批处理任务脚本及控制文件，并调度后台进程执行 (多目录隔离架构版)
 * @details 提取实体控制表参数，迭代计算目标网格尺寸并重构。
 * 针对每个收敛步，在主工作目录下创建独立的子工作空间 (Sub-directory) 以避免 LS-DYNA 默认输出文件 (d3plot等) 的覆盖碰撞。
 * 生成基于相对路径跳转的 Windows 批处理脚本，并隐式唤醒 QProcess 调度执行。
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

    QString solverPath = solverPathEdit->text().isEmpty() ? "D:\\Program Files\\ANSYS Inc\\v241\\ansys\\bin\\winx64\\lsdyna_sp.exe" : solverPathEdit->text();
    batStream << "@echo off\nset DYNA_PATH=\"" << solverPath << "\"\n\n";

    QProgressDialog progress("正在生成多梯度网格隔离工作区...", "取消", 0, steps, this);
    progress.setWindowModality(Qt::WindowModal);

    // 4. 执行网格迭代重构与主控文件组装
    for (int i = 0; i < steps; ++i) {
        progress.setValue(i);
        if (progress.wasCanceled()) break;

        // 4.1 核心：为当前收敛步创建独立的子目录空间
        QString stepFolderName = QString("Step_%1").arg(i + 1);
        QDir rootDir(m_workingDirectory);
        if (!rootDir.exists(stepFolderName)) {
            rootDir.mkpath(stepFolderName);
        }
        QString stepDirPath = rootDir.filePath(stepFolderName);

        QString stepInfo = QString("Step %1 : ").arg(i + 1);
        // 4.2 遍历并重构所有实体的网格
        for (const auto& s : settings) {
            MeshEntity* mutableEntity = m_repository.getMutableEntity(s.name);
            double currentSize = s.baseSize * std::pow(s.factor, i);
            remeshEntityWithNewSize(mutableEntity, currentSize);
            stepInfo += QString("[%1: %2mm] ").arg(s.name).arg(currentSize, 0, 'f', 4);
        }

        QString jobName = QString("MeshConv_Step%1").arg(i + 1);

        // 4.3 将纯几何网格数据导出至对应的子目录中
        QString meshFileName = jobName + "_mesh.k";
        exportToKFile(QDir(stepDirPath).filePath(meshFileName));

        // 4.4 构建主控文件 (Master Control Deck) 并写入子目录
        QString controlFileName = jobName + "_run.k";
        QFile controlFile(QDir(stepDirPath).filePath(controlFileName));

        if (controlFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&controlFile);
            out << "*KEYWORD\n*TITLE\n" << jobName << "\n";
            out << "*INCLUDE\n" << meshFileName << "\n";
            if (m_globalControlCard != nullptr) {
                out << QString::fromStdString(m_globalControlCard->to_string());
            }
            out << QString::fromStdString(m_deck.generateDeck());
            out << "*END\n";
            controlFile.close();
        }

        // 4.5 写入批处理队列：切换至子目录 (cd) -> 执行计算 -> 返回上级目录 (cd ..)
        batStream << "echo ========================================\n";
        batStream << "echo Running " << stepInfo << "\n";
        batStream << "cd " << stepFolderName << "\n";
        batStream << "%DYNA_PATH% I=" << controlFileName << " NCPU=" << cpuCoresSpin->value() << " MEMORY=2000m\n";
        batStream << "cd ..\n\n";
    }

    batStream << "echo All Mesh Convergence Jobs Finished!\n";
    batFile.close();
    progress.setValue(steps);
    glWidget->update();

    // =========================================================
    // 5. 批处理进程自动调度与执行
    // =========================================================
    if (!m_meshBatchProcess) {
        m_meshBatchProcess = new QProcess(this);
        connect(m_meshBatchProcess, &QProcess::readyReadStandardOutput, this, &MainWindow::updateMeshMonitorConsole);
        connect(m_meshBatchProcess, &QProcess::readyReadStandardError, this, &MainWindow::updateMeshMonitorConsole);
    }

    if (m_meshBatchProcess->state() == QProcess::Running) {
        QMessageBox::warning(this, "资源冲突", "后台计算引擎正在运行中，请等待当前任务结束后再提交。");
        return;
    }

    meshMonitorConsole->clear();
    meshMonitorConsole->append("========================================");
    meshMonitorConsole->append("[系统提示] 开始执行实体级网格收敛性批处理队列...");
    meshMonitorConsole->append("[系统提示] 主工作目录: " + m_workingDirectory);
    meshMonitorConsole->append("========================================\n");

    // 重置并激活 QCustomPlot 绘图与定时器
    m_currentMeshStepToMonitor = 1;
    m_lastGlstatPos = 0;
    meshConvergencePlot->graph(0)->data()->clear();

    // 格式化路径，防止空格导致 cmd.exe 闪退
    m_meshBatchProcess->setWorkingDirectory(m_workingDirectory);
    QString safeBatPath = "\"" + QDir::toNativeSeparators(batFilePath) + "\"";
    m_meshBatchProcess->start("cmd.exe", QStringList() << "/c" << safeBatPath);

    m_meshMonitorTimer->start(1000);
}

/**
 * @brief 执行网格收敛性结果的批量读取与多曲线汇总绘制
 * @details
 * 1. 自动遍历 Step_1 至 Step_N 文件夹，无需依赖 UI 实体控制列表。
 * 2. 从 matsum 文件的 {BEGIN LEGEND} 区块中动态提取实体 ID 与名称。
 * 3. 从结果文件首行提取计算工况标题（通常包含网格尺寸定义）。
 * 4. 自动为每个迭代步创建独立图层，并在图例中整合步长、实体名及网格参数。
 */
 /**
  * @brief 批量解析网格收敛性结果并执行多实体多步长对比绘图
  * @details
  * 1. 自动遍历 Step 文件夹，直接从 matsum 文件中提取实体定义与物理参数。
  * 2. 支持在一个图表中叠加绘制所有 Step 中所有实体的时域曲线。
  * 3. 针对大规模数据文件（>100MB），通过 QCoreApplication::processEvents() 保持 UI 响应。
  * 4. 自动计算合速度（Resultant Velocity）并适配 glstat 风格的点阵数据格式。
  */
void MainWindow::handleAnalyzeConvergence() {
    if (m_workingDirectory.isEmpty()) {
        QMessageBox::warning(this, "路径缺失", "请设置工作目录。");
        return;
    }

    // 1. 自动探测 Step 文件夹数量 (解决读取不到第4个文件夹的问题)
    int actualSteps = 0;
    while (QDir(m_workingDirectory).exists(QString("Step_%1").arg(actualSteps + 1))) {
        actualSteps++;
    }

    if (actualSteps == 0) {
        QMessageBox::warning(this, "结果缺失", "未在工作目录下找到任何 Step_X 文件夹。");
        return;
    }

    int metricIndex = comboTargetMetric->currentIndex();
    double tolerance = spinTolerance->value() / 100.0;

    if (meshConvergencePlot) {
        meshConvergencePlot->clearGraphs();
        meshConvergencePlot->legend->setVisible(true);
        meshConvergencePlot->legend->setFont(QFont(font().family(), 8));
    }

    QList<QColor> colorPool = { Qt::red, Qt::blue, Qt::green, Qt::magenta, Qt::darkCyan, Qt::darkYellow, Qt::gray };
    int colorIdx = 0;

    // 存储结构：Map<实体名, Map<Step编号, DataPair>>
    struct CurveData { QVector<double> t; QVector<double> v; };
    QMap<QString, QMap<int, CurveData>> allEntityData;

    QProgressDialog progress("正在执行全量曲线误差分析...", "取消", 0, actualSteps, this);
    progress.setWindowModality(Qt::WindowModal);

    for (int i = 0; i < actualSteps; ++i) {
        progress.setValue(i);
        QCoreApplication::processEvents();
        if (progress.wasCanceled()) break;

        QString stepName = QString("Step_%1").arg(i + 1);
        QDir stepDir(QDir(m_workingDirectory).filePath(stepName));
        QString matsumPath = stepDir.filePath("matsum");

        QFile file(matsumPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;

        QTextStream in(&file);
        QString meshTitle = in.readLine().trimmed();
        QMap<int, QString> entityNames;
        bool inLegend = false;
        double currentTime = -1.0;

        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.isEmpty()) continue;

            if (line.contains("{BEGIN LEGEND}")) { inLegend = true; continue; }
            if (line.contains("{END LEGEND}")) { inLegend = false; continue; }
            if (inLegend && !line.contains("Entity #")) {
                QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
                if (parts.size() >= 2) entityNames[parts[0].toInt()] = parts[1];
                continue;
            }

            QString lowerLine = line.toLower();
            if (lowerLine.startsWith("time =")) {
                currentTime = lowerLine.section('=', 1).trimmed().toDouble();
            }
            else if (currentTime >= 0.0 && lowerLine.startsWith("mat.#=")) {
                int matId = lowerLine.split(QRegularExpression("[\\s=]+"), Qt::SkipEmptyParts)[1].toInt();
                QString name = entityNames.value(matId, QString("Mat_%1").arg(matId));

                double val = 0.0;
                if (metricIndex == 2) { // 速度
                    in.readLine(); in.readLine(); // 跳过 mom 行和进入 rbv 行
                    QString rbvLine = in.readLine().trimmed().toLower();
                    if (rbvLine.startsWith("x-rbv")) {
                        QStringList vP = rbvLine.split(QRegularExpression("[\\s=]+"), Qt::SkipEmptyParts);
                        if (vP.size() >= 6) val = qSqrt(qPow(vP[1].toDouble(), 2) + qPow(vP[3].toDouble(), 2) + qPow(vP[5].toDouble(), 2));
                    }
                }
                else { // 能量
                    QString key = (metricIndex == 0) ? "inten=" : "kinen=";
                    if (lowerLine.contains(key)) val = lowerLine.section(key, 1).trimmed().split(" ").first().toDouble();
                }
                allEntityData[name][i + 1].t.append(currentTime);
                allEntityData[name][i + 1].v.append(val);
            }
        }
        file.close();
    }

    // 2. 绘图与基于曲线的相对误差计算
    QString report = QString("【网格收敛性分析报告 - 自动探测到 %1 个工况】\n").arg(actualSteps);
    report += "误差标准：全时域曲线 L2 范数相对偏差\n--------------------------------------------------\n";

    QMapIterator<QString, QMap<int, CurveData>> entIt(allEntityData);
    while (entIt.hasNext()) {
        entIt.next();
        QString entName = entIt.key();
        report += QString("\n实体: %1\n").arg(entName);

        QMap<int, CurveData> stepsData = entIt.value();
        for (int i = 1; i <= actualSteps; ++i) {
            if (!stepsData.contains(i)) continue;

            // 绘制曲线
            QCPGraph* graph = meshConvergencePlot->addGraph();
            graph->setPen(QPen(colorPool[colorIdx % colorPool.size()], 1.5));
            graph->setName(QString("Step %1 | %2").arg(i).arg(entName));
            graph->setData(stepsData[i].t, stepsData[i].v);
            colorIdx++;

            // 曲线误差对比 (当前步与前一步)
            if (i > 1 && stepsData.contains(i - 1)) {
                const CurveData& cPrev = stepsData[i - 1];
                const CurveData& cCurr = stepsData[i];

                // 计算全时域误差：Sum(|V_curr - V_prev| * dt) / Sum(|V_prev| * dt)
                double diffIntegral = 0;
                double baseIntegral = 0;

                // 使用较细步长的曲线作为采样基准进行线性插值对比
                for (int k = 1; k < cCurr.t.size(); ++k) {
                    double t = cCurr.t[k];
                    double dt = t - cCurr.t[k - 1];
                    double vCurr = cCurr.v[k];

                    // 在前一步曲线中寻找相同时间点的插值
                    double vPrev = 0;
                    if (t <= cPrev.t.last()) {
                        auto it = std::lower_bound(cPrev.t.begin(), cPrev.t.end(), t);
                        int idx = std::distance(cPrev.t.begin(), it);
                        if (idx > 0 && idx < cPrev.t.size()) {
                            double t0 = cPrev.t[idx - 1], t1 = cPrev.t[idx];
                            double v0 = cPrev.v[idx - 1], v1 = cPrev.v[idx];
                            vPrev = v0 + (v1 - v0) * (t - t0) / (t1 - t0);
                        }
                        else vPrev = cPrev.v[idx];
                    }

                    diffIntegral += std::abs(vCurr - vPrev) * dt;
                    baseIntegral += std::abs(vPrev) * dt;
                }

                double curveError = diffIntegral / (baseIntegral + 1e-12);
                report += QString(" - Step %1 vs %2 曲线相对偏差: %3%\n").arg(i - 1).arg(i).arg(curveError * 100.0, 0, 'f', 2);
                if (curveError <= tolerance) report += "   [结果] 曲线形态已收敛\n";
            }
        }
    }

    if (meshConvergencePlot) {
        meshConvergencePlot->rescaleAxes();
        meshConvergencePlot->replot();
    }
    QMessageBox::information(this, "分析完成", report);
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

/**
 * @brief 实时读取后台批处理进程的输出流，并重定向至 GUI 控制台
 */
void MainWindow::handleBatchProcessOutput() {
    if (!m_batchProcess) return;

    QByteArray outputData = m_batchProcess->readAllStandardOutput();
    QString outputStr = QString::fromLocal8Bit(outputData);

    // 🌟 根据模式路由日志
    if (m_isUpAndDownMode && thresholdConsole) {
        thresholdConsole->append(outputStr);
        QScrollBar* scrollBar = thresholdConsole->verticalScrollBar();
        if (scrollBar) scrollBar->setValue(scrollBar->maximum());
    }
    else if (solverConsole) {
        solverConsole->append(outputStr);
        QScrollBar* scrollBar = solverConsole->verticalScrollBar();
        if (scrollBar) scrollBar->setValue(scrollBar->maximum());
    }
}
/**
 * @brief LS-DYNA 批处理求解完成后的异步回调槽函数 (处理起爆阈值寻优逻辑)
 * * @param exitCode 求解器进程退出的状态码
 * @param exitStatus 求解器进程的退出状态 (正常退出/崩溃等)
 * * @details
 * 该函数是“升降法/二分法”寻优引擎的中央大脑。每次 LS-DYNA 求解完毕后触发。
 * 其核心执行流程分为四个阶段：
 * 0. [脱机拦截]：检测用户是否强行终止进程，若是则安全切断闭环序列。
 * 1. [状态研判]：结合早退旗标 (Early Detonation/Misfire) 与结果文件，综合判定当前工况是否发生起爆。
 * 2. [阈值收敛]：基于纯标量模长 (Magnitude) 的状态机，动态收紧上下限 (Upper/Lower Bound)，
 * 并计算出下一次迭代的目标合速度。该算法严格保证了求解空间不出现负速度，彻底解耦了速度大小与冲击夹角。
 * 3. [流程驱动]：判断是否满足收敛容差或达到最大迭代步数。若未结束，则将状态传至下一轮并唤醒组装函数。
 */
void MainWindow::handleBatchProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    // ==============================================================
    // 阶段 0：脱机拦截（处理用户在 UI 界面中途强行终止的情况）
    // ==============================================================
    if (!m_isUpAndDownMode) {
        if (thresholdConsole) {
            thresholdConsole->append("   [系统指令] 检测到用户已强行脱机，寻优闭环序列彻底终止。");
        }
        if (btnSkipStep) btnSkipStep->setEnabled(false);
        return;
    }

    // ==============================================================
    // 阶段 1：求解器运行状态与起爆结果综合研判
    // ==============================================================

    // 1.1 检查求解器是否异常崩溃
    if (exitStatus == QProcess::CrashExit) {
        thresholdConsole->append("   [致命错误] LS-DYNA 求解器发生异常崩溃！已强制终止寻优流程。");
        QMessageBox::critical(this, "求解器崩溃", "LS-DYNA 进程异常终止，请检查 K 文件网格质量或接触设置！");
        m_isUpAndDownMode = false; // 关闭寻优状态
        return;
    }

    // 1.2 综合判定本轮是否起爆 (结合实时监控探针的早退旗标)
    bool isDetonated = false;

    if (m_isEarlyDetonated) {
        thresholdConsole->append("   [状态判定] 实时探针已在求解中途捕获到起爆突跃 (Early Detonated) -> 判定为【起爆】(X)");
        isDetonated = true;
    }
    else if (m_isEarlyMisfire) {
        thresholdConsole->append("   [状态判定] 实时探针侦测到内能连续衰减且无突跃 (Early Misfire) -> 判定为【死火】(O)");
        isDetonated = false;
    }
    else {
        // [预留扩展口]：若求解器跑完全程且未触发早退机制，则需读取 d3plot 或 elout/glstat 的最终反应度进行兜底判定
        // 此处暂时默认：如果跑完全程都没触发起爆早退，则判定为死火
        thresholdConsole->append("   [状态判定] 求解跑完全程，未触发起爆突跃 -> 判定为【死火】(O)");
        isDetonated = false;
    }

    // ==============================================================
    // 阶段 2：核心智能控制律 —— 纯标量域的步长搜索与二分收敛
    // ==============================================================

    /**
     * @note 标量域寻优算法说明
     * currentMag 严格代表合速度的绝对标量值 (Magnitude)。
     * 取消向量正负号后，寻优逻辑完美符合标准的“单调函数二分求根”模型。
     */
    double currentMag = m_upDownCurrentVelocity;

    // 2.1 动态收紧二分边界
    if (isDetonated) {
        // 当前发生起爆，说明速度偏高。该速度成为新的“起爆上限”
        if (m_upperBoundMag < 0 || currentMag < m_upperBoundMag) {
            m_upperBoundMag = currentMag;
        }
    }
    else {
        // 当前发生死火，说明速度偏低。该速度成为新的“死火下限”
        if (m_lowerBoundMag < 0 || currentMag > m_lowerBoundMag) {
            m_lowerBoundMag = currentMag;
        }
    }

    double nextMag = 0.0;
    bool isConverged = false;

    // 2.2 阶段 2A：二分收敛阶段 (已成功同时捕获上限与下限)
    if (m_lowerBoundMag >= 0 && m_upperBoundMag >= 0) {
        nextMag = (m_lowerBoundMag + m_upperBoundMag) / 2.0;

        thresholdConsole->append(QString("   [二分法逼近] 阈值边界已锁定在 V ∈ [%1, %2] cm/μs，提取中值测试。")
            .arg(m_lowerBoundMag, 0, 'f', 4).arg(m_upperBoundMag, 0, 'f', 4));

        // 收敛截断准则 (Tolerance: 0.5 m/s 即 0.00005 cm/us)
        if (std::abs(m_upperBoundMag - m_lowerBoundMag) <= 0.00005) {
            thresholdConsole->append("   [★★★ 寻优成功 ★★★] 边界误差已达到收敛容差限制 (≤ 0.00005 cm/μs)。");
            thresholdConsole->append(QString("   最终临界起爆速度 V50 ≈ %1 cm/μs").arg(nextMag, 0, 'f', 5));
            isConverged = true;
        }
    }
    // 2.3 阶段 2B：盲搜外推阶段 (只有单侧边界，按用户设定的固定步长探测另一侧边界)
    else if (isDetonated) {
        // 当前起爆，但没有下限 -> 持续减速以寻找死火点
        nextMag = currentMag - m_upDownStepSize;
        thresholdConsole->append(QString("   [步长搜索] 持续减速以探寻死火下限边界，步进量: -%1 cm/μs").arg(m_upDownStepSize));
    }
    else {
        // 当前死火，但没有上限 -> 持续加速以寻找起爆点
        nextMag = currentMag + m_upDownStepSize;
        thresholdConsole->append(QString("   [步长搜索] 持续加速以探寻起爆上限边界，步进量: +%1 cm/μs").arg(m_upDownStepSize));
    }

    // 2.4 物理下限保护与容错截断
    // 破片速度的模长绝对不可能为负数或0，强制拉回极小正数以防止方向反转或除零崩溃
    if (nextMag <= 0.0) {
        thresholdConsole->append("   [严重警告] 迭代计算出的目标速度逼近 0 或产生负数，已自动重置为极小正值截断 (0.0001 cm/μs)。");
        nextMag = 0.0001;
    }

    // ==============================================================
    // 阶段 3：流程驱动与下一轮调度
    // ==============================================================

    // 清理本轮早退状态，为下一轮做准备
    m_isEarlyDetonated = false;
    m_isEarlyMisfire = false;

    // 判断终止条件：已收敛，或已达到最大允许迭代次数
    if (isConverged || m_upDownCurrentStep >= m_upDownMaxSteps) {

        if (!isConverged) {
            thresholdConsole->append("   [寻优终止] 已达到用户设定的最大迭代次数，算法被强制截断。");
        }

        // 恢复 UI 控件状态，安全退出寻优模式
        if (btnSkipStep) btnSkipStep->setEnabled(false);
        m_isUpAndDownMode = false;

        QMessageBox::information(this, "寻优完成", "起爆速度阈值寻优任务已顺利结束！\n请查看终端日志获取 V50 数据。");
    }
    else {
        // 尚未结束，将新计算出的标量速度挂载到全局变量
        m_upDownCurrentVelocity = nextMag;

        thresholdConsole->append(QString("\n>>> 准备启动第 %1 / %2 轮迭代计算... >>>")
            .arg(m_upDownCurrentStep + 1).arg(m_upDownMaxSteps));

        // 调度下一轮计算 (重构 K 文件并启动 LS-DYNA)
        // [注]：此函数内部会安全执行 m_upDownCurrentStep 的递增
        executeNextUpAndDownStep();
    }
}

/**
 * @brief 组装并调度寻优序列的单步求解工况
 * @details 该函数在生成 *.k 文件落盘前，使用底层文本流拦截技术执行两项硬覆写：
 * 1. 将控制卡片与实体卡片合并扫描，拦截 *INITIAL_VELOCITY_GENERATION，重构目标初速。
 * 2. 拦截 *DATABASE_MATSUM 并结合 *CONTROL_TERMINATION，按总时长比例 (1/1000)
 * 自适应动态调节采样步长，确保探针在任意时长的工况中均具备足够且均匀的解析密度。
 */
void MainWindow::executeNextUpAndDownStep() {
    if (!thresholdConsole) return;

    m_upDownCurrentStep++;

    thresholdConsole->append(QString("\n[调度执行] 第 %1/%2 帧迭代 | 目标初速: %3 cm/μs")
        .arg(m_upDownCurrentStep).arg(m_upDownMaxSteps).arg(m_upDownCurrentVelocity, 0, 'f', 4));

    QString jobName = QString("VelocityOpt_Step%1").arg(m_upDownCurrentStep);
    QString stepFolderName = QString("Step_%1").arg(m_upDownCurrentStep);
    QDir rootDir(m_workingDirectory);

    if (!rootDir.exists(stepFolderName)) {
        rootDir.mkpath(stepFolderName);
    }
    m_upDownCurrentDir = rootDir.filePath(stepFolderName);

    // 1. 序列化关键字文件 (*.k)
    QString controlFileName = jobName + "_run.k";
    QFile controlFile(QDir(m_upDownCurrentDir).filePath(controlFileName));
    if (controlFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&controlFile);
        out << "*KEYWORD\n*TITLE\n" << jobName << "\n";
        out << "*INCLUDE\n../shared_mesh_geometry.k\n";

        // =====================================================================
        // 核心技术：合并卡片流并执行底层文本拦截与覆写
        // =====================================================================
        QString fullDeckText = "";
        if (m_globalControlCard != nullptr) {
            fullDeckText += QString::fromStdString(m_globalControlCard->to_string());
            if (!fullDeckText.endsWith("\n")) fullDeckText += "\n";
        }
        fullDeckText += QString::fromStdString(m_deck.generateDeck());

        QStringList lines = fullDeckText.split('\n');

        bool inVelocityBlock = false;
        bool inTerminationBlock = false;
        bool inMatsumBlock = false;
        bool isVelocityOverwritten = false;

        double endTime = -1.0;
        int matsumLineIndex = -1;

        // 遍历合并后的完整文本流，定位并覆写靶向关键字
        for (int i = 0; i < lines.size(); ++i) {
            QString line = lines[i];

            // A. 拦截并覆写实体初始速度
            if (line.trimmed().startsWith("*INITIAL_VELOCITY_GENERATION")) {
                inVelocityBlock = true;
                continue;
            }
            if (inVelocityBlock) {
                // 跳过注释行和空行
                if (line.trimmed().startsWith("$") || line.trimmed().isEmpty()) continue;

                // 拆分 LS-DYNA 卡片数据 (按空白字符跳过空项)
                QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
                if (parts.size() >= 6) {

                    // 获取用户设定的原始基准方向向量
                    double base_vx = m_simSetupUI.icVx->value();
                    double base_vy = m_simSetupUI.icVy->value();
                    double base_vz = m_simSetupUI.icVz->value();

                    // 计算基准向量的合速度模长
                    double base_mag = std::sqrt(base_vx * base_vx + base_vy * base_vy + base_vz * base_vz);

                    double new_vx = 0.0, new_vy = 0.0, new_vz = 0.0;

                    // 🌟 防御性编程：防止除零崩溃 (用户初始输入全为0的情况)
                    if (base_mag > 1e-9) {
                        /**
                         * @note 空间等比缩放算法 (Proportional Scaling)
                         * 为了保证迭代时破片与装药的撞击夹角严格不变：
                         * 缩放因子 Scale = 目标合速度 / 基准合速度
                         * 新向量 V_new = V_base * Scale
                         */
                        double scale = m_upDownCurrentVelocity / base_mag;
                        new_vx = base_vx * scale;
                        new_vy = base_vy * scale;
                        new_vz = base_vz * scale;
                    }
                    else {
                        // 容错处理：若基准向量为零，默认沿 Z 轴负方向(垂直向下)冲击
                        new_vz = -m_upDownCurrentVelocity;
                    }

                    /**
                     * @brief LS-DYNA 卡片格式化重构
                     * 严格遵循标准的 10 字符宽度(10-character width) Fortran 格式
                     * parts[0]: Node Set ID / Part ID
                     * parts[1]: styp (定义类型)
                     * parts[2]: omega (旋转角速度)
                     */
                    QString newLine = QString(" %1%2%3%4%5%6")
                        .arg(parts[0].toInt(), 9)
                        .arg(parts[1].toInt(), 10)
                        .arg(parts[2].toInt(), 10)
                        .arg(new_vx, 10, 'f', 5)
                        .arg(new_vy, 10, 'f', 5)
                        .arg(new_vz, 10, 'f', 5);

                    // 补全由于 10 字符限制可能截断的剩余参数 (相位、刚体等)
                    for (int j = 6; j < parts.size(); ++j) {
                        newLine += QString("%1").arg(parts[j].toInt(), 10);
                    }

                    lines[i] = newLine;
                    inVelocityBlock = false;
                    isVelocityOverwritten = true;

                    thresholdConsole->append(QString("   [参数更新] 初始速度已重构 (空间方向锁定) -> 目标模长: %1 cm/μs, 分量: V=(%2, %3, %4)")
                        .arg(m_upDownCurrentVelocity, 0, 'f', 4)
                        .arg(new_vx, 0, 'f', 5).arg(new_vy, 0, 'f', 5).arg(new_vz, 0, 'f', 5));
                }
            }

            // B. 提取控制卡片中的总仿真时长 (ENDTIME)
            if (line.trimmed().startsWith("*CONTROL_TERMINATION")) {
                inTerminationBlock = true;
                continue;
            }
            if (inTerminationBlock) {
                if (line.trimmed().startsWith("$") || line.trimmed().isEmpty()) continue;
                QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
                if (!parts.isEmpty()) {
                    endTime = parts[0].toDouble();
                    inTerminationBlock = false;
                }
            }

            // C. 锁定 MATSUM 数据库控制卡的参数行索引
            if (line.trimmed().startsWith("*DATABASE_MATSUM")) {
                inMatsumBlock = true;
                continue;
            }
            if (inMatsumBlock) {
                if (line.trimmed().startsWith("$") || line.trimmed().isEmpty()) continue;
                matsumLineIndex = i;
                inMatsumBlock = false;
            }
        }

        if (!isVelocityOverwritten) {
            thresholdConsole->append("   [严重警告] 未匹配到 *INITIAL_VELOCITY_GENERATION 关键字，速度可能未成功写入。");
        }

        // D. 自适应调节 MATSUM 采样输出步长
        if (endTime > 0.0 && matsumLineIndex >= 0) {
            // 设置自适应比例 (将总时长均分为 1000 份采样间隔)
            const double matsumResolution = 1000.0;
            double dynamicDt = endTime / matsumResolution;

            // 安全限制：防止总时间极小导致步长越界
            if (dynamicDt < 0.005) dynamicDt = 0.005;

            QString originalMatsumLine = lines[matsumLineIndex];
            QStringList parts = originalMatsumLine.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

            if (!parts.isEmpty()) {
                QString newMatsumLine = QString("%1").arg(dynamicDt, 10, 'f', 5);
                for (int j = 1; j < parts.size(); ++j) {
                    newMatsumLine += QString("%1").arg(parts[j].toInt(), 10);
                }
                lines[matsumLineIndex] = newMatsumLine;
                thresholdConsole->append(QString("   [自适应调节] MATSUM 采样步长已动态适配为 %1 μs (总时长: %2 μs)")
                    .arg(dynamicDt, 0, 'f', 5).arg(endTime, 0, 'f', 2));
            }
        }

        // 一次性将覆写后的全部文本写入文件
        out << lines.join('\n');
        out << "*END\n";
        controlFile.close();
    }

    // 2. 生成求解器批处理脚本 (*.bat)
    QString batFilePath = QDir(m_upDownCurrentDir).filePath("run_step.bat");
    QFile batFile(batFilePath);

    QString solverPath = m_dynaSolverPath;
    if (solverPath.isEmpty()) {
        solverPath = solverPathEdit->text().trimmed();
    }

    QFileInfo solverInfo(solverPath);
    QString solverDir = solverInfo.absolutePath();
    QString intelRuntimePath = m_dynaEnvPath;

    if (batFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream batStream(&batFile);
        batStream << "@echo off\n";
 
        batStream << "set \"PATH=" << QDir::toNativeSeparators(solverDir) << ";"
            << QDir::toNativeSeparators(intelRuntimePath) << ";%PATH%\"\n";
        batStream << "set \"DYNA_PATH=" << QDir::toNativeSeparators(solverPath) << "\"\n";
        batStream << "\"%DYNA_PATH%\" I=" << controlFileName << " NCPU=" << cpuCoresSpin->value() << " MEMORY=2000m\n";
        batFile.close();
    }

    // 3. 配置 QProcess 运行时上下文
    m_batchProcess->setWorkingDirectory(m_upDownCurrentDir);
    m_batchProcess->setProcessChannelMode(QProcess::MergedChannels);
    m_batchProcess->setProcessEnvironment(QProcessEnvironment::systemEnvironment());

    QStringList args;
    args << "/c" << "run_step.bat";

    thresholdConsole->append("   [系统指令] 正在启动 LS-DYNA 求解器...");
    m_batchProcess->start("cmd.exe", args);

    if (!m_batchProcess->waitForStarted(3000)) {
        thresholdConsole->append(QString("   [致命错误] 求解进程启动失败: %1").arg(m_batchProcess->errorString()));
        m_isUpAndDownMode = false;
        return;
    }

    // 4. 初始化状态机并挂载异步探针轮询
    m_isEarlyDetonated = false;
    m_isEarlyMisfire = false;
    m_baselineTime = -1.0;
    m_initialFragKE = -1.0;

    if (!m_matsumPollingTimer) {
        m_matsumPollingTimer = new QTimer(this);
        connect(m_matsumPollingTimer, &QTimer::timeout, this, &MainWindow::handleMatsumPolling);
    }
    m_matsumPollingTimer->start(3000);
}

/**
 * @brief 解析计算结果文件以研判是否达到起爆阈值 (高精度部件级内能判定法)
 * @details 读取指定工况子目录下的 matsum ASCII 日志文件，基于正则表达式
 * 动态追踪并提取特定炸药部件 (Part) 仿真最终时刻的系统总内能。
 * 此算法有效屏蔽了非含能部件 (如弹壳、靶板) 撞击变形造成的塑性功干扰。
 * @param stepDir 需评估的工况子目录物理路径
 * @param explosivePartId 目标炸药药柱的物理部件编号 (Part ID)
 * @return bool 若判定为发生起爆突跃则返回 true，否则返回 false
 */
bool MainWindow::checkDetonationResult(const QString& stepDir, int explosivePartId) {
    // 强制转为读取部件级能量统计文件 (matsum)
    QString matsumPath = QDir(stepDir).filePath("matsum");
    std::ifstream file(matsumPath.toLocal8Bit().constData());

    // 异常处理：若目标文件缺失，保守判定为未起爆
    if (!file.is_open()) {
        if (thresholdConsole) {
            thresholdConsole->append("[解析警告] 未找到 matsum 文件，无法提取药柱内能！请检查控制文件中是否已配置 *DATABASE_MATSUM。");
        }
        return false;
    }

    std::string line;
    // 匹配 "part 1" 或 "part       1" (捕获编号)
    std::regex partRegex(R"(^\s*part\s+(\d+))");
    // 匹配 "internal energy  0.123E+06" (捕获数值)
    std::regex internalRegex(R"(^\s*internal energy\s+([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?))");
    std::smatch match;

    int currentReadingPartId = -1;
    double finalExplosiveEnergy = 0.0;

    // 状态机式逐行解析：追踪当前所在的 Part 块，并精准提取内能
    while (std::getline(file, line)) {
        std::string lowerLine = line;
        std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);

        // 1. 探针捕获：判断当前行是否为部件数据块的起始标识
        if (std::regex_search(lowerLine, match, partRegex)) {
            currentReadingPartId = std::stoi(match[1].str());
        }
        // 2. 数据提取：如果当前数据块属于指定的炸药部件，则尝试提取内能
        else if (currentReadingPartId == explosivePartId) {
            if (std::regex_search(lowerLine, match, internalRegex)) {
                // 不断用新值覆盖旧值，文件读取结束时保留的即为最终时刻的内能
                finalExplosiveEnergy = std::stod(match[1].str());
            }
        }
    }
    file.close();

    // 阈值标定：根据特定炸药的体积与爆炸热进行严格设定。
    // 示例：典型的炸药起爆后，内能将呈现指数级突增 (例如达到 10^6 ~ 10^9 量级)
    const double detonationEnergyThreshold = 1.0e6;
    return (finalExplosiveEnergy > detonationEnergyThreshold);
}

/**
 * @brief 导出基础仿真环境文件 (不触发计算引擎)
 * @details 将内存中的实体网格数据序列化并写入至指定工作目录下的公共 INCLUDE 文件中。
 * 为用户在正式提交闭环计算序列前，提供审查网格拓扑质量与控制卡片的前置检查点。
 */
void MainWindow::handleGenerateThresholdFiles() {
    if (m_workingDirectory.isEmpty()) {
        QMessageBox::warning(this, "上下文缺失", "请优先在顶部菜单配置系统的有效工作目录路径。");
        return;
    }

    // 将网格几何数据固化为底层求解器标准的关键字文件格式
    QString sharedMeshFileName = "shared_mesh_geometry.k";
    exportToKFile(QDir(m_workingDirectory).filePath(sharedMeshFileName));

    QMessageBox::information(this, "文件导出完毕",
        "公共几何网格文件已成功写入目标目录。\n系统当前处于待命状态，您可以审查网格后，随时点击【提交并启动寻优】进入闭环队列。");
}

/**
 * @brief 触发闭环寻优状态机并挂载首个测试计算任务
 * @details 负责校验前置条件，重置状态机的上下文参数 (初速、步长、最大迭代深度)，
 * 并调度 executeNextUpAndDownStep() 开启升降法生命周期的第一帧迭代。
 */
void MainWindow::handleSubmitThresholdTask() {
    if (m_workingDirectory.isEmpty()) {
        QMessageBox::warning(this, "上下文缺失", "请优先配置系统工作目录路径。");
        return;
    }

    // 拦截并发冲突：检测计算引擎的执行状态标识
    if (m_batchProcess && m_batchProcess->state() == QProcess::Running) {
        QMessageBox::warning(this, "系统资源锁定", "检测到后台已有计算求解器正在运行。\n如需挂载新寻优任务，请先点击【终止后台进程】。");
        return;
    }

    // 1. 冗余校验：强制同步生成最新的几何文件，规避脏数据读入
    QString sharedMeshFileName = "shared_mesh_geometry.k";
    exportToKFile(QDir(m_workingDirectory).filePath(sharedMeshFileName));

    // 2. 初始化闭环控制律 (Control Law) 状态参量
    m_isUpAndDownMode = true;
    m_upDownCurrentStep = 0;
    m_upDownMaxSteps = spinMaxSteps->value();
    m_upDownCurrentVelocity = spinStartVelocity->value();
    m_upDownStepSize = spinVelocityStep->value();
    m_upDownHistory.clear();

    // 3. 视图重定向与系统日志初始化
    if (solveTaskTabs) {
        solveTaskTabs->setCurrentIndex(1); // 自动切至自动化批处理 Tab
    }
    if (thresholdConsole) {
        thresholdConsole->clear();
        thresholdConsole->append("========================================");
        thresholdConsole->append("[状态机初始化] 启动 Bruceton 升降法动态寻优闭环...");
        thresholdConsole->append(QString("   [边界约束配置] 样本容量: %1 发 | 初始速度: %2 cm/μs | 变分步长: %3 cm/μs")
            .arg(m_upDownMaxSteps).arg(m_upDownCurrentVelocity).arg(m_upDownStepSize));
        thresholdConsole->append("========================================\n");
    }

    // 4. 驱动事件泵，挂载初始求解堆栈
    executeNextUpAndDownStep();
}

/**
 * @brief 强行挂起寻优状态机并释放底层求解器进程树句柄
 * @details 获取当前挂载的批处理子进程 PID，通过调用 Windows 原生 taskkill 指令，
 * 向该 PID 及其衍生出的所有求解器派生进程发送强制结束信号 (/F /T)，彻底回收计算资源。
 */
void MainWindow::handleTerminateProcess() {
    if (!m_batchProcess || m_batchProcess->state() == QProcess::NotRunning) {
        QMessageBox::information(this, "状态校验", "当前计算管线处于空闲状态，未检测到正在运行的求解进程。");
        return;
    }

    // 1. 关闭状态机的研判逻辑门，截断进程异常结束回调信号对下一次迭代的级联触发
    m_isUpAndDownMode = false;

    // 2. 提取宿主进程标识符 (PID)
    qint64 rootPid = m_batchProcess->processId();

    // 3. 组装系统级进程终结指令流
    QString killCmd = "taskkill";
    QStringList killArgs;
    // 参数释义: /F (Force) 强制终结, /T (Tree) 递归终结子孙进程树 (彻底杀死 LS-DYNA 内核)
    killArgs << "/F" << "/T" << "/PID" << QString::number(rootPid);

    // 阻塞式调用操作系统底层 API 实施绞杀
    QProcess::execute(killCmd, killArgs);

    // 4. 输出资源释放审计日志
    if (solverConsole) {
        solverConsole->append("\n[中断响应] 捕获系统最高优先级中断请求。");
        solverConsole->append(QString("[清理执行] 寻优状态机已强行脱机，底层求解进程树 (根 PID: %1) 已被销毁！").arg(rootPid));
        solverConsole->append("========================================\n");
    }
}

/**
 * @brief 从 UI 配置列表中嗅探并提取破片冲击的初始合速度模长 (标量)
 * @details
 * 该函数使用正则表达式解析用户设定的初始速度向量 V=(Vx, Vy, Vz)。
 * [核心逻辑变更]：不再强行分配符号，而是严格计算三维向量的绝对模长 (Magnitude)。
 * 提取的标量模长将作为后续“升降法/二分法”寻优的初始基准值，从而彻底解耦速度的“大小”与“方向”。
 */
void MainWindow::handleRefreshInitialVelocity() {
    if (!m_simSetupUI.setupSummaryList) return;

    double v_mag = 0.0;
    bool isFound = false;

    // 逆序遍历，获取最新的速度配置项
    for (int i = m_simSetupUI.setupSummaryList->count() - 1; i >= 0; --i) {
        QString itemText = m_simSetupUI.setupSummaryList->item(i)->text();

        if (itemText.contains("[初始速度]")) {
            // 正则匹配提取向量分量，例如: V=(0.15, -0.02, 0.0)
            std::regex vRegex(R"(V=\(([^,]+),\s*([^,]+),\s*([^)]+)\))");
            std::smatch match;
            std::string stdText = itemText.toStdString();

            if (std::regex_search(stdText, match, vRegex)) {
                double vx = std::stod(match[1].str());
                double vy = std::stod(match[2].str());
                double vz = std::stod(match[3].str());

                // 🌟 物理算法核心：计算三维合速度的绝对模长 (标量，恒大于等于0)
                // 公式: |V| = sqrt(Vx^2 + Vy^2 + Vz^2)
                v_mag = std::sqrt(vx * vx + vy * vy + vz * vz);

                isFound = true;
                break;
            }
        }
    }

    if (isFound) {
        // 将标量模长赋予寻优控件
        spinStartVelocity->setValue(v_mag);
        QMessageBox::information(this, "参数读取成功",
            QString("已成功提取破片冲击初速绝对合速度 (V0 = %1 cm/μs)。\n"
                "在后续寻优迭代中，程序将严格保证空间冲击夹角不变，仅对合速度大小进行等比例缩放！")
            .arg(v_mag, 0, 'f', 4));
    }
    else {
        QMessageBox::warning(this, "数据缺失", "未在配置列表中侦测到 [初始速度] 数据块！");
    }
}
/**
 * @brief 异步解析 matsum 时程文件，基于目标系统动能演化进行起爆与死火状态机研判
 * @details
 * 核心判定准则：
 * 1. 基准标定：捕获 time = 0.0 帧，累加分类为"破片"的 Part 的初始动能 (m_initialFragKE)。
 * 2. 起爆判定 (Detonation)：目标系统当前总动能 > 破片初始总动能的 80%。
 * 3. 死火判定 (Misfire)  ：目标系统总动能从其历史峰值衰减超过 20% (即当前动能 < peakTargetKE * 80%)，
 * 表明反应仅为纯机械碰撞与塑性阻尼耗散，未发生化学做功。
 */
void MainWindow::handleMatsumPolling() {
    // 状态校验：仅在寻优模式及求解器运行期间执行
    if (!m_isUpAndDownMode || !m_batchProcess) return;

    // ====================================================
    // 1. 实体映射：提取破片集合的标准化命名
    // ====================================================
    std::set<std::string> targetFragmentNames;
    for (const auto& pair : m_repository.getAllEntities()) {
        const MeshEntity& entity = pair.second;

        // 严格匹配中文类别标识
        if (entity.category == "破片") {
            QString qName = entity.name.trimmed();
            std::string fragName = qName.toStdString();
            std::transform(fragName.begin(), fragName.end(), fragName.begin(), ::tolower);
            if (!fragName.empty()) {
                targetFragmentNames.insert(fragName);
            }
        }
    }

    if (targetFragmentNames.empty()) return;

    // ====================================================
    // 2. 初始化文件流与正则解析器
    // ====================================================
    QString matsumPath = QDir(m_upDownCurrentDir).filePath("matsum");
    std::ifstream file(matsumPath.toLocal8Bit().constData());
    if (!file.is_open()) return;

    std::string line;
    std::regex legendStart(R"(\{BEGIN LEGEND\})");
    std::regex legendEnd(R"(\{END LEGEND\})");
    std::regex legendEntry(R"(^\s*(\d+)\s+(\S+))");
    std::regex timeRegex(R"(time\s*=\s*([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?))");
    std::regex matRegex(R"(mat\.#=\s*(\d+)\s+inten=\s*([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?)\s+kinen=\s*([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?))");

    std::set<int> fragmentIds;
    bool inLegend = false;
    double currentTime = -1.0;
    std::map<int, double> currentFrameKE;
    std::smatch match;

    // 目标系统动能历史峰值
    double peakTargetKE = 0.0;

    // ====================================================
    // 3. 解析文本流
    // ====================================================
    while (std::getline(file, line)) {
        // 3.1 解析 LEGEND 区块，构建破片 Part ID 映射表
        if (std::regex_search(line, legendStart)) { inLegend = true; continue; }
        if (std::regex_search(line, legendEnd)) { inLegend = false; continue; }
        if (inLegend) {
            if (std::regex_search(line, match, legendEntry)) {
                int id = std::stoi(match[1].str());
                std::string partName = match[2].str();
                std::transform(partName.begin(), partName.end(), partName.begin(), ::tolower);
                for (const auto& targetName : targetFragmentNames) {
                    if (partName.find(targetName) != std::string::npos) {
                        fragmentIds.insert(id);
                        break;
                    }
                }
            }
            continue;
        }

        // 3.2 解析时程数据区块
        std::string lowerLine = line;
        std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);

        if (std::regex_search(lowerLine, match, timeRegex)) {
            // 结算上一帧目标系统动能，并更新历史峰值
            if (!currentFrameKE.empty()) {
                double previousFrameTargetKE = 0.0;
                for (const auto& pair : currentFrameKE) {
                    if (fragmentIds.find(pair.first) == fragmentIds.end()) {
                        previousFrameTargetKE += pair.second;
                    }
                }
                if (previousFrameTargetKE > peakTargetKE) {
                    peakTargetKE = previousFrameTargetKE;
                }
            }

            // 实时锁定 t=0 帧，标定基准初始动能
            if (m_baselineTime < 0.0 && currentTime == 0.0 && !currentFrameKE.empty()) {
                m_baselineTime = 0.0;
                m_initialFragKE = 0.0;
                for (int id : fragmentIds) {
                    m_initialFragKE += currentFrameKE[id];
                }
            }

            currentTime = std::stod(match[1].str());
            currentFrameKE.clear();
        }
        else if (std::regex_search(lowerLine, match, matRegex)) {
            int id = std::stoi(match[1].str());
            currentFrameKE[id] = std::stod(match[3].str());
        }
    }
    file.close();

    // 边界处理：文件恰好结束于 t=0.0 帧末尾
    if (m_baselineTime < 0.0 && currentTime == 0.0 && !currentFrameKE.empty()) {
        m_baselineTime = 0.0;
        m_initialFragKE = 0.0;
        for (int id : fragmentIds) m_initialFragKE += currentFrameKE[id];
    }

    // 数据有效性校验
    if (currentTime < 0.0 || fragmentIds.empty() || currentFrameKE.empty() || m_initialFragKE <= 0.0) {
        return;
    }

    // ====================================================
    // 4. 计算当前帧目标系统总动能，并进行最终峰值同步
    // ====================================================
    double currentTargetKinetic = 0.0;
    for (const auto& pair : currentFrameKE) {
        if (fragmentIds.find(pair.first) == fragmentIds.end()) {
            currentTargetKinetic += pair.second;
        }
    }

    if (currentTargetKinetic > peakTargetKE) {
        peakTargetKE = currentTargetKinetic;
    }

    // ====================================================
    // 5. 核心状态机判决
    // ====================================================
    const double detonationThreshold = m_initialFragKE * 0.8;

    // 准则 A：起爆判定 (能量突跃越限)
    if (currentTargetKinetic > detonationThreshold) {
        if (m_matsumPollingTimer) m_matsumPollingTimer->stop();
        m_isEarlyDetonated = true;

        if (thresholdConsole) {
            thresholdConsole->append(QString("[探针截断] t=%1 时, 目标系统动能 (%2) 突破起爆阈值 (%3)。")
                .arg(currentTime, 0, 'f', 2)
                .arg(currentTargetKinetic, 0, 'e', 4)
                .arg(detonationThreshold, 0, 'e', 4));
            thresholdConsole->append("[状态研判] 判定为：起爆 (X)。");
        }

        qint64 rootPid = m_batchProcess->processId();
        QProcess::execute("taskkill", QStringList() << "/F" << "/T" << "/PID" << QString::number(rootPid));
        return;
    }

    // 准则 B：死火判定 (动能衰减)
    // 设定底噪容差 (1e-4)，当动能从明确的历史峰值回落超过 20% 时截断
    const double noiseTolerance = 1e-4;
    if (peakTargetKE > noiseTolerance && currentTargetKinetic < peakTargetKE * 0.8) {
        if (m_matsumPollingTimer) m_matsumPollingTimer->stop();
        m_isEarlyMisfire = true;

        if (thresholdConsole) {
            thresholdConsole->append(QString("[探针截断] t=%1 时，目标系统动能发生显著衰减 (峰值:%2，当前:%3)。")
                .arg(currentTime, 0, 'f', 2)
                .arg(peakTargetKE, 0, 'e', 4)
                .arg(currentTargetKinetic, 0, 'e', 4));
            thresholdConsole->append("[状态研判] 能量行为符合阻尼耗散规律，判定为：死火 (O)。");
        }

        qint64 rootPid = m_batchProcess->processId();
        QProcess::execute("taskkill", QStringList() << "/F" << "/T" << "/PID" << QString::number(rootPid));
        return;
    }
}

/**
 * @brief 处理用户手动干预：跳过当前寻优步
 * @details 弹出决策对话框让用户手动裁定当前工况的物理结果 (起爆/死火)。
 * 随后强杀底层 LS-DYNA 求解进程，利用全局状态标记 (m_isEarlyDetonated/Misfire)
 * 无缝欺骗 handleBatchProcessFinished 状态机，使其基于人工指定的判定结果
 * 继续执行升降法或二分法流转，极大地节省无效计算时间。
 */
void MainWindow::handleSkipCurrentStep() {
    // 拦截误触
    if (!m_isUpAndDownMode || !m_batchProcess || m_batchProcess->state() != QProcess::Running) {
        QMessageBox::warning(this, "无效操作", "当前没有正在运行的寻优计算任务。");
        return;
    }

    // 弹出人工决策对话框
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("手动跳过当前步");
    msgBox.setText("即将强行终止底层 LS-DYNA 求解器。\n请人工判定当前工况的物理结果：\n(寻优算法将根据您的裁定计算下一帧速度)");

    QPushButton* btnDetonate = msgBox.addButton("判定为：起爆", QMessageBox::ActionRole);
    QPushButton* btnMisfire = msgBox.addButton("判定为：死火", QMessageBox::ActionRole);
    QPushButton* btnCancel = msgBox.addButton("取消操作", QMessageBox::RejectRole);

    msgBox.exec();

    if (msgBox.clickedButton() == btnCancel) {
        return; // 用户取消，仿真继续
    }

    // 1. 关停异步探针，防止干扰
    if (m_matsumPollingTimer) m_matsumPollingTimer->stop();

    // 2. 根据用户人工判定，注入虚假状态标记
    if (msgBox.clickedButton() == btnDetonate) {
        m_isEarlyDetonated = true;
        if (thresholdConsole) thresholdConsole->append("   [人工干预] 用户强行截断当前工况，并指定结果为：起爆 (X)！");
    }
    else if (msgBox.clickedButton() == btnMisfire) {
        m_isEarlyMisfire = true;
        if (thresholdConsole) thresholdConsole->append("   [人工干预] 用户强行截断当前工况，并指定结果为：死火 (O)！");
    }
    qint64 rootPid = m_batchProcess->processId();
    QProcess::execute("taskkill", QStringList() << "/F" << "/T" << "/PID" << QString::number(rootPid));
}

/**
 * @brief 唤起全局系统设置对话框
 * @details 允许用户在 UI 界面动态配置底层 LS-DYNA 求解器的绝对路径与环境变量运行时目录。
 * 配置修改后将同步覆盖前端输入框，并作用于后续的所有单次求解与自动化批处理寻优流。
 */
void MainWindow::onGlobalSettings() {
    QDialog dialog(this);
    dialog.setWindowTitle("全局系统设置 (Global Settings)");
    dialog.setMinimumWidth(600);

    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);
    QFormLayout* formLayout = new QFormLayout();

    // 1. LS-DYNA 求解器路径配置区
    QHBoxLayout* solverLayout = new QHBoxLayout();
    QLineEdit* solverEdit = new QLineEdit(m_dynaSolverPath);
    QPushButton* solverBtn = new QPushButton("浏览...");
    solverLayout->addWidget(solverEdit);
    solverLayout->addWidget(solverBtn);
    formLayout->addRow("LS-DYNA 求解器路径 (.exe):", solverLayout);

    // 2. LS-DYNA 环境变量依赖库配置区
    QHBoxLayout* envLayout = new QHBoxLayout();
    QLineEdit* envEdit = new QLineEdit(m_dynaEnvPath);
    QPushButton* envBtn = new QPushButton("浏览...");
    envLayout->addWidget(envEdit);
    envLayout->addWidget(envBtn);
    formLayout->addRow("运行时环境/依赖库目录 (Dir):", envLayout);

    mainLayout->addLayout(formLayout);

    // 3. 对话框标准按钮与布局
    QDialogButtonBox* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(btnBox);

    // 绑定浏览按钮事件
    connect(solverBtn, &QPushButton::clicked, [&]() {
        QString path = QFileDialog::getOpenFileName(&dialog, "选择 LS-DYNA 求解器程序", "C:/", "可执行文件 (*.exe);;所有文件 (*.*)");
        if (!path.isEmpty()) solverEdit->setText(QDir::toNativeSeparators(path));
        });

    connect(envBtn, &QPushButton::clicked, [&]() {
        QString path = QFileDialog::getExistingDirectory(&dialog, "选择 LS-DYNA 运行时依赖目录", "C:/");
        if (!path.isEmpty()) envEdit->setText(QDir::toNativeSeparators(path));
        });

    connect(btnBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    // 4. 数据回写与状态同步
    if (dialog.exec() == QDialog::Accepted) {
        m_dynaSolverPath = solverEdit->text().trimmed();
        m_dynaEnvPath = envEdit->text().trimmed();

        // 智能同步：顺便刷新 UI 界面上单次求解面板里的输入框
        if (solverPathEdit) {
            solverPathEdit->setText(m_dynaSolverPath);
        }

        logCommand("System", "全局设置已更新：求解器引擎路径变更。");
    }
}

/**
 * @brief 处理运行已有网格收敛性批处理文件 (.bat) 的槽函数
 * * 该函数负责弹出文件选择对话框，读取用户选择的批处理文件，
 * 并通过独立的 QProcess 调度执行。执行过程中的标准输出和错误
 * 会被实时重定向到网格收敛专属的监控终端 (meshMonitorConsole) 中。
 * 同时会重置并启动定时器，以支持 QCustomPlot 的实时数据绘制。
 */
void MainWindow::handleRunExistingBat() {
    QString batPath = QFileDialog::getOpenFileName(this, "选择批处理文件", m_workingDirectory, "批处理文件 (*.bat)");
    if (batPath.isEmpty()) return;

    if (!m_meshBatchProcess) {
        m_meshBatchProcess = new QProcess(this);
        m_meshBatchProcess->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_meshBatchProcess, &QProcess::readyReadStandardOutput, this, &MainWindow::updateMeshMonitorConsole);
    }

    if (m_meshBatchProcess->state() == QProcess::Running) {
        m_meshBatchProcess->kill();
        m_meshBatchProcess->waitForFinished();
    }

    meshMonitorConsole->clear();

    // 使用你项目已有的 m_dynaSolverPath 和 m_dynaEnvPath 配置环境
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    // 1. 注入求解器目录
    if (!m_dynaSolverPath.isEmpty()) {
        QString solverDir = QFileInfo(m_dynaSolverPath).absolutePath();
        env.insert("PATH", QDir::toNativeSeparators(solverDir) + ";" + env.value("PATH"));
    }

    // 2. 注入你设置的运行时环境/依赖库目录
    if (!m_dynaEnvPath.isEmpty()) {
        env.insert("PATH", QDir::toNativeSeparators(m_dynaEnvPath) + ";" + env.value("PATH"));
    }

    m_meshBatchProcess->setProcessEnvironment(env);
    m_meshBatchProcess->setWorkingDirectory(QFileInfo(batPath).absolutePath());

    QString nativePath = QDir::toNativeSeparators(batPath);
    m_meshBatchProcess->start("cmd.exe", QStringList() << "/c" << nativePath);

    if (m_meshMonitorTimer) {
        m_currentMeshStepToMonitor = 1;
        m_lastGlstatPos = 0;
        if (meshConvergencePlot && meshConvergencePlot->graph(0)) {
            meshConvergencePlot->graph(0)->data()->clear();
            meshConvergencePlot->replot();
        }
        m_meshMonitorTimer->start(1000);
    }
}

void MainWindow::updateMeshMonitorConsole() {
    if (!m_meshBatchProcess || !meshMonitorConsole) return;

    QByteArray data = m_meshBatchProcess->readAllStandardOutput();
    if (data.isEmpty()) return;

    QString text = QString::fromLocal8Bit(data);

    meshMonitorConsole->moveCursor(QTextCursor::End);
    meshMonitorConsole->insertPlainText(text);
    meshMonitorConsole->ensureCursorVisible();

    QRegularExpression re("Running Step (\\d+)");
    auto match = re.match(text);
    if (match.hasMatch()) {
        m_currentMeshStepToMonitor = match.captured(1).toInt();
        m_lastGlstatPos = 0;
    }
}

/**
 * @brief 响应监控指标下拉列表状态改变事件
 * @details 清空当前视图数据并更新 Y 轴标签。若求解器处于运行状态，则立即触发数据抓取与重绘。
 */
void MainWindow::onMeshMonitorMetricChanged() {
    if (meshConvergencePlot && meshConvergencePlot->graphCount() > 0) {
        meshConvergencePlot->graph(0)->data()->clear();
        meshConvergencePlot->yAxis->setLabel(comboTargetMetric->currentText());
        meshConvergencePlot->replot();
    }

    if (m_meshBatchProcess && m_meshBatchProcess->state() == QProcess::Running) {
        updateMeshConvergencePlot();
    }
}

/**
 * @brief 定时读取 LS-DYNA 状态文件并更新绘图视图
 * @details 采用影子副本（Shadow Copy）机制绕过文件排他锁，并包含完整的终端状态诊断输出。
 */
 /**
  * @brief 定时抓取 LS-DYNA 后处理数据并刷新监控图表
  * @note 针对 R14 版本 glstat 点阵填充格式及 matsum 跨行数据结构进行深度适配
  */
void MainWindow::updateMeshConvergencePlot() {
    if (m_workingDirectory.isEmpty() || m_currentMeshStepToMonitor <= 0) return;
    if (!meshConvergencePlot || meshConvergencePlot->graphCount() == 0) return;

    // 索引映射：0-靶板内能, 1-系统动能, 2-弹体速度
    int metricIdx = comboTargetMetric->currentIndex();
    QString stepDir = QDir(m_workingDirectory).filePath(QString("Step_%1").arg(m_currentMeshStepToMonitor));
    QString fileName = (metricIdx == 2) ? "matsum" : "glstat";
    QString filePath = QDir(stepDir).filePath(fileName);

    QFileInfo checkFile(filePath);
    if (!checkFile.exists() || checkFile.size() == 0) {
        if (meshMonitorConsole) meshMonitorConsole->append("[系统] 正在等待求解器生成结果文件...");
        return;
    }

    // 影子拷贝，规避 Windows 强制文件锁
    QString tempPath = filePath + "_shadow_copy";
    QFile::remove(tempPath);
    if (!QFile::copy(filePath, tempPath)) return;

    QFile file(tempPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QFile::remove(tempPath);
        return;
    }

    // 全量刷新，清空旧数据
    meshConvergencePlot->graph(0)->data()->clear();

    QTextStream in(&file);
    double currentTime = -1.0;
    int pointCount = 0;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed().toLower();
        if (line.isEmpty()) continue;

        if (metricIdx == 2) {
            // ====================== 解析 matsum ======================
            if (line.startsWith("time =")) {
                currentTime = line.section('=', 1).trimmed().toDouble();
            }
            else if (currentTime >= 0.0 && line.startsWith("mat.#=")) {
                // 找到材料 ID (假设弹体是材料 1，若为材料 2 则修改 contains("2"))
                if (line.contains("1")) {
                    in.readLine(); // 跳过下一行 (x-mom, y-mom...)
                    QString rbvLine = in.readLine().trimmed().toLower(); // 这一行才是 x-rbv
                    if (rbvLine.startsWith("x-rbv")) {
                        // 提取 x-rbv=, y-rbv=, z-rbv= 后的数值
                        QStringList parts = rbvLine.split(QRegularExpression("[\\s=]+"), Qt::SkipEmptyParts);
                        if (parts.size() >= 6) {
                            double vx = parts[1].toDouble();
                            double vy = parts[3].toDouble();
                            double vz = parts[5].toDouble();
                            double v_res = qSqrt(vx * vx + vy * vy + vz * vz);
                            meshConvergencePlot->graph(0)->addData(currentTime, v_res);
                            pointCount++;
                        }
                    }
                    currentTime = -1.0; // 重置，找下一个时间步
                }
            }
        }
        else {
            // ====================== 解析 glstat ======================
            // 处理 "time...........................   1.2345E+00"
            if (line.startsWith("time") && line.contains("...")) {
                QStringList p = line.split(QRegularExpression("[\\.\\s]+"), Qt::SkipEmptyParts);
                if (!p.isEmpty()) currentTime = p.last().toDouble();
            }
            else if (currentTime >= 0.0) {
                QString target = (metricIdx == 0) ? "internal energy" : "kinetic energy";
                if (line.contains(target) && line.contains("...")) {
                    QStringList p = line.split(QRegularExpression("[\\.\\s]+"), Qt::SkipEmptyParts);
                    if (!p.isEmpty()) {
                        meshConvergencePlot->graph(0)->addData(currentTime, p.last().toDouble());
                        pointCount++;
                    }
                    currentTime = -1.0;
                }
            }
        }
    }

    file.close();
    QFile::remove(tempPath);

    if (meshMonitorConsole && pointCount > 0) {
        meshMonitorConsole->append(QString("[排错] 成功抓取到 %1 的 %2 个数据点").arg(fileName).arg(pointCount));
    }

    if (pointCount > 0) {
        meshConvergencePlot->graph(0)->rescaleAxes();
        meshConvergencePlot->replot();
    }
}
/**
 * @brief 强行终止当前正在运行的网格收敛性批处理进程
 * * 该函数会调用系统级 kill 指令终止 cmd.exe 及其派生的子进程（如 lsdyna），
 * 并同步停止实时绘图定时器，确保 UI 状态回滚。
 */
void MainWindow::handleStopMeshBatch() {
    if (m_meshBatchProcess && m_meshBatchProcess->state() == QProcess::Running) {
        // 1. 弹出确认对话框防止误操作
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            "确认停止",
            "确定要强行终止当前的批处理计算任务吗？这可能导致结果文件损坏。",
            QMessageBox::Yes | QMessageBox::No
        );

        if (reply == QMessageBox::Yes) {
            // 2. 强行杀掉进程树
            m_meshBatchProcess->kill();
            m_meshBatchProcess->waitForFinished(3000); // 阻塞等待最多3秒以确保清理完毕

            // 3. 停止实时绘图定时器
            if (m_meshMonitorTimer) {
                m_meshMonitorTimer->stop();
            }

            // 4. 更新终端显示
            if (meshMonitorConsole) {
                meshMonitorConsole->append("\n----------------------------------------------------------------");
                meshMonitorConsole->append("[系统警告] 用户手动触发了强行终止命令。");
                meshMonitorConsole->append("[系统警告] 进程已杀死，计算已中断。");
                meshMonitorConsole->append("----------------------------------------------------------------\n");
            }

            QMessageBox::information(this, "提示", "任务已成功终止。");
        }
    }
    else {
        QMessageBox::information(this, "提示", "当前没有正在运行的批处理任务。");
    }
}

/**
 * @brief 初始化单次求解器右侧的实时监控图表与控制面板
 * @param layout 传入的布局 (对应 mainHLayout)
 */
void MainWindow::setupSolverPlotUI(QBoxLayout* layout) {
    if (!layout) return;

    // 1. 创建整体容器 Widget 和垂直布局
    QWidget* plotContainer = new QWidget();
    QVBoxLayout* containerLayout = new QVBoxLayout(plotContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);

    // 2. 创建顶部控制栏水平布局
    QHBoxLayout* headerLayout = new QHBoxLayout();

    // 数据源选择
    headerLayout->addWidget(new QLabel("监控源:"));
    m_solverDataSourceCombo = new QComboBox();
    m_solverDataSourceCombo->addItems({ "全局能量 (GLSTAT)", "节点历程 (NODOUT)", "单元历程 (ELOUT)" });
    headerLayout->addWidget(m_solverDataSourceCombo);

    // ID 选择
    headerLayout->addWidget(new QLabel(" ID:"));
    m_solverIdCombo = new QComboBox();
    m_solverIdCombo->setMinimumWidth(80);
    headerLayout->addWidget(m_solverIdCombo);

    // 参数选择
    headerLayout->addWidget(new QLabel(" 参数:"));
    m_solverParamCombo = new QComboBox();
    m_solverParamCombo->setMinimumWidth(120);
    headerLayout->addWidget(m_solverParamCombo);

    headerLayout->addStretch();

    containerLayout->addLayout(headerLayout);

    // 3. 初始化图表并加到垂直布局里
    m_solverPlot = new QCustomPlot();
    m_solverPlot->setMinimumHeight(350);
    m_solverPlot->legend->setVisible(true);
    m_solverPlot->xAxis->setLabel("时间 (Time) [μs]");

    containerLayout->addWidget(m_solverPlot, 1);

    // 4. 将整体容器加到主界面的 layout 中
    layout->addWidget(plotContainer);

    // 5. 信号绑定
    connect(m_solverDataSourceCombo, &QComboBox::currentTextChanged, this, &MainWindow::updateSolverPlotUI);
    connect(m_solverIdCombo, &QComboBox::currentTextChanged, this, &MainWindow::redrawSolverPlot);
    connect(m_solverParamCombo, &QComboBox::currentTextChanged, this, &MainWindow::redrawSolverPlot);

    // 定时器初始化
    m_solverPlotTimer = new QTimer(this);
    connect(m_solverPlotTimer, &QTimer::timeout, this, &MainWindow::updateSolverPlot);

    // 初始化一次 UI
    updateSolverPlotUI();
}

/**
 * @brief 定时器触发的槽函数：执行解析并更新求解监控图表
 */
void MainWindow::updateSolverPlot() {
    // 获取正在求解的 K 文件路径
    QString currentKFilePath = kFilePathEdit->text();
    if (currentKFilePath.isEmpty()) return;

    // 获取工作目录
    QString workDir = QFileInfo(currentKFilePath).absolutePath();
    if (workDir.isEmpty() || !QDir(workDir).exists()) return;

    QString glstatPath = QDir(workDir).filePath("glstat");
    QString nodoutPath = QDir(workDir).filePath("nodout");
    QString eloutPath = QDir(workDir).filePath("elout");

    // 清空缓存，防内存累积和曲线重复
    m_rtGlstatTime.clear(); m_rtGlstatKe.clear(); m_rtGlstatIe.clear();
    m_rtNodoutTimeMap.clear(); m_rtNodoutData.clear();
    m_rtEloutTimeMap.clear(); m_rtEloutData.clear();

    m_rtNodoutTime = 0.0; m_rtNodoutIsDataBlock = false;
    m_rtEloutTime = 0.0; m_rtEloutId = -1; m_rtEloutBlock = 0;

    // 直接从头读取最新的文件进度
    parseRealTimeGlstat(glstatPath);
    parseRealTimeNodout(nodoutPath);
    parseRealTimeElout(eloutPath);

    // 刷新下拉菜单 (内部有防闪烁机制)
    updateSolverPlotUI();
    // 重绘曲线
    redrawSolverPlot();
}

/**
 * @brief 动态更新下拉框面板的可见性与级联内容
 */
void MainWindow::updateSolverPlotUI() {
    if (!m_solverDataSourceCombo || !m_solverIdCombo || !m_solverParamCombo) return;

    QString source = m_solverDataSourceCombo->currentText();
    m_solverIdCombo->blockSignals(true);
    m_solverParamCombo->blockSignals(true);

    if (source.contains("GLSTAT")) {
        m_solverIdCombo->setVisible(false);
        m_solverParamCombo->setVisible(false);
    }
    else if (source.contains("NODOUT")) {
        m_solverIdCombo->setVisible(true);
        m_solverParamCombo->setVisible(true);

        if (m_solverParamCombo->count() == 0 || !m_solverParamCombo->currentText().contains("速度")) {
            m_solverParamCombo->clear();
            m_solverParamCombo->addItems({ "res-vel (合速度)", "x-disp (X位移)", "y-disp (Y位移)", "z-disp (Z位移)",
                                          "x-vel (X速度)", "y-vel (Y速度)", "z-vel (Z速度)",
                                          "x-accl (X加速度)", "y-accl (Y加速度)", "z-accl (Z加速度)" });
        }

        QList<int> ids = m_rtNodoutData.keys();
        if (m_solverIdCombo->count() - 1 != ids.size()) {
            QString currentSel = m_solverIdCombo->currentText();
            m_solverIdCombo->clear();
            m_solverIdCombo->addItem("全画 (All)");
            std::sort(ids.begin(), ids.end());
            for (int id : ids) m_solverIdCombo->addItem(QString::number(id));

            int idx = m_solverIdCombo->findText(currentSel);
            m_solverIdCombo->setCurrentIndex(idx != -1 ? idx : 0);
        }
    }
    else if (source.contains("ELOUT")) {
        m_solverIdCombo->setVisible(true);
        m_solverParamCombo->setVisible(true);

        if (m_solverParamCombo->count() == 0 || !m_solverParamCombo->currentText().contains("反应度")) {
            m_solverParamCombo->clear();
            m_solverParamCombo->addItems({ "reaction_degree (反应度)", "effsg (等效应力)", "yield (屈服函数/塑性应变)",
                                          "sig-xx (X正应力)", "sig-yy (Y正应力)", "sig-zz (Z正应力)" });
        }

        QList<int> ids = m_rtEloutData.keys();
        if (m_solverIdCombo->count() - 1 != ids.size()) {
            QString currentSel = m_solverIdCombo->currentText();
            m_solverIdCombo->clear();
            m_solverIdCombo->addItem("全画 (All)");
            std::sort(ids.begin(), ids.end());
            for (int id : ids) m_solverIdCombo->addItem(QString::number(id));

            int idx = m_solverIdCombo->findText(currentSel);
            m_solverIdCombo->setCurrentIndex(idx != -1 ? idx : 0);
        }
    }

    m_solverIdCombo->blockSignals(false);
    m_solverParamCombo->blockSignals(false);
}

/**
 * @brief 根据当前下拉框设定执行核心画布重绘
 */
void MainWindow::redrawSolverPlot() {
    if (!m_solverPlot) return;
    QString source = m_solverDataSourceCombo->currentText();
    QString param = m_solverParamCombo->currentText();
    QString idStr = m_solverIdCombo->currentText();

    m_solverPlot->clearGraphs();

    if (source.contains("GLSTAT")) {
        m_solverPlot->yAxis->setLabel("能量");
        if (!m_rtGlstatTime.isEmpty()) {
            m_solverPlot->addGraph();
            m_solverPlot->graph(0)->setData(m_rtGlstatTime, m_rtGlstatKe);
            m_solverPlot->graph(0)->setName("动能 (KE)");
            m_solverPlot->graph(0)->setPen(QPen(Qt::blue, 2));

            m_solverPlot->addGraph();
            m_solverPlot->graph(1)->setData(m_rtGlstatTime, m_rtGlstatIe);
            m_solverPlot->graph(1)->setName("内能 (IE)");
            m_solverPlot->graph(1)->setPen(QPen(Qt::red, 2));
        }
    }
    else if (source.contains("NODOUT")) {
        m_solverPlot->yAxis->setLabel(param);
        if (idStr == "全画 (All)") {
            int c = 0;
            for (int id : m_rtNodoutData.keys()) {
                if (!m_rtNodoutData[id].contains(param)) continue;
                m_solverPlot->addGraph();
                m_solverPlot->graph()->setData(m_rtNodoutTimeMap[id], m_rtNodoutData[id][param]);
                m_solverPlot->graph()->setName(QString("Node %1").arg(id));
                QColor col = QColor::fromHsv((c++ * 50) % 360, 200, 200);
                m_solverPlot->graph()->setPen(QPen(col, 2));
            }
        }
        else {
            int id = idStr.toInt();
            if (m_rtNodoutData.contains(id) && m_rtNodoutData[id].contains(param)) {
                m_solverPlot->addGraph();
                m_solverPlot->graph()->setData(m_rtNodoutTimeMap[id], m_rtNodoutData[id][param]);
                m_solverPlot->graph()->setName(QString("Node %1").arg(id));
                m_solverPlot->graph()->setPen(QPen(Qt::darkBlue, 2.5));
            }
        }
    }
    else if (source.contains("ELOUT")) {
        m_solverPlot->yAxis->setLabel(param);
        if (idStr == "全画 (All)") {
            int c = 0;
            for (int id : m_rtEloutData.keys()) {
                if (!m_rtEloutData[id].contains(param)) continue;
                m_solverPlot->addGraph();
                m_solverPlot->graph()->setData(m_rtEloutTimeMap[id], m_rtEloutData[id][param]);
                m_solverPlot->graph()->setName(QString("Elem %1").arg(id));
                QColor col = QColor::fromHsv((c++ * 50) % 360, 200, 200);
                m_solverPlot->graph()->setPen(QPen(col, 2));
            }
        }
        else {
            int id = idStr.toInt();
            if (m_rtEloutData.contains(id) && m_rtEloutData[id].contains(param)) {
                m_solverPlot->addGraph();
                m_solverPlot->graph()->setData(m_rtEloutTimeMap[id], m_rtEloutData[id][param]);
                m_solverPlot->graph()->setName(QString("Elem %1").arg(id));
                m_solverPlot->graph()->setPen(QPen(Qt::darkRed, 2.5));
            }
        }
    }

    m_solverPlot->rescaleAxes();
    m_solverPlot->replot();
}

/**
 * @brief GLSTAT 断点续读解析引擎
 */
void MainWindow::parseRealTimeGlstat(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    double currentTime = 0.0;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.startsWith("time", Qt::CaseInsensitive)) {
            currentTime = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).last().toDouble();
        }
        else if (line.startsWith("kinetic energy", Qt::CaseInsensitive)) {
            m_rtGlstatTime.append(currentTime);
            m_rtGlstatKe.append(line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).last().toDouble());
        }
        else if (line.startsWith("internal energy", Qt::CaseInsensitive)) {
            m_rtGlstatIe.append(line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).last().toDouble());
        }
    }
    file.close();
}

/**
 * @brief NODOUT 断点续读解析引擎 (防粘连格式处理)
 */
void MainWindow::parseRealTimeNodout(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();

        if (line.contains("n o d a l") && line.contains("at time")) {
            m_rtNodoutTime = line.section("time", -1).remove(")").trimmed().toDouble();
            m_rtNodoutIsDataBlock = false;
        }
        else if (line.startsWith("nodal point")) m_rtNodoutIsDataBlock = true;
        else if (line.isEmpty() || line.startsWith("legend")) m_rtNodoutIsDataBlock = false;

        if (m_rtNodoutIsDataBlock && !line.isEmpty() && line[0].isDigit()) {
            QString cleanLine = line;
            cleanLine.replace("E-", "E_").replace("e-", "e_").replace("-", " -").replace("E_", "E-").replace("e_", "e-");
            QStringList parts = cleanLine.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

            if (parts.size() >= 10) {
                int nodeId = parts[0].toInt();
                m_rtNodoutTimeMap[nodeId].append(m_rtNodoutTime);
                m_rtNodoutData[nodeId]["x-disp (X位移)"].append(parts[1].toDouble());
                m_rtNodoutData[nodeId]["y-disp (Y位移)"].append(parts[2].toDouble());
                m_rtNodoutData[nodeId]["z-disp (Z位移)"].append(parts[3].toDouble());

                double vx = parts[4].toDouble(), vy = parts[5].toDouble(), vz = parts[6].toDouble();
                m_rtNodoutData[nodeId]["x-vel (X速度)"].append(vx);
                m_rtNodoutData[nodeId]["y-vel (Y速度)"].append(vy);
                m_rtNodoutData[nodeId]["z-vel (Z速度)"].append(vz);
                m_rtNodoutData[nodeId]["res-vel (合速度)"].append(std::sqrt(vx * vx + vy * vy + vz * vz));

                m_rtNodoutData[nodeId]["x-accl (X加速度)"].append(parts[7].toDouble());
                m_rtNodoutData[nodeId]["y-accl (Y加速度)"].append(parts[8].toDouble());
                m_rtNodoutData[nodeId]["z-accl (Z加速度)"].append(parts[9].toDouble());
            }
        }
    }
    file.close();
}

/**
 * @brief ELOUT 断点续读解析引擎 (带多区块记忆与状态机处理)
 */
void MainWindow::parseRealTimeElout(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        if (line.contains("e l e m e n t") && line.contains("at time")) {
            QRegularExpression timeRegex("at time\\s+([\\d\\.\\+\\-E]+)");
            auto match = timeRegex.match(line);
            if (match.hasMatch()) m_rtEloutTime = match.captured(1).toDouble();

            if (line.contains("s t r e s s")) m_rtEloutBlock = 1;
            else if (line.contains("h i s t r y") || line.contains("h i s t o r y")) m_rtEloutBlock = 2;
            else m_rtEloutBlock = 0;
            m_rtEloutId = -1;
            continue;
        }

        if (m_rtEloutBlock == 0) continue;

        if (line.contains("-")) {
            QString firstToken = line.split("-").first().trimmed();
            bool ok; int id = firstToken.toInt(&ok);
            if (ok) { m_rtEloutId = id; continue; }
        }

        if (m_rtEloutId != -1 && (line.contains("elastic") || line.contains("plastic") || line.contains("failed"))) {
            QString cleanLine = line;
            cleanLine.replace("E-", "E_").replace("e-", "e_").replace("-", " -").replace("E_", "E-").replace("e_", "e-");
            QStringList parts = cleanLine.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.size() < 10) continue;

            if (m_rtEloutBlock == 1) { // STRESS 块
                if (!m_rtEloutTimeMap[m_rtEloutId].contains(m_rtEloutTime)) {
                    m_rtEloutTimeMap[m_rtEloutId].append(m_rtEloutTime);
                }
                m_rtEloutData[m_rtEloutId]["sig-xx (X正应力)"].append(parts[2].toDouble());
                m_rtEloutData[m_rtEloutId]["sig-yy (Y正应力)"].append(parts[3].toDouble());
                m_rtEloutData[m_rtEloutId]["sig-zz (Z正应力)"].append(parts[4].toDouble());
                m_rtEloutData[m_rtEloutId]["effsg (等效应力)"].append(parts[8].toDouble());
                m_rtEloutData[m_rtEloutId]["yield (屈服函数/塑性应变)"].append(parts[9].toDouble());
            }
            else if (m_rtEloutBlock == 2) { // HISTORY 块
                double reaction = parts[9].toDouble();
                if (reaction < 0.0) reaction = 0.0;
                if (reaction > 1.0) reaction = 1.0;
                m_rtEloutData[m_rtEloutId]["reaction_degree (反应度)"].append(reaction);
            }
        }
    }
    file.close();
}

/**
 * @brief [核心后处理引擎] 借助内置 Python 脚本提取 d3plot 中的炸药最终反应度
 * * @details
 * 该函数采用“C++ 动态生成 Python 脚本”的策略。在运行时，C++ 将自动在当前工况工作目录下
 * 生成 `extract_d3plot.py`，并利用系统的 python 环境静默调用开源库 `lasso-python`，
 * 绕过复杂的二进制解析，直接、高效、无损地抽取目标单元的附加历史变量（History Variable 8）。
 * * @param workDir 当前仿真工况所在的绝对目录路径 (要求该目录下必须存在 d3plot 文件)
 * @param targetElemId 目标观测炸药单元的全局 ID (例如 137952)
 * * @note 运行环境强依赖：系统必须配置 Python 环境变量，且必须安装 `lasso-python` 与 `numpy`。
 * * @return bool 返回综合物理判定结果：
 * - true: 提取到的历史变量最大值 >= 0.5 (判定为发生稳态起爆)
 * - false: 提取到的最大值 < 0.5，或读取过程发生任何异常 (判定为死火)
 */
bool MainWindow::checkDetonationFromD3plotPython(const QString& workDir, int targetElemId) {
    // 1. 定义工作目录下的路径：确保 Python 脚本和生成的 CSV 都存放在当前工况文件夹内，防止并发冲突
    QString d3plotPath = QDir(workDir).filePath("d3plot");
    QString scriptPath = QDir(workDir).filePath("extract_d3plot.py");
    QString csvPath = d3plotPath + "_reaction.csv";

    // ==============================================================================
    // 2. 🌟 脚本部署阶段：C++ 动态释放底层 Python 提取引擎到工作目录
    // ==============================================================================
    QFile scriptFile(scriptPath);
    if (scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&scriptFile);

        // 使用 C++11 原始字符串字面量(Raw String Literal) 封装 Python 源码
        out << R"(
import sys
import numpy as np
from lasso.dyna import D3plot, ArrayType

if len(sys.argv) != 3:
    sys.exit(1)

d3plot_path = sys.argv[1]
target_elem_id = int(sys.argv[2])

try:
    # 快速映射 d3plot 二进制结构体
    d3p = D3plot(d3plot_path)
    
    # 提取全局实体附加历史变量与节点映射表
    history_vars = d3p.arrays[ArrayType.element_solid_history_vars]
    elem_ids = d3p.arrays[ArrayType.element_solid_node_indexes][:, 0]
    
    # 寻址目标单元的内部索引
    internal_idx = np.where(elem_ids == target_elem_id)[0][0]
    times = d3p.arrays[ArrayType.global_timesteps]
    
    # 靶向抽取第 8 个历史变量 (索引为7, 即反应度)
    reaction_degrees = history_vars[:, internal_idx, 7]
    
    # 极速落盘为结构化 CSV 数据
    out_csv = d3plot_path + "_reaction.csv"
    np.savetxt(out_csv, np.column_stack((times, reaction_degrees)), delimiter=",", fmt="%.6f")
    
    print("SUCCESS")
except Exception as e:
    print(f"ERROR: {e}")
)";
        scriptFile.close();
    }
    else {
        if (thresholdConsole) thresholdConsole->append("   [致命错误] 无法在计算子目录下生成 Python 数据提取脚本！");
        return false;
    }

    // ==============================================================================
    // 3. 进程调度阶段：静默唤醒 Python 引擎并注入参数
    // ==============================================================================
    QProcess pythonProcess;
    QStringList args;
    args << scriptPath << d3plotPath << QString::number(targetElemId);

    if (thresholdConsole) thresholdConsole->append("   [后处理] 正在后台静默启动 Python 引擎解析 d3plot 二进制结果...");
    pythonProcess.start("python", args);

    // 阻塞等待解析完成 (设置 30 秒超时防御)
    if (!pythonProcess.waitForFinished(30000)) {
        if (thresholdConsole) thresholdConsole->append("   [读取超时] Python 数据提取进程响应超时！请检查环境配置。");
        pythonProcess.kill();
        return false;
    }

    // 解析标准输出流，诊断底层运行状态
    QString output = pythonProcess.readAllStandardOutput().trimmed();
    if (output != "SUCCESS") {
        QString errorOut = pythonProcess.readAllStandardError().trimmed();
        if (thresholdConsole) {
            thresholdConsole->append(QString("   [引擎报错] %1 | %2\n   提示: 请确认运行环境已执行 'pip install lasso-python numpy'").arg(output).arg(errorOut));
        }
        return false;
    }

    // ==============================================================================
    // 4. 数据回收阶段：解析生成的 CSV 曲线以定位峰值
    // ==============================================================================
    double maxReaction = 0.0;
    QFile csvFile(csvPath);
    if (csvFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&csvFile);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();

            // 跳过无效行 (空行或非数字起始行)
            if (line.isEmpty() || (!line[0].isDigit() && line[0] != '.')) continue;

            QStringList parts = line.split(QRegularExpression("[,\\s]+"), Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                double reaction = parts[1].toDouble();
                if (reaction > maxReaction) {
                    maxReaction = reaction;
                }
            }
        }
        csvFile.close();
    }
    else {
        if (thresholdConsole) thresholdConsole->append("   [读取错误] 引擎已执行，但 C++ 无法打开目标 CSV 数据文件！");
        return false;
    }

    // ==============================================================================
    // 5. 结论输出阶段
    // ==============================================================================
    if (thresholdConsole) {
        thresholdConsole->append(QString("   [解析完毕] 目标观测单元 (ID:%1) 最终最大反应度: %2")
            .arg(targetElemId).arg(maxReaction, 0, 'f', 4));
    }

    // 物理阈值截断准则：只要爆轰过程有任意时刻反应度超过 50%，即判定为发生了不可逆的宏观起爆
    return (maxReaction >= 0.5);
}
#include <QMainWindow>
#include "mainwindow.h"
#include "gmsh.h"
#include "glwidget.h"
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QDebug>
#include "MeshUtils.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {

    glWidget = new GLWidget(this);
        setCentralWidget(glWidget);

        startMeshing();
        // 连接绘图完成信号
        connect(glWidget, &GLWidget::drawingComplete, this, &MainWindow::onDrawingComplete);

        // 初始化菜单栏
        createMenuBar();

        // 初始化工具栏
        createToolBars();

        // 初始化状态栏
        createStatusBar();

        // 初始化停靠窗口
        createDockWidgets();


        // 初始化命令行
        createCommandLine();

        setWindowTitle("OpenGL CAD Demo");
        resize(1024, 768);

        

        //clear()
        clean();
        
        m_meshManager = new MeshManager(this);

        // 核心：当网格文件准备好后，自动触发 parseMeshFile 进行解析和渲染
        connect(m_meshManager, &MeshManager::meshReady, this, &MainWindow::parseMeshFile);

        // 错误处理：如果 Gmsh 报错，打印到日志
        connect(m_meshManager, &MeshManager::errorOccurred, this, [this](QString msg) {
            logCommand("Error", msg);
            });
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
}

void MainWindow::createToolBars() {
    // 文件工具栏
    fileToolBar = addToolBar("File");
    fileToolBar->addAction("New");
    fileToolBar->addAction("Open");
    fileToolBar->addAction("Save");

    // 编辑工具栏
    editToolBar = addToolBar("Edit");
    editToolBar->addAction("Cut");
    editToolBar->addAction("Copy");
    editToolBar->addAction("Paste");

    // 视图工具栏
    viewToolBar = addToolBar("View");
    viewToolBar->addAction("Zoom In");
    viewToolBar->addAction("Zoom Out");
    viewToolBar->addAction("Reset View");
}

void MainWindow::createStatusBar() {
    statusBar = new QStatusBar(this);
    setStatusBar(statusBar);
    QLabel *coordLabel = new QLabel("X: 0, Y: 0, Z: 0", this);
    statusBar->addPermanentWidget(coordLabel);
    statusBar->showMessage("Ready");
}

void MainWindow::createDockWidgets() {
    // 属性停靠窗口
    propertiesDock = new QDockWidget("Properties", this);
    propertiesDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    propertiesEditor = new QTextEdit();
    propertiesEditor->setPlainText("Object Properties:\n- Color: Red\n- Size: 1x1x1");
    propertiesDock->setWidget(propertiesEditor);
    propertiesDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, propertiesDock);

    // 图层停靠窗口
    layersDock = new QDockWidget("Layers", this);
    layersDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    layersTree = new QTreeWidget();
    layersTree->setHeaderLabel("Layer Name");
    QTreeWidgetItem *layer1 = new QTreeWidgetItem(layersTree);
    layer1->setText(0, "Layer 1");
    QTreeWidgetItem *layer2 = new QTreeWidgetItem(layersTree);
    layer2->setText(0, "Layer 2");
    layersDock->setWidget(layersTree);
    addDockWidget(Qt::RightDockWidgetArea, layersDock);

    // 命令停靠窗口
    commandDock = new QDockWidget("Command History", this);
    commandDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    commandHistoryEdit = new QTextEdit();  // 初始化 commandHistoryEdit
    commandHistoryEdit->setReadOnly(true);
    //QListWidget *commandHistoryList = new QListWidget();
    commandDock->setWidget(commandHistoryEdit);
    addDockWidget(Qt::BottomDockWidgetArea, commandDock);

    // substanceDock
    substanceDock = new QDockWidget("Substances", this);
    substanceDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    substanceTree = new QTreeWidget();
    substanceTree->setHeaderLabel("substanceTree");
    substanceDock->setWidget(substanceTree);
    substanceDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::LeftDockWidgetArea, substanceDock);

    //boundaryDock
    boundaryDock = new QDockWidget("Boundaries", this);
    boundaryDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    boundaryTree = new QTreeWidget();
    boundaryTree->setHeaderLabel("boundaryTree");
    boundaryDock->setWidget(boundaryTree);
    boundaryDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::LeftDockWidgetArea, boundaryDock);

    //interactionDock
    interactionDock = new QDockWidget("Interactions", this);
    interactionDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    interactionTree = new QTreeWidget();
    interactionTree->setHeaderLabel("interactionTree");
    interactionDock->setWidget(interactionTree);
    interactionDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::LeftDockWidgetArea, interactionDock);

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

    if (cmd == "line") {
       
        DrawLine(args);

    } else if (cmd == "circle") {
       // startCircleDrawing();
    } else if (cmd == "rectangle") {
       // startRectangleDrawing();
    }  else if (cmd == "cube") {
               if (args.size() == 11) {

				   QString instance_name = args[1];
                   float x_division = args[2].toFloat();
                   float y_division = args[3].toFloat();
                   float z_division = args[4].toFloat();
                   float x = args[5].toFloat();
                   float y = args[6].toFloat();
                   float z = args[7].toFloat();
				   float x_scale = args[8].toFloat(); 
				   float y_scale = args[9].toFloat();
				   float z_scale = args[10].toFloat();

                   std::vector<MeshPoint> cube_points = CubicBlock::building_basic_cubic_brick(x_division, y_division, z_division, x, y, z, x_scale, y_scale, z_scale);
                   drawables.push_back( Instance( instance_name, cube_points ) );
                 
                   glWidget->setDrawableInstances(drawables);
                   insert_points_vector(points, cube_points);
				   
                   glWidget->update();
				   logCommand(command, QString("Created cube '%1' with dimensions %2x%3x%4.").arg(instance_name).arg(x).arg(y).arg(z));

                   QTreeWidgetItem* instance_generate = new QTreeWidgetItem(substanceTree);
                   instance_generate->setText(0, instance_name);

               } 
               else {
                   logCommand(command, "Error: Missing argument for 'cube' command.");
               }
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

        points.clear();

		drawables.clear();
        glWidget->setDrawableInstances(drawables);

        drawables_lines.clear();
        glWidget->setDrawableLines(drawables_lines);
        
        substanceTree->clear();

        logCommand(command, "Cleared all!");
    } else if (cmd == "help") {
        showHelp();
    } 
    else if (cmd == "cylinder") {
    
		generateCylindricalMesh(3.8, 10.0, 0.1);
    }

    else if (cmd == "shell") {
    
		generateCylindricalShellMesh(3.8, 8.0, 0.6, 0.6, 0.1);
    }

    else if (cmd == "orb") {
    
        generateOrbMesh(0.38, 0.05);
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

        // --- 剩下的交给信号槽逻辑，当信号 meshReady 触发时渲染 ---
        logCommand(command, "Sphere generation task sent to manager...");
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

    drawables.clear();
    drawables_lines.clear();
    points.clear();

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

    qDebug() << "网格划分完成！文件已保存在 output.msh";
}

void MainWindow::parseMeshFile(QString fileName) {

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    QString line;
    std::vector<MeshPoint> nodes;
	std::vector<Hexahedron> hexes;

    QString entityName = QFileInfo(fileName).baseName();

    while (!in.atEnd()) {
        line = in.readLine().trimmed();

        // 找节点数据块
        if (line == "$Nodes") {
            int numNodes = in.readLine().trimmed().toInt();
            for (int i = 0; i < numNodes; ++i) {
                QStringList data = in.readLine().split(" ");
                if (data.size() >= 4) {
                    MeshPoint node;
                   
                    node.pos.setX( data[1].toDouble() );
                    node.pos.setY(data[2].toDouble());
                    node.pos.setZ(data[3].toDouble());
					nodes.push_back(node);
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
                    
                        hex[j] = data[3 +numTags+ j].toInt();
                    }
					hexes.push_back(hex);
                }
            }
        }
    }

    std::vector<MeshPoint> points;
    for (auto& n : nodes)
        points.push_back(n);

    DrawCommand cmd;
    cmd.type = DrawCommand::Points;
    cmd.pointsCmd.points = points;
    cmd.pointsCmd.size = 2.5f;

    //glWidget->clearDrawCommands();
    glWidget->submitDrawCommand(cmd);

    std::vector<Vector3> wireLines = buildHexWireframe(nodes, hexes);
   // auto wireLines = buildHexWireframe( nodes, hexes );

    MeshEntity newEntity;
    newEntity.name = entityName;
    newEntity.type = "Empty"; // 或者是从某个地方传过来的类型
    newEntity.nodes = points;
    newEntity.hexes = hexes;
    newEntity.wireLines = wireLines;

    m_repository.addEntity(entityName, newEntity);

    DrawCommand lineCmd;
    lineCmd.type = DrawCommand::Lines;
    lineCmd.linesCmd.points = wireLines;
    lineCmd.linesCmd.width = 2.0f;

    glWidget->submitDrawCommand(lineCmd);
    redrawAllEntities();

    qDebug() << "done parsing and wireframe generated!";
    qDebug() << "Wireframe submitted with" << wireLines.size() / 2 << "lines.";
    file.close();
}

void MainWindow::generateCylindricalMesh( double radius, double height, double meshSize) {

    int nC = 2 * round((radius / (2 * 1.414) ) / meshSize );
	int nH = round(height / meshSize);
    QString geoContent = QString(R"(
////////////////////////////////////////////////////
// Cylindrical O-grid mesh (parameterized)
////////////////////////////////////////////////////

// 1. 参数定义
R = %1;
H = %2;

nC = %3;
nH = %4;

L = R /(2 * 1.414);
Rproj = R / 1.414;
nR = nC / 1.414;

Printf("L = %g, R = %g, H = %g", L, R, H);

// 2. 点定义
Point(1)  = {0, 0, 0};

Point(2)  = { L,  L, 0};
Point(3)  = {-L,  L, 0};
Point(4)  = {-L, -L, 0};
Point(5)  = { L, -L, 0};

Point(6)  = { Rproj,  Rproj, 0};
Point(7)  = {-Rproj,  Rproj, 0};
Point(8)  = {-Rproj, -Rproj, 0};
Point(9)  = { Rproj, -Rproj, 0};

Point(10) = { 2*L, 0, 0};
Point(11) = { 0, 2*L, 0};
Point(12) = {-2*L, 0, 0};
Point(13) = { 0,-2*L, 0};

// 3. 线定义
Circle(1) = {2, 13, 3};
Circle(2) = {3, 10, 4};
Circle(3) = {4, 11, 5};
Circle(4) = {5, 12, 2};

Circle(5) = {6, 1, 7};
Circle(6) = {7, 1, 8};
Circle(7) = {8, 1, 9};
Circle(8) = {9, 1, 6};

Line(9)  = {2, 6};
Line(10) = {3, 7};
Line(11) = {4, 8};
Line(12) = {5, 9};

// 4. 面定义
Curve Loop(1) = {1, 2, 3, 4};
Plane Surface(1) = {1};

Curve Loop(2) = {9, 5, -10, -1};
Plane Surface(2) = {2};

Curve Loop(3) = {10, 6, -11, -2};
Plane Surface(3) = {3};

Curve Loop(4) = {11, 7, -12, -3};
Plane Surface(4) = {4};

Curve Loop(5) = {12, 8, -9, -4};
Plane Surface(5) = {5};

// 5. 结构化约束
Transfinite Curve {1,2,3,4,5,6,7,8} = nC;
Transfinite Curve {9,10,11,12} = nR;

Transfinite Surface {1,2,3,4,5};
Recombine Surface {1,2,3,4,5};

// 6. 扫掠 3D
Extrude {0, 0, H} {
  Surface{1,2,3,4,5};
  Layers{nH};
  Recombine;
}

Mesh 3;
Mesh.MshFileVersion = 2.2;
)")
.arg(radius)
.arg(height)
.arg(nC)
.arg(nH);
    // 2. 写入物理文件
    
    QString appPath = QCoreApplication::applicationDirPath();
    QFile::remove("output_geo.geo");
    QFile::remove("output_geo.msh");
    QString geoFile = appPath + "/output_geo.geo";
    QString mshFile = appPath + "/output_geo.msh";

    QFile file(geoFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << geoContent;
        file.close();
    }
    
    QProcess* gmsh = new QProcess();
    QStringList args;
    args << geoFile << "-3" << "-o" << mshFile; // -3 表示三维划分

    gmsh->start(appPath + "/gmsh.exe", args);

    if (!gmsh->waitForFinished()) {
        qDebug() << "Gmsh failed:" << gmsh->errorString();
    }
    else {
        qDebug() << "Success! You may find the mesh file in: " << mshFile;
        parseMeshFile(mshFile);
        // 接下来可以调用之前的 parseMshFile(mshFile) 进行解析
    }
}

void MainWindow::generateCylindricalShellMesh(double radius, double height, double lid, double wall ,double meshSize) {

    int nC = 2 * round((radius / (2 * 1.414)) / meshSize);
    int nH = round(height / meshSize);

	int nH_cap = round(lid / meshSize);
	int nR_wall = round(wall / meshSize);

    int nH_void = round(height / meshSize);
    QString geoContent = QString(R"(
// 1. 参数定义
R_in = %1; 
L_val = R_in / (2 * 1.414); 
Wall = %4;
 
R_out = R_in + Wall;
H_height = %2;
H_cap = %3; // lid height 
H_void = H_height - 2* H_cap; 
nC = %5; 
nR_in = nC / 1.414; 
nR_wall = %6; 
nH_cap = %7; 
nH_void = %8;

// 2. 基础面定义 (Z=0)
Point(1) = {0, 0, 0}; 
Point(2) = {L_val, L_val, 0};    Point(3) = {-L_val, L_val, 0};
Point(4) = {-L_val, -L_val, 0};  Point(5) = {L_val, -L_val, 0};
p = R_in / 1.414;
Point(6) = {p, p, 0};    Point(7) = {-p, p, 0};
Point(8) = {-p, -p, 0};  Point(9) = {p, -p, 0};
p2 = R_out / 1.414;
Point(10) = {p2, p2, 0}; Point(11) = {-p2, p2, 0};
Point(12) = {-p2, -p2, 0}; Point(13) = {p2, -p2, 0};

Line(1)={2,3}; Line(2)={3,4}; Line(3)={4,5}; Line(4)={5,2};
Line(5)={2,6}; Line(6)={3,7}; Line(7)={4,8}; Line(8)={5,9};
Circle(9)={6,1,7}; Circle(10)={7,1,8}; Circle(11)={8,1,9}; Circle(12)={9,1,6};
Line(13)={6,10}; Line(14)={7,11}; Line(15)={8,12}; Line(16)={9,13};
Circle(17)={10,1,11}; Circle(18)={11,1,12}; Circle(19)={12,1,13}; Circle(20)={13,1,10};

Curve Loop(101) = {1, 2, 3, 4};            Plane Surface(1) = {101}; 
Curve Loop(102) = {5, 9, -6, -1};          Plane Surface(2) = {102}; 
Curve Loop(103) = {6, 10, -7, -2};         Plane Surface(3) = {103};
Curve Loop(104) = {7, 11, -8, -3};         Plane Surface(4) = {104};
Curve Loop(105) = {8, 12, -5, -4};         Plane Surface(5) = {105};
Curve Loop(106) = {13, 17, -14, -9};       Plane Surface(6) = {106}; 
Curve Loop(107) = {14, 18, -15, -10};      Plane Surface(7) = {107};
Curve Loop(108) = {15, 19, -16, -11};      Plane Surface(8) = {108};
Curve Loop(109) = {16, 20, -13, -12};      Plane Surface(9) = {109};

Transfinite Surface {1:9}; Recombine Surface {1:9};
Transfinite Curve {1:4, 9:12, 17:20} = nC;
Transfinite Curve {5:8} = nR_in; Transfinite Curve {13:16} = nR_wall;

// 3. 顺序拉伸
// 第一层：底部端盖 (9个体积)
out1[] = Extrude {0, 0, H_cap} { Surface{1:9}; Layers{nH_cap}; Recombine; };

// 第二层：中间层 (9个体积)
out2[] = Extrude {0, 0, H_void} { 
  Surface{out1[0], out1[6], out1[12], out1[18], out1[24], out1[30], out1[36], out1[42], out1[48]}; 
  Layers{nH_void}; Recombine; 
};

// 第三层：顶部端盖 (9个体积)
out3[] = Extrude {0, 0, H_cap} { 
  Surface{out2[0], out2[6], out2[12], out2[18], out2[24], out2[30], out2[36], out2[42], out2[48]}; 
  Layers{nH_cap}; Recombine; 
};

// 4. 【核心黑科技】递归删除中间不想要的 5 个体积
// 这样中间在界面上就彻底消失了，变成真正的空腔
Recursive Delete {
  Volume{out2[1], out2[7], out2[13], out2[19], out2[25]};
}

// 5. 重新约束顶部的网格 (因为删除可能导致拓扑连接标记刷新)
// 只要面还在，网格策略就能保住
Transfinite Volume {out3[1], out3[7], out3[13], out3[19], out3[25], out3[31], out3[37], out3[43], out3[49]};

// 6. 物理组
Physical Volume("Solid_Part") = {
  out1[1], out1[7], out1[13], out1[19], out1[25], out1[31], out1[37], out1[43], out1[49], // 底
  out2[31], out2[37], out2[43], out2[49],                                                // 侧壁
  out3[1], out3[7], out3[13], out3[19], out3[25], out3[31], out3[37], out3[43], out3[49]  // 顶
};

Mesh 3;
)")
.arg(radius)
.arg(height)
.arg(lid)
.arg(wall)
.arg(nC)
.arg(nR_wall)
.arg(nH_cap)
.arg(nH_cap);
    // 2. 写入物理文件

    QString appPath = QCoreApplication::applicationDirPath();
    QFile::remove("output_geo.geo");
    QFile::remove("output_geo.msh");
    QString geoFile = appPath + "/output_geo.geo";
    QString mshFile = appPath + "/output_geo.msh";

    QFile file(geoFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << geoContent;
        file.close();
    }

    QProcess* gmsh = new QProcess();
    QStringList args;
    args << geoFile << "-3" << "-o" << mshFile; // -3 表示三维划分

    gmsh->start(appPath + "/gmsh.exe", args);

    if (!gmsh->waitForFinished()) {
        qDebug() << "Gmsh failed:" << gmsh->errorString();
    }
    else {
        qDebug() << "Success! You may find the mesh file in: " << mshFile;
        parseMeshFile(mshFile);
        // 接下来可以调用之前的 parseMshFile(mshFile) 进行解析
    }
}

void MainWindow::generateOrbMesh(double radius, double meshSize) {
    int nC = 2 * round((radius / (2 * 1.414)) / meshSize);

QString geoContent = QString(R"(
// =======================================================
// Gmsh 结构化球形 O-Grid 脚本 (修正 Surface Loop 定义)
// =======================================================

// --- 1. 参数设置 ---
R = %1;           
meshSize = %2;    

_nC = Round(R * 1.57 / meshSize);
_nR = Round(R / meshSize);
nC = (_nC > 2) ? _nC : 2;
nR = (_nR > 2) ? _nR : 2;

L = R * 0.45;  
P = R / Sqrt(3); 

// --- 2. 点定义 ---
Point(100) = {0, 0, 0}; 
Point(1) = { L,  L,  L}; Point(2) = {-L,  L,  L}; Point(3) = {-L, -L,  L}; Point(4) = { L, -L,  L};
Point(5) = { L,  L, -L}; Point(6) = {-L,  L, -L}; Point(7) = {-L, -L, -L}; Point(8) = { L, -L, -L};
Point(11) = { P,  P,  P}; Point(12) = {-P,  P,  P}; Point(13) = {-P, -P,  P}; Point(14) = { P, -P,  P};
Point(15) = { P,  P, -P}; Point(16) = {-P,  P, -P}; Point(17) = {-P, -P, -P}; Point(18) = { P, -P, -P};

// --- 3. 线段定义 ---
Line(1)={1,2}; Line(2)={2,3}; Line(3)={3,4}; Line(4)={4,1};
Line(5)={5,6}; Line(6)={6,7}; Line(7)={7,8}; Line(8)={8,5};
Line(9)={1,5}; Line(10)={2,6}; Line(11)={3,7}; Line(12)={4,8};
Circle(21)={11,100,12}; Circle(22)={12,100,13}; Circle(23)={13,100,14}; Circle(24)={14,100,11};
Circle(25)={15,100,16}; Circle(26)={16,100,17}; Circle(27)={17,100,18}; Circle(28)={18,100,15};
Circle(29)={11,100,15}; Circle(30)={12,100,16}; Circle(31)={13,100,17}; Circle(32)={14,100,18};
Line(41)={1,11}; Line(42)={2,12}; Line(43)={3,13}; Line(44)={4,14};
Line(45)={5,15}; Line(46)={6,16}; Line(47)={7,17}; Line(48)={8,18};

// --- 4. 表面定义 ---
Curve Loop(101)={1,2,3,4};     Plane Surface(101)={101}; 
Curve Loop(102)={5,6,7,8};     Plane Surface(102)={102}; 
Curve Loop(103)={1,10,-5,-9};  Plane Surface(103)={103}; 
Curve Loop(104)={2,11,-6,-10}; Plane Surface(104)={104}; 
Curve Loop(105)={3,12,-7,-11}; Plane Surface(105)={105}; 
Curve Loop(106)={4,9,-8,-12};  Plane Surface(106)={106}; 

Curve Loop(201)={21,22,23,24}; Surface(201)={201}; 
Curve Loop(202)={25,26,27,28}; Surface(202)={202}; 
Curve Loop(203)={21,30,-25,-29}; Surface(203)={203}; 
Curve Loop(204)={22,31,-26,-30}; Surface(204)={204}; 
Curve Loop(205)={23,32,-27,-31}; Surface(205)={205}; 
Curve Loop(206)={24,29,-28,-32}; Surface(206)={206}; 

Curve Loop(301)={41,21,-42,-1}; Surface(301)={301}; 
Curve Loop(302)={42,22,-43,-2}; Surface(302)={302};
Curve Loop(303)={43,23,-44,-3}; Surface(303)={303}; 
Curve Loop(304)={44,24,-41,-4}; Surface(304)={304};
Curve Loop(305)={45,25,-46,-5}; Surface(305)={305}; 
Curve Loop(306)={46,26,-47,-6}; Surface(306)={306};
Curve Loop(307)={47,27,-48,-7}; Surface(307)={307}; 
Curve Loop(308)={48,28,-45,-8}; Surface(308)={308};
Curve Loop(309)={41,29,-45,-9}; Surface(309)={309}; 
Curve Loop(310)={42,30,-46,-10}; Surface(310)={310};
Curve Loop(311)={43,31,-47,-11}; Surface(311)={311}; 
Curve Loop(312)={44,32,-48,-12}; Surface(312)={312};

// --- 5. 体积定义 (显式定义 Surface Loop) ---
Surface Loop(1) = {101, 102, 103, 104, 105, 106}; Volume(1) = {1}; 
Surface Loop(2) = {101, 201, 301, 302, 303, 304}; Volume(2) = {2}; 
Surface Loop(3) = {102, 202, 305, 306, 307, 308}; Volume(3) = {3}; 
Surface Loop(4) = {103, 203, 301, 305, 309, 310}; Volume(4) = {4}; 
Surface Loop(5) = {104, 204, 302, 306, 310, 311}; Volume(5) = {5}; 
Surface Loop(6) = {105, 205, 303, 307, 311, 312}; Volume(6) = {6}; 
Surface Loop(7) = {106, 206, 304, 308, 312, 309}; Volume(7) = {7}; 

// --- 6. 约束与重组 ---
Transfinite Curve {1:12, 21:32} = nC;
Transfinite Curve {41:48} = nR;
Transfinite Surface "*";
Transfinite Volume "*";
Recombine Surface "*";
Mesh.RecombineAll = 1;

// --- 7. 纯 5 号元素配置 ---
Physical Volume("SphereHex") = {1, 2, 3, 4, 5, 6, 7};
Mesh.SaveAll = 0; 

Mesh.ElementOrder = 1;
Mesh.MshFileVersion = 2.2;
Mesh 3;
    )")
.arg(radius).arg(meshSize)
;
    // 2. 写入物理文件

    QString appPath = QCoreApplication::applicationDirPath();
    QFile::remove("output_geo.geo");
    QFile::remove("output_geo.msh");
    QString geoFile = appPath + "/output_geo.geo";
    QString mshFile = appPath + "/output_geo.msh";

    QFile file(geoFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << geoContent;
        file.close();
    }

    QProcess* gmsh = new QProcess();
    QStringList args;
    args << geoFile << "-3" << "-o" << mshFile; // -3 表示三维划分

    gmsh->start(appPath + "/gmsh.exe", args);

    if (!gmsh->waitForFinished()) {
        qDebug() << "Gmsh failed:" << gmsh->errorString();
    }
    else {
        qDebug() << "Success! You may find the mesh file in: " << mshFile;
        parseMeshFile(mshFile);
        // 接下来可以调用之前的 parseMshFile(mshFile) 进行解析
    }
}

void MainWindow::redrawAllEntities() {
    // 1. 清空 GLWidget 里的旧命令
    glWidget->clearDrawCommands();

    // 2. 遍历仓库里的所有实体
    const auto& allEntities = m_repository.getAllEntities();
    for (const auto& pair : allEntities) {
        const MeshEntity& entity = pair.second;

        // 提交点云渲染命令 (如果你需要)
        DrawCommand ptCmd;
        ptCmd.type = DrawCommand::Points;
        ptCmd.pointsCmd.points = entity.nodes;
        ptCmd.pointsCmd.size = 2.5f;
        glWidget->submitDrawCommand(ptCmd);

        // 提交线框渲染命令
        DrawCommand lineCmd;
        lineCmd.type = DrawCommand::Lines;
        lineCmd.linesCmd.points = entity.wireLines;
        lineCmd.linesCmd.width = 2.0f;
        glWidget->submitDrawCommand(lineCmd);
    }

    // 3. 触发显卡刷新
    glWidget->update();
}
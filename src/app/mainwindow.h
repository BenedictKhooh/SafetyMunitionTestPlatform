#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtWidgets>
#include <vector>

#include "src/core/common/common_types.h"
#include "src/core/common/MeshGenerator.h"
#include "src/core/common/MeshManager.h"
#include "src/core/generation/SphereGenerator.h"
#include "src/core/generation/CylinderGenerator.h"
#include "src/core/generation/CylindricalShellGenerator.h"
#include "src/core/generation/CubeGenerator.h"
#include "src/core/generation/HemisphereGenerator.h"
#include "src/core/generation/FrustumGenerator.h"
#include "src/core/generation/HalfCylinderGenerator.h"
#include "src/core/generation/HalfCylindricalShellGenerator.h"
#include "src/core/generation/OpenCylindricalShellGenerator.h"
#include "src/core/generation/HexagonalPrismGenerator.h"
#include "src/core/generation/PentagonalPrismGenerator.h"
#include "src/core/generation/TriangularPrismGenerator.h"
#include "src/core/common/EntityRepository.h"
#include "commandline.h"

// Forward declaration
class GLWidget;
class QPushButton;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onCommandEntered(const QString &command);
    void onDrawingComplete();
    void startMeshing();

    void parseMeshFile( QString fileName );

    void onShapeTypeChanged(const QString& text); // 响应下拉菜单切换
    void onGenerateButtonClicked();               // 响应生成按钮

private:
    void createMenuBar();
    void createToolBars();
    void createStatusBar();
    void createDockWidgets();
    void createCommandLine();
    void logCommand(const QString &command, const QString &response = "");
    void clean();
    void showHelp();

    QComboBox* m_shapeComboBox;
    QWidget* m_paramContainer;    // 动态参数的容器
    QVBoxLayout* m_paramLayout;   // 参数容器的布局
    // 用来记录当前界面上所有的输入框，方便读取数据
    QMap<QString, QDoubleSpinBox*> m_paramInputs;
    QLineEdit* m_nameInput;
    // 统一添加数值行的函数
    void addNumParam(const QString& labelText, const QString& key, double defaultValue);

    // 一个小工具函数：快速添加一行带标签的输入框
    void addParamRow(const QString& labelText, const QString& key, double defaultValue);

    QTextEdit *commandHistoryEdit;

    void DrawLine(QStringList args);

    std::vector<Instance> drawables;
    std::vector<GeoLine> drawables_lines;
    MeshManager* m_meshManager;

    // UI Widgets
    GLWidget *glWidget;
    QMenuBar *menuBar;
    QToolBar *fileToolBar;
    QToolBar *editToolBar;
    QToolBar *viewToolBar;
    QStatusBar *statusBar;
    QDockWidget *propertiesDock;
    QDockWidget *layersDock;
    QDockWidget *commandDock;
    QDockWidget *substanceDock;
    QDockWidget *boundaryDock;
    QDockWidget* interactionDock;

    CommandLine *commandLine;
    QTextEdit *propertiesEditor;
    QTreeWidget *layersTree;
    QTreeWidget *substanceTree;
    void updateSubstanceTree();
    void onSubstanceTreeContextMenu(const QPoint& pos);

    void handleDeleteEntity(QString name);

    QTreeWidget *boundaryTree;
    QTreeWidget* interactionTree;

   /*  Data containers for the reconstruction process
    std::vector<MeshPoint> m_points;
    std::vector<MeshPoint> m_points_sphere;*/

    //store all the points
    EntityRepository m_repository; // 实体仓库实例
    void redrawAllEntities();      // 封装一个全量重绘的函数
    void clearAllEntities();

    void exportToKFile(const QString& fileName); // 导出逻辑


    std::unordered_set<MeshPoint, MeshPointHasher, MeshPointComparator> points;

    AdjacencyGraph m_adjGraph;
    AdjacencyGraph m_adjGraph_sphere;
    std::vector<QuadFace> m_faces;
    std::vector<Hexahedron> m_hexahedra;

};
#endif // MAINWINDOW_H

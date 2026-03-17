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

private:
    void createMenuBar();
    void createToolBars();
    void createStatusBar();
    void createDockWidgets();
    void createCommandLine();
    void logCommand(const QString &command, const QString &response = "");
    void clean();
    void showHelp();

private:
    // 定义一个结构体来管理每个独立窗口的 UI 控件
    struct GeneratorUI {
        QComboBox* shapeComboBox;
        QWidget* paramContainer;
        QVBoxLayout* paramLayout;
        QLineEdit* nameInput;
        QMap<QString, QDoubleSpinBox*> paramInputs;
    };

    // 声明三个独立窗口的 UI 管理器
    GeneratorUI m_fragmentUI; // 破片
    GeneratorUI m_shellUI;    // 壳体
    GeneratorUI m_chargeUI;   // 装药

    // 声明统一的创建窗口的辅助函数
    void setupGeneratorTab(QTabWidget* tabWidget, const QString& title, const QStringList& shapes, GeneratorUI& ui);
    // 重构的槽函数，将具体的 ui 结构体作为参数传入
    void handleShapeTypeChanged(GeneratorUI& ui, const QString& text);
    void handleGenerateButtonClicked(GeneratorUI& ui);

    // 更新辅助函数，加入 GeneratorUI 参数
    void addNumParamToUI(GeneratorUI& ui, const QString& labelText, const QString& key, double defaultValue);

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

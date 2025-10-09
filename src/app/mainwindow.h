#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtWidgets>
#include <vector>
#include "src/core/reconstruction/reconstruction_engine.h"
#include "src/core/generation/sphere_projection.h"
#include "src/core/generation/cylinder_projection.h"
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

private:
    void createMenuBar();
    void createToolBars();
    void createStatusBar();
    void createDockWidgets();
    void createCommandLine();
    void logCommand(const QString &command, const QString &response = "");
    void showHelp();
    QTextEdit *commandHistoryEdit;

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

    CommandLine *commandLine;
    QTextEdit *propertiesEditor;
    QTreeWidget *layersTree;
    QTreeWidget *substanceTree;
    // Data containers for the reconstruction process
    std::vector<MeshPoint> m_points;
    std::vector<MeshPoint> m_points_sphere;

    AdjacencyGraph m_adjGraph;
    AdjacencyGraph m_adjGraph_sphere;
    std::vector<QuadFace> m_faces;
    std::vector<Hexahedron> m_hexahedra;

};
#endif // MAINWINDOW_H

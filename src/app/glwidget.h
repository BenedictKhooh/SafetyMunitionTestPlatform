#ifndef GLWIDGET_H
#define GLWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include "src/core/reconstruction/reconstruction_engine.h"
#include "src/core/common/common_types.h"
#include "Drawable.h"

class GLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit GLWidget(QWidget *parent = nullptr);
    ~GLWidget() override;

    // --- Public interface for updating drawable data ---
    void setPoints(const std::vector<MeshPoint>& points);
    void setAdjacencyGraph(const AdjacencyGraph& graph);
    void setFaces(const std::vector<QuadFace>& faces);
    void setHexahedra(const std::vector<Hexahedron>& hexahedra);
	void setDrawableInstances(std::vector<Instance> instances) { drawable_buffer = instances;  }
    void setDrawableLines(std::vector<GeoLine> lines) { drawable_line_buffer = lines; }

    void reset();
    float rotationX, rotationY, rotationZ;
    float scaleFactor;
    QVector3D translation;

    void drawGrid();
    void drawCube();

    enum class DrawingMode { None, Line, Circle, Rectangle };
    DrawingMode currentMode;
    QPointF startPoint;
    QList<QPair<QPointF, QPointF>> lines;
    QList<QPair<QPointF, float>> circles;
    QList<QPair<QPointF, QPointF>> rectangles;
    QPoint lastMousePos;

    QOpenGLBuffer m_pointVBO;
    QOpenGLShaderProgram m_shaderProgram;

protected:
    // --- OpenGL Event Handlers ---
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    // --- Qt Event Handlers for Camera Control ---
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

public:

    // --- Drawing Helper Functions ---
    void drawPoints( std::vector<MeshPoint> );
    void drawPoints(std::vector<Vector3>);
    void drawGraph( AdjacencyGraph );
    void drawFaces( std::vector<QuadFace> );
    void drawHexahedra( std::vector<Hexahedron> );
    void drawAxes();
    void drawLines(const std::vector<Vector3>& points);
    QPoint project(const QMatrix4x4 &mvp, const QVector3D &point3d);

    // --- Data Storage ---
    std::vector<MeshPoint> m_points;
    AdjacencyGraph m_adjGraph;
    std::vector<QuadFace> m_faces;
    std::vector<Hexahedron> m_hexahedra;

    std::vector<Instance> drawable_buffer;
    std::vector<GeoLine> drawable_line_buffer;


    // --- Camera and Transformation Matrices ---
    QMatrix4x4 m_projMatrix;
    QMatrix4x4 m_viewMatrix;
    QVector2D m_lastMousePos;
    float m_zoom;
    QQuaternion m_rotation;

	// --Drawable Command Interface---A
    void submitDrawCommand(const DrawCommand& cmd);
    void clearDrawCommands();
    std::vector<DrawCommand> m_drawCommands;


signals:
    void drawingComplete();

};

#endif // GLWIDGET_H

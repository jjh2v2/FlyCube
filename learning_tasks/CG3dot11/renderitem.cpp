#include "renderitem.h"

class SwapContext
{
public:
    SwapContext(QOpenGLContext *ctx, QSurface *surf)
    {
        p_ctx = QOpenGLContext::currentContext();
        p_surf = p_ctx->surface();
        ctx->makeCurrent(surf);
    }
    ~SwapContext()
    {
        p_ctx->makeCurrent(p_surf);
    }

private:
    QOpenGLContext *p_ctx;
    QSurface *p_surf;
};

RenderItem::RenderItem() : input_polygon(PrintPolygon::Instance()), m_input(false)
{
    connect(this, SIGNAL(windowChanged(QQuickWindow*)), this, SLOT(handleWindowChanged(QQuickWindow*)));
    ctx.reset(new QOpenGLContext(this));
    ctx->create();
}

bool RenderItem::isInput() const
{
    return m_input;
}

void RenderItem::setInput(bool val)
{
    if (m_input == val)
        return;

    m_input = val;

    if (m_input)
        cleanScene();

    emit inputChanged(val);
}

std::pair<float, float> RenderItem::pointToGL(int point_x, int point_y)
{
    int m_width = this->window()->width();
    int m_height = this->window()->height();
    float ratio = (float)m_height / (float)m_width;
    float XPixelSize = 2.0f * (1.0f * point_x / m_width);
    float YPixelSize = 2.0f * (1.0f * point_y / m_height);
    float Xposition = XPixelSize - 1.0f;
    float Yposition = (2.0f - YPixelSize) - 1.0f;
    if (m_width >= m_height)
        Xposition /= ratio;
    else
        Yposition *= ratio;

    return{ Xposition, Yposition };
}

void RenderItem::doSendPoint(int point_x, int point_y)
{
    SwapContext set_ctx(ctx.get(), window());
    glm::vec2 point;
    std::tie(point.x, point.y) = pointToGL(point_x, point_y);
    input_polygon.add_point(point);
    m_points.push_back(point);
    if (m_points.size() >= 3)
    {
        setInput(false);
        finishBox();
    }
}

void RenderItem::doSendFuturePoint(int point_x, int point_y)
{
    SwapContext set_ctx(ctx.get(), window());
    glm::vec2 point;
    std::tie(point.x, point.y) = pointToGL(point_x, point_y);
    input_polygon.future_point(point);
}

void RenderItem::handleWindowChanged(QQuickWindow *win)
{
    if (win)
    {
        connect(win, SIGNAL(beforeSynchronizing()), this, SLOT(sync()), Qt::DirectConnection);
        connect(win, SIGNAL(sceneGraphInvalidated()), this, SLOT(cleanup()), Qt::DirectConnection);
        win->setClearBeforeRendering(false);
    }
}

void RenderItem::cleanScene()
{
    SwapContext set_ctx(ctx.get(), window());
    input_polygon.clear();
    m_points.clear();
}

void RenderItem::finishBox()
{
    if (m_points.size() != 3)
        return ;
    glm::vec2 a = m_points[0] - m_points[1];
    glm::vec2 b = m_points[2] - m_points[1];
    m_points.push_back(m_points[1] + (a + b));
    input_polygon.add_point(m_points.back());

    m_points.push_back(m_points.front());
    input_polygon.add_point(m_points.back());

    input_polygon.finish_box();
}

void RenderItem::paint()
{
    paintPolygon();
}

void RenderItem::paintPolygon()
{
    SwapContext set_ctx(ctx.get(), window());
    input_polygon.draw();
}

void RenderItem::sync()
{
    SwapContext set_ctx(ctx.get(), window());
    static bool initSt = false;
    if (!initSt)
    {
        ogl_LoadFunctions();
        connect(window(), SIGNAL(beforeRendering()), this, SLOT(paint()), Qt::DirectConnection);
        initSt = input_polygon.init();
    }
    input_polygon.resize(0, 0, window()->width(), window()->height());
}

void RenderItem::cleanup()
{
    SwapContext set_ctx(ctx.get(), window());
    input_polygon.destroy();
}
#include <campello_widgets/ui/path.hpp>
#include <cmath>

namespace systems::leal::campello_widgets
{

    void Path::moveTo(float x, float y)
    {
        PathCommand cmd;
        cmd.type = PathCommandType::moveTo;
        cmd.p1 = Offset{x, y};
        commands_.push_back(cmd);
        current_point_ = Offset{x, y};
        start_point_ = current_point_;
        has_current_point_ = true;
    }

    void Path::lineTo(float x, float y)
    {
        if (!has_current_point_) {
            moveTo(x, y);
            return;
        }
        PathCommand cmd;
        cmd.type = PathCommandType::lineTo;
        cmd.p1 = Offset{x, y};
        commands_.push_back(cmd);
        current_point_ = Offset{x, y};
    }

    void Path::cubicTo(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y)
    {
        if (!has_current_point_) {
            moveTo(x, y);
            return;
        }
        PathCommand cmd;
        cmd.type = PathCommandType::cubicTo;
        cmd.cp1 = Offset{cp1x, cp1y};
        cmd.cp2 = Offset{cp2x, cp2y};
        cmd.p1 = Offset{x, y};
        commands_.push_back(cmd);
        current_point_ = Offset{x, y};
    }

    void Path::quadTo(float cpx, float cpy, float x, float y)
    {
        if (!has_current_point_) {
            moveTo(x, y);
            return;
        }
        PathCommand cmd;
        cmd.type = PathCommandType::quadTo;
        cmd.cp1 = Offset{cpx, cpy};
        cmd.p1 = Offset{x, y};
        commands_.push_back(cmd);
        current_point_ = Offset{x, y};
    }

    void Path::arcTo(const Rect& rect, float start_angle, float sweep_angle, bool force_move_to)
    {
        if (force_move_to || !has_current_point_) {
            // Calculate start point on the oval
            float a = rect.width * 0.5f;
            float b = rect.height * 0.5f;
            float cx = rect.left() + a;
            float cy = rect.top() + b;
            float x = cx + a * std::cos(start_angle);
            float y = cy + b * std::sin(start_angle);
            moveTo(x, y);
        }
        
        PathCommand cmd;
        cmd.type = PathCommandType::arcTo;
        cmd.p1 = Offset{rect.left(), rect.top()};  // Store rect origin
        cmd.cp1 = Offset{rect.width, rect.height}; // Store rect size
        cmd.start_angle = start_angle;
        cmd.sweep_angle = sweep_angle;
        commands_.push_back(cmd);
        
        // Update current point to end of arc
        float a = rect.width * 0.5f;
        float b = rect.height * 0.5f;
        float cx = rect.left() + a;
        float cy = rect.top() + b;
        float end_angle = start_angle + sweep_angle;
        current_point_ = Offset{
            cx + a * std::cos(end_angle),
            cy + b * std::sin(end_angle)
        };
    }

    void Path::arcToPoint(const Offset& arc_end, const Offset& arc_control, float radius)
    {
        // Simplified implementation - just draw a line
        // Full implementation would compute the tangent arc
        if (!has_current_point_) {
            moveTo(arc_end.x, arc_end.y);
            return;
        }
        // For now, just line to the end point
        lineTo(arc_end.x, arc_end.y);
    }

    void Path::close()
    {
        if (!has_current_point_) return;
        PathCommand cmd;
        cmd.type = PathCommandType::close;
        commands_.push_back(cmd);
        current_point_ = start_point_;
    }

    void Path::addRect(const Rect& rect)
    {
        moveTo(rect.left(), rect.top());
        lineTo(rect.right(), rect.top());
        lineTo(rect.right(), rect.bottom());
        lineTo(rect.left(), rect.bottom());
        close();
    }

    void Path::addOval(const Rect& rect)
    {
        // Use arc to approximate oval with 4 cubic bezier curves
        float cx = rect.x + rect.width * 0.5f;
        float cy = rect.y + rect.height * 0.5f;
        float rx = rect.width * 0.5f;
        float ry = rect.height * 0.5f;
        
        // Magic constant for cubic bezier approximation of circle
        const float k = 0.5522847498f;
        
        moveTo(cx + rx, cy);
        cubicTo(cx + rx, cy + k * ry, cx + k * rx, cy + ry, cx, cy + ry);
        cubicTo(cx - k * rx, cy + ry, cx - rx, cy + k * ry, cx - rx, cy);
        cubicTo(cx - rx, cy - k * ry, cx - k * rx, cy - ry, cx, cy - ry);
        cubicTo(cx + k * rx, cy - ry, cx + rx, cy - k * ry, cx + rx, cy);
        close();
    }

    void Path::addRRect(const RRect& rrect)
    {
        // Simplified: draw as regular rect if radius is small
        float rx, ry;
        rrect.getSafeRadii(rx, ry);
        
        if (rx < 0.001f || ry < 0.001f) {
            addRect(rrect.rect);
            return;
        }
        
        // For now, just draw the rect without rounded corners
        // Full implementation would use arcs at corners
        addRect(rrect.rect);
    }

    void Path::addArc(const Rect& oval, float start_angle, float sweep_angle)
    {
        arcTo(oval, start_angle, sweep_angle, true);
    }

    void Path::addPolygon(const std::vector<Offset>& points, bool close_path)
    {
        if (points.empty()) return;
        
        moveTo(points[0].x, points[0].y);
        for (size_t i = 1; i < points.size(); ++i) {
            lineTo(points[i].x, points[i].y);
        }
        if (close_path) {
            close();
        }
    }

    void Path::reset()
    {
        commands_.clear();
        has_current_point_ = false;
        fill_type_ = FillType::winding;
    }

    Rect Path::getBounds() const
    {
        if (commands_.empty()) {
            return Rect::fromLTWH(0, 0, 0, 0);
        }
        
        float min_x = std::numeric_limits<float>::max();
        float min_y = std::numeric_limits<float>::max();
        float max_x = std::numeric_limits<float>::lowest();
        float max_y = std::numeric_limits<float>::lowest();
        
        for (const auto& cmd : commands_) {
            switch (cmd.type) {
                case PathCommandType::moveTo:
                case PathCommandType::lineTo:
                    min_x = std::min(min_x, cmd.p1.x);
                    min_y = std::min(min_y, cmd.p1.y);
                    max_x = std::max(max_x, cmd.p1.x);
                    max_y = std::max(max_y, cmd.p1.y);
                    break;
                case PathCommandType::cubicTo:
                    min_x = std::min({min_x, cmd.cp1.x, cmd.cp2.x, cmd.p1.x});
                    min_y = std::min({min_y, cmd.cp1.y, cmd.cp2.y, cmd.p1.y});
                    max_x = std::max({max_x, cmd.cp1.x, cmd.cp2.x, cmd.p1.x});
                    max_y = std::max({max_y, cmd.cp1.y, cmd.cp2.y, cmd.p1.y});
                    break;
                case PathCommandType::quadTo:
                    min_x = std::min({min_x, cmd.cp1.x, cmd.p1.x});
                    min_y = std::min({min_y, cmd.cp1.y, cmd.p1.y});
                    max_x = std::max({max_x, cmd.cp1.x, cmd.p1.x});
                    max_y = std::max({max_y, cmd.cp1.y, cmd.p1.y});
                    break;
                case PathCommandType::arcTo:
                    // Approximate with rect bounds
                    min_x = std::min(min_x, cmd.p1.x);
                    min_y = std::min(min_y, cmd.p1.y);
                    max_x = std::max(max_x, cmd.p1.x + cmd.cp1.x);
                    max_y = std::max(max_y, cmd.p1.y + cmd.cp1.y);
                    break;
                case PathCommandType::close:
                    break;
            }
        }
        
        return Rect::fromLTRB(min_x, min_y, max_x, max_y);
    }

    bool Path::contains(const Offset& point) const
    {
        // Simplified point-in-path test using ray casting
        // This is a basic implementation that works for simple polygons
        if (commands_.empty()) return false;
        
        int crossings = 0;
        Offset prev = start_point_;
        bool has_prev = false;
        
        for (const auto& cmd : commands_) {
            switch (cmd.type) {
                case PathCommandType::moveTo:
                    if (has_prev && prev.y != cmd.p1.y) {
                        // Check crossing
                    }
                    prev = cmd.p1;
                    has_prev = true;
                    break;
                case PathCommandType::lineTo: {
                    if (has_prev) {
                        // Ray casting algorithm
                        if ((prev.y > point.y) != (cmd.p1.y > point.y)) {
                            float x = prev.x + (cmd.p1.x - prev.x) * 
                                     (point.y - prev.y) / (cmd.p1.y - prev.y);
                            if (x > point.x) crossings++;
                        }
                    }
                    prev = cmd.p1;
                    has_prev = true;
                    break;
                }
                default:
                    break;
            }
        }
        
        return (crossings % 2) == 1;
    }

    void Path::transform(const Matrix4& matrix)
    {
        for (auto& cmd : commands_) {
            auto transform_point = [&matrix](const Offset& p) -> Offset {
                float x = matrix.data[0] * p.x + matrix.data[4] * p.y + matrix.data[12];
                float y = matrix.data[1] * p.x + matrix.data[5] * p.y + matrix.data[13];
                return Offset{x, y};
            };
            
            switch (cmd.type) {
                case PathCommandType::moveTo:
                case PathCommandType::lineTo:
                    cmd.p1 = transform_point(cmd.p1);
                    break;
                case PathCommandType::cubicTo:
                    cmd.cp1 = transform_point(cmd.cp1);
                    cmd.cp2 = transform_point(cmd.cp2);
                    cmd.p1 = transform_point(cmd.p1);
                    break;
                case PathCommandType::quadTo:
                    cmd.cp1 = transform_point(cmd.cp1);
                    cmd.p1 = transform_point(cmd.p1);
                    break;
                case PathCommandType::arcTo:
                    // Transform the rect bounds
                    cmd.p1 = transform_point(cmd.p1);
                    cmd.cp1 = Offset{
                        cmd.cp1.x * matrix.data[0],
                        cmd.cp1.y * matrix.data[5]
                    };
                    break;
                case PathCommandType::close:
                    break;
            }
        }
    }

    Path Path::copy() const
    {
        Path p;
        p.commands_ = commands_;
        p.fill_type_ = fill_type_;
        p.current_point_ = current_point_;
        p.start_point_ = start_point_;
        p.has_current_point_ = has_current_point_;
        return p;
    }

} // namespace systems::leal::campello_widgets

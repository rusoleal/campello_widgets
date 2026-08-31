#include <campello_widgets/ui/path.hpp>
#include "gpu/path_tessellation.hpp"

#include <clipper2/clipper.h>

namespace systems::leal::campello_widgets
{
    namespace
    {
        Clipper2Lib::PathsD toClipperPaths(const std::vector<PathContour>& contours)
        {
            Clipper2Lib::PathsD paths;
            paths.reserve(contours.size());
            for (const auto& contour : contours)
            {
                Clipper2Lib::PathD path;
                path.reserve(contour.size());
                for (const auto& v : contour)
                    path.emplace_back(static_cast<double>(v.x), static_cast<double>(v.y));
                paths.push_back(std::move(path));
            }
            return paths;
        }

        Clipper2Lib::FillRule toFillRule(Path::FillType type)
        {
            return type == Path::FillType::evenOdd
                ? Clipper2Lib::FillRule::EvenOdd
                : Clipper2Lib::FillRule::NonZero;
        }
    } // namespace

    Path Path::combine(PathOperation op, const Path& path1, const Path& path2)
    {
        const Clipper2Lib::PathsD subjects = toClipperPaths(buildPathContours(path1));
        const Clipper2Lib::PathsD clips    = toClipperPaths(buildPathContours(path2));
        const Clipper2Lib::FillRule fill_rule = toFillRule(path1.fillType());

        Clipper2Lib::ClipType clip_type = Clipper2Lib::ClipType::Union;
        bool swap_subject_and_clip = false;
        switch (op)
        {
        case PathOperation::difference:
            clip_type = Clipper2Lib::ClipType::Difference;
            break;
        case PathOperation::intersect:
            clip_type = Clipper2Lib::ClipType::Intersection;
            break;
        case PathOperation::unionOp:
            clip_type = Clipper2Lib::ClipType::Union;
            break;
        case PathOperation::xorOp:
            clip_type = Clipper2Lib::ClipType::Xor;
            break;
        case PathOperation::reverseDifference:
            clip_type = Clipper2Lib::ClipType::Difference;
            swap_subject_and_clip = true;
            break;
        }

        // Decimal digits of precision Clipper2 scales floats by internally (its sweep-line
        // core operates on int64 coordinates for exactness). Default is 2; bumped to 4 for
        // finer sub-pixel fidelity given this is UI/screen-space geometry, well under the
        // library's CLIPPER2_MAX_DECIMAL_PRECISION=8 build default.
        constexpr int kPrecision = 4;

        const Clipper2Lib::PathsD result = swap_subject_and_clip
            ? Clipper2Lib::BooleanOp(clip_type, fill_rule, clips, subjects, kPrecision)
            : Clipper2Lib::BooleanOp(clip_type, fill_rule, subjects, clips, kPrecision);

        Path out;
        for (const auto& ring : result)
        {
            if (ring.empty()) continue;
            out.moveTo(static_cast<float>(ring[0].x), static_cast<float>(ring[0].y));
            for (std::size_t i = 1; i < ring.size(); ++i)
                out.lineTo(static_cast<float>(ring[i].x), static_cast<float>(ring[i].y));
            out.close();
        }
        out.setFillType(FillType::winding);
        return out;
    }

} // namespace systems::leal::campello_widgets

// SPDX-License-Identifier: MIT
#include "rinswf/frame_renderer.hpp"
#include <cassert>
int main() {
    using namespace rinswf;
    Surface target;
    assert(make_surface(8,8,target));
    FrameAssets assets;
    Bitmap image; image.character_id=42;image.width=1;image.height=1;
    image.rgba={255,0,0,255};
    assets.bitmaps.images.emplace(42,image);
    FrameSnapshot frame;
    DisplayEntry object;object.character_id=42;object.depth=1;
    object.has_matrix=true;object.matrix.translate_x=40;object.matrix.translate_y=20;
    frame.objects.push_back(object);
    assert(render_frame(frame,assets,target));
    assert(target.rgba[(1*8+2)*4]==255);
    assert(target.rgba[(1*8+2)*4+3]==255);
    auto before=target.rgba;
    frame.objects[0].character_id=999;
    assert(!render_frame(frame,assets,target));
    assert(target.rgba==before);
    frame.objects[0].character_id=42;
    frame.objects[0].matrix.scale_x=2.0;
    assert(!render_frame(frame,assets,target));
    assert(target.rgba==before);
    // Duplicate shape/bitmap IDs must fail without mutating previously collected assets.
    Document duplicate;
    Tag lossless;
    lossless.code=20;
    lossless.data={42,0}; // duplicate decode fails safely on truncated input
    duplicate.tags.push_back(lossless);
    FrameAssets unchanged=assets;
    assert(!collect_frame_assets(duplicate,unchanged));
    assert(unchanged.bitmaps.images.size()==assets.bitmaps.images.size());
    // Affine placement: a 90-degree rotation of the 1x1 bitmap.
    frame.objects[0].matrix.scale_x=0;
    frame.objects[0].matrix.scale_y=0;
    frame.objects[0].matrix.rotate_skew_0=-1;
    frame.objects[0].matrix.rotate_skew_1=1;
    frame.objects[0].matrix.translate_x=60;
    frame.objects[0].matrix.translate_y=20;
    Surface rotated;
    assert(make_surface(8,8,rotated));
    assert(render_frame(frame,assets,rotated));
    assert(rotated.rgba[(1*8+2)*4+3]==255);
    // Singular transforms must fail without modifying the target.
    frame.objects[0].matrix.rotate_skew_1=0;
    const auto preserved=rotated.rgba;
    assert(!render_frame(frame,assets,rotated));
    assert(rotated.rgba==preserved);
    // Affine Shape: scale a solid square through the frame renderer.
    ShapeGeometry square;
    square.character_id=7;
    square.fills.push_back({{255,0,0,255}});
    square.commands.push_back({ShapeCommandKind::move_to,{}, {0,0},0,0,0});
    square.commands.push_back({ShapeCommandKind::line_to,{}, {40,0},0,1,0});
    square.commands.push_back({ShapeCommandKind::line_to,{}, {40,40},0,1,0});
    square.commands.push_back({ShapeCommandKind::line_to,{}, {0,40},0,1,0});
    square.commands.push_back({ShapeCommandKind::line_to,{}, {0,0},0,1,0});
    assets.shapes.emplace(7,square);
    FrameSnapshot shape_frame;
    DisplayEntry placed;
    placed.character_id=7;placed.depth=1;placed.has_matrix=true;
    placed.matrix.scale_x=2;placed.matrix.scale_y=2;
    shape_frame.objects.push_back(placed);
    Surface shape_surface;
    assert(make_surface(16,16,shape_surface));
    assert(render_frame(shape_frame,assets,shape_surface));
    assert(shape_surface.rgba[(1*16+1)*4+3]>0);
    // Interpolation of opaque colors at a half-pixel shift.
    Bitmap gradient;gradient.character_id=55;gradient.width=2;gradient.height=1;
    gradient.rgba={255,0,0,255,0,0,255,255};
    assets.bitmaps.images.emplace(55,gradient);
    FrameSnapshot filtered;
    DisplayEntry shifted;shifted.depth=1;shifted.character_id=55;
    shifted.has_matrix=true;shifted.matrix.translate_x=10;
    filtered.objects.push_back(shifted);
    Surface filtered_surface;
    assert(make_surface(4,4,filtered_surface));
    assert(render_frame(filtered,assets,filtered_surface));
    assert(filtered_surface.rgba[(0*4+1)*4]>0);
    assert(filtered_surface.rgba[(0*4+1)*4+2]>0);
    // A solid vector square should retain its filled interior under rotation.
    placed.matrix.scale_x=0; placed.matrix.scale_y=0;
    placed.matrix.rotate_skew_0=-1; placed.matrix.rotate_skew_1=1;
    placed.matrix.translate_x=100; placed.matrix.translate_y=0;
    shape_frame.objects[0]=placed;
    clear(shape_surface,{0,0,0,0});
    assert(render_frame(shape_frame,assets,shape_surface));
    assert(shape_surface.rgba[(1*16+4)*4+3]>0);
    // Uniform scaling preserves a stroke's circular cross section.
    ShapeGeometry stroked;
    stroked.character_id=88;
    ShapeLine line;
    line.width=20;
    line.color={10,240,20,255};
    stroked.lines.push_back(line);
    stroked.commands.push_back({ShapeCommandKind::move_to,{}, {20,40},0,0,0});
    stroked.commands.push_back({ShapeCommandKind::line_to,{}, {100,40},0,0,1});
    assets.shapes.emplace(88,stroked);
    FrameSnapshot stroke_frame;
    DisplayEntry stroke_entry;
    stroke_entry.depth=1;stroke_entry.character_id=88;stroke_entry.has_matrix=true;
    stroke_entry.matrix.scale_x=2;stroke_entry.matrix.scale_y=2;
    stroke_frame.objects.push_back(stroke_entry);
    Surface stroke_surface;
    assert(make_surface(16,16,stroke_surface));
    assert(render_frame(stroke_frame,assets,stroke_surface));
    assert(stroke_surface.rgba[(4*16+6)*4+3]>0);
    // Non-uniform scaling of a butt-capped straight stroke uses its transformed outline.
    stroked.lines[0].start_cap=1;
    stroked.lines[0].end_cap=1;
    assets.shapes[88]=stroked;
    stroke_frame.objects[0].matrix.scale_x=3;
    stroke_frame.objects[0].matrix.scale_y=1;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    assert(stroke_surface.rgba[(2*16+9)*4+3]>0);
    // Two straight edges with butt caps can share the affine outline path.
    ShapeGeometry polyline=stroked;
    polyline.commands.push_back({ShapeCommandKind::line_to,{}, {100,120},0,0,1});
    assets.shapes[88]=polyline;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    assert(stroke_surface.rgba[(4*16+9)*4+3]>0);
    // An anisotropically scaled polyline also supports a bevel outer join.
    polyline.lines[0].join=1;
    assets.shapes[88]=polyline;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    assert(stroke_surface.rgba[(4*16+9)*4+3]>0);
    // Miter joins and limit-triggered bevel fallback use the same transformed outlines.
    polyline.lines[0].join=2;
    polyline.lines[0].miter_limit=5.0;
    assets.shapes[88]=polyline;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    polyline.lines[0].miter_limit=1.0;
    assets.shapes[88]=polyline;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    return 0;
}

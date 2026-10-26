// SPDX-License-Identifier: MIT
#include "rinswf/frame_renderer.hpp"
#include <cassert>
#include <limits>
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
    // Scaling bitmap placements is supported by the affine renderer.
    frame.objects[0].matrix.scale_x=2.0;
    Surface scaled;
    assert(make_surface(8,8,scaled));
    assert(render_frame(frame,assets,scaled));
    assert(scaled.rgba[(1*8+3)*4+3]>0);
    // The previous target remains unchanged when rendering into another surface.
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
    // Nonfinite placement matrices are rejected without touching the target.
    frame.objects[0].matrix.scale_x=std::numeric_limits<double>::infinity();
    assert(!render_frame(frame,assets,rotated));
    assert(rotated.rgba==preserved);
    frame.objects[0].matrix.scale_x=1.0;
    frame.objects[0].matrix.rotate_skew_0=std::numeric_limits<double>::quiet_NaN();
    assert(!render_frame(frame,assets,rotated));
    assert(rotated.rgba==preserved);
    // An invertible but strongly anisotropic bitmap matrix must be safe.
    frame.objects[0].matrix.scale_x=1e10;
    frame.objects[0].matrix.scale_y=1e-10;
    frame.objects[0].matrix.rotate_skew_0=0;
    frame.objects[0].matrix.rotate_skew_1=0;
    assert(render_frame(frame,assets,rotated));
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
    // Round joins use a tessellated circular sector under affine scaling.
    polyline.lines[0].join=0;
    assets.shapes[88]=polyline;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    assert(stroke_surface.rgba[(4*16+9)*4+3]>0);
    // Round and square endcaps must remain valid after nonuniform transforms.
    polyline.lines[0].start_cap=0;
    polyline.lines[0].end_cap=0;
    assets.shapes[88]=polyline;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    polyline.lines[0].start_cap=2;
    polyline.lines[0].end_cap=2;
    assets.shapes[88]=polyline;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    // Repeating a translucent edge must not double its opacity.
    ShapeGeometry translucent=polyline;
    translucent.lines[0].color.a=128;
    assets.shapes[88]=translucent;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    const auto alpha_once=stroke_surface.rgba[(2*16+9)*4+3];
    translucent.commands.push_back({ShapeCommandKind::move_to,{}, {20,40},0,0,0});
    translucent.commands.push_back({ShapeCommandKind::line_to,{}, {100,40},0,0,1});
    assets.shapes[88]=translucent;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    assert(stroke_surface.rgba[(2*16+9)*4+3]==alpha_once);
    // Oppositely traversed duplicate segments must union, not cancel.
    ShapeGeometry reversed=translucent;
    reversed.commands.push_back({ShapeCommandKind::move_to,{}, {100,40},0,0,0});
    reversed.commands.push_back({ShapeCommandKind::line_to,{}, {20,40},0,0,1});
    assets.shapes[88]=reversed;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    assert(stroke_surface.rgba[(2*16+9)*4+3]>0);
    // Clipped contour scratch space must preserve pixels away from a local stroke.
    Surface clipped;
    assert(make_surface(256,256,clipped));
    FrameSnapshot distant;
    DisplayEntry far_entry;
    far_entry.depth=1;far_entry.character_id=88;far_entry.has_matrix=true;
    far_entry.matrix.scale_x=3;far_entry.matrix.scale_y=1;
    far_entry.matrix.translate_x=3000;far_entry.matrix.translate_y=3000;
    distant.objects.push_back(far_entry);
    assert(render_frame(distant,assets,clipped));
    assert(clipped.rgba[3]==0);
    // The union mask must composite correctly at a nonzero origin.
    Surface offset_surface;
    assert(make_surface(256,256,offset_surface));
    assert(render_frame(distant,assets,offset_surface));
    assert(offset_surface.rgba[3]==0);
    // Sparse-looking union of translucent paths uses single-channel coverage.
    Surface one_channel;
    assert(make_surface(256,256,one_channel));
    assert(render_frame(distant,assets,one_channel));
    assert(one_channel.rgba==offset_surface.rgba);
    // Distant translucent contours are composited through sparse tile storage.
    Surface sparse_union;
    assert(make_surface(256,256,sparse_union));
    assert(render_frame(distant,assets,sparse_union));
    assert(sparse_union.rgba==offset_surface.rgba);
    // A quadratic centerline can be tessellated and transformed anisotropically.
    ShapeGeometry curved_stroke=polyline;
    curved_stroke.commands.clear();
    curved_stroke.commands.push_back({ShapeCommandKind::move_to,{}, {20,40},0,0,0});
    curved_stroke.commands.push_back({ShapeCommandKind::quadratic_to,{60,140},{100,40},0,0,1});
    assets.shapes[88]=curved_stroke;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    assert(stroke_surface.rgba[(5*16+9)*4+3]>0);
    // Extremely curved segments must be rejected rather than silently clamped.
    ShapeGeometry extreme=curved_stroke;
    extreme.commands[1].control={0,2000000000};
    assets.shapes[88]=extreme;
    auto preserved=stroke_surface.rgba;
    assert(!render_frame(stroke_frame,assets,stroke_surface));
    assert(stroke_surface.rgba==preserved);
    // Tiny quadratic curves whose subsegments round to the same twip
    // must still render successfully without zero-length join geometry.
    ShapeGeometry tiny=curved_stroke;
    tiny.commands[1].control={21,41};
    tiny.commands[1].endpoint={22,40};
    assets.shapes[88]=tiny;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    // Explicitly closed quadratic contours join their final and first edges.
    ShapeGeometry closed_curve=curved_stroke;
    closed_curve.commands.push_back({ShapeCommandKind::quadratic_to,{20,120},{20,40},0,0,1});
    assets.shapes[88]=closed_curve;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    // Closing a curve must suppress its nominal square-cap extensions.
    closed_curve.lines[0].start_cap=2;
    closed_curve.lines[0].end_cap=2;
    assets.shapes[88]=closed_curve;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    closed_curve.lines[0].start_cap=0;
    closed_curve.lines[0].end_cap=0;
    assets.shapes[88]=closed_curve;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    // Crossing reversed strokes should render without cancellation or repeated alpha.
    ShapeGeometry cross=translucent;
    cross.commands.clear();
    cross.commands.push_back({ShapeCommandKind::move_to,{}, {20,40},0,0,0});
    cross.commands.push_back({ShapeCommandKind::line_to,{}, {100,120},0,0,1});
    cross.commands.push_back({ShapeCommandKind::move_to,{}, {100,40},0,0,0});
    cross.commands.push_back({ShapeCommandKind::line_to,{}, {20,120},0,0,1});
    assets.shapes[88]=cross;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    assert(stroke_surface.rgba[(4*16+9)*4+3]>0);
    // Affine outline union must honor the caller's tile budget and fail atomically.
    Surface budget_target;
    assert(make_surface(256,256,budget_target));
    ShapeGeometry wide=cross;
    wide.commands.clear();
    wide.commands.push_back({ShapeCommandKind::move_to,{}, {20,800},0,0,0});
    wide.commands.push_back({ShapeCommandKind::line_to,{}, {2200,800},0,0,1});
    assets.shapes[88]=wide;
    FrameSnapshot budget_frame;
    DisplayEntry budget_entry;
    budget_entry.depth=1;budget_entry.character_id=88;budget_entry.has_matrix=true;
    budget_entry.matrix.scale_x=2;budget_entry.matrix.scale_y=1;
    budget_frame.objects.push_back(budget_entry);
    const auto pristine=budget_target.rgba;
    const auto too_small=render_frame(budget_frame,assets,budget_target,{1,4096});
    assert(!too_small && too_small.error==Error::size_limit);
    assert(budget_target.rgba==pristine);
    assert(render_frame(budget_frame,assets,budget_target,{16,65536}));
    // Multiple overlapping and reversed contours must remain stable with
    // the cached tile lookup and degenerate contour skipping.
    ShapeGeometry overlapping=cross;
    overlapping.commands.push_back({ShapeCommandKind::move_to,{}, {20,40},0,0,0});
    overlapping.commands.push_back({ShapeCommandKind::line_to,{}, {20,40},0,0,1});
    assets.shapes[88]=overlapping;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    assert(stroke_surface.rgba[(4*16+9)*4+3]>0);
    // Multiple separated strokes force many sparse tiles and hash-table growth.
    ShapeGeometry spread=translucent;
    spread.commands.clear();
    for(int i=0;i<24;++i) {
        const int x=200+i*180;
        spread.commands.push_back({ShapeCommandKind::move_to,{}, {x,200},0,0,0});
        spread.commands.push_back({ShapeCommandKind::line_to,{}, {x+80,200},0,0,1});
    }
    assets.shapes[88]=spread;
    Surface spread_surface;
    assert(make_surface(512,64,spread_surface));
    FrameSnapshot spread_frame;
    DisplayEntry spread_entry;
    spread_entry.depth=1;spread_entry.character_id=88;spread_entry.has_matrix=true;
    spread_entry.matrix.scale_x=2;spread_entry.matrix.scale_y=1;
    spread_frame.objects.push_back(spread_entry);
    assert(render_frame(spread_frame,assets,spread_surface,{128,128*4096}));
    assert(spread_surface.rgba[(10*512+20)*4+3]>0);
    // Two half-pixel rectangles on opposite sides of one pixel should
    // union to full coverage rather than taking only the maximum half.
    ShapeGeometry subpixel;
    subpixel.character_id=88;
    ShapeLine subline; subline.width=10; subline.color={200,50,20,128};
    subline.start_cap=1;subline.end_cap=1;
    subpixel.lines.push_back(subline);
    subpixel.commands.push_back({ShapeCommandKind::move_to,{}, {10,0},0,0,0});
    subpixel.commands.push_back({ShapeCommandKind::line_to,{}, {10,20},0,0,1});
    subpixel.commands.push_back({ShapeCommandKind::move_to,{}, {20,0},0,0,0});
    subpixel.commands.push_back({ShapeCommandKind::line_to,{}, {20,20},0,0,1});
    assets.shapes[88]=subpixel;
    Surface samples;
    assert(make_surface(8,8,samples));
    FrameSnapshot sample_frame;
    DisplayEntry sample_entry;
    sample_entry.depth=1;sample_entry.character_id=88;sample_entry.has_matrix=true;
    sample_entry.matrix.scale_x=2;sample_entry.matrix.scale_y=1;
    sample_frame.objects.push_back(sample_entry);
    assert(render_frame(sample_frame,assets,samples));
    assert(samples.rgba[3]>0);
    // Slanted intersecting contours exercise all four scanline sample rows.
    ShapeGeometry slanted=cross;
    slanted.commands.push_back({ShapeCommandKind::move_to,{}, {30,30},0,0,0});
    slanted.commands.push_back({ShapeCommandKind::line_to,{}, {110,110},0,0,1});
    assets.shapes[88]=slanted;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    assert(stroke_surface.rgba[(4*16+9)*4+3]>0);
    // Adjacent overlapping and reversed contours exercise sorted X intersections.
    ShapeGeometry winding=slanted;
    winding.commands.push_back({ShapeCommandKind::move_to,{}, {110,110},0,0,0});
    winding.commands.push_back({ShapeCommandKind::line_to,{}, {30,30},0,0,1});
    assets.shapes[88]=winding;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    assert(stroke_surface.rgba[(4*16+9)*4+3]>0);
    // Dense crossing paths exercise the large-active-edge sorting branch.
    ShapeGeometry dense=translucent;
    dense.commands.clear();
    for(int i=0;i<40;++i) {
        const int x=20+i*5;
        dense.commands.push_back({ShapeCommandKind::move_to,{}, {x,0},0,0,0});
        dense.commands.push_back({ShapeCommandKind::line_to,{}, {x+60,240},0,0,1});
    }
    assets.shapes[88]=dense;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    // A tall slanted stroke traverses multiple X-reanchoring intervals.
    ShapeGeometry tall=translucent;
    tall.commands.clear();
    tall.commands.push_back({ShapeCommandKind::move_to,{}, {400,400},0,0,0});
    tall.commands.push_back({ShapeCommandKind::line_to,{}, {900,9000},0,0,1});
    assets.shapes[88]=tall;
    Surface tall_surface;
    assert(make_surface(128,512,tall_surface));
    FrameSnapshot tall_frame;
    DisplayEntry tall_entry;
    tall_entry.depth=1;
    tall_entry.character_id=88;
    tall_entry.has_matrix=true;
    tall_entry.matrix.scale_x=2;
    tall_entry.matrix.scale_y=1;
    tall_frame.objects.push_back(tall_entry);
    assert(render_frame(tall_frame,assets,tall_surface));
    // Interpolated center: x=2*(20+(200-20)*(45-20)/(450-20)) ~= 60.93.
    assert(tall_surface.rgba[(200*128+61)*4+3]>0);
    // A contour completely outside the surface must not write any pixels.
    ShapeGeometry offscreen=stroked;
    offscreen.commands.clear();
    offscreen.commands.push_back({ShapeCommandKind::move_to,{}, {10000,10000},0,0,0});
    offscreen.commands.push_back({ShapeCommandKind::line_to,{}, {12000,14000},0,0,1});
    assets.shapes[88]=offscreen;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    for(std::size_t i=3;i<stroke_surface.rgba.size();i+=4)
        assert(stroke_surface.rgba[i]==0);
    // Fully clipped contours do not require any sparse-mask tiles.
    assert(render_frame(stroke_frame,assets,stroke_surface,{0,0}));
    for(std::size_t i=3;i<stroke_surface.rgba.size();i+=4)
        assert(stroke_surface.rgba[i]==0);
    // Offscreen contours should neither change the visible stroke nor
    // consume the small sparse-mask budget.
    assets.shapes[88]=stroked;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface,{1,8192}));
    const auto visible_stroke=stroke_surface.rgba;
    ShapeGeometry combined=stroked;
    combined.commands.insert(combined.commands.end(),
                             offscreen.commands.begin(),offscreen.commands.end());
    assets.shapes[88]=combined;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface,{1,8192}));
    assert(stroke_surface.rgba==visible_stroke);
    // Clipping against the left edge must agree with an unclipped rendering.
    ShapeGeometry clipped=stroked;
    clipped.commands.clear();
    clipped.commands.push_back({ShapeCommandKind::move_to,{}, {-80,60},0,0,0});
    clipped.commands.push_back({ShapeCommandKind::line_to,{}, {140,100},0,0,1});
    assets.shapes[88]=clipped;
    Surface clipped_small,clipped_large;
    assert(make_surface(16,16,clipped_small));
    assert(make_surface(32,16,clipped_large));
    FrameSnapshot clipped_frame=stroke_frame;
    clipped_frame.objects[0].matrix.translate_x=0;
    clipped_frame.objects[0].matrix.translate_y=0;
    assert(render_frame(clipped_frame,assets,clipped_small));
    assert(render_frame(clipped_frame,assets,clipped_large));
    for(std::size_t y=0;y<16;++y)
        for(std::size_t x=0;x<16;++x)
            for(std::size_t channel=0;channel<4;++channel)
                assert(clipped_small.rgba[(y*16+x)*4+channel] ==
                       clipped_large.rgba[(y*32+x)*4+channel]);
    // The same world-space pixels must survive right, top and bottom clipping.
    // Compare the common top-left viewport against a larger surface.
    for(int boundary=0;boundary<3;++boundary) {
        ShapeGeometry edge_shape=stroked;
        edge_shape.commands.clear();
        if(boundary==0) { // right edge
            edge_shape.commands.push_back({ShapeCommandKind::move_to,{}, {180,60},0,0,0});
            edge_shape.commands.push_back({ShapeCommandKind::line_to,{}, {450,170},0,0,1});
        } else if(boundary==1) { // top edge
            edge_shape.commands.push_back({ShapeCommandKind::move_to,{}, {30,-110},0,0,0});
            edge_shape.commands.push_back({ShapeCommandKind::line_to,{}, {180,130},0,0,1});
        } else { // bottom edge
            edge_shape.commands.push_back({ShapeCommandKind::move_to,{}, {30,210},0,0,0});
            edge_shape.commands.push_back({ShapeCommandKind::line_to,{}, {180,420},0,0,1});
        }
        assets.shapes[88]=edge_shape;
        Surface small,large;
        assert(make_surface(16,16,small));
        assert(make_surface(32,32,large));
        FrameSnapshot a=stroke_frame,b=stroke_frame;
        a.objects[0].matrix.translate_x=0;
        a.objects[0].matrix.translate_y=0;
        b.objects[0].matrix.translate_x=0;
        b.objects[0].matrix.translate_y=0;
        assert(render_frame(a,assets,small));
        assert(render_frame(b,assets,large));
        for(std::size_t y=0;y<16;++y)
            for(std::size_t x=0;x<16;++x)
                for(std::size_t c=0;c<4;++c)
                    assert(small.rgba[(y*16+x)*4+c]==large.rgba[(y*32+x)*4+c]);
    }
    // Compare with a translated larger viewport too: the same world-space
    // region must render identically even when a negative edge is visible.
    ShapeGeometry shifted_shape=stroked;
    shifted_shape.commands.clear();
    shifted_shape.commands.push_back({ShapeCommandKind::move_to,{}, {-150,-120},0,0,0});
    shifted_shape.commands.push_back({ShapeCommandKind::line_to,{}, {380,370},0,0,1});
    assets.shapes[88]=shifted_shape;
    Surface shifted_small,shifted_large;
    assert(make_surface(16,16,shifted_small));
    assert(make_surface(48,48,shifted_large));
    FrameSnapshot origin_frame=stroke_frame,offset_frame=stroke_frame;
    origin_frame.objects[0].matrix.translate_x=0;
    origin_frame.objects[0].matrix.translate_y=0;
    offset_frame.objects[0].matrix.translate_x=16*20;
    offset_frame.objects[0].matrix.translate_y=16*20;
    assert(render_frame(origin_frame,assets,shifted_small));
    assert(render_frame(offset_frame,assets,shifted_large));
    for(std::size_t y=0;y<16;++y)
        for(std::size_t x=0;x<16;++x)
            for(std::size_t c=0;c<4;++c)
                assert(shifted_small.rgba[(y*16+x)*4+c] ==
                       shifted_large.rgba[((y+16)*48+x+16)*4+c]);
    // Multiple contours crossing different edges must union consistently.
    ShapeGeometry multi=stroked;
    multi.commands.clear();
    multi.commands.push_back({ShapeCommandKind::move_to,{}, {-40,80},0,0,0});
    multi.commands.push_back({ShapeCommandKind::line_to,{}, {390,80},0,0,1});
    multi.commands.push_back({ShapeCommandKind::move_to,{}, {160,-60},0,0,0});
    multi.commands.push_back({ShapeCommandKind::line_to,{}, {160,390},0,0,1});
    assets.shapes[88]=multi;
    Surface multi_small,multi_large;
    assert(make_surface(16,16,multi_small));
    assert(make_surface(32,32,multi_large));
    clipped_frame.objects[0].matrix.translate_x=0;
    clipped_frame.objects[0].matrix.translate_y=0;
    assert(render_frame(clipped_frame,assets,multi_small));
    assert(render_frame(clipped_frame,assets,multi_large));
    for(std::size_t y=0;y<16;++y)
        for(std::size_t x=0;x<16;++x)
            for(std::size_t c=0;c<4;++c)
                assert(multi_small.rgba[(y*16+x)*4+c]==multi_large.rgba[(y*32+x)*4+c]);
    // A stroke crossing the pixel center exercises monotonic intersection cursors.
    ShapeGeometry cursor_shape=stroked;
    cursor_shape.commands.clear();
    cursor_shape.commands.push_back({ShapeCommandKind::move_to,{}, {-30,120},0,0,0});
    cursor_shape.commands.push_back({ShapeCommandKind::line_to,{}, {370,120},0,0,1});
    assets.shapes[88]=cursor_shape;
    Surface cursor_small,cursor_large;
    assert(make_surface(16,16,cursor_small));
    assert(make_surface(32,16,cursor_large));
    assert(render_frame(clipped_frame,assets,cursor_small));
    assert(render_frame(clipped_frame,assets,cursor_large));
    for(std::size_t y=0;y<16;++y)
        for(std::size_t x=0;x<16;++x)
            for(std::size_t c=0;c<4;++c)
                assert(cursor_small.rgba[(y*16+x)*4+c] ==
                       cursor_large.rgba[(y*32+x)*4+c]);
    // Reversed coincident paths must not cancel the union opacity.
    ShapeGeometry coincident=stroked;
    coincident.commands.clear();
    coincident.commands.push_back({ShapeCommandKind::move_to,{}, {20,80},0,0,0});
    coincident.commands.push_back({ShapeCommandKind::line_to,{}, {260,80},0,0,1});
    assets.shapes[88]=coincident;
    Surface one_path,two_paths;
    assert(make_surface(32,16,one_path));
    assert(make_surface(32,16,two_paths));
    assert(render_frame(clipped_frame,assets,one_path));
    coincident.commands.push_back({ShapeCommandKind::move_to,{}, {260,80},0,0,0});
    coincident.commands.push_back({ShapeCommandKind::line_to,{}, {20,80},0,0,1});
    assets.shapes[88]=coincident;
    assert(render_frame(clipped_frame,assets,two_paths));
    assert(one_path.rgba==two_paths.rgba);
    // Empty subpixel rows between two distant strokes must not preserve
    // stale AET intersections or alter the union of the two contours.
    ShapeGeometry separated=stroked;
    separated.commands.clear();
    separated.commands.push_back({ShapeCommandKind::move_to,{}, {20,40},0,0,0});
    separated.commands.push_back({ShapeCommandKind::line_to,{}, {180,40},0,0,1});
    separated.commands.push_back({ShapeCommandKind::move_to,{}, {20,280},0,0,0});
    separated.commands.push_back({ShapeCommandKind::line_to,{}, {180,280},0,0,1});
    assets.shapes[88]=separated;
    Surface gap_surface;
    assert(make_surface(32,32,gap_surface));
    assert(render_frame(clipped_frame,assets,gap_surface));
    assert(gap_surface.rgba[(2*32+10)*4+3]>0);
    assert(gap_surface.rgba[(14*32+10)*4+3]>0);
    assert(gap_surface.rgba[(8*32+10)*4+3]==0);
    // Several contours entering and leaving on the same subpixel row must
    // keep the active-edge index consistent, including the last element.
    ShapeGeometry churn=stroked;
    churn.commands.clear();
    for(int i=0;i<12;++i) {
        const int y=20+i*20;
        churn.commands.push_back({ShapeCommandKind::move_to,{}, {20,y},0,0,0});
        churn.commands.push_back({ShapeCommandKind::line_to,{}, {220,y},0,0,1});
    }
    assets.shapes[88]=churn;
    Surface churn_surface;
    assert(make_surface(32,32,churn_surface));
    assert(render_frame(clipped_frame,assets,churn_surface));
    assert(churn_surface.rgba[(1*32+10)*4+3]>0);
    // A gap followed by another stroke must reset the re-anchoring cadence.
    ShapeGeometry restart=stroked;
    restart.commands.clear();
    restart.commands.push_back({ShapeCommandKind::move_to,{}, {20,20},0,0,0});
    restart.commands.push_back({ShapeCommandKind::line_to,{}, {200,20},0,0,1});
    restart.commands.push_back({ShapeCommandKind::move_to,{}, {20,440},0,0,0});
    restart.commands.push_back({ShapeCommandKind::line_to,{}, {200,440},0,0,1});
    assets.shapes[88]=restart;
    Surface restart_surface;
    assert(make_surface(32,32,restart_surface));
    assert(render_frame(clipped_frame,assets,restart_surface));
    assert(restart_surface.rgba[(1*32+10)*4+3]>0);
    assert(restart_surface.rgba[(22*32+10)*4+3]>0);
    assert(restart_surface.rgba[(12*32+10)*4+3]==0);
    // Exact quarter-grid sampling boundary: x=2.125 pixels after the
    // anisotropic affine transform. A translated viewport must agree.
    ShapeGeometry sample_edge=stroked;
    sample_edge.commands.clear();
    sample_edge.commands.push_back({ShapeCommandKind::move_to,{}, {17,20},0,0,0});
    sample_edge.commands.push_back({ShapeCommandKind::line_to,{}, {17,460},0,0,1});
    assets.shapes[88]=sample_edge;
    FrameSnapshot edge_origin=stroke_frame,edge_shifted=stroke_frame;
    edge_origin.objects[0].matrix.scale_x=2.5;
    edge_origin.objects[0].matrix.scale_y=1.0;
    edge_origin.objects[0].matrix.translate_x=0;
    edge_origin.objects[0].matrix.translate_y=0;
    edge_shifted=edge_origin;
    edge_shifted.objects[0].matrix.translate_x=16*20;
    edge_shifted.objects[0].matrix.translate_y=16*20;
    Surface edge_small,edge_large;
    assert(make_surface(16,32,edge_small));
    assert(make_surface(48,64,edge_large));
    assert(render_frame(edge_origin,assets,edge_small));
    assert(render_frame(edge_shifted,assets,edge_large));
    for(std::size_t yy=0;yy<32;++yy)
        for(std::size_t xx=0;xx<16;++xx)
            for(std::size_t ch=0;ch<4;++ch)
                assert(edge_small.rgba[(yy*16+xx)*4+ch] ==
                       edge_large.rgba[((yy+16)*48+xx+16)*4+ch]);
    // Render a shape through a sprite and compose its two translations.
    SpriteTimeline sprite_asset;
    sprite_asset.character_id=90;
    FrameSnapshot sprite_frame;
    DisplayEntry sprite_child;
    sprite_child.depth=1;
    sprite_child.character_id=7;
    sprite_child.has_matrix=true;
    sprite_child.matrix.translate_x=20;
    sprite_frame.objects.push_back(sprite_child);
    sprite_asset.frames.push_back(sprite_frame);
    assets.sprites.emplace(90,sprite_asset);
    FrameSnapshot root_sprite;
    DisplayEntry sprite_instance;
    sprite_instance.depth=1;
    sprite_instance.character_id=90;
    sprite_instance.has_matrix=true;
    sprite_instance.matrix.translate_x=40;
    sprite_instance.matrix.translate_y=20;
    root_sprite.objects.push_back(sprite_instance);
    Surface sprite_surface;
    assert(make_surface(16,16,sprite_surface));
    assert(render_frame(root_sprite,assets,sprite_surface));
    assert(sprite_surface.rgba[(1*16+3)*4+3]>0);
    // Self-referential sprite definitions fail at the nesting budget.
    SpriteTimeline cycle;
    cycle.character_id=91;
    FrameSnapshot cycle_frame;
    DisplayEntry cycle_entry;
    cycle_entry.depth=1;
    cycle_entry.character_id=91;
    cycle_frame.objects.push_back(cycle_entry);
    cycle.frames.push_back(cycle_frame);
    assets.sprites.emplace(91,cycle);
    root_sprite.objects[0].character_id=91;
    const auto unchanged_sprite=sprite_surface.rgba;
    assert(!render_frame(root_sprite,assets,sprite_surface));
    assert(sprite_surface.rgba==unchanged_sprite);
    // Sprite frame selection follows the parent playback clock and loops.
    SpriteTimeline animated;
    animated.character_id=92;
    FrameSnapshot first,second;
    DisplayEntry first_image;
    first_image.depth=1;
    first_image.character_id=42;
    first.objects.push_back(first_image);
    DisplayEntry second_image=first_image;
    second_image.matrix.translate_x=80;
    second_image.has_matrix=true;
    second.objects.push_back(second_image);
    animated.frames={first,second};
    assets.sprites.emplace(92,animated);
    FrameSnapshot animated_root;
    DisplayEntry animated_instance;
    animated_instance.depth=1;
    animated_instance.character_id=92;
    animated_root.objects.push_back(animated_instance);
    Surface animated_a,animated_b,animated_loop;
    assert(make_surface(8,8,animated_a));
    assert(make_surface(8,8,animated_b));
    assert(make_surface(8,8,animated_loop));
    assert(render_frame_at(animated_root,assets,animated_a,0));
    assert(render_frame_at(animated_root,assets,animated_b,1));
    assert(render_frame_at(animated_root,assets,animated_loop,2));
    assert(animated_a.rgba[3]>0);
    assert(animated_b.rgba[(0*8+4)*4+3]>0);
    assert(animated_a.rgba==animated_loop.rgba);
    // External clock controls pause, seek and advance without a hidden timer.
    TimelinePlayback controller;
    assert(controller.frame==0 && controller.playing);
    controller.advance(2);
    assert(controller.frame==2);
    controller.pause();
    controller.advance(10);
    assert(controller.frame==2);
    controller.seek(1);
    assert(controller.frame==1);
    controller.resume();
    controller.advance();
    assert(controller.frame==2);
    Surface controlled;
    assert(make_surface(8,8,controlled));
    assert(render_playback(animated_root,assets,controlled,controller));
    assert(controlled.rgba==animated_loop.rgba);
    // Root timeline playback switches snapshots and restarts at the loop.
    TimelineResult root_timeline;
    FrameSnapshot empty_root;
    root_timeline.frames={animated_root,empty_root};
    TimelinePlayback root_clock;
    Surface root_first,root_second,root_looped;
    assert(make_surface(8,8,root_first));
    assert(make_surface(8,8,root_second));
    assert(make_surface(8,8,root_looped));
    assert(render_timeline_playback(root_timeline,assets,root_first,root_clock));
    root_clock.advance();
    assert(render_timeline_playback(root_timeline,assets,root_second,root_clock));
    for(auto byte:root_second.rgba)assert(byte==0);
    root_clock.advance();
    assert(render_timeline_playback(root_timeline,assets,root_looped,root_clock));
    assert(root_first.rgba==root_looped.rgba);
    root_clock.pause();
    root_clock.advance(5);
    assert(root_clock.frame==2);
    // Two root-depth instances of the same Sprite have independent clocks.
    TimelineResult parallel;
    FrameSnapshot parallel_frame;
    DisplayEntry sprite_a,sprite_b;
    sprite_a.depth=1; sprite_a.character_id=92;
    sprite_b.depth=2; sprite_b.character_id=92;
    sprite_b.has_matrix=true; sprite_b.matrix.translate_y=40;
    parallel_frame.objects={sprite_a,sprite_b};
    parallel.frames.push_back(parallel_frame);
    SpritePlayback instance_controls;
    instance_controls.seek(1,0);
    instance_controls.seek(2,1);
    instance_controls.stop(1);
    instance_controls.advance();
    assert(instance_controls.instances.at(1).frame==0);
    assert(instance_controls.instances.at(2).frame==2);
    instance_controls.seek(2,1);
    TimelinePlayback parallel_clock;
    Surface parallel_surface;
    assert(make_surface(8,8,parallel_surface));
    assert(render_timeline_playback(parallel,assets,parallel_surface,
                                    parallel_clock,instance_controls));
    assert(parallel_surface.rgba[3]>0);
    assert(parallel_surface.rgba[(2*8+4)*4+3]>0);
    instance_controls.play(1);
    instance_controls.advance();
    assert(instance_controls.instances.at(1).frame==1);
    instance_controls.erase(2);
    assert(!instance_controls.instances.contains(2));
    // Replacing/removing a Sprite at an occupied depth discards its old clock,
    // but a matrix-only move of the same placement preserves it.
    SpritePlayback lifecycle;
    FrameSnapshot first_placement;
    DisplayEntry initial;
    initial.depth=7;
    initial.character_id=92;
    initial.placed_frame=0;
    first_placement.objects.push_back(initial);
    lifecycle.synchronize(first_placement,assets);
    lifecycle.seek(7,5);
    FrameSnapshot moved_placement=first_placement;
    moved_placement.objects[0].matrix.translate_x=20;
    lifecycle.synchronize(moved_placement,assets);
    assert(lifecycle.instances.at(7).frame==5);
    FrameSnapshot replacement_placement=first_placement;
    replacement_placement.objects[0].placed_frame=3;
    lifecycle.synchronize(replacement_placement,assets);
    assert(!lifecycle.instances.contains(7));
    lifecycle.stop(7);
    lifecycle.synchronize(FrameSnapshot{},assets);
    assert(!lifecycle.instances.contains(7));
    assert(!lifecycle.identities.contains(7));
    // Stateful player synchronizes removals and replacements without
    // requiring the caller to invoke SpritePlayback::synchronize().
    TimelineResult lifecycle_timeline;
    FrameSnapshot with_sprite;
    DisplayEntry lifecycle_entry;
    lifecycle_entry.depth=5;
    lifecycle_entry.character_id=92;
    with_sprite.objects.push_back(lifecycle_entry);
    FrameSnapshot without_sprite;
    lifecycle_timeline.frames={with_sprite,without_sprite};
    TimelinePlayer player;
    Surface player_surface;
    assert(make_surface(8,8,player_surface));
    assert(player.render(lifecycle_timeline,assets,player_surface));
    player.sprites.stop(5);
    assert(player.sprites.instances.contains(5));
    player.advance();
    assert(player.render(lifecycle_timeline,assets,player_surface));
    assert(!player.sprites.instances.contains(5));
    player.advance();
    assert(player.render(lifecycle_timeline,assets,player_surface));
    assert(player.clock.frame==2 && player.last_cycle==1);
    player.sprites.seek(5,1);
    player.seek(0);
    assert(player.render(lifecycle_timeline,assets,player_surface));
    assert(!player.sprites.instances.contains(5));
    // Two nested instances at the same child depth can be controlled
    // independently through their distinct parent-depth paths.
    SpritePlayback nested_controls;
    nested_controls.seek_path({1,3},4);
    nested_controls.seek_path({2,3},7);
    nested_controls.stop_path({1,3});
    nested_controls.advance(2);
    assert(nested_controls.nested_instances.at({1,3}).frame==4);
    assert(nested_controls.nested_instances.at({2,3}).frame==9);
    nested_controls.play_path({1,3});
    nested_controls.advance();
    assert(nested_controls.nested_instances.at({1,3}).frame==5);
    nested_controls.erase_path({2,3});
    assert(!nested_controls.nested_instances.contains({2,3}));
    // Nested Sprite states are removed when their parent disappears, and
    // replacement at the same nested depth resets the old local clock.
    SpriteTimeline parent_sprite;
    parent_sprite.character_id=94;
    FrameSnapshot parent_child;
    DisplayEntry nested_child;
    nested_child.depth=3;
    nested_child.character_id=92;
    parent_child.objects.push_back(nested_child);
    parent_sprite.frames.push_back(parent_child);
    assets.sprites.emplace(94,parent_sprite);
    FrameSnapshot parent_root;
    DisplayEntry parent_entry;
    parent_entry.depth=6;
    parent_entry.character_id=94;
    parent_root.objects.push_back(parent_entry);
    SpritePlayback nested_lifecycle;
    nested_lifecycle.synchronize(parent_root,assets);
    nested_lifecycle.synchronize_nested(parent_root,assets,0);
    nested_lifecycle.seek_path({6,3},7);
    nested_lifecycle.synchronize_nested(parent_root,assets,0);
    assert(nested_lifecycle.nested_instances.at({6,3}).frame==7);
    assets.sprites.at(94).frames[0].objects[0].placed_frame=1;
    nested_lifecycle.synchronize_nested(parent_root,assets,1);
    assert(!nested_lifecycle.nested_instances.contains({6,3}));
    nested_lifecycle.seek_path({6,3},4);
    nested_lifecycle.synchronize_nested(FrameSnapshot{},assets,1);
    assert(!nested_lifecycle.nested_instances.contains({6,3}));
    // Replacing a parent at the same depth invalidates all its descendants,
    // even when a new parent exposes the same child depth and character ID.
    SpritePlayback parent_reset;
    FrameSnapshot root_before;
    DisplayEntry root_before_entry;
    root_before_entry.depth=6;
    root_before_entry.character_id=94;
    root_before_entry.placed_frame=0;
    root_before.objects.push_back(root_before_entry);
    parent_reset.synchronize(root_before,assets);
    parent_reset.synchronize_nested(root_before,assets,0);
    parent_reset.seek_path({6,3},11);
    FrameSnapshot root_after=root_before;
    root_after.objects[0].placed_frame=5;
    parent_reset.synchronize(root_after,assets);
    parent_reset.synchronize_nested(root_after,assets,5);
    assert(!parent_reset.nested_instances.contains({6,3}));
    parent_reset.seek_path({6,3},3);
    parent_reset.synchronize(FrameSnapshot{},assets);
    parent_reset.synchronize_nested(FrameSnapshot{},assets,5);
    assert(!parent_reset.nested_instances.contains({6,3}));
    // Replacing a nested child must also invalidate a grandchild's clock.
    SpriteTimeline middle;
    middle.character_id=95;
    FrameSnapshot middle_frame;
    DisplayEntry grandchild;
    grandchild.depth=4;
    grandchild.character_id=92;
    middle_frame.objects.push_back(grandchild);
    middle.frames.push_back(middle_frame);
    assets.sprites.emplace(95,middle);
    assets.sprites.at(94).frames[0].objects[0].character_id=95;
    assets.sprites.at(94).frames[0].objects[0].placed_frame=0;
    SpritePlayback generations;
    generations.synchronize(parent_root,assets);
    generations.synchronize_nested(parent_root,assets,0);
    generations.seek_path({6,3,4},8);
    generations.seek_path({6,3},0);
    assets.sprites.at(94).frames[0].objects[0].placed_frame=2;
    generations.synchronize_nested(parent_root,assets,2);
    assert(!generations.nested_instances.contains({6,3,4}));
    // Color transforms apply to the isolated source, not the backdrop.
    FrameSnapshot color_frame;
    DisplayEntry tinted;
    tinted.depth=1;
    tinted.character_id=42;
    tinted.has_color_transform=true;
    tinted.color_transform.multiply[3]=0.5;
    color_frame.objects.push_back(tinted);
    Surface tinted_surface;
    assert(make_surface(8,8,tinted_surface));
    assert(render_frame(color_frame,assets,tinted_surface));
    assert(tinted_surface.rgba[3]==128);
    // An additive channel term changes the source color before blending.
    color_frame.objects[0].color_transform.multiply[0]=0.0;
    color_frame.objects[0].color_transform.add[2]=100;
    clear(tinted_surface,{0,0,0,0});
    assert(render_frame(color_frame,assets,tinted_surface));
    assert(tinted_surface.rgba[0]==0);
    assert(tinted_surface.rgba[2]==100);
    // PlaceObject3 blending is applied to a separate source layer.
    Bitmap blend_bitmap;
    blend_bitmap.character_id=99;
    blend_bitmap.width=1; blend_bitmap.height=1;
    blend_bitmap.rgba={200,100,50,255};
    assets.bitmaps.images.emplace(99,blend_bitmap);
    FrameSnapshot blend_frame;
    DisplayEntry blend_entry;
    blend_entry.depth=1;blend_entry.character_id=99;
    blend_frame.objects.push_back(blend_entry);
    Surface blend_surface;
    assert(make_surface(2,2,blend_surface));
    clear(blend_surface,{100,150,200,255});
    blend_frame.objects[0].blend_mode=3;
    assert(render_frame(blend_frame,assets,blend_surface));
    assert(blend_surface.rgba[0]==78);
    assert(blend_surface.rgba[1]==59);
    assert(blend_surface.rgba[2]==39);
    clear(blend_surface,{100,150,200,255});
    blend_frame.objects[0].blend_mode=4;
    assert(render_frame(blend_frame,assets,blend_surface));
    assert(blend_surface.rgba[0]==222);
    clear(blend_surface,{100,150,200,255});
    blend_frame.objects[0].blend_mode=8;
    assert(render_frame(blend_frame,assets,blend_surface));
    assert(blend_surface.rgba[0]==255);
    // Additional blend modes against opaque source/background.
    const unsigned modes[]={5,6,7,9,10};
    const unsigned expected_red[]={200,100,100,0,155};
    for(unsigned i=0;i<5;++i) {
        clear(blend_surface,{100,150,200,255});
        blend_frame.objects[0].blend_mode=std::uint8_t(modes[i]);
        assert(render_frame(blend_frame,assets,blend_surface));
        assert(blend_surface.rgba[0]==expected_red[i]);
    }
    // Overlay branches on the backdrop; HardLight branches on the source.
    clear(blend_surface,{100,150,200,255});
    blend_frame.objects[0].blend_mode=13;
    assert(render_frame(blend_frame,assets,blend_surface));
    assert(blend_surface.rgba[0]==157);
    clear(blend_surface,{100,150,200,255});
    blend_frame.objects[0].blend_mode=14;
    assert(render_frame(blend_frame,assets,blend_surface));
    assert(blend_surface.rgba[0]==188);
    // Overlay uses the backdrop threshold; HardLight uses the source threshold.
    clear(blend_surface,{100,150,200,255});
    blend_frame.objects[0].blend_mode=13;
    assert(render_frame(blend_frame,assets,blend_surface));
    assert(blend_surface.rgba[0]==157); // 2*200*100/255
    assert(blend_surface.rgba[1]==127); // screen with backdrop 150
    clear(blend_surface,{100,150,200,255});
    blend_frame.objects[0].blend_mode=14;
    assert(render_frame(blend_frame,assets,blend_surface));
    assert(blend_surface.rgba[0]==188); // screen with source 200
    assert(blend_surface.rgba[1]==118); // multiply with source 100
    // BlurFilter expands the alpha footprint of a single bitmap pixel.
    FrameSnapshot blurred_frame;
    DisplayEntry blurred_entry;
    blurred_entry.depth=1;
    blurred_entry.character_id=42;
    blurred_entry.has_blur=true;
    blurred_entry.blur={1,1,1};
    blurred_entry.has_matrix=true;
    blurred_entry.matrix.translate_x=40;
    blurred_entry.matrix.translate_y=40;
    blurred_frame.objects.push_back(blurred_entry);
    Surface blurred_surface;
    assert(make_surface(8,8,blurred_surface));
    assert(render_frame(blurred_frame,assets,blurred_surface));
    assert(blurred_surface.rgba[(2*8+2)*4+3]>0);
    assert(blurred_surface.rgba[(2*8+3)*4+3]>0);
    assert(blurred_surface.rgba[(2*8+3)*4+3]<255);
    // Outer GlowFilter expands the silhouette with green translucent pixels.
    FrameSnapshot glow_frame;
    DisplayEntry glow_entry;
    glow_entry.depth=1;glow_entry.character_id=42;
    glow_entry.has_glow=true;
    glow_entry.glow.red=0;glow_entry.glow.green=255;
    glow_entry.glow.blue=0;glow_entry.glow.alpha=255;
    glow_entry.glow.blur={1,1,1};
    glow_entry.has_matrix=true;
    glow_entry.matrix.translate_x=40;
    glow_entry.matrix.translate_y=40;
    glow_frame.objects.push_back(glow_entry);
    Surface glow_surface_test;
    assert(make_surface(8,8,glow_surface_test));
    assert(render_frame(glow_frame,assets,glow_surface_test));
    assert(glow_surface_test.rgba[(2*8+2)*4]==255);
    assert(glow_surface_test.rgba[(2*8+3)*4+1]==255);
    assert(glow_surface_test.rgba[(2*8+3)*4+3]>0);
    assert(glow_surface_test.rgba[(2*8+3)*4+3]<255);
    // A bounded right-offset shadow adds alpha outside the source bounds.
    FrameSnapshot shadow_frame;
    DisplayEntry shadow_entry;
    shadow_entry.depth=1;shadow_entry.character_id=42;
    shadow_entry.has_shadow=true;
    shadow_entry.shadow.red=0;
    shadow_entry.shadow.green=0;
    shadow_entry.shadow.blue=0;
    shadow_entry.shadow.alpha=255;
    shadow_entry.shadow.offset_x=2;
    shadow_entry.shadow.blur={1,1,1};
    shadow_entry.has_matrix=true;
    shadow_entry.matrix.translate_x=40;
    shadow_entry.matrix.translate_y=40;
    shadow_frame.objects.push_back(shadow_entry);
    Surface shadow_surface_test;
    assert(make_surface(8,8,shadow_surface_test));
    assert(render_frame(shadow_frame,assets,shadow_surface_test));
    assert(shadow_surface_test.rgba[(2*8+2)*4]==255);
    assert(shadow_surface_test.rgba[(2*8+4)*4+3]>0);
    assert(shadow_surface_test.rgba[(2*8+4)*4+3]<255);
    // ColorMatrixFilter swaps red and blue without using modified inputs.
    FrameSnapshot matrix_frame;
    DisplayEntry matrix_entry;
    matrix_entry.depth=1;
    matrix_entry.character_id=42;
    FilterEffect matrix_effect;
    matrix_effect.type=6;
    // Input bitmap 42 is opaque red; swap R and B.
    matrix_effect.color_matrix.values[0]=0;
    matrix_effect.color_matrix.values[2]=1;
    matrix_effect.color_matrix.values[10]=1;
    matrix_effect.color_matrix.values[12]=0;
    matrix_entry.filters.push_back(matrix_effect);
    matrix_frame.objects.push_back(matrix_entry);
    Surface matrix_surface;
    assert(make_surface(8,8,matrix_surface));
    assert(render_frame(matrix_frame,assets,matrix_surface));
    assert(matrix_surface.rgba[0]==0);
    assert(matrix_surface.rgba[2]==255);
    assert(matrix_surface.rgba[3]==255);
    // ColorMatrix alpha bias must not make off-object pixels opaque.
    FrameSnapshot alpha_bias_frame;
    DisplayEntry alpha_bias_entry;
    alpha_bias_entry.depth=1;
    alpha_bias_entry.character_id=42;
    FilterEffect alpha_bias_filter;
    alpha_bias_filter.type=6;
    alpha_bias_filter.color_matrix.values[19]=64.0f;
    alpha_bias_entry.filters.push_back(alpha_bias_filter);
    alpha_bias_frame.objects.push_back(alpha_bias_entry);
    Surface alpha_bias_surface;
    assert(make_surface(8,8,alpha_bias_surface));
    assert(render_frame(alpha_bias_frame,assets,alpha_bias_surface));
    assert(alpha_bias_surface.rgba[3]==255);
    assert(alpha_bias_surface.rgba[(7*8+7)*4+3]==0);
    // Ordered Blur -> ColorMatrix affects only the blurred silhouette,
    // never the unrelated transparent corners of the destination.
    FrameSnapshot chained_filters;
    DisplayEntry chained_entry;
    chained_entry.depth=1;
    chained_entry.character_id=42;
    chained_entry.has_matrix=true;
    chained_entry.matrix.translate_x=60;
    chained_entry.matrix.translate_y=60;
    FilterEffect first_blur;
    first_blur.type=1;
    first_blur.blur={1,1,1};
    FilterEffect then_blue;
    then_blue.type=6;
    then_blue.color_matrix.values[0]=0;
    then_blue.color_matrix.values[2]=1;
    then_blue.color_matrix.values[10]=1;
    then_blue.color_matrix.values[12]=0;
    chained_entry.filters={first_blur,then_blue};
    chained_filters.objects.push_back(chained_entry);
    Surface chained_surface;
    assert(make_surface(8,8,chained_surface));
    assert(render_frame(chained_filters,assets,chained_surface));
    assert(chained_surface.rgba[(3*8+3)*4+2]>0);
    assert(chained_surface.rgba[(3*8+4)*4+3]>0);
    assert(chained_surface.rgba[(7*8+7)*4+3]==0);
    return 0;
}

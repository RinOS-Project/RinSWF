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
    return 0;
}

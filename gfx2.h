#ifndef _GFX_H_
#define _GFX_H_

#include "color.h"

namespace pxr
{
namespace gfx
{

// Enumeration of all available rendering layers for use in draw calls.
//
// A rendering layer is conceptualised as a virtual screen of fixed resolution independent of
// window size or display resolution. The purpose of these virtual screens is to permit the
// development of display resolution dependent games e.g. space invaders which has a fixed
// world size of 224x256 pixels. Virtual screens allow the game logic to be programmed as if
// the screen has a fixed resolution.
//
// Layers use a 2D cartesian coordinate space with the origin in the bottom-left, y-axis
// ascening up the window and x-axis ascending rightward in the window.
//
//          y
//          ^     [layer coordinate space]
//          |
//  origin  o--> x
//
// The size ratio between a layer pixel and a real display pixel is controlled by the pixel 
// size mode (see modes below).
//
// The color of pixels drawn to a layer is controlled by the color mode.
//
// The position of a layer w.r.t the window is controlled by the position mode (a layer may
// not necessarily fill the entire window).
//
// Layers do not support color blending or alpha transparency. The alpha channel is however used
// as a color key where an alpha = 0 is used to skip a pixel when drawing. This allows layers
// to be actually layered rather than each layer fully obscuring the one below. A value of 0 is
// chosen as 0 is used for full transparency by convention. Thus in image editing software such
// as GIMP fully transparent pixels in the editor will also be so in game.
//
// When rendering layers are rendered to the window via the painters algorithm in the order in 
// which they are declared in this enumeration.
//
enum Layer
{
  LAYER_BACKGROUND,
  LAYER_STAGE,
  LAYER_UI,
  LAYER_DEBUG,
  LAYER_COUNT
};

// Modes apply to each rendering layer and can be set independently for each layer.
//
// By default layers use: 
//      ColorMode     = FULL_RGB
//      PixelSizeMode = AUTO_MAX
//      PositionMode  = CENTER

// The color mode controls the final color of pixels that result from all draw calls.
//
// The modes apply as follows:
//
//      FULL_RGB     - unrestricted colors; colors taken from arguments in draw call.
//
//      YAXIS_BANDED - restricted colors; the color of a pixel is determined by its y-axis
//                     position on the layer being drawn to. The bands set the colors mapped
//                     to each position. Color arguments in draw calls are ignored.
//
//      XAXIS_BANDED - restricted colors; the color of a pixel is determined by its x-axis
//                     position on the layer being drawn to. The bands set the colors mapped
//                     to each position. Color arguments in draw calls are ignored.
//
enum class ColorMode
{
  FULL_RGB,
  YAXIS_BANDED,
  XAXIS_BANDED
};

// The pixel size mode controls the size of the pixels of a layer. Minimum pixel size is 1, the
// maximum size is determined by the opengl implementation used.
//
// The modes apply as follows:
//
//      MANUAL   - pixel size is set manually to a fixed value and does not change when the
//                 window resizes.
//
//      AUTO_MIN - pixel size is automatically set to he minimum size of 1 and does not change
//                 when the window resizes (since it is already at the minimum).
//
//      AUTO_MAX - pixel size is automatically maximised to scale the layer to fit the window,
//                 thus pixel size changes as the window resizes. Pixel sizes are restricted to
//                 integer multiples of the real pixel size of the display.
//
enum class PixelSizeMode
{
  MANUAL,
  AUTO_MIN,
  AUTO_MAX
};

// The position mode controls the position of a layer w.r.t the window.
// 
// The modes apply as follows:
//
//      MANUAL       - the layer's origin is at a fixed window coordinate.
//
//      CENTER       - the layer automatically moves to maintain a central position in the 
//                     window as the window resizes.
//
//      TOP_LEFT     - the layer is clamped to the top-left of the window.
//       
//      TOP_RIGHT    - the layer is clamped to the top-right of the window.
//
//      BOTTOM_LEFT  - the layer is clamped to the bottom-left of the window.
//
//      BOTTOM_RIGHT -  the layer is clamped to the bottom-right of the window.
//
enum class PositionMode
{
  MANUAL,
  CENTER,
  TOP_LEFT,
  TOP_RIGHT,
  BOTTOM_LEFT,
  BOTTOM_RIGHT
};

// Color bands apply to a single axis (x or y). All pixels with x/y position
// within the range [lo,hi) adopt the color of the band. 
struct ColorBand
{
  ColorBand() : _color{0, 0, 0, 0}, _lo{0}, _hi{0}{}
  ColorBand(Color4u color, lo, hi) : _color{color}, _lo{lo}, _hi{hi}{}

  Color4u _color;
  int _lo;
  int _hi;
};

// Configuration struct to be used with 'initialize'.
struct Configuration
{
  std::string _windowTitle;
  Vector2i _windowSize;
  Vector2i _backgroundLayerSize;
  Vector2i _stageLayerSize;
  Vector2i _uiLayerSize;
  Vector2i _debugLayerSize;
  bool _fullscreen;
};

// Initializes the rendering subsystem. Returns true if success and false if fatal error. Upon
// returning false calls to other rendering functions have undefined results.
bool initialize(Config config);

// Must be called whenever the window resizes to update layer positions, virtual pixel sizes, 
// the viewport etc.
void onWindowResize(Vector2i windowSize);

// Clears the entire window to a solid color.
void clearWindow(Color4u color);

// Clears a layer such that nothing is drawn for that layer.
void clearLayer(Layer layer);

// Fills a layer with a solid shade, i.e. sets all color channels of all pixels to 'shade' 
// value. If shade == 0 this call has the same effect as 'clearLayer'. It is thus not
// possible to fill a layer pure black.
void fastFillLayer(int shade, Layer layer);

// Fills a layer with a solid color, i.e. sets all pixels to said color. This is a slow 
// operation to be used only if you need a specific color, use 'fastFillLayer' or 'clearLayer' 
// for simple clearing ops. It is not recommended this function is used in a tight loop such
// as the mainloop.
void slowFillLayer(Color4u color, Layer layer);

void drawSprite(Layer layer);
void drawBitmap(Layer layer);
void drawRectangle(Layer layer);
void drawLine(Layer layer);
void drawParticles(Layer layer);
void drawPixel(Vector2i position, Color4u color, Layer layer);
void drawText(Layer layer);

// Must be called once all drawing is done to present the results to the window.
void present();

// Sets the color mode for a specific rendering layer. Changes in color mode only effect future
// draw calls; the pixels on the layer are not changed by this call. If setting a color banding
// mode use 'setLayerColorBands' to configure the bands. By default there is a single white band.
void setLayerColorMode(ColorMode mode, Layer layer);

// Sets the method of determining the virtual pixel size of a layer. Has immediate effect.
void setLayerPixelSizeMode(PixelSizeMode mode, Layer layer);

// Sets the method of positioning the layer in the window. Has immediate effect.
void setLayerPositionMode(PositionMode mode, Layer layer);

// Sets the position of a layer. Has no effect if layer is not in position mode MANUAL.
void setLayerPosition(Layer layer);

// Sets the pixel size of a layer. Has no effect if the layer is not in pixel size mode MANUAL.
void setLayerPixelSize(Layer layer);

// Sets the color bands which apply to draw calls for a layer. Has no effect if the layer is
// not in a color banding mode.
//
// The following rules apply to bands:
// - Bands form an ordered set with elements ordered by ascending 'hi' range value. 
// - Overlapping bands are clipped by clamping the lo value of band 'i+1' to the hi value of 
//   band 'i'. 
// - If multiple bands have equal hi values, the first band encountered in arg 'bands' takes 
//   precedence; all others rejected.
// - All bands are clipped to the size of the layer, thus valid [lo,hi) ranges are between
//   0 and layer x/y size.
// - Bands are only used if the layer is in a color banding mode, use 'setLayerColorMode' to
//   enable a banding mode.
void setLayerColorBands(std::vector<ColorBand> bands, Layer layer);

} // namespace gfx
} // namespace pxr

#endif

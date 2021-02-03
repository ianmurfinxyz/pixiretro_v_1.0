#include <cassert>
#include "cutscene.h"
#include "xmlutil.h"
#include "math.h"
#include "gfx.h"
#include "log.h"

#include <iostream>

//
// TODO
//
// problem: if I only redraw the scene elements when they change then the cases when I am running
// a cutscene along side the game, so the game is drawing some elements and the cutscene others
// then the game will clear the screen each redraw but the cutscene wont redraw each time the
// game does (only when scene elements change). Thus will have to do 1 of 2 things:
//
// - create a seperate screen for cutscenes
// - or disable the redraw optimisation on the cutscenes when drawing them along side the game
//

namespace pxr
{
namespace cut
{

Animation::Animation(gfx::ResourceKey_t spriteKey, int startFrame, int layer, float frameFrequency, Mode mode) :
  _mode{mode},
  _spriteKey{spriteKey},
  _frame{startFrame},
  _startFrame{startFrame},
  _layer{layer},
  _frameCount{0},
  _framePeriod{1.f / frameFrequency},
  _frameFrequency{frameFrequency},
  _frameClock{0.f}
{
  assert(STATIC <= _mode && _mode <= RAND);

  _frameCount = gfx::getSpriteFrameCount(_spriteKey);
  assert(startFrame < _frameCount);

  if(_frameFrequency == 0.f)
    _mode = STATIC;
}

bool Animation::update(float dt)
{
  if(_mode == STATIC)
    return false;

  bool isChange{false};

  _frameClock += dt;
  if(_frameClock > _framePeriod){
    if(_mode == LOOP)
      ++_frame;
    else if(_mode == RAND)
      ++_frame;             // TODO: choose random frame

    _frameClock = 0.f;
    isChange = true;
  }

  if(_frame > _frameCount) 
    _frame = 0;

  return isChange;
}

void Animation::reset()
{
  _frame = _startFrame;
  _frameClock = 0.f;
}

Transition::Transition(std::vector<TPoint> points, float duration) :
  _points{std::move(points)},
  _position{0.f, 0.f},
  _duration{duration},
  _clock{0.f},
  _from{0},
  _to{1},
  _isDone{false}
{
  assert(_points.size() != 0);

  for(auto& p : _points)
    std::clamp(p._phase, 0.f, 1.f);

  std::sort(_points.begin(), _points.end(), [](const TPoint& p0, const TPoint& p1){
    return p0._phase <= p1._phase;
  });

  if(_points.size() == 1 || duration == 0.f){
    _to = 0;
    _position = _points.front()._position;
    _isDone = true;
  }
}

bool Transition::update(float dt)
{
  if(_isDone)
    return false;

  float phase {0.f};

  _clock += dt;
  phase = _clock / _duration;

  if(phase > _points[_to]._phase){
    if(_points[_to]._phase == 1.f){
      _isDone = true;
      phase = 1.f;
    }
    else{
      ++_from;
      ++_to;
    }
  }

  phase -= _points[_from]._phase;
  _position._x = lerp(_points[_from]._position._x, _points[_to]._position._x, phase);
  _position._y = lerp(_points[_from]._position._y, _points[_to]._position._y, phase);

  return true;
}

void Transition::reset()
{
  _from = 0;
  _to = 1;
  _clock = 0.f;
  _isDone = false;

  if(_points.size() == 1){
    _to = 0;
    _position = _points.front()._position;
    _isDone = true;
  }
}

SceneElement::SceneElement(Animation animation, Transition transition, float startTime, float duration) :
  _animation{animation},
  _transition{std::move(transition)},
  _startTime{startTime},
  _duration{duration},
  _clock{0.f}
{
  _state = _startTime == 0.f ? State::ACTIVE : State::PENDING;
}

bool SceneElement::update(float dt)
{
  if(_clock < 0.f)
    _state = State::DONE;

  if(_state == State::DONE)
    return false;

  if(_state == State::PENDING){
    _clock += dt;
    if(_clock >= _startTime){
      _state = State::ACTIVE;
      std::cout << "activated time=" << _clock << "s" << std::endl;
      dt = _clock - _startTime;
      _clock = 0.f;
    }
  }

  bool aChanged, tChanged;

  if(_state == State::ACTIVE){
    _clock += dt;
    if(_clock >= _duration){
      dt = _clock - _duration;
      std::cout << "deactivated time=" << _clock << "s" << std::endl;
      _clock = -1.f;
    }
    aChanged = _animation.update(dt);
    tChanged = _transition.update(dt);
  }

  return aChanged || tChanged;
}

void SceneElement::draw(int screenid)
{
  if(_state != State::ACTIVE)
    return;

  gfx::drawSprite(_transition.getPosition(), _animation.getSpriteKey(), _animation.getFrame(), screenid); 
}

void SceneElement::reset()
{
  _animation.reset();
  _transition.reset();
  _clock = 0.f;
  _state = _startTime == 0.f ? State::ACTIVE : State::PENDING;
}

Cutscene::Cutscene() : 
  _needsRedraw{true},
  _elements{}
{}

bool Cutscene::load(std::string name)
{
  log::log(log::INFO, log::msg_cut_loading, name);

  std::string xmlpath{};
  xmlpath += RESOURCE_PATH_CUTSCENES;
  xmlpath += name;
  xmlpath += XML_RESOURCE_EXTENSION_CUTSCENES;
  XMLDocument doc{};
  if(!parseXmlDocument(&doc, xmlpath))
    return false;

  XMLElement* xmlscene{nullptr};
  XMLElement* xmlelement{nullptr};

  if(!extractChildElement(&doc, &xmlscene, "scene")) return false;

  float timingStart;
  float timingDuration;
  float transitionDuration;
  float transitionPhase;
  float frequency;
  int transitionX;
  int transitionY;
  int startFrame;
  int layer;
  int mode;
  int spriteKey;

  XMLElement* xmltiming{nullptr};
  XMLElement* xmlanimation{nullptr};
  XMLElement* xmltransition{nullptr};
  XMLElement* xmlpoint{nullptr};

  if(!extractChildElement(xmlscene, &xmlelement, "element")) return false;
  do{
    if(!extractChildElement(xmlelement, &xmltiming, "timing")) return false;
    if(!extractFloatAttribute(xmltiming, "start", &timingStart)) return false;
    if(!extractFloatAttribute(xmltiming, "duration", &timingDuration)) return false;

    if(!extractChildElement(xmlelement, &xmlanimation, "animation")) return false;
    if(!extractIntAttribute(xmlanimation, "spritekey", &spriteKey)) return false;
    if(!extractIntAttribute(xmlanimation, "startframe", &startFrame)) return false;
    if(!extractIntAttribute(xmlanimation, "layer", &layer)) return false;
    if(!extractIntAttribute(xmlanimation, "mode", &mode)) return false;
    if(!extractFloatAttribute(xmlanimation, "frequency", &frequency)) return false;

    if(!extractChildElement(xmlelement, &xmltransition, "transition")) return false;
    if(!extractFloatAttribute(xmltransition, "duration", &transitionDuration)) return false;

    std::vector<Transition::TPoint> _tpoints{};
    if(!extractChildElement(xmltransition, &xmlpoint, "point")) return false;
    do{
      if(!extractIntAttribute(xmlpoint, "x", &transitionX)) return false;
      if(!extractIntAttribute(xmlpoint, "y", &transitionY)) return false;
      if(!extractFloatAttribute(xmlpoint, "phase", &transitionPhase)) return false;
      _tpoints.push_back({Vector2f(transitionX, transitionY), transitionPhase});
      xmlpoint = xmlpoint->NextSiblingElement("point");
    }
    while(xmlpoint != 0);

    Animation animation{spriteKey, startFrame, layer, frequency, static_cast<Animation::Mode>(mode)}; 
    Transition transition{std::move(_tpoints), transitionDuration};
    _elements.push_back({animation, std::move(transition), timingStart, timingDuration});
     
    xmlelement = xmlelement->NextSiblingElement("element");
  }
  while(xmlelement != 0);

  std::sort(_elements.begin(), _elements.end(), [](const SceneElement& e0, const SceneElement& e1){
    return e0.getAnimation().getLayer() < e1.getAnimation().getLayer();
  });

  return true;
}

void Cutscene::update(float dt)
{
  int changes{0};
  for(auto& element : _elements)
    changes += static_cast<int>(element.update(dt));
  if(changes)
    _needsRedraw = true;
}

void Cutscene::draw(int screenid)
{
  //if(!_needsRedraw)
   // return;

  for(auto& element : _elements)
    element.draw(screenid);

  _needsRedraw = false;
}

void Cutscene::reset()
{
  for(auto& element : _elements)
    element.reset();
  _needsRedraw = true;
}

} // namespace cut
} // namespace pxr
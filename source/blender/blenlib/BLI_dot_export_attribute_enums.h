#ifndef __BLI_DOT_EXPORT_ATTRIBUTE_ENUMS_H__
#define __BLI_DOT_EXPORT_ATTRIBUTE_ENUMS_H__

#include "BLI_string_ref.h"

namespace BLI {
namespace DotExport {

namespace Attr_rankdir {
enum Enum {
  LeftToRight,
  TopToBottom,
};

static StringRef to_string(Enum value)
{
  switch (value) {
    case LeftToRight:
      return "LR";
    case TopToBottom:
      return "TB";
  }
  return "";
}
}  // namespace Attr_rankdir

namespace Attr_shape {
enum Enum {
  Rectangle,
  Ellipse,
  Circle,
  Point,
  Diamond,
  Square,
};

static StringRef to_string(Enum value)
{
  switch (value) {
    case Rectangle:
      return "rectangle";
    case Ellipse:
      return "ellipse";
    case Circle:
      return "circle";
    case Point:
      return "point";
    case Diamond:
      return "diamond";
    case Square:
      return "square";
  }
  return "";
}
}  // namespace Attr_shape

namespace Attr_arrowType {
enum Enum {
  Normal,
  Inv,
  Dot,
  None,
  Empty,
  Box,
  Vee,
};

static StringRef to_string(Enum value)
{
  switch (value) {
    case Normal:
      return "normal";
    case Inv:
      return "inv";
    case Dot:
      return "dot";
    case None:
      return "none";
    case Empty:
      return "empty";
    case Box:
      return "box";
    case Vee:
      return "vee";
  }
  return "";
}
}  // namespace Attr_arrowType

namespace Attr_dirType {
enum Enum { Forward, Back, Both, None };

static StringRef to_string(Enum value)
{
  switch (value) {
    case Forward:
      return "forward";
    case Back:
      return "back";
    case Both:
      return "both";
    case None:
      return "none";
  }
  return "";
}
}  // namespace Attr_dirType

}  // namespace DotExport
}  // namespace BLI

#endif /* __BLI_DOT_EXPORT_ATTRIBUTE_ENUMS_H__ */

/*
Copyright_License {

  Top Hat Soaring Glide Computer - http://www.tophatsoaring.org/
  Copyright (C) 2000-2016 The Top Hat Soaring Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#include "TophatWidgets/TaskNavSliderShape.hpp"

#include "Look/DialogLook.hpp"
#include "Look/InfoBoxLook.hpp"
#include "Look/IconLook.hpp"
#include "Look/MapLook.hpp"
#include "Look/WaypointLook.hpp"
#include "Look/TaskLook.hpp"
#include "Look/TrafficLook.hpp"
#include "Renderer/WaypointIconRenderer.hpp"
#include "Renderer/WaypointRendererSettings.hpp"
#include "Screen/Canvas.hpp"
#include "Screen/Layout.hpp"
#include "Language/Language.hpp"
#include "Util/StaticString.hxx"

#include "Interface.hpp"
#include "Components.hpp"
#include "UIGlobals.hpp"

#include "Task/Ordered/OrderedTask.hpp"
#include "Terrain/TerrainSettings.hpp"
#include "Screen/Color.hpp"
#include "Formatter/AngleFormatter.hpp"
#include "Formatter/GlideRatioFormatter.hpp"
#include "Formatter/UserUnits.hpp"
#include "Formatter/TimeFormatter.hpp"
#include "Util/StringFormat.hpp"
#include "Asset.hpp"

#ifdef ENABLE_OPENGL
#include "Screen/OpenGL/Texture.hpp"
#include "Screen/OpenGL/Scope.hpp"
#include "Screen/OpenGL/Compatibility.hpp"
#endif

unsigned
SliderShape::GetSumFontHeight()
{
  return nav_slider_look.small_font.GetHeight() +
      nav_slider_look.medium_font.GetHeight() +
      nav_slider_look.large_font.GetHeight();
}

SliderShape::VisibilityLevel
SliderShape::GetVisibilityLevel(Canvas &canvas, RasterPoint poly[])
{
  const PixelRect rc = canvas.GetRect();
  RasterPoint left_tip = poly[7];
  RasterPoint left_body = poly[5];
  left_body.x += Layout::Scale(1);

  RasterPoint right_tip = poly[2];
  RasterPoint right_body = poly[1];
  right_body.x -= Layout::Scale(1);

  if (rc.IsInside(left_tip) && rc.IsInside(right_tip))
    return Full;

  if (rc.IsInside(left_tip)) {
    if (rc.IsInside(left_body))
      return LeftTipAndBody;
    else
      return LeftTip;
  } else if (rc.IsInside(right_tip)) {
    if (rc.IsInside(right_body))
      return RightTipAndBody;
    else
      return RightTip;
  } else
    return NotVisible;
}

void
SliderShape::DrawBackgroundAll(Canvas &canvas, const RasterPoint poly[])
{
  /* clear background */
  canvas.SelectWhitePen();
  canvas.DrawPolygon(poly, 8);
  if (IsKobo()) {
    const PixelScalar y = nav_slider_look.background_pen_width;
    canvas.Select(nav_slider_look.background_pen);
    assert(canvas.GetRect().IsInside({poly[0].x, y}));
    assert(canvas.GetRect().IsInside({poly[1].x, y}));
    canvas.DrawLine(poly[0].x, y, poly[1].x, y);
  }
}

void
SliderShape::DrawOutlineAll(Canvas &canvas, const RasterPoint poly[],
                            bool use_wide_pen)
{
  /* draw with normal width but don't draw top line */
  canvas.Select(nav_slider_look.GetBorderPen(use_wide_pen));
  canvas.DrawTwoLines(poly[1], poly[2], poly[3]);
  canvas.DrawTwoLines(poly[3], poly[4], poly[5]);
  canvas.DrawTwoLines(poly[5], poly[6], poly[7]);
  canvas.DrawLine(poly[7].x, poly[7].y, poly[0].x, poly[0].y);
}

bool
SliderShape::DrawOutline(Canvas &canvas, const PixelRect &rc, bool use_wide_pen)
{
  PixelRect canvas_rect = canvas.GetRect();

  PixelScalar x_offset = rc.left;
  PixelScalar y_offset =  0;
  RasterPoint poly[8];
  RasterPoint poly_raw[8];

  unsigned width = nav_slider_look.GetBorderPenWidth(use_wide_pen);
  /* KOBO dithering centers odd shaped widths within 1/2 pixel,
   * and we need to stay within the canvas or memory gets corrupted
   * The lines have square ends so the diagonal ones actually go
   * a pixel past the end vertically and horizontally */
#ifdef KOBO
  PixelScalar width_offset = 1;
  PixelScalar top_line_offset = 2;
#else
  PixelScalar width_offset = 0;
  PixelScalar top_line_offset = 1;
#endif

  for (unsigned i=0; i < 8; i++) {
    PixelScalar x = GetPoint(i).x + x_offset;
    PixelScalar y = GetPoint(i).y + y_offset;

    poly_raw[i].y = y;
    poly_raw[i].x = x;

    x = std::max(x, PixelScalar(canvas_rect.left + width / 2 + width_offset));
    x = std::min(x, PixelScalar(canvas_rect.right - width / 2 - 1));
    y = std::max(y, PixelScalar(canvas_rect.top + top_line_offset));
    y = std::min(y, PixelScalar(canvas_rect.bottom - width / 2 - 1));

    poly[i].y = y;
    poly[i].x = x;

    assert(canvas_rect.IsInside({x, y}));
  }

  const VisibilityLevel visibility = GetVisibilityLevel(canvas, poly_raw);

  if (visibility == NotVisible)
    return false;

#ifdef _WIN32
  canvas.Select(nav_slider_look.GetBorderPen(use_wide_pen));
  canvas.DrawPolygon(poly, 8);
  return true;
#endif

  switch (visibility) {
  case Full:
  case LeftTipAndBody:
  case RightTipAndBody:
    DrawBackgroundAll(canvas, poly);
    DrawOutlineAll(canvas, poly, use_wide_pen);
    break;

  /** some or all of the left tip, but no body */
  case LeftTip:
  case RightTip:
    canvas.SelectWhitePen();
    canvas.DrawPolygon(poly, 8); // could this be the problem? repeated poly points?
    canvas.Select(nav_slider_look.GetBorderPen(use_wide_pen));
    if (visibility == LeftTip)
      canvas.DrawTwoLines(poly[0], poly[6], poly[5]);
    else
      canvas.DrawTwoLines(poly[1], poly[2], poly[4]);
    break;

  /* no part is visible */
  case NotVisible:
    gcc_unreachable();
    break;
  }

  return true;
}

#ifdef _WIN32
void
SliderShape::PaintBackground(Canvas &canvas, unsigned idx,
                             unsigned list_length,
                             const DialogLook &dialog_look,
                             const PixelRect rc_outer)
{
  // clear area b/c Win32 does not draw background transparently
  UPixelScalar x_offset = rc_outer.left;
  if (idx == 0) {
    RasterPoint left_mid = GetPoint(7);
    canvas.DrawFilledRectangle(0, 0, x_offset + left_mid.x, rc_outer.bottom,
                               nav_slider_look.background_brush);
  }
  if (idx == (list_length - 1)) {
    RasterPoint right_mid = GetPoint(3);
    canvas.DrawFilledRectangle(x_offset + right_mid.x, 0,
        x_offset + right_mid.x + GetHintWidth() + 1,
        rc_outer.bottom,
        nav_slider_look.background_brush);
  }
}
#endif

void
SliderShape::DrawInvalid(Canvas &canvas, const PixelRect rc_outer,
                         const PixelRect rc, unsigned idx,
                         bool selected, bool use_wide_pen)
{
  StaticString<120> nav_buffer;
  const Font &nav_font = nav_slider_look.medium_font;
  canvas.SetTextColor(dialog_look.list.GetTextColor(selected, true, false));
  canvas.Select(nav_slider_look.GetBackgroundBrush(selected));
  DrawOutline(canvas, rc_outer, use_wide_pen);
  canvas.Select(nav_font);
  nav_buffer = _("Click to navigate");
  unsigned width = canvas.CalcTextWidth(nav_buffer.c_str());
  int left = rc.left + (rc.right - rc.left - width) / 2;
  if (left > 0)
    canvas.TextAutoClipped(left,
                           rc.top + (rc.bottom - rc.top -
                               nav_font.GetHeight()) / 2,
                               nav_buffer.c_str());
#ifdef _WIN32
  if (HasDraggableScreen()) // PC or WM
    PaintBackground(canvas, idx, 1, dialog_look, rc_outer);
#endif
}

const Font&
SliderShape::GetNameFont() const
{
  return nav_slider_look.large_font;
}

const Font&
SliderShape::GetAltitudeFont() const
{
  return nav_slider_look.small_font;
}

const Font&
SliderShape::GetDistanceFont() const
{
  return nav_slider_look.medium_font;
}

const Font&
SliderShape::GetTypeFont(bool is_start) const
{
  return nav_slider_look.small_font;
}

void
SliderShape::GetTypeText(TypeBuffer &type_buffer, TaskType task_mode,
                         unsigned idx, unsigned task_size, bool is_start,
                         bool is_finish, bool is_aat, bool navigate_to_target)
{
  // calculate but don't yet draw label "goto" abort, tp#
  switch (task_mode) {
  case TaskType::ORDERED:
    if (task_size == 0)
      type_buffer = _("Go'n home:");

    else if (is_start)
        type_buffer = _("Start");
    else if (is_finish)
      type_buffer = _("Finish");
    else if (is_aat && navigate_to_target)
      // append "Target" text to distance in center
      type_buffer.clear();
    else if (is_aat)
      type_buffer.Format(_T("%s %u"), _("Center"), idx);
    else
      type_buffer.Format(_T("%s %u"), _("TP"), idx);

    break;
  case TaskType::GOTO:
  case TaskType::TEAMMATE:
  case TaskType::ABORT:
    type_buffer = _("Goto:");
    break;

  case TaskType::NONE:
    type_buffer = _("Go'n home:");

    break;
  }
}

void
SliderShape::Draw(Canvas &canvas, const PixelRect rc_outer,
                  unsigned idx, bool selected, bool is_current_tp,
                  const TCHAR *tp_name,
                  const Waypoint *twp,
                  bool has_entered, bool has_exited,
                  TaskType task_mode, TaskFactoryType task_factory_type,
                  unsigned task_size,
                  bool tp_valid, fixed tp_distance, bool distance_valid,
                  fixed tp_altitude_difference,
                  bool altitude_difference_valid,
                  Angle delta_bearing,
                  bool bearing_valid,
                  fixed gradient,
                  bool gr_valid,
                  bool use_wide_pen,
                  bool navigate_to_target)
{
  const DialogLook &dialog_look = UIGlobals::GetDialogLook();
  const IconLook &icon_look = UIGlobals::GetIconLook();
  const TrafficLook &traffic_look = UIGlobals::GetLook().traffic;

  const bool is_teammate = (task_mode == TaskType::TEAMMATE);
  const bool is_ordered = (task_mode == TaskType::ORDERED);
  const bool is_aat = (task_factory_type ==  TaskFactoryType::AAT);
  const bool is_start = idx == 0;
  const bool is_finish = idx + 1 == task_size;

  bool draw_checkmark = is_ordered && (task_size > 1)
      && ((!is_start && has_entered) || (is_start && has_exited));

  TypeBuffer type_buffer;
  UPixelScalar width;
  PixelScalar left;
  PixelRect rc = rc_outer;
  rc.left += 3 * GetHintWidth() / 2;
  rc.right -= 3 * GetHintWidth() / 2;

  if (!tp_valid) {
    DrawInvalid(canvas, rc_outer, rc, idx, selected, use_wide_pen);
    return;
  }

  canvas.SetTextColor(dialog_look.list.GetTextColor(selected, true, false));
  canvas.Select(nav_slider_look.GetBackgroundBrush(selected));
  if (!DrawOutline(canvas, rc_outer, use_wide_pen))
    return;

#ifdef _WIN32
  if (HasDraggableScreen()) // PC or WM
    PaintBackground(canvas, idx,
                    (task_mode == TaskType::GOTO) ? 1 : task_size,
                    dialog_look, rc_outer);
#endif

  const unsigned line_one_y_offset = rc.top + GetLine1Y();
  const unsigned line_two_y_offset = rc.top + GetLine2Y();

  UPixelScalar distance_width = 0u;
  UPixelScalar label_width = 0u;
  UPixelScalar height_width = 0u;
  StaticString<30> distance_buffer(_T(""));
  StaticString<100> height_buffer(_T(""));

  /**
   * Type
   * Draw type
   **/
  GetTypeText(type_buffer, task_mode, idx, task_size, is_start,
              is_finish, is_aat, navigate_to_target);

  canvas.Select(GetTypeFont(is_start));
  label_width = canvas.CalcTextWidth(type_buffer.c_str());

  // Draw arrival altitude right upper corner
  if (altitude_difference_valid) {

    canvas.Select(GetAltitudeFont());
    FormatRelativeUserAltitude(tp_altitude_difference, height_buffer.buffer(),
                               true);
    height_width = canvas.CalcTextWidth(height_buffer.c_str());
    left = rc.right - height_width;
    if (left > 0)
      canvas.TextAutoClipped(left, line_one_y_offset, height_buffer.c_str());
  }

  /**
   * Bearing
   * bearing chevrons for ordered when not start
   * or for non ordered task
   **/
  BearingDirection bearing_direction = BearingDirection::None;
  bool do_bearing = false;
  Angle bearing;
  if (is_current_tp && bearing_valid && is_ordered && idx > 0) {
    do_bearing = true;
    bearing = delta_bearing;
  } else if (!is_ordered &&
      bearing_valid) {
    do_bearing = true;
    bearing = delta_bearing;
  }

  /**
   * Distance & GR
   * draw distance centered between label and altitude.
   * draw label if room
   **/
  StaticString<30> distance_only_buffer(_T(""));

  if (distance_valid) {
    FormatUserDistance(tp_distance, distance_only_buffer.buffer(), true, 1);
  }

  distance_buffer.clear();
  if (navigate_to_target &&
      task_size > 0 &&
      !is_start &&
      !is_finish) {
    distance_buffer.Format(_T("%s: "), _("Target"));
  }
  distance_buffer.append(distance_only_buffer.c_str());
  if (gr_valid && ui_settings.navbar_enable_gr) {
    if (gradient <= fixed(0)) {
      distance_buffer.append(_T(" [##]"));
    }
    else if (gradient >= fixed(99.5)) {
      distance_buffer.append(_T(" [99+]"));
    }
    else {
      StaticString<10> glide_ratio_buffer(_T(""));
      FormatGlideRatio(glide_ratio_buffer.buffer(), glide_ratio_buffer.capacity(), gradient);
      distance_buffer.AppendFormat(_T(" [%s]"), glide_ratio_buffer.c_str());
    }
  }

  if (distance_valid || (gr_valid && ui_settings.navbar_enable_gr) ) {
    canvas.Select(GetDistanceFont());
    distance_width = canvas.CalcTextWidth(distance_buffer.c_str());

    UPixelScalar offset = rc.left;
    if ((PixelScalar)(distance_width + height_width) <
        (PixelScalar)(rc.right - rc.left - label_width -
            Layout::FastScale(15))) {
      canvas.Select(GetTypeFont(is_start));
      left = rc.left;
      if (left > 0 && ui_settings.navbar_enable_tp_index)
        canvas.TextAutoClipped(left, line_one_y_offset, type_buffer.c_str());
      offset = rc.left +
          (rc.right - rc.left - distance_width) / 2;
    }

    canvas.Select(GetDistanceFont());
    left = offset;
    if (left > 0)
      canvas.TextAutoClipped(left, line_one_y_offset, distance_buffer.c_str());

    if (do_bearing)
      bearing_direction = (BearingDirection)DrawBearing(canvas, rc_outer,bearing);

  } else { // just type type label
    if (ui_settings.navbar_enable_tp_index) {
      canvas.TextAutoClipped(rc.left, line_one_y_offset, type_buffer.c_str());
    }
  }

  /**
   * Name
   * Draw tp name, truncated to leave space before rt. bearing if drawn
   */
  canvas.Select(GetNameFont());
  PixelSize icon_size {0, 0};
  UPixelScalar left_icon;
  // only draw target or turnpoint icon if no checkmark
  const TaskLook &task_look = UIGlobals::GetMapLook().task;

  const bool draw_teammate =  !draw_checkmark && is_teammate;
  const bool draw_target =    !draw_checkmark && !draw_teammate && is_aat && navigate_to_target;
  const bool draw_turnpoint = !draw_checkmark  && !draw_teammate && !draw_target && twp != nullptr;
  const bool draw_icon = (draw_checkmark || draw_target || draw_turnpoint || draw_teammate);
  assert (!(draw_target && draw_turnpoint));
  assert (!(draw_teammate && draw_turnpoint));
  assert (!(draw_teammate && draw_target));
  assert (!(draw_teammate && draw_checkmark));

  // icon is only used for target or checkmark.  WaypointRenderer is used otherwise
  const MaskedIcon *icon = draw_checkmark ? &icon_look.hBmpCheckMark :
      draw_teammate ? &traffic_look.teammate_icon : &task_look.target_icon;

  if (draw_icon)
    icon_size = icon->GetSize();

  PixelRect rc_name(rc_outer.left + GetHintWidth(), rc_outer.top,
                    rc_outer.right - GetHintWidth(), rc_outer.bottom);

  width = canvas.CalcTextWidth(tp_name) + icon_size.cx;

  if ((PixelScalar)width > (rc_name.right - rc_name.left)) {
    if (is_current_tp && bearing_direction != BearingDirection::Right)
        rc_name.right += GetHintWidth() / 2;
    if (is_current_tp && bearing_direction == BearingDirection::Right)
        rc_name.right -= Layout::Scale(5);

    left_icon = rc_name.left;

  } else
    left_icon = rc_name.left + (rc_name.right - rc_name.left - width) / 2;

  // TODO make clip to show bearing icon and also clip for canvas
  canvas.DrawClippedText(left_icon + icon_size.cx,
                  line_two_y_offset,
                  rc_name.right - rc_name.left - icon_size.cx / 2, tp_name);

  // draw checkmark next to name if oz entered else for aat, tp / target icon
  if (draw_icon) {
    const int offsety = ((PixelScalar)line_two_y_offset + icon_size.cy <= rc.bottom) ?
        line_two_y_offset + (rc.bottom - line_two_y_offset - icon_size.cy) / 2 - Layout::Scale(1)
        : rc.bottom - icon_size.cy - Layout::Scale(1);

    RasterPoint upper_left(left_icon, rc.top + offsety);
    RasterPoint lower_right(upper_left.x,
                            upper_left.y);
    if (canvas.GetRect().IsInside(upper_left) && canvas.GetRect().IsInside(lower_right)) {
      if (draw_checkmark) {
        icon->DrawUpperLeft(canvas, upper_left); // draws from center of icon
      } else {

        RasterPoint pt = upper_left;
        unsigned name_height = (unsigned)GetNameFont().GetHeight() / 2;
        pt.y += name_height / 2;

        if (draw_target || draw_teammate) {
          icon->Draw(canvas, pt); // draws from center of icon

        } else if (draw_turnpoint) {

          const WaypointLook &waypoint_look = UIGlobals::GetMapLook().waypoint;
          WaypointIconRenderer wir(wp_renderer_settings, waypoint_look, canvas);
          WaypointIconRenderer::Reachability reachability =
              (altitude_difference_valid && tp_altitude_difference > fixed(0)) ?
                  WaypointIconRenderer::Reachability::ReachableTerrain :
                  WaypointIconRenderer::Reachability::Unreachable;

          wir.Draw(*twp, pt, reachability, true);
        }
      }
    }
  }
}

unsigned
SliderShape::DrawBearing(Canvas &canvas, const PixelRect &rc_outer, const Angle &bearing)
{
  enum bearing_levels {
    first = 2,
    second = 10,
    third = 20,
    fourth = 30,
  };
  const IconLook &icon_look = UIGlobals::GetIconLook();
  const MaskedIcon *icon_bearing = nullptr;
  BearingDirection direction = BearingDirection::None;
  if (bearing.AsDelta().Degrees() > fixed(first)) {
    if (bearing.AsDelta().Degrees() > fixed(fourth))
      icon_bearing = &icon_look.hBmpBearingRightFour;
    else if (bearing.AsDelta().Degrees() > fixed(third))
      icon_bearing = &icon_look.hBmpBearingRightThree;
    else if (bearing.AsDelta().Degrees() > fixed(second))
      icon_bearing = &icon_look.hBmpBearingRightTwo;
    else
      icon_bearing = &icon_look.hBmpBearingRightOne;
    direction = BearingDirection::Right;
  }

  if (bearing.AsDelta().Degrees() < fixed(-first)) {
    if (bearing.AsDelta().Degrees() < fixed(-fourth))
      icon_bearing = &icon_look.hBmpBearingLeftFour;
    else if (bearing.AsDelta().Degrees() < fixed(-third))
      icon_bearing = &icon_look.hBmpBearingLeftThree;
    else if (bearing.AsDelta().Degrees() < fixed(-second))
      icon_bearing = &icon_look.hBmpBearingLeftTwo;
    else
      icon_bearing = &icon_look.hBmpBearingLeftOne;
    direction = BearingDirection::Left;
  }

  if (direction == BearingDirection::None)
    return direction;

  PixelSize icon_bearing_size = icon_bearing->GetSize();
  const PixelScalar vert_margin = points[2].y - icon_bearing_size.cy / 2;

  UPixelScalar x_offset = (direction == BearingDirection::Left) ? 1 :
      GetWidth() - icon_bearing_size.cx;

  RasterPoint upper_left(rc_outer.left + x_offset, vert_margin);
  RasterPoint lower_right(upper_left.x + icon_bearing_size.cx,
                          upper_left.y + icon_bearing_size.cy);
  if (canvas.GetRect().IsInside(upper_left) &&
      canvas.GetRect().IsInside(lower_right)) {
#ifdef ENABLE_OPENGL
    icon_bearing->DrawUpperLeft(canvas, upper_left);
#else
    icon_bearing->DrawUpperLeft(canvas, upper_left);
#endif
  }
  return direction;
}

void
SliderShape::Resize(UPixelScalar map_width)
{
  const UPixelScalar arrow_point_bluntness = 0;
  const UPixelScalar raw_total_width = Layout::Scale(360);

  UPixelScalar total_height = nav_slider_look.large_font.GetHeight()
      + nav_slider_look.medium_font.GetHeight() - Layout::Scale(2);

  total_height = std::max(total_height, UPixelScalar(
      bearing_icon_size.cy));

  UPixelScalar raw_hint_width =
      (total_height - arrow_point_bluntness) / 2;  // make 45 degree angle

  raw_hint_width = std::max(raw_hint_width, UPixelScalar(
      bearing_icon_size.cx / 2));

  total_height = std::max(total_height, UPixelScalar(
      raw_hint_width * 2 + arrow_point_bluntness));

  SetLine1Y(0u);
  SetLine2Y(total_height - nav_slider_look.large_font.GetHeight() - 1);
  SetLine3Y(0u);

  //top
  points[0].x = raw_hint_width;
  points[0].y = 0;
  points[1].x = raw_total_width - raw_hint_width;
  points[1].y = 0;

  //right arrow tip
  points[2].x = raw_total_width;
  points[2].y = (total_height - arrow_point_bluntness) / 2;
  points[3].x = raw_total_width;
  points[3].y = (total_height + arrow_point_bluntness) / 2;

  //bottom
  points[4].x = points[1].x;
  points[4].y = total_height;
  points[5].x = points[0].x;
  points[5].y = total_height;

  //left arrow tip
  points[6].x = 0;
  points[6].y = points[3].y;
  points[7].x = 0;
  points[7].y = points[2].y;

  PixelScalar amount_to_grow_x = map_width
      - (GetHintWidth() * 2) - raw_total_width - 1;
  PixelScalar neg_min_grow = points[5].x - points[4].x;
  amount_to_grow_x = (amount_to_grow_x < neg_min_grow) ? neg_min_grow :
      amount_to_grow_x;

  for (unsigned i = 1; i <= 4; i++)
    points[i].x += amount_to_grow_x;
}

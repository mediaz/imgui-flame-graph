// The MIT License(MIT)
//
// Copyright(c) 2019 Sandy Carter
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <imgui_widget_flamegraph.h>

#include "imgui.h"
#include "imgui_internal.h"

void ImGuiWidgetFlameGraph::PlotFlame(const char* label,
									  void (*values_getter)(float* start,
															float* end,
															ImU8* level,
															const char** caption,
															const char** tooltip,
															ImColor* color,
															bool* is_hovered_externally,
															const void* data,
															int idx),
									  const void* data,
									  int values_count,
									  int values_offset,
									  const char* overlay_text,
									  float* scale_min,
									  float* scale_max,
									  ImVec2 graph_size,
									  float zoom_speed,
	void (*interaction_callback)(ImGuiWidgetFlameGraph::InteractionType interaction_type, const void* data, int idx))
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    // Find the maximum depth
    ImU8 maxDepth = 0;
    for (int i = values_offset; i < values_count; ++i)
    {
        ImU8 depth;
        values_getter(nullptr, nullptr, &depth, nullptr, nullptr, nullptr, nullptr, data, i);
        maxDepth = ImMax(maxDepth, depth);
    }

    const auto blockHeight = ImGui::GetTextLineHeight() + (style.FramePadding.y * 2);
    const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
    if (graph_size.x == 0.0f)
        graph_size.x = ImGui::CalcItemWidth();
    if (graph_size.y == 0.0f)
        graph_size.y = label_size.y + (style.FramePadding.y * 3) + blockHeight * (maxDepth + 1);

    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + graph_size);
    const ImRect inner_bb(frame_bb.Min + style.FramePadding, frame_bb.Max - style.FramePadding);
    const ImRect total_bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0));
    ImGui::ItemSize(total_bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(total_bb, 0, &frame_bb))
        return;

    
    float v_scale_min = scale_min ? *scale_min : FLT_MAX;
	float v_scale_max = scale_max ? *scale_max : FLT_MAX;

	// Determine scale from values if not specified
	float v_min = FLT_MAX;
	float v_max = -FLT_MAX;
	for (int i = values_offset; i < values_count; i++)
	{
		float v_start, v_end;
		values_getter(&v_start, &v_end, nullptr, nullptr, nullptr, nullptr, nullptr, data, i);
		if (v_start == v_start) // Check non-NaN values
			v_min = ImMin(v_min, v_start);
		if (v_end == v_end) // Check non-NaN values
			v_max = ImMax(v_max, v_end);
	}
	if (v_scale_min == FLT_MAX)
		v_scale_min = v_min;
	if (v_scale_max == FLT_MAX)
		v_scale_max = v_max;

    bool dynamic_zoom_enabled = scale_min && scale_max;

    if (dynamic_zoom_enabled && ImGui::IsItemHovered() && ImGui::IsMouseHoveringRect(inner_bb.Min, inner_bb.Max))
    {
        // Zoom on mouse wheel based on the mouse position
        const float wheel = ImGui::GetIO().MouseWheel;
		bool updated = false;
        if (wheel != 0)
        {
		    float mouse_pos_percentage =
			    (ImGui::GetIO().MousePos.x - inner_bb.Min.x) / inner_bb.GetWidth();
			mouse_pos_percentage = ImClamp(mouse_pos_percentage, 0.0f, 1.0f);

            float mouse_pos_percentage_scaled = mouse_pos_percentage * (v_scale_max - v_scale_min) + v_scale_min;

            float min_mouse_diff = mouse_pos_percentage_scaled - v_scale_min;
			float max_mouse_diff = v_scale_max - mouse_pos_percentage_scaled;

            v_scale_min = v_scale_min + min_mouse_diff * zoom_speed * wheel;
			v_scale_max = v_scale_max - max_mouse_diff * zoom_speed * wheel;
            			
            updated = true;
        }

		// Pan the view based on the mouse movement
		if (ImGui::IsMouseDragging(1))
		{
			ImVec2 mouse_delta = ImGui::GetIO().MouseDelta;
			float duration = v_scale_max - v_scale_min;
			float delta = duration * (mouse_delta.x / inner_bb.GetWidth());
			float diff = ImMin(v_max - v_scale_max, v_scale_min - v_min);
            delta = ImMin(delta, diff);

            v_scale_min -= delta;
			v_scale_max -= delta;
		    
            updated = true;
        }

        v_scale_min = ImMax(v_scale_min, v_min);
        v_scale_max = ImMin(v_scale_max, v_max);

        if (updated)
		{
			*scale_min = v_scale_min;
			*scale_max = v_scale_max;
		}
    }

    ImGui::RenderFrame(frame_bb.Min, frame_bb.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

    bool any_hovered = false;
    if (values_count - values_offset >= 1)
    {
        for (int i = values_offset; i < values_count; ++i)
        {
            float stageStart = 0.0f, stageEnd = 0.0f;
			ImU8 depth = 0;
            const char* caption = nullptr;
			const char* tooltip = nullptr;
			ImColor color{};
            bool is_hovered_externally = false;
            values_getter(&stageStart, &stageEnd, &depth, &caption, &tooltip, &color, &is_hovered_externally, data, i);

            auto duration = v_scale_max - v_scale_min;
            if (duration == 0)
            {
                return;
            }

            auto start = stageStart - v_scale_min;
            auto end = stageEnd - v_scale_min;

            auto startX = static_cast<float>(start / (double)duration);
            auto endX = static_cast<float>(end / (double)duration);

            startX = ImClamp(startX, 0.0f, 1.0f);
            endX = ImClamp(endX, 0.0f, 1.0f);

            if (startX == 1.0f || endX == 0.0f)
				continue;

            float width = inner_bb.Max.x - inner_bb.Min.x;
            float height = blockHeight * (maxDepth - depth + 1) - style.FramePadding.y;

            auto pos0 = inner_bb.Min + ImVec2(startX * width, height);
            auto pos1 = inner_bb.Min + ImVec2(endX * width, height + blockHeight);

            bool v_hovered = false;
            if (ImGui::IsMouseHoveringRect(pos0, pos1))
            {
                if (interaction_callback)
                {
					interaction_callback(IMGUI_INTERACTION_TYPE_HOVERED, data, i);
                }
				if (*tooltip)
					ImGui::SetTooltip("%s", tooltip);
				else
                    ImGui::SetTooltip("%s: %8.4g", caption, stageEnd - stageStart);
                v_hovered = true;
                any_hovered = v_hovered;
            }

            if (dynamic_zoom_enabled && ImGui::IsMouseHoveringRect(pos0, pos1) && ImGui::IsMouseClicked(0))
			{
				*scale_min = stageStart;
				*scale_max = stageEnd;
                if (interaction_callback)
                {
					interaction_callback(IMGUI_INTERACTION_TYPE_CLICKED, data, i);
                }
			}

            float h, s, v;

            ImGui::ColorConvertRGBtoHSV(color.Value.x, color.Value.y, color.Value.z, h, s, v);

			ImU32 color_base = ImGui::ColorConvertFloat4ToU32(ImColor::HSV(h, s, v)) & 0x77FFFFFF;
            ImU32 color_hovered = ImGui::ColorConvertFloat4ToU32(ImColor::HSV(h, s, v + 0.2f)) & 0x77FFFFFF;
			ImU32 outline = ImGui::ColorConvertFloat4ToU32(ImColor::HSV(h, s, v - 0.2f)) & 0x7FFFFFFF;
			ImU32 outline_hovered = ImGui::ColorConvertFloat4ToU32(ImColor::HSV(h, s, v + 0.4f)) & 0x7FFFFFFF;


            window->DrawList->AddRectFilled(
				pos0, pos1, (is_hovered_externally || v_hovered) ? color_hovered : color_base);
			window->DrawList->AddRect(pos0, pos1, (is_hovered_externally || v_hovered) ? outline_hovered : outline);
            auto textSize = ImGui::CalcTextSize(caption);
            auto boxSize = (pos1 - pos0);
            auto textOffset = ImVec2(0.0f, 0.0f);
            if (textSize.x < boxSize.x)
            {
                textOffset = ImVec2(0.5f, 0.5f) * (boxSize - textSize);
                ImGui::RenderText(pos0 + textOffset, caption);
            }
        }

        // Text overlay
        if (overlay_text)
            ImGui::RenderTextClipped(ImVec2(frame_bb.Min.x, frame_bb.Min.y + style.FramePadding.y), frame_bb.Max, overlay_text, NULL, NULL, ImVec2(0.5f,0.0f));

        if (label_size.x > 0.0f)
            ImGui::RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, inner_bb.Min.y), label);
    }

    if (!any_hovered && ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Total: %8.4g", scale_max - scale_min);
    }
}

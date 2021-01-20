/*
Copyright (c) 2021

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <imgui.h>
#include <imgui_internal.h>

namespace ImGui
{
    void ToggleButton(const char *str_id, bool *v)
    {
        ImVec2      p         = ImGui::GetCursorScreenPos();
        ImDrawList *draw_list = ImGui::GetForegroundDrawList();
        float       height    = ImGui::GetFrameHeight();
        float       width     = height * 1.55f;
        float       radius    = height * 0.50f;

        ImGui::Button(str_id, ImVec2(width, height));
        if (ImGui::IsItemClicked()) *v = !*v;

        float t = *v ? 1.0f : 0.0f;

        ImGuiContext &g          = *ImGui::GetCurrentContext();
        float         ANIM_SPEED = 0.08f;
        if (g.LastActiveId == g.CurrentWindow->GetID(str_id))    // && g.LastActiveIdTimer <
                                                                 // ANIM_SPEED)
        {
            float t_anim = ImSaturate(g.LastActiveIdTimer / ANIM_SPEED);
            t            = *v ? (t_anim) : (1.0f - t_anim);
        }

        ImU32 col_bg;
        if (ImGui::IsItemHovered())
            col_bg = ImGui::GetColorU32(ImVec4(0.78f, 0.78f, 0.78f, 1.0f));
        else
            col_bg = ImGui::GetColorU32(ImVec4(0.85f, 0.85f, 0.85f, 1.0f));

        draw_list->AddRectFilled(p, ImVec2(p.x + width, p.y + height), col_bg, height * 0.5f);
        draw_list->AddCircleFilled(
          ImVec2(p.x + radius + t * (width - radius * 2.0f), p.y + radius),
          radius - 1.5f,
          IM_COL32(255, 255, 255, 255));
    }

}    // namespace ImGui

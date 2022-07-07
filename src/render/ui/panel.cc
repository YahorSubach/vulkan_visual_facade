﻿#include "panel.h"

#include <glm/glm/gtc/matrix_transform.hpp>

render::ui::Panel::Panel(int x, int y, int width, int height): x_(x), y_(y), width_(width), height_(height)
{
    local_transform = glm::identity<glm::mat4>();
}

render::ui::Panel::Panel(const Panel& panel) : x_(panel.x_), y_(panel.y_), width_(panel.width_), height_(panel.height_), local_transform(panel.local_transform), children(panel.children), image_(panel.image_)
{
    for (auto&& child : children)
    {
        child.SetParent(*this);
    }
}


void render::ui::Panel::CollectRender(glm::mat4 parent_transform, std::vector<std::pair<glm::mat4, const Image&>>& to_render)
{
    if (image_)
    {
        std::pair<glm::mat4, const Image&> res(parent_transform * local_transform, *image_);

        to_render.push_back(res);
    }

    for (auto&& child : children)
    {
        child.CollectRender(parent_transform * local_transform, to_render);
    }
}

void render::ui::Panel::AddChild(const Panel& panel)
{
    children.push_back(panel);
    children.back().SetParent(*this);
}

void render::ui::Panel::SetParent(const Panel& parent)
{
    parent_ = stl_util::NullableRef<const Panel>(parent);

    local_transform = glm::translate(glm::identity<glm::mat4>(), glm::vec3(1.0f * x_ / parent_->width_, 1.0f * y_ / parent_->height_, 1.0f));
    local_transform = glm::scale(local_transform, glm::vec3(1.0f * width_ / parent_->width_, 1.0f * height_ / parent_->height_, 1.0f));
}

render::ui::TextBlock::TextBlock(const UI& ui, int x, int y, int font_size, const std::string& text) : Panel(x, y, 0 ,0)
{
    width_ = 0;
    height_ = font_size;

    children.reserve(text.length());

    std::vector<GlyphPanel> glyphs;

    for (char c : text)
    {
        auto glyph = ui.GetGlyph(c, font_size);;

        if (glyph.bitmap)
        {
            glyphs.emplace_back(width_, 0, font_size, glyph);
        }

        width_ += glyph.advance;
        height_ = max(height_, glyph.bitmap_y + glyph.bitmap_heigth);
    }

    for (auto&& glyph : glyphs)
    {
        AddChild(glyph);
    }

    image_ = ui.test_image_;
}


render::ui::GlyphPanel::GlyphPanel(int x, int y, int font_size, render::ui::Glyph glyph): Panel(x,y,glyph.advance,font_size), character_panel_(glyph.bitmap_x, glyph.bitmap_y, glyph.bitmap_width, glyph.bitmap_heigth)
{
    character_panel_.image_ = stl_util::NullableRef<const Image>(glyph.bitmap);
    AddChild(character_panel_);
}

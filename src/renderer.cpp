#include "renderer.h"
#include "bot.h"
#include "stage.h"
#include "powerup.h"
#include "combat.h"
#include "components.h"
#include <cmath>
#include <algorithm>

namespace ScrapHeap {

// Static player colors
static const std::vector<SDL_Color> PLAYER_COLORS = {
    {255, 80, 80, 255},   // Red
    {80, 80, 255, 255},   // Blue
    {80, 255, 80, 255},   // Green
    {255, 255, 80, 255},  // Yellow
    {255, 80, 255, 255},  // Magenta
    {80, 255, 255, 255},  // Cyan
    {255, 160, 80, 255},  // Orange
    {200, 200, 200, 255}  // White
};

Renderer& Renderer::instance() {
    static Renderer renderer;
    return renderer;
}

bool Renderer::initialize(SDL_Renderer* renderer, const std::string& fontPath) {
    sdlRenderer = renderer;

    // Initialize TTF
    if (!TTF_Init()) {
        SDL_Log("Failed to initialize SDL_ttf: %s", SDL_GetError());
        return false;
    }

    // Try to load pixel art font first (Press Start 2P)
    const char* pixelFonts[] = {
        "assets/PressStart2P-Regular.ttf",
        "../assets/PressStart2P-Regular.ttf",
        "./assets/PressStart2P-Regular.ttf"
    };

    for (const char* pixelFont : pixelFonts) {
        if (!fontLarge) fontLarge = TTF_OpenFont(pixelFont, 24);
        if (!fontMedium) fontMedium = TTF_OpenFont(pixelFont, 16);
        if (!fontSmall) fontSmall = TTF_OpenFont(pixelFont, 8);
        if (fontLarge && fontMedium && fontSmall) {
            SDL_Log("Loaded pixel font: %s", pixelFont);
            break;
        }
    }

    // Try specified font path
    if (!fontLarge || !fontMedium || !fontSmall) {
        if (!fontLarge) fontLarge = TTF_OpenFont(fontPath.c_str(), 24);
        if (!fontMedium) fontMedium = TTF_OpenFont(fontPath.c_str(), 16);
        if (!fontSmall) fontSmall = TTF_OpenFont(fontPath.c_str(), 8);
    }

    // Fall back to system fonts
    if (!fontLarge || !fontMedium || !fontSmall) {
        SDL_Log("Failed to load pixel font, trying fallbacks");
        const char* fallbackFonts[] = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/TTF/DejaVuSans.ttf",
            "/usr/share/fonts/dejavu/DejaVuSans.ttf",
            "/System/Library/Fonts/Helvetica.ttc",
            "C:\\Windows\\Fonts\\arial.ttf"
        };

        for (const char* fallback : fallbackFonts) {
            if (!fontLarge) fontLarge = TTF_OpenFont(fallback, 24);
            if (!fontMedium) fontMedium = TTF_OpenFont(fallback, 16);
            if (!fontSmall) fontSmall = TTF_OpenFont(fallback, 12);
            if (fontLarge && fontMedium && fontSmall) break;
        }
    }

    if (!fontLarge || !fontMedium || !fontSmall) {
        SDL_Log("Could not load any fonts!");
        return false;
    }

    return true;
}

void Renderer::shutdown() {
    if (fontLarge) { TTF_CloseFont(fontLarge); fontLarge = nullptr; }
    if (fontMedium) { TTF_CloseFont(fontMedium); fontMedium = nullptr; }
    if (fontSmall) { TTF_CloseFont(fontSmall); fontSmall = nullptr; }
    TTF_Quit();
}

void Renderer::clear(SDL_Color color) {
    SDL_SetRenderDrawColor(sdlRenderer, color.r, color.g, color.b, color.a);
    SDL_RenderClear(sdlRenderer);
}

void Renderer::present() {
    SDL_RenderPresent(sdlRenderer);
}

void Renderer::drawText(const std::string& text, float x, float y,
                        TTF_Font* font, SDL_Color color, TextAlign align) {
    if (text.empty() || !font) return;

    SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), 0, color);
    if (!surface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(sdlRenderer, surface);
    if (!texture) {
        SDL_DestroySurface(surface);
        return;
    }

    float w = static_cast<float>(surface->w);
    float h = static_cast<float>(surface->h);

    SDL_FRect destRect;
    destRect.w = w;
    destRect.h = h;
    destRect.y = y;

    switch (align) {
        case TextAlign::Left:
            destRect.x = x;
            break;
        case TextAlign::Center:
            destRect.x = x - w / 2.0f;
            break;
        case TextAlign::Right:
            destRect.x = x - w;
            break;
    }

    SDL_RenderTexture(sdlRenderer, texture, nullptr, &destRect);

    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
}

void Renderer::drawTextShadow(const std::string& text, float x, float y,
                              TTF_Font* font, SDL_Color color, TextAlign align) {
    SDL_Color shadow = {0, 0, 0, 200};
    drawText(text, x + 2, y + 2, font, shadow, align);
    drawText(text, x, y, font, color, align);
}

Vec2 Renderer::getTextSize(const std::string& text, TTF_Font* font) {
    if (text.empty() || !font) return Vec2(0, 0);

    int w, h;
    TTF_GetStringSize(font, text.c_str(), 0, &w, &h);
    return Vec2(static_cast<float>(w), static_cast<float>(h));
}

void Renderer::drawRect(float x, float y, float w, float h, SDL_Color color, bool filled) {
    SDL_SetRenderDrawColor(sdlRenderer, color.r, color.g, color.b, color.a);
    SDL_FRect rect = {x, y, w, h};
    if (filled) {
        SDL_RenderFillRect(sdlRenderer, &rect);
    } else {
        SDL_RenderRect(sdlRenderer, &rect);
    }
}

void Renderer::drawRectOutline(float x, float y, float w, float h, SDL_Color color, float thickness) {
    SDL_SetRenderDrawColor(sdlRenderer, color.r, color.g, color.b, color.a);
    // Top
    SDL_FRect top = {x, y, w, thickness};
    SDL_RenderFillRect(sdlRenderer, &top);
    // Bottom
    SDL_FRect bottom = {x, y + h - thickness, w, thickness};
    SDL_RenderFillRect(sdlRenderer, &bottom);
    // Left
    SDL_FRect left = {x, y, thickness, h};
    SDL_RenderFillRect(sdlRenderer, &left);
    // Right
    SDL_FRect right = {x + w - thickness, y, thickness, h};
    SDL_RenderFillRect(sdlRenderer, &right);
}

void Renderer::drawFilledCircle(float cx, float cy, float radius, SDL_Color color) {
    // Draw circle using triangles (fan from center)
    const int segments = 32;
    SDL_Vertex vertices[segments + 2];

    // Center vertex
    vertices[0].position = {cx, cy};
    vertices[0].color = {
        color.r / 255.0f, color.g / 255.0f,
        color.b / 255.0f, color.a / 255.0f
    };

    for (int i = 0; i <= segments; ++i) {
        float angle = (i / static_cast<float>(segments)) * 2.0f * PI;
        vertices[i + 1].position = {
            cx + std::cos(angle) * radius,
            cy + std::sin(angle) * radius
        };
        vertices[i + 1].color = vertices[0].color;
    }

    // Build indices for triangle fan
    int indices[segments * 3];
    for (int i = 0; i < segments; ++i) {
        indices[i * 3] = 0;
        indices[i * 3 + 1] = i + 1;
        indices[i * 3 + 2] = i + 2;
    }

    SDL_RenderGeometry(sdlRenderer, nullptr, vertices, segments + 2,
                       indices, segments * 3);
}

void Renderer::drawCircle(float cx, float cy, float radius, SDL_Color color, bool filled) {
    if (filled) {
        drawFilledCircle(cx, cy, radius, color);
    } else {
        drawCircleOutline(cx, cy, radius, color, 2.0f);
    }
}

void Renderer::drawCircleOutline(float cx, float cy, float radius, SDL_Color color, float thickness) {
    SDL_SetRenderDrawColor(sdlRenderer, color.r, color.g, color.b, color.a);
    const int segments = 32;
    for (int i = 0; i < segments; ++i) {
        float a1 = (i / static_cast<float>(segments)) * 2.0f * PI;
        float a2 = ((i + 1) / static_cast<float>(segments)) * 2.0f * PI;
        float x1 = cx + std::cos(a1) * radius;
        float y1 = cy + std::sin(a1) * radius;
        float x2 = cx + std::cos(a2) * radius;
        float y2 = cy + std::sin(a2) * radius;
        SDL_RenderLine(sdlRenderer, x1, y1, x2, y2);
    }
}

void Renderer::drawLine(float x1, float y1, float x2, float y2, SDL_Color color, float thickness) {
    SDL_SetRenderDrawColor(sdlRenderer, color.r, color.g, color.b, color.a);
    SDL_RenderLine(sdlRenderer, x1, y1, x2, y2);
}

void Renderer::drawTriangle(float x1, float y1, float x2, float y2, float x3, float y3,
                            SDL_Color color, bool filled) {
    if (filled) {
        SDL_Vertex vertices[3];
        SDL_FColor fcolor = {
            color.r / 255.0f, color.g / 255.0f,
            color.b / 255.0f, color.a / 255.0f
        };
        vertices[0].position = {x1, y1}; vertices[0].color = fcolor;
        vertices[1].position = {x2, y2}; vertices[1].color = fcolor;
        vertices[2].position = {x3, y3}; vertices[2].color = fcolor;
        SDL_RenderGeometry(sdlRenderer, nullptr, vertices, 3, nullptr, 0);
    } else {
        SDL_SetRenderDrawColor(sdlRenderer, color.r, color.g, color.b, color.a);
        SDL_RenderLine(sdlRenderer, x1, y1, x2, y2);
        SDL_RenderLine(sdlRenderer, x2, y2, x3, y3);
        SDL_RenderLine(sdlRenderer, x3, y3, x1, y1);
    }
}

void Renderer::drawRotatedRect(float cx, float cy, float width, float height,
                               float angle, SDL_Color color) {
    // Calculate corners
    float cos_a = std::cos(angle);
    float sin_a = std::sin(angle);
    float hw = width / 2.0f;
    float hh = height / 2.0f;

    // Corner offsets before rotation
    float corners[4][2] = {
        {-hw, -hh}, {hw, -hh}, {hw, hh}, {-hw, hh}
    };

    SDL_Vertex vertices[4];
    SDL_FColor fcolor = {
        color.r / 255.0f, color.g / 255.0f,
        color.b / 255.0f, color.a / 255.0f
    };

    for (int i = 0; i < 4; ++i) {
        float rx = corners[i][0] * cos_a - corners[i][1] * sin_a;
        float ry = corners[i][0] * sin_a + corners[i][1] * cos_a;
        vertices[i].position = {cx + rx, cy + ry};
        vertices[i].color = fcolor;
    }

    int indices[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(sdlRenderer, nullptr, vertices, 4, indices, 6);
}

void Renderer::drawGradientRect(float x, float y, float w, float h,
                                SDL_Color topColor, SDL_Color bottomColor) {
    SDL_Vertex vertices[4];

    SDL_FColor topFC = {
        topColor.r / 255.0f, topColor.g / 255.0f,
        topColor.b / 255.0f, topColor.a / 255.0f
    };
    SDL_FColor bottomFC = {
        bottomColor.r / 255.0f, bottomColor.g / 255.0f,
        bottomColor.b / 255.0f, bottomColor.a / 255.0f
    };

    vertices[0].position = {x, y};
    vertices[0].color = topFC;
    vertices[1].position = {x + w, y};
    vertices[1].color = topFC;
    vertices[2].position = {x + w, y + h};
    vertices[2].color = bottomFC;
    vertices[3].position = {x, y + h};
    vertices[3].color = bottomFC;

    int indices[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(sdlRenderer, nullptr, vertices, 4, indices, 6);
}

void Renderer::drawGradientRectH(float x, float y, float w, float h,
                                 SDL_Color leftColor, SDL_Color rightColor) {
    SDL_Vertex vertices[4];

    SDL_FColor leftFC = {
        leftColor.r / 255.0f, leftColor.g / 255.0f,
        leftColor.b / 255.0f, leftColor.a / 255.0f
    };
    SDL_FColor rightFC = {
        rightColor.r / 255.0f, rightColor.g / 255.0f,
        rightColor.b / 255.0f, rightColor.a / 255.0f
    };

    vertices[0].position = {x, y};
    vertices[0].color = leftFC;
    vertices[1].position = {x + w, y};
    vertices[1].color = rightFC;
    vertices[2].position = {x + w, y + h};
    vertices[2].color = rightFC;
    vertices[3].position = {x, y + h};
    vertices[3].color = leftFC;

    int indices[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(sdlRenderer, nullptr, vertices, 4, indices, 6);
}

void Renderer::drawRoundedRect(float x, float y, float w, float h, float radius,
                               SDL_Color color, bool filled) {
    // Simplified rounded rect - draw main rect plus corner circles
    if (filled) {
        // Main body
        drawRect(x + radius, y, w - 2 * radius, h, color, true);
        drawRect(x, y + radius, w, h - 2 * radius, color, true);

        // Corners
        drawFilledCircle(x + radius, y + radius, radius, color);
        drawFilledCircle(x + w - radius, y + radius, radius, color);
        drawFilledCircle(x + radius, y + h - radius, radius, color);
        drawFilledCircle(x + w - radius, y + h - radius, radius, color);
    } else {
        drawCircleOutline(x + radius, y + radius, radius, color, 2.0f);
        drawCircleOutline(x + w - radius, y + radius, radius, color, 2.0f);
        drawCircleOutline(x + radius, y + h - radius, radius, color, 2.0f);
        drawCircleOutline(x + w - radius, y + h - radius, radius, color, 2.0f);
    }
}

void Renderer::drawPanel(float x, float y, float w, float h, SDL_Color bgColor,
                         SDL_Color borderColor, float borderThickness) {
    // Shadow
    SDL_Color shadow = {0, 0, 0, 60};
    drawRect(x + 4, y + 4, w, h, shadow, true);

    // Background
    drawRect(x, y, w, h, bgColor, true);

    // Border
    drawRectOutline(x, y, w, h, borderColor, borderThickness);

    // Subtle highlight at top
    SDL_Color highlight = {255, 255, 255, 30};
    drawRect(x + 2, y + 2, w - 4, 2, highlight, true);
}

void Renderer::drawGlowPanel(float x, float y, float w, float h,
                             SDL_Color baseColor, float glowIntensity) {
    // Outer glow layers
    for (int i = 3; i >= 0; --i) {
        float offset = i * 3.0f;
        Uint8 alpha = static_cast<Uint8>(20 * glowIntensity * (4 - i));
        SDL_Color glowColor = {baseColor.r, baseColor.g, baseColor.b, alpha};
        drawRect(x - offset, y - offset, w + offset * 2, h + offset * 2, glowColor, true);
    }

    // Gradient background
    SDL_Color topColor = {
        static_cast<Uint8>(std::min(255, baseColor.r + 30)),
        static_cast<Uint8>(std::min(255, baseColor.g + 30)),
        static_cast<Uint8>(std::min(255, baseColor.b + 30)),
        baseColor.a
    };
    SDL_Color bottomColor = {
        static_cast<Uint8>(baseColor.r * 0.7f),
        static_cast<Uint8>(baseColor.g * 0.7f),
        static_cast<Uint8>(baseColor.b * 0.7f),
        baseColor.a
    };
    drawGradientRect(x, y, w, h, topColor, bottomColor);

    // Border
    SDL_Color borderColor = {
        static_cast<Uint8>(std::min(255, baseColor.r + 60)),
        static_cast<Uint8>(std::min(255, baseColor.g + 60)),
        static_cast<Uint8>(std::min(255, baseColor.b + 60)),
        255
    };
    drawRectOutline(x, y, w, h, borderColor, 2.0f);
}

void Renderer::drawHexagon(float cx, float cy, float radius, float angle, SDL_Color color, bool filled) {
    SDL_Vertex vertices[7];  // 6 corners + center for fan
    SDL_FColor fcolor = {
        color.r / 255.0f, color.g / 255.0f,
        color.b / 255.0f, color.a / 255.0f
    };

    // Center vertex
    vertices[0].position = {cx, cy};
    vertices[0].color = fcolor;

    // 6 corners
    for (int i = 0; i < 6; ++i) {
        float cornerAngle = angle + (i / 6.0f) * 2.0f * PI;
        vertices[i + 1].position = {
            cx + std::cos(cornerAngle) * radius,
            cy + std::sin(cornerAngle) * radius
        };
        vertices[i + 1].color = fcolor;
    }

    if (filled) {
        int indices[18];
        for (int i = 0; i < 6; ++i) {
            indices[i * 3] = 0;
            indices[i * 3 + 1] = i + 1;
            indices[i * 3 + 2] = (i + 1) % 6 + 1;
        }
        SDL_RenderGeometry(sdlRenderer, nullptr, vertices, 7, indices, 18);
    } else {
        SDL_SetRenderDrawColor(sdlRenderer, color.r, color.g, color.b, color.a);
        for (int i = 0; i < 6; ++i) {
            int next = (i + 1) % 6;
            SDL_RenderLine(sdlRenderer,
                vertices[i + 1].position.x, vertices[i + 1].position.y,
                vertices[next + 1].position.x, vertices[next + 1].position.y);
        }
    }
}

void Renderer::drawDiamond(float cx, float cy, float width, float height, float angle, SDL_Color color) {
    float cos_a = std::cos(angle);
    float sin_a = std::sin(angle);
    float hw = width / 2.0f;
    float hh = height / 2.0f;

    // Diamond corners (top, right, bottom, left before rotation)
    float corners[4][2] = {
        {0, -hh}, {hw, 0}, {0, hh}, {-hw, 0}
    };

    SDL_Vertex vertices[4];
    SDL_FColor fcolor = {
        color.r / 255.0f, color.g / 255.0f,
        color.b / 255.0f, color.a / 255.0f
    };

    for (int i = 0; i < 4; ++i) {
        float rx = corners[i][0] * cos_a - corners[i][1] * sin_a;
        float ry = corners[i][0] * sin_a + corners[i][1] * cos_a;
        vertices[i].position = {cx + rx, cy + ry};
        vertices[i].color = fcolor;
    }

    int indices[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(sdlRenderer, nullptr, vertices, 4, indices, 6);
}

void Renderer::drawBot(const Bot& bot, SDL_Color color, float offsetX, float offsetY) {
    const auto& frame = ComponentRegistry::instance().getFrame(bot.frameIndex);
    Vec2 facing = bot.getFacingVector();

    // Apply camera offset to bot position
    float bx = bot.x + offsetX;
    float by = bot.y + offsetY;

    // Draw shadow
    SDL_Color shadowColor = {0, 0, 0, 80};
    float shadowOffset = 4.0f;

    // Draw body based on frame shape
    SDL_Color darkColor = {
        static_cast<Uint8>(color.r * 0.6f),
        static_cast<Uint8>(color.g * 0.6f),
        static_cast<Uint8>(color.b * 0.6f),
        255
    };
    SDL_Color lightColor = {
        static_cast<Uint8>(std::min(255, color.r + 40)),
        static_cast<Uint8>(std::min(255, color.g + 40)),
        static_cast<Uint8>(std::min(255, color.b + 40)),
        255
    };

    switch (frame.shape) {
        case FrameShape::Square: {
            // Square bot with beveled look
            float size = bot.radius * 1.6f;
            drawRotatedRect(bx + shadowOffset, by + shadowOffset, size, size, bot.angle, shadowColor);
            drawRotatedRect(bx, by, size, size, bot.angle, color);
            // Inner highlight
            drawRotatedRect(bx, by, size * 0.7f, size * 0.7f, bot.angle, lightColor);
            break;
        }
        case FrameShape::Rectangle: {
            // Wide rectangle (tank)
            float w = bot.radius * 2.2f;
            float h = bot.radius * 1.4f;
            drawRotatedRect(bx + shadowOffset, by + shadowOffset, w, h, bot.angle, shadowColor);
            drawRotatedRect(bx, by, w, h, bot.angle, color);
            // Track marks
            Vec2 perp(-facing.y, facing.x);
            drawRotatedRect(bx + perp.x * h * 0.3f, by + perp.y * h * 0.3f,
                           w * 0.9f, h * 0.2f, bot.angle, darkColor);
            drawRotatedRect(bx - perp.x * h * 0.3f, by - perp.y * h * 0.3f,
                           w * 0.9f, h * 0.2f, bot.angle, darkColor);
            break;
        }
        case FrameShape::Triangle: {
            // Wedge/dart shape - aggressive pointed front
            float size = bot.radius * 1.8f;
            Vec2 perp(-facing.y, facing.x);
            float frontX = bx + facing.x * size * 0.6f;
            float frontY = by + facing.y * size * 0.6f;
            float backX = bx - facing.x * size * 0.4f;
            float backY = by - facing.y * size * 0.4f;

            // Shadow
            drawTriangle(frontX + shadowOffset, frontY + shadowOffset,
                        backX - perp.x * size * 0.5f + shadowOffset, backY - perp.y * size * 0.5f + shadowOffset,
                        backX + perp.x * size * 0.5f + shadowOffset, backY + perp.y * size * 0.5f + shadowOffset,
                        shadowColor, true);
            // Main body
            drawTriangle(frontX, frontY,
                        backX - perp.x * size * 0.5f, backY - perp.y * size * 0.5f,
                        backX + perp.x * size * 0.5f, backY + perp.y * size * 0.5f,
                        color, true);
            // Cockpit
            drawTriangle(bx + facing.x * size * 0.1f, by + facing.y * size * 0.1f,
                        bx - perp.x * size * 0.2f, by - perp.y * size * 0.2f,
                        bx + perp.x * size * 0.2f, by + perp.y * size * 0.2f,
                        lightColor, true);
            break;
        }
        case FrameShape::Circle: {
            // Round disc
            drawFilledCircle(bx + shadowOffset, by + shadowOffset, bot.radius, shadowColor);
            drawFilledCircle(bx, by, bot.radius, color);
            drawFilledCircle(bx - 2, by - 2, bot.radius * 0.5f, lightColor);
            break;
        }
        case FrameShape::Diamond: {
            // Small diamond shape
            float size = bot.radius * 1.5f;
            drawDiamond(bx + shadowOffset, by + shadowOffset, size, size * 1.3f, bot.angle, shadowColor);
            drawDiamond(bx, by, size, size * 1.3f, bot.angle, color);
            drawDiamond(bx, by, size * 0.4f, size * 0.5f, bot.angle, lightColor);
            break;
        }
        case FrameShape::Hexagon: {
            // Hexagonal frame
            drawHexagon(bx + shadowOffset, by + shadowOffset, bot.radius, bot.angle, shadowColor, true);
            drawHexagon(bx, by, bot.radius, bot.angle, color, true);
            drawHexagon(bx, by, bot.radius * 0.5f, bot.angle + PI / 6.0f, lightColor, true);
            break;
        }
    }

    // Draw front direction indicator
    float indicatorDist = bot.radius * 0.8f;
    float indicatorX = bx + facing.x * indicatorDist;
    float indicatorY = by + facing.y * indicatorDist;
    SDL_Color indicatorColor = {255, 255, 255, 200};
    drawFilledCircle(indicatorX, indicatorY, 4.0f, indicatorColor);

    // Draw weapon
    drawBotWeapon(bot, color, offsetX, offsetY);

    // Draw grabbed indicator
    if (bot.grabState == GrabState::Grabbed) {
        SDL_Color tint = {255, 100, 100, 180};
        for (int i = 0; i < 4; ++i) {
            float angle = (i / 4.0f) * 2.0f * PI + bot.angle;
            float px = bx + std::cos(angle) * (bot.radius + 8);
            float py = by + std::sin(angle) * (bot.radius + 8);
            drawFilledCircle(px, py, 5.0f, tint);
        }
    }

    // Draw shield effect
    if (bot.shieldActive) {
        SDL_Color shield = {100, 180, 255, 150};
        drawCircleOutline(bx, by, bot.radius + 10, shield, 3.0f);
        drawCircleOutline(bx, by, bot.radius + 6, shield, 2.0f);
    }

    // Draw anchor effect
    if (bot.anchorActive) {
        SDL_Color anchor = {200, 200, 200, 220};
        drawRect(bx - bot.radius - 5, by - 3, bot.radius * 2 + 10, 6, anchor, true);
        drawRect(bx - 3, by - bot.radius - 5, 6, bot.radius * 2 + 10, anchor, true);
    }

    // Draw berserk effect
    if (bot.berserkActive) {
        SDL_Color berserk = {255, 50, 50, 150};
        drawCircleOutline(bx, by, bot.radius + 5, berserk, 2.0f);
    }

    // Draw overdrive effect
    if (bot.overdriveActive) {
        SDL_Color overdrive = {255, 200, 50, 150};
        drawCircleOutline(bx, by, bot.radius + 7, overdrive, 2.0f);
    }
}

void Renderer::drawBotWeapon(const Bot& bot, SDL_Color color, float offsetX, float offsetY) {
    const auto& weapon = ComponentRegistry::instance().getWeapon(bot.weaponIndex);
    const auto& frame = ComponentRegistry::instance().getFrame(bot.frameIndex);
    Vec2 facing = bot.getFacingVector();

    float bx = bot.x + offsetX;
    float by = bot.y + offsetY;

    float weaponOffset;
    switch (frame.shape) {
        case FrameShape::Rectangle: weaponOffset = bot.radius * 1.15f; break;
        case FrameShape::Triangle:  weaponOffset = bot.radius * 0.7f; break;
        case FrameShape::Diamond:   weaponOffset = bot.radius * 0.8f; break;
        case FrameShape::Square:    weaponOffset = bot.radius * 0.85f; break;
        default:                    weaponOffset = bot.radius * 0.95f; break;
    }

    if (weapon.name == "Spinner") {
        float spinnerRadius = bot.radius * 0.75f;
        float spinnerX = bx + facing.x * weaponOffset;
        float spinnerY = by + facing.y * weaponOffset;

        // Base spinner disc (always visible)
        SDL_Color baseColor = {100, 100, 110, 255};
        drawCircle(spinnerX, spinnerY, spinnerRadius, baseColor, true);

        // Spinning blades - always animate using a continuous rotation based on time
        // Use bot.angle as base plus a fast-spinning animation
        // Color intensity based on spinnerSpeed
        uint8_t intensity = static_cast<uint8_t>(155 + 100 * bot.spinnerSpeed);
        SDL_Color bladeColor = {intensity, static_cast<uint8_t>(intensity * 0.6f), 50, 255};

        // Blade animation - spins fast continuously, speed affects color only
        // Use SDL_GetTicks for continuous animation independent of spinnerSpeed
        float animTime = SDL_GetTicks() / 1000.0f;
        float spinRate = 8.0f + bot.spinnerSpeed * 12.0f;  // Base spin + speed bonus
        float bladeAngle = bot.angle + animTime * spinRate;

        for (int i = 0; i < 4; ++i) {
            float a = bladeAngle + i * PI / 2.0f;
            float bx1 = spinnerX + std::cos(a) * spinnerRadius * 0.2f;
            float by1 = spinnerY + std::sin(a) * spinnerRadius * 0.2f;
            float bx2 = spinnerX + std::cos(a) * spinnerRadius * 0.95f;
            float by2 = spinnerY + std::sin(a) * spinnerRadius * 0.95f;
            drawLine(bx1, by1, bx2, by2, bladeColor, 4.0f);
        }

        // Metal rim with glow when at high speed
        SDL_Color rimColor = bot.spinnerSpeed > 0.8f ?
            SDL_Color{255, 200, 100, 255} : SDL_Color{180, 180, 190, 255};
        drawCircleOutline(spinnerX, spinnerY, spinnerRadius, rimColor, 2.0f);
    }
    else if (weapon.name == "Clamp") {
        Vec2 perp(-facing.y, facing.x);
        float jawLen = bot.radius * 0.5f;
        float jawWidth = bot.radius * 0.2f;
        float jawSpacing = bot.radius * 0.3f;

        SDL_Color jawColor = {150, 150, 160, 255};

        float lx = bx + facing.x * weaponOffset - perp.x * jawSpacing;
        float ly = by + facing.y * weaponOffset - perp.y * jawSpacing;
        drawRotatedRect(lx, ly, jawLen, jawWidth, bot.angle - 0.3f, jawColor);

        float rx = bx + facing.x * weaponOffset + perp.x * jawSpacing;
        float ry = by + facing.y * weaponOffset + perp.y * jawSpacing;
        drawRotatedRect(rx, ry, jawLen, jawWidth, bot.angle + 0.3f, jawColor);
    }
    else if (weapon.name == "Hammer") {
        float hammerX = bx + facing.x * (weaponOffset + bot.radius * 0.2f);
        float hammerY = by + facing.y * (weaponOffset + bot.radius * 0.2f);
        SDL_Color hammerColor = bot.hammerWindingUp ?
            SDL_Color{255, 200, 100, 255} : SDL_Color{180, 180, 190, 255};
        drawRotatedRect(hammerX, hammerY, bot.radius * 0.5f, bot.radius * 0.3f,
                        bot.angle, hammerColor);
    }
    else if (weapon.name == "Battering Ram") {
        float ramX = bx + facing.x * weaponOffset;
        float ramY = by + facing.y * weaponOffset;
        SDL_Color ramColor = {100, 100, 120, 255};
        drawRotatedRect(ramX, ramY, bot.radius * 0.3f, bot.radius * 0.8f,
                        bot.angle, ramColor);
    }
}

void Renderer::drawHealthBar(float x, float y, float width, float height,
                             float current, float max, SDL_Color fgColor, SDL_Color bgColor) {
    // Background
    drawRect(x, y, width, height, bgColor, true);

    // Foreground
    float ratio = clamp(current / max, 0.0f, 1.0f);
    drawRect(x, y, width * ratio, height, fgColor, true);

    // Border
    SDL_Color border = {200, 200, 200, 255};
    drawRectOutline(x, y, width, height, border, 1.0f);
}

void Renderer::drawProgressBar(float x, float y, float width, float height,
                               float progress, SDL_Color fgColor, SDL_Color bgColor) {
    drawRect(x, y, width, height, bgColor, true);
    float ratio = clamp(progress, 0.0f, 1.0f);
    drawRect(x, y, width * ratio, height, fgColor, true);
}

void Renderer::drawStage(const StageDef& stage, float offsetX, float offsetY) {
    // === 16-BIT RETRO ARENA FLOOR ===

    // Base floor color
    drawRect(offsetX, offsetY, stage.width, stage.height, stage.backgroundColor, true);

    // Checkerboard pattern for retro floor
    SDL_Color tileColor1 = stage.backgroundColor;
    SDL_Color tileColor2 = {
        static_cast<Uint8>(std::max(0, stage.backgroundColor.r - 12)),
        static_cast<Uint8>(std::max(0, stage.backgroundColor.g - 12)),
        static_cast<Uint8>(std::max(0, stage.backgroundColor.b - 12)),
        255
    };
    float tileSize = 32.0f;
    for (float ty = 0; ty < stage.height; ty += tileSize) {
        for (float tx = 0; tx < stage.width; tx += tileSize) {
            int tileX = static_cast<int>(tx / tileSize);
            int tileY = static_cast<int>(ty / tileSize);
            if ((tileX + tileY) % 2 == 1) {
                float tw = std::min(tileSize, stage.width - tx);
                float th = std::min(tileSize, stage.height - ty);
                drawRect(offsetX + tx, offsetY + ty, tw, th, tileColor2, true);
            }
        }
    }

    // Center circle marking
    float centerX = offsetX + stage.width / 2.0f;
    float centerY = offsetY + stage.height / 2.0f;
    float circleRadius = std::min(stage.width, stage.height) * 0.2f;
    SDL_Color circleColor = {
        static_cast<Uint8>(std::min(255, stage.wallColor.r + 30)),
        static_cast<Uint8>(std::min(255, stage.wallColor.g + 30)),
        static_cast<Uint8>(std::min(255, stage.wallColor.b + 30)),
        80
    };
    drawCircleOutline(centerX, centerY, circleRadius, circleColor, 3.0f);
    drawCircleOutline(centerX, centerY, circleRadius * 0.3f, circleColor, 2.0f);

    // Cross lines in center
    SDL_Color crossColor = {circleColor.r, circleColor.g, circleColor.b, 50};
    drawLine(centerX - circleRadius, centerY, centerX + circleRadius, centerY, crossColor, 2.0f);
    drawLine(centerX, centerY - circleRadius, centerX, centerY + circleRadius, crossColor, 2.0f);

    // === CHUNKY RETRO WALLS ===
    float wallThickness = 12.0f;
    SDL_Color wallMain = stage.wallColor;
    SDL_Color wallDark = {
        static_cast<Uint8>(stage.wallColor.r * 0.5f),
        static_cast<Uint8>(stage.wallColor.g * 0.5f),
        static_cast<Uint8>(stage.wallColor.b * 0.5f),
        255
    };
    SDL_Color wallLight = {
        static_cast<Uint8>(std::min(255, stage.wallColor.r + 50)),
        static_cast<Uint8>(std::min(255, stage.wallColor.g + 50)),
        static_cast<Uint8>(std::min(255, stage.wallColor.b + 50)),
        255
    };

    // Outer wall frame (3D beveled look)
    // Top wall
    drawRect(offsetX - wallThickness, offsetY - wallThickness,
             stage.width + wallThickness * 2, wallThickness, wallMain, true);
    drawRect(offsetX - wallThickness, offsetY - wallThickness,
             stage.width + wallThickness * 2, 3, wallLight, true);  // Top highlight
    drawRect(offsetX - wallThickness, offsetY - 3,
             stage.width + wallThickness * 2, 3, wallDark, true);  // Bottom shadow

    // Bottom wall
    drawRect(offsetX - wallThickness, offsetY + stage.height,
             stage.width + wallThickness * 2, wallThickness, wallMain, true);
    drawRect(offsetX - wallThickness, offsetY + stage.height,
             stage.width + wallThickness * 2, 3, wallLight, true);
    drawRect(offsetX - wallThickness, offsetY + stage.height + wallThickness - 3,
             stage.width + wallThickness * 2, 3, wallDark, true);

    // Left wall
    drawRect(offsetX - wallThickness, offsetY,
             wallThickness, stage.height, wallMain, true);
    drawRect(offsetX - wallThickness, offsetY, 3, stage.height, wallLight, true);
    drawRect(offsetX - 3, offsetY, 3, stage.height, wallDark, true);

    // Right wall
    drawRect(offsetX + stage.width, offsetY,
             wallThickness, stage.height, wallMain, true);
    drawRect(offsetX + stage.width, offsetY, 3, stage.height, wallLight, true);
    drawRect(offsetX + stage.width + wallThickness - 3, offsetY, 3, stage.height, wallDark, true);

    // Inner edge highlight (arena border)
    drawRect(offsetX, offsetY, stage.width, 2, wallLight, true);
    drawRect(offsetX, offsetY + stage.height - 2, stage.width, 2, wallDark, true);
    drawRect(offsetX, offsetY, 2, stage.height, wallLight, true);
    drawRect(offsetX + stage.width - 2, offsetY, 2, stage.height, wallDark, true);

    // Corner posts (decorative)
    float postSize = 16.0f;
    SDL_Color postColor = {255, 200, 50, 255};
    SDL_Color postDark = {180, 140, 30, 255};

    // Top-left corner post
    drawRect(offsetX - wallThickness, offsetY - wallThickness, postSize, postSize, postColor, true);
    drawRect(offsetX - wallThickness, offsetY - wallThickness, postSize, 2, {255, 240, 150, 255}, true);
    drawRect(offsetX - wallThickness + postSize - 2, offsetY - wallThickness, 2, postSize, postDark, true);

    // Top-right corner post
    drawRect(offsetX + stage.width + wallThickness - postSize, offsetY - wallThickness, postSize, postSize, postColor, true);
    drawRect(offsetX + stage.width + wallThickness - postSize, offsetY - wallThickness, postSize, 2, {255, 240, 150, 255}, true);
    drawRect(offsetX + stage.width + wallThickness - 2, offsetY - wallThickness, 2, postSize, postDark, true);

    // Bottom-left corner post
    drawRect(offsetX - wallThickness, offsetY + stage.height + wallThickness - postSize, postSize, postSize, postColor, true);
    drawRect(offsetX - wallThickness, offsetY + stage.height + wallThickness - postSize, postSize, 2, {255, 240, 150, 255}, true);
    drawRect(offsetX - wallThickness + postSize - 2, offsetY + stage.height + wallThickness - postSize, 2, postSize, postDark, true);

    // Bottom-right corner post
    drawRect(offsetX + stage.width + wallThickness - postSize, offsetY + stage.height + wallThickness - postSize, postSize, postSize, postColor, true);
    drawRect(offsetX + stage.width + wallThickness - postSize, offsetY + stage.height + wallThickness - postSize, postSize, 2, {255, 240, 150, 255}, true);
    drawRect(offsetX + stage.width + wallThickness - 2, offsetY + stage.height + wallThickness - postSize, 2, postSize, postDark, true);

    // Draw hazards
    for (const auto& hazard : stage.hazards) {
        drawHazard(hazard, offsetX, offsetY);
    }
}

void Renderer::drawHazard(const Hazard& hazard, float offsetX, float offsetY) {
    SDL_Color hazardColor;

    switch (hazard.type) {
        case HazardType::Flames:
            hazardColor = hazard.isActive ?
                SDL_Color{255, 100, 50, 200} : SDL_Color{100, 50, 30, 100};
            break;
        case HazardType::ElectrifiedWall:
            hazardColor = {100, 150, 255, 200};
            break;
        case HazardType::Pit:
            hazardColor = {20, 20, 20, 255};
            break;
        default:
            return;
    }

    if (hazard.isCircular) {
        drawCircle(offsetX + hazard.x, offsetY + hazard.y, hazard.radius,
                   hazardColor, true);
    } else {
        drawRect(offsetX + hazard.x, offsetY + hazard.y,
                 hazard.width, hazard.height, hazardColor, true);
    }
}

void Renderer::drawPowerup(const Powerup& powerup, float offsetX, float offsetY) {
    if (!powerup.active) {
        // Show spawn countdown if about to spawn
        if (powerup.spawnCountdown > 0 && powerup.spawnCountdown <= 3.0f) {
            SDL_Color countdown = {150, 150, 150, 128};
            drawCircle(offsetX + powerup.x, offsetY + powerup.y,
                       powerup.radius * 0.5f, countdown, false);
        }
        return;
    }

    const auto& def = PowerupManager::instance().getDefinition(powerup.type);

    // Draw powerup
    drawCircle(offsetX + powerup.x, offsetY + powerup.y, powerup.radius,
               def.color, true);

    // Draw icon based on type
    SDL_Color iconColor = {255, 255, 255, 255};
    float cx = offsetX + powerup.x;
    float cy = offsetY + powerup.y;

    switch (powerup.type) {
        case PowerupType::HealthPack:
            // Plus sign
            drawRect(cx - 8, cy - 2, 16, 4, iconColor, true);
            drawRect(cx - 2, cy - 8, 4, 16, iconColor, true);
            break;
        case PowerupType::SpeedBoost:
            // Lightning bolt shape
            drawTriangle(cx, cy - 8, cx - 5, cy + 2, cx + 2, cy - 2, iconColor, true);
            drawTriangle(cx, cy + 8, cx + 5, cy - 2, cx - 2, cy + 2, iconColor, true);
            break;
        case PowerupType::DamageBoost:
            // Small skull shape (simplified)
            drawCircle(cx, cy - 2, 6, iconColor, true);
            drawRect(cx - 4, cy + 2, 8, 4, iconColor, true);
            break;
        case PowerupType::Shield:
            // Shield outline
            drawCircleOutline(cx, cy, 8, iconColor, 2.0f);
            break;
        case PowerupType::CooldownReset:
            // Spiral/swirl (simplified as arc)
            drawCircleOutline(cx, cy, 6, iconColor, 2.0f);
            break;
    }
}

void Renderer::drawCombatEvent(const CombatEvent& event, float offsetX, float offsetY) {
    float progress = event.timer / 0.5f;  // 0 to 1, fading
    float alpha = 255.0f * std::min(1.0f, progress * 2.0f);  // Quick fade in, slow fade out
    if (alpha <= 0) return;

    float x = offsetX + event.x;
    float y = offsetY + event.y;

    switch (event.type) {
        case CombatEvent::Type::Impact: {
            // Spark explosion effect
            float sparkAlpha = alpha * progress;
            int numSparks = 8 + static_cast<int>(event.value / 20.0f);
            float baseRadius = 8.0f + event.value * 0.15f;
            float spread = (1.0f - progress) * 25.0f;  // Sparks spread outward

            // Central flash
            SDL_Color flashColor = {255, 255, 200, static_cast<Uint8>(sparkAlpha * 0.8f)};
            drawCircle(x, y, baseRadius * progress, flashColor, true);

            // Spark particles
            for (int i = 0; i < numSparks; ++i) {
                float angle = (i / static_cast<float>(numSparks)) * 2.0f * PI;
                angle += event.value * 0.1f;  // Slight rotation based on force

                float sparkDist = spread * (0.5f + 0.5f * std::sin(angle * 3.0f + event.value));
                float sx = x + std::cos(angle) * sparkDist;
                float sy = y + std::sin(angle) * sparkDist;

                // Alternate colors: yellow, orange, white
                SDL_Color sparkColor;
                if (i % 3 == 0) {
                    sparkColor = {255, 220, 100, static_cast<Uint8>(sparkAlpha)};
                } else if (i % 3 == 1) {
                    sparkColor = {255, 150, 50, static_cast<Uint8>(sparkAlpha)};
                } else {
                    sparkColor = {255, 255, 255, static_cast<Uint8>(sparkAlpha * 0.8f)};
                }

                float sparkSize = 2.0f + 2.0f * progress;
                drawCircle(sx, sy, sparkSize, sparkColor, true);

                // Spark trail
                float trailX = x + std::cos(angle) * sparkDist * 0.5f;
                float trailY = y + std::sin(angle) * sparkDist * 0.5f;
                sparkColor.a = static_cast<Uint8>(sparkAlpha * 0.5f);
                drawLine(sx, sy, trailX, trailY, sparkColor, 1.5f);
            }
            break;
        }
        case CombatEvent::Type::Damage: {
            SDL_Color color = {255, 200, 50, static_cast<Uint8>(alpha)};
            float size = 10.0f + event.value * 0.3f;
            drawCircle(x, y, size, color, false);
            break;
        }
        case CombatEvent::Type::Death: {
            SDL_Color color = {255, 50, 50, static_cast<Uint8>(alpha)};
            drawCircle(x, y, 30, color, false);
            break;
        }
        default:
            break;
    }
}

void Renderer::drawMine(const Mine& mine, float offsetX, float offsetY) {
    if (mine.exploded) return;

    SDL_Color color = mine.armed ?
        SDL_Color{255, 50, 50, 255} : SDL_Color{150, 150, 50, 255};

    drawCircle(offsetX + mine.x, offsetY + mine.y, Mine::RADIUS * 0.8f, color, true);

    // Blinking indicator when armed
    if (mine.armed) {
        SDL_Color blink = {255, 255, 255, 200};
        drawCircle(offsetX + mine.x, offsetY + mine.y, 5, blink, true);
    }
}

void Renderer::drawSmokeCloud(const SmokeCloud& cloud, float offsetX, float offsetY) {
    float lifeRatio = cloud.timer / SmokeCloud::DURATION;
    float baseAlpha = 180.0f * lifeRatio;
    float cx = offsetX + cloud.x;
    float cy = offsetY + cloud.y;

    // Draw multiple overlapping smoke puffs for realistic smoke effect
    int numPuffs = 8;
    for (int i = 0; i < numPuffs; ++i) {
        // Consistent offset based on puff index and cloud position
        float angle = (i * 0.785f) + (cloud.x + cloud.y) * 0.01f;
        float dist = cloud.radius * (0.3f + (i % 3) * 0.2f);
        float puffX = cx + std::cos(angle) * dist;
        float puffY = cy + std::sin(angle) * dist;

        // Varying sizes for each puff
        float puffRadius = cloud.radius * (0.4f + (i % 4) * 0.15f);

        // Varying alpha for depth effect
        float puffAlpha = baseAlpha * (0.5f + (i % 3) * 0.2f);

        // Slight color variation (grey to darker grey)
        uint8_t grey = 80 + (i % 4) * 15;
        SDL_Color puffColor = {grey, grey, static_cast<uint8_t>(grey + 10), static_cast<Uint8>(puffAlpha)};

        drawCircle(puffX, puffY, puffRadius, puffColor, true);
    }

    // Central darker core
    SDL_Color coreColor = {60, 60, 70, static_cast<Uint8>(baseAlpha * 0.7f)};
    drawCircle(cx, cy, cloud.radius * 0.5f, coreColor, true);

    // Outer wisps - lighter
    for (int i = 0; i < 4; ++i) {
        float angle = i * 1.57f + cloud.timer * 0.5f;  // Slow rotation
        float dist = cloud.radius * 0.8f;
        float wispX = cx + std::cos(angle) * dist;
        float wispY = cy + std::sin(angle) * dist;
        SDL_Color wispColor = {120, 120, 130, static_cast<Uint8>(baseAlpha * 0.3f)};
        drawCircle(wispX, wispY, cloud.radius * 0.3f, wispColor, true);
    }
}

void Renderer::drawGrabTether(const Bot& grabber, const Bot& grabbed,
                              float offsetX, float offsetY) {
    SDL_Color tether = {200, 150, 50, 255};
    drawLine(offsetX + grabber.x, offsetY + grabber.y,
             offsetX + grabbed.x, offsetY + grabbed.y, tether, 3.0f);
}

const std::vector<SDL_Color>& Renderer::getPlayerColors() {
    return PLAYER_COLORS;
}

SDL_Color Renderer::getPlayerColor(int index) {
    if (index < 0 || index >= static_cast<int>(PLAYER_COLORS.size())) {
        return PLAYER_COLORS[0];
    }
    return PLAYER_COLORS[index];
}

} // namespace ScrapHeap

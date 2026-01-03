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

void Renderer::drawBot(const Bot& bot, SDL_Color color) {
    const auto& frame = ComponentRegistry::instance().getFrame(bot.frameIndex);
    Vec2 facing = bot.getFacingVector();

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
            drawRotatedRect(bot.x + shadowOffset, bot.y + shadowOffset, size, size, bot.angle, shadowColor);
            drawRotatedRect(bot.x, bot.y, size, size, bot.angle, color);
            // Inner highlight
            drawRotatedRect(bot.x, bot.y, size * 0.7f, size * 0.7f, bot.angle, lightColor);
            break;
        }
        case FrameShape::Rectangle: {
            // Wide rectangle (tank)
            float w = bot.radius * 2.2f;
            float h = bot.radius * 1.4f;
            drawRotatedRect(bot.x + shadowOffset, bot.y + shadowOffset, w, h, bot.angle, shadowColor);
            drawRotatedRect(bot.x, bot.y, w, h, bot.angle, color);
            // Track marks
            Vec2 perp(-facing.y, facing.x);
            drawRotatedRect(bot.x + perp.x * h * 0.3f, bot.y + perp.y * h * 0.3f,
                           w * 0.9f, h * 0.2f, bot.angle, darkColor);
            drawRotatedRect(bot.x - perp.x * h * 0.3f, bot.y - perp.y * h * 0.3f,
                           w * 0.9f, h * 0.2f, bot.angle, darkColor);
            break;
        }
        case FrameShape::Triangle: {
            // Wedge/dart shape - aggressive pointed front
            float size = bot.radius * 1.8f;
            Vec2 perp(-facing.y, facing.x);
            float frontX = bot.x + facing.x * size * 0.6f;
            float frontY = bot.y + facing.y * size * 0.6f;
            float backX = bot.x - facing.x * size * 0.4f;
            float backY = bot.y - facing.y * size * 0.4f;

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
            drawTriangle(bot.x + facing.x * size * 0.1f, bot.y + facing.y * size * 0.1f,
                        bot.x - perp.x * size * 0.2f, bot.y - perp.y * size * 0.2f,
                        bot.x + perp.x * size * 0.2f, bot.y + perp.y * size * 0.2f,
                        lightColor, true);
            break;
        }
        case FrameShape::Circle: {
            // Round disc
            drawFilledCircle(bot.x + shadowOffset, bot.y + shadowOffset, bot.radius, shadowColor);
            drawFilledCircle(bot.x, bot.y, bot.radius, color);
            drawFilledCircle(bot.x - 2, bot.y - 2, bot.radius * 0.5f, lightColor);
            break;
        }
        case FrameShape::Diamond: {
            // Small diamond shape
            float size = bot.radius * 1.5f;
            drawDiamond(bot.x + shadowOffset, bot.y + shadowOffset, size, size * 1.3f, bot.angle, shadowColor);
            drawDiamond(bot.x, bot.y, size, size * 1.3f, bot.angle, color);
            drawDiamond(bot.x, bot.y, size * 0.4f, size * 0.5f, bot.angle, lightColor);
            break;
        }
        case FrameShape::Hexagon: {
            // Hexagonal frame
            drawHexagon(bot.x + shadowOffset, bot.y + shadowOffset, bot.radius, bot.angle, shadowColor, true);
            drawHexagon(bot.x, bot.y, bot.radius, bot.angle, color, true);
            drawHexagon(bot.x, bot.y, bot.radius * 0.5f, bot.angle + PI / 6.0f, lightColor, true);
            break;
        }
    }

    // Draw front direction indicator
    float indicatorDist = bot.radius * 0.8f;
    float indicatorX = bot.x + facing.x * indicatorDist;
    float indicatorY = bot.y + facing.y * indicatorDist;
    SDL_Color indicatorColor = {255, 255, 255, 200};
    drawFilledCircle(indicatorX, indicatorY, 4.0f, indicatorColor);

    // Draw weapon
    drawBotWeapon(bot, color);

    // Draw grabbed indicator
    if (bot.grabState == GrabState::Grabbed) {
        SDL_Color tint = {255, 100, 100, 180};
        for (int i = 0; i < 4; ++i) {
            float angle = (i / 4.0f) * 2.0f * PI + bot.angle;
            float px = bot.x + std::cos(angle) * (bot.radius + 8);
            float py = bot.y + std::sin(angle) * (bot.radius + 8);
            drawFilledCircle(px, py, 5.0f, tint);
        }
    }

    // Draw shield effect
    if (bot.shieldActive) {
        SDL_Color shield = {100, 180, 255, 150};
        drawCircleOutline(bot.x, bot.y, bot.radius + 10, shield, 3.0f);
        drawCircleOutline(bot.x, bot.y, bot.radius + 6, shield, 2.0f);
    }

    // Draw anchor effect
    if (bot.anchorActive) {
        SDL_Color anchor = {200, 200, 200, 220};
        drawRect(bot.x - bot.radius - 5, bot.y - 3, bot.radius * 2 + 10, 6, anchor, true);
        drawRect(bot.x - 3, bot.y - bot.radius - 5, 6, bot.radius * 2 + 10, anchor, true);
    }

    // Draw berserk effect
    if (bot.berserkActive) {
        SDL_Color berserk = {255, 50, 50, 150};
        drawCircleOutline(bot.x, bot.y, bot.radius + 5, berserk, 2.0f);
    }

    // Draw overdrive effect
    if (bot.overdriveActive) {
        SDL_Color overdrive = {255, 200, 50, 150};
        drawCircleOutline(bot.x, bot.y, bot.radius + 7, overdrive, 2.0f);
    }
}

void Renderer::drawBotWeapon(const Bot& bot, SDL_Color color) {
    const auto& weapon = ComponentRegistry::instance().getWeapon(bot.weaponIndex);
    Vec2 facing = bot.getFacingVector();

    if (weapon.name == "Spinner") {
        // Draw spinner disc
        float spinnerRadius = bot.radius * 0.6f;
        float spinnerX = bot.x + facing.x * bot.radius * 0.3f;
        float spinnerY = bot.y + facing.y * bot.radius * 0.3f;

        SDL_Color spinnerColor = {200, 200, 200, 255};
        if (bot.spinnerSpeed > 0.1f) {
            // Animate based on spinner speed
            spinnerColor.r = static_cast<Uint8>(200 + 55 * bot.spinnerSpeed);
        }
        drawCircle(spinnerX, spinnerY, spinnerRadius * bot.spinnerSpeed + spinnerRadius * 0.3f,
                   spinnerColor, true);
    }
    else if (weapon.name == "Clamp") {
        // Draw clamp jaws
        Vec2 perp(-facing.y, facing.x);
        float jawLen = bot.radius * 0.5f;
        float jawWidth = bot.radius * 0.2f;
        float jawOffset = bot.radius * 0.3f;

        SDL_Color jawColor = {150, 150, 160, 255};

        // Left jaw
        float lx = bot.x + facing.x * bot.radius * 0.8f - perp.x * jawOffset;
        float ly = bot.y + facing.y * bot.radius * 0.8f - perp.y * jawOffset;
        drawRotatedRect(lx, ly, jawLen, jawWidth, bot.angle - 0.3f, jawColor);

        // Right jaw
        float rx = bot.x + facing.x * bot.radius * 0.8f + perp.x * jawOffset;
        float ry = bot.y + facing.y * bot.radius * 0.8f + perp.y * jawOffset;
        drawRotatedRect(rx, ry, jawLen, jawWidth, bot.angle + 0.3f, jawColor);
    }
    else if (weapon.name == "Hammer") {
        // Draw hammer head
        float hammerX = bot.x + facing.x * bot.radius * 1.1f;
        float hammerY = bot.y + facing.y * bot.radius * 1.1f;
        SDL_Color hammerColor = {180, 180, 190, 255};
        if (bot.hammerWindingUp) {
            hammerColor = {255, 200, 100, 255};
        }
        drawRotatedRect(hammerX, hammerY, bot.radius * 0.5f, bot.radius * 0.3f,
                        bot.angle, hammerColor);
    }
    else if (weapon.name == "Battering Ram") {
        // Draw ram front plate
        float ramX = bot.x + facing.x * bot.radius * 0.9f;
        float ramY = bot.y + facing.y * bot.radius * 0.9f;
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
    // Draw floor with retro grid pattern
    drawRect(offsetX, offsetY, stage.width, stage.height, stage.backgroundColor, true);

    // Draw grid lines for retro look
    SDL_Color gridColor = {
        static_cast<Uint8>(std::min(255, stage.backgroundColor.r + 15)),
        static_cast<Uint8>(std::min(255, stage.backgroundColor.g + 15)),
        static_cast<Uint8>(std::min(255, stage.backgroundColor.b + 15)),
        100
    };
    float gridSize = 40.0f;
    for (float x = gridSize; x < stage.width; x += gridSize) {
        drawLine(offsetX + x, offsetY, offsetX + x, offsetY + stage.height, gridColor, 1.0f);
    }
    for (float y = gridSize; y < stage.height; y += gridSize) {
        drawLine(offsetX, offsetY + y, offsetX + stage.width, offsetY + y, gridColor, 1.0f);
    }

    // Draw walls with chunky retro border
    float wallThickness = 8.0f;
    SDL_Color wallDark = {
        static_cast<Uint8>(stage.wallColor.r * 0.6f),
        static_cast<Uint8>(stage.wallColor.g * 0.6f),
        static_cast<Uint8>(stage.wallColor.b * 0.6f),
        255
    };
    SDL_Color wallLight = {
        static_cast<Uint8>(std::min(255, stage.wallColor.r + 40)),
        static_cast<Uint8>(std::min(255, stage.wallColor.g + 40)),
        static_cast<Uint8>(std::min(255, stage.wallColor.b + 40)),
        255
    };

    // Outer wall (dark)
    drawRect(offsetX - wallThickness, offsetY - wallThickness,
             stage.width + wallThickness * 2, wallThickness, wallDark, true);  // Top
    drawRect(offsetX - wallThickness, offsetY + stage.height,
             stage.width + wallThickness * 2, wallThickness, wallLight, true);  // Bottom
    drawRect(offsetX - wallThickness, offsetY,
             wallThickness, stage.height, wallDark, true);  // Left
    drawRect(offsetX + stage.width, offsetY,
             wallThickness, stage.height, wallLight, true);  // Right

    // Inner wall highlight
    drawRect(offsetX, offsetY, stage.width, 2, wallLight, true);  // Top inner
    drawRect(offsetX, offsetY + stage.height - 2, stage.width, 2, wallDark, true);  // Bottom inner
    drawRect(offsetX, offsetY, 2, stage.height, wallLight, true);  // Left inner
    drawRect(offsetX + stage.width - 2, offsetY, 2, stage.height, wallDark, true);  // Right inner

    // Corner accents
    SDL_Color cornerColor = {255, 200, 50, 200};
    float cornerSize = 12.0f;
    drawRect(offsetX, offsetY, cornerSize, cornerSize, cornerColor, true);
    drawRect(offsetX + stage.width - cornerSize, offsetY, cornerSize, cornerSize, cornerColor, true);
    drawRect(offsetX, offsetY + stage.height - cornerSize, cornerSize, cornerSize, cornerColor, true);
    drawRect(offsetX + stage.width - cornerSize, offsetY + stage.height - cornerSize, cornerSize, cornerSize, cornerColor, true);

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
    float alpha = 255.0f * (1.0f - event.timer / 0.5f);
    if (alpha <= 0) return;

    switch (event.type) {
        case CombatEvent::Type::Damage: {
            SDL_Color color = {255, 200, 50, static_cast<Uint8>(alpha)};
            float size = 10.0f + event.value * 0.3f;
            drawCircle(offsetX + event.x, offsetY + event.y, size, color, false);
            break;
        }
        case CombatEvent::Type::Death: {
            SDL_Color color = {255, 50, 50, static_cast<Uint8>(alpha)};
            drawCircle(offsetX + event.x, offsetY + event.y, 30, color, false);
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
    float alpha = 150.0f * (cloud.timer / SmokeCloud::DURATION);
    SDL_Color color = {100, 100, 100, static_cast<Uint8>(alpha)};
    drawCircle(offsetX + cloud.x, offsetY + cloud.y, cloud.radius, color, true);
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

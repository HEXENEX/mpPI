// SDL2 + JSON-based UI menu system with '5' key as select/click and '8' key as back/hold
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>

using json = nlohmann::json;

const int SCREEN_WIDTH = 320;
const int SCREEN_HEIGHT = 240;
const int FONT_SIZE = 22;
const int BUTTON_HEIGHT = FONT_SIZE + 4;

const SDL_Color COLOR_BG = {255, 255, 255, 255};
const SDL_Color COLOR_BTN = {255, 255, 255, 255};
const SDL_Color COLOR_SEL = {150, 150, 255, 255};
const SDL_Color COLOR_TXT = {0, 0, 0, 255};

struct Button {
    SDL_Rect rect;
    std::string label;
    std::string target;
};

using Menu = std::vector<Button>;
std::unordered_map<std::string, Menu> menus;
std::vector<std::string> menuHistory;

Menu loadMenu(const json& jmenu, const std::string& key) {
    Menu menu;
    if (!jmenu.contains(key)) return menu;
    const auto& items = jmenu[key];
    for (size_t i = 0; i < items.size(); ++i) {
        Button btn;
        btn.rect = {0, static_cast<int>(i * BUTTON_HEIGHT), SCREEN_WIDTH, BUTTON_HEIGHT};
        btn.label = items[i].value("label", "");
        btn.target = items[i].value("target", "");
        menu.push_back(btn);
    }
    return menu;
}

void renderText(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y) {
    SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), COLOR_TXT);
    if (!surface) return;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect dst = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, nullptr, &dst);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

int main() {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    SDL_Window* window = SDL_CreateWindow("Menu UI", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    TTF_Font* font = TTF_OpenFont("assets/PodiumSans.ttf", FONT_SIZE);

    json jmenus;
    std::ifstream("assets/menus.json") >> jmenus;

    for (auto& [key, val] : jmenus.items())
        menus[key] = loadMenu(jmenus, key);

    std::string currentMenu = "main_menu";
    int selectedIndex = 0, scrollOffset = 0;

    bool running = true;
    SDL_Event e;

    bool backKeyHeld = false;
    bool backKeyPressed = false;
    Uint32 backKeyDownTime = 0;

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;

            else if (e.type == SDL_MOUSEWHEEL) {
                if (e.wheel.y < 0 && selectedIndex > 0) selectedIndex--;
                else if (e.wheel.y > 0 && selectedIndex < (int)menus[currentMenu].size() - 1) selectedIndex++;
            }

            else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_5) {
                    std::string target = menus[currentMenu][selectedIndex].target;
                    if (!target.empty() && menus.count(target)) {
                        menuHistory.push_back(currentMenu);
                        currentMenu = target;
                        selectedIndex = 0;
                        scrollOffset = 0;
                    }
                }

                else if (e.key.keysym.sym == SDLK_8) {
                    if (!backKeyHeld) {
                        backKeyHeld = true;
                        backKeyPressed = false;
                        backKeyDownTime = SDL_GetTicks();
                    }
                }
            }

            else if (e.type == SDL_KEYUP && e.key.keysym.sym == SDLK_8) {
                backKeyHeld = false;
                if (!backKeyPressed) {
                    // Short press
                    if (!menuHistory.empty()) {
                        currentMenu = menuHistory.back();
                        menuHistory.pop_back();
                        selectedIndex = 0;
                        scrollOffset = 0;
                    }
                }
            }
        }

        // Long press logic for 8 key
        if (backKeyHeld) {
            Uint32 now = SDL_GetTicks();
            if (!backKeyPressed && now - backKeyDownTime >= 1000) {
                currentMenu = "main_menu";
                menuHistory.clear();
                selectedIndex = 0;
                scrollOffset = 0;
                backKeyHeld = false;
                backKeyPressed = true;
            }
        }

        int selY = selectedIndex * BUTTON_HEIGHT;
        int totalHeight = BUTTON_HEIGHT * menus[currentMenu].size();
        scrollOffset = std::max(0, std::min(selY - SCREEN_HEIGHT / 2, totalHeight - SCREEN_HEIGHT));

        SDL_SetRenderDrawColor(renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
        SDL_RenderClear(renderer);

        for (size_t i = 0; i < menus[currentMenu].size(); ++i) {
            Button btn = menus[currentMenu][i];
            btn.rect.y = static_cast<int>(i * BUTTON_HEIGHT) - scrollOffset;
            SDL_Color btnColor = (i == selectedIndex) ? COLOR_SEL : COLOR_BTN;
            SDL_SetRenderDrawColor(renderer, btnColor.r, btnColor.g, btnColor.b, 255);
            SDL_RenderFillRect(renderer, &btn.rect);
            renderText(renderer, font, btn.label, 10, btn.rect.y + 4);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();
    return 0;
}

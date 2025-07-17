#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <stack>
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace fs = std::filesystem;

const int SCREEN_WIDTH = 320;
const int SCREEN_HEIGHT = 240;
const int FONT_SIZE = 20;
const int BUTTON_HEIGHT = FONT_SIZE + 4;

const Uint32 LONG_PRESS_THRESHOLD = 750; // milliseconds

const SDL_Color COLOR_BG = {255, 255, 255, 255};
const SDL_Color COLOR_BTN = {255, 255, 255, 255};
const SDL_Color COLOR_SEL = {150, 150, 255, 255};
const SDL_Color COLOR_TXT = {0, 0, 0, 255};

int selected_index = 0;

struct Button {
    SDL_Rect rect;
    std::string label;
    std::string target;
};

struct ButtonState {
    bool pressed = false;
    bool long_triggered = false;
    Uint32 press_time = 0;
};

struct MenuState {
    std::string name;
    int index;
};

std::unordered_map<SDL_Keycode, ButtonState> keyStates;
std::vector<Button> buttons;
std::string current_menu = "menus/main_menu";
std::stack<MenuState> menu_history;

void draw_buttons(SDL_Renderer* renderer, TTF_Font* font, int selected_index) {
    const int padding_x = 4;
    const int padding_y = 2;

    for (size_t i = 0; i < buttons.size(); i++) {
        const auto& btn = buttons[i];

        SDL_SetRenderDrawColor(renderer,
            ((int)i == selected_index) ? COLOR_SEL.r : COLOR_BTN.r,
            ((int)i == selected_index) ? COLOR_SEL.g : COLOR_BTN.g,
            ((int)i == selected_index) ? COLOR_SEL.b : COLOR_BTN.b,
            255);
        SDL_RenderFillRect(renderer, &btn.rect);

        SDL_Surface* text_surface = TTF_RenderText_Blended(font, btn.label.c_str(), COLOR_TXT);
        SDL_Texture* text_texture = SDL_CreateTextureFromSurface(renderer, text_surface);

        int text_w = 0, text_h = 0;
        TTF_SizeText(font, btn.label.c_str(), &text_w, &text_h);

        SDL_Rect text_rect = {
            btn.rect.x + padding_x,
            btn.rect.y + padding_y,
            text_w,
            text_h
        };

        SDL_RenderCopy(renderer, text_texture, NULL, &text_rect);

        SDL_FreeSurface(text_surface);
        SDL_DestroyTexture(text_texture);
    }
}

void load_menu(const std::string& menu_path, int restore_index = 0) {
    buttons.clear();
    
    // Make a local copy to avoid any reference issues
    std::string safe_menu_path = menu_path;
    
    std::cout << safe_menu_path << "\n";

    if (!fs::exists(safe_menu_path) || !fs::is_directory(safe_menu_path)) {
        std::cout << "Menu path not found: " << safe_menu_path << "\n";
        return;
    }

    current_menu = safe_menu_path;
    int y = 0;

    std::vector<std::string> ordered_entries;
    std::vector<std::string> all_entries;

    // Read .config for order if it exists
    std::string config_path = safe_menu_path + "/.config";
    if (fs::exists(config_path) && fs::is_regular_file(config_path)) {
        std::ifstream config_file(config_path);
        std::string line;
        while (std::getline(config_file, line)) {
            if (!line.empty() && line[0] != '.') {
                ordered_entries.push_back(line);
            }
        }
    }

    // Collect all visible entries (skip those starting with '.')
    for (const auto& entry : fs::directory_iterator(safe_menu_path)) {
        std::string name = entry.path().filename().string();
        if (name.empty() || name[0] == '.') continue;
        all_entries.push_back(name);
    }

    // If no config, sort all_entries alphabetically and use that
    if (ordered_entries.empty()) {
        ordered_entries = all_entries;
        std::sort(ordered_entries.begin(), ordered_entries.end());
    } else {
        // Include any entries not in .config at the end, sorted
        for (const auto& name : all_entries) {
            if (std::find(ordered_entries.begin(), ordered_entries.end(), name) == ordered_entries.end()) {
                ordered_entries.push_back(name);
            }
        }
    }

    // Create buttons in order
    for (const auto& name : ordered_entries) {
        // Build the full path as a string directly
        std::string full_path = safe_menu_path;
        if (full_path.back() != '/') {
            full_path += '/';
        }
        full_path += name;

        Button b;
        b.rect = {0, y, SCREEN_WIDTH, BUTTON_HEIGHT};

        // Detect if it's a label file
        if (name.rfind("label_", 0) == 0) {  // starts with "label_"
            std::ifstream label_file(full_path);
            std::string label_text;
            std::getline(label_file, label_text);
            b.label = label_text.empty() ? "(empty)" : label_text;
            b.target = ""; // no submenu or launch
        }
        else {
            b.label = name;

            if (fs::is_directory(full_path)) {
                b.target = full_path;  // submenu path as string
            } else {
                b.target = "__LAUNCH__" + full_path;  // launchable path as string
            }
        }

        buttons.push_back(b);
        y += BUTTON_HEIGHT;
    }

    if (buttons.empty()) {
        std::cout << "No entries found in directory\n";
    }
}

void check_long_presses() {
    Uint32 now = SDL_GetTicks();

    for (auto& [key, state] : keyStates) {
        if (state.pressed && !state.long_triggered) {
            Uint32 held = now - state.press_time;
            if (held >= LONG_PRESS_THRESHOLD) {
                state.long_triggered = true;
                std::cout << "[" << SDL_GetKeyName(key) << "] LONG press trigger\n";
                switch(key) {
                    case SDLK_w: {
                        // clear menu history
                        while (!menu_history.empty()) {
                            menu_history.pop();
                        }

                        load_menu("menus/main_menu"); // go back to main menu when menu button is held
                        selected_index = 0;
                        break;
                    }
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cout << "SDL Init Error: " << SDL_GetError() << "\n";
        return 1;
    }

    if (TTF_Init() != 0) {
        std::cout << "TTF Init Error: " << TTF_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("mpPI",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH, SCREEN_HEIGHT,
        SDL_WINDOW_SHOWN);

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    TTF_Font* font = TTF_OpenFont("assets/PodiumSans.ttf", FONT_SIZE);
    if (!font) {
        std::cout << "Failed to load font: " << TTF_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    load_menu(current_menu);

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;

            if (event.type == SDL_MOUSEWHEEL) {
                if (event.wheel.y > 0) {
                    selected_index--;
                    if (selected_index < 0) selected_index = 0;
                } else if (event.wheel.y < 0) {
                    selected_index++;
                    if (selected_index >= (int)buttons.size()) selected_index = (int)buttons.size() - 1;
                }
            }

            if (event.type == SDL_KEYDOWN) {
                SDL_Keycode key = event.key.keysym.sym;
                if (!keyStates[key].pressed) {
                    keyStates[key].pressed = true;
                    keyStates[key].press_time = SDL_GetTicks();
                    keyStates[key].long_triggered = false;
                }
            }

            if (event.type == SDL_KEYUP) {
                SDL_Keycode key = event.key.keysym.sym;
                bool is_long = keyStates[key].long_triggered;

                if (!is_long) {
                    switch (key) {
                        case SDLK_s: {
                            if (selected_index < 0 || selected_index >= (int)buttons.size()) {
                                std::cout << "Invalid selected_index: " << selected_index << "\n";
                                break;
                            }
                            
                            const auto& btn = buttons[selected_index];
                            std::cout << "[S] short press: " << btn.label << "\n";

                            if (btn.target.empty()) {
                                std::cout << "Target is empty, no action\n";
                            } else if (btn.target.compare(0, 10, "__LAUNCH__") == 0) {
                                std::string executable = btn.target.substr(10);
                                std::cout << "Launching: " << executable << "\n";
                                int ret = std::system(executable.c_str());
                                std::cout << "Exited with code: " << ret << "\n";
                            } else {
                                std::cout << "Navigating to submenu: " << btn.target << "\n";
                                // Make a copy of the target string to avoid reference issues
                                std::string target_copy = btn.target;
                                menu_history.push({ current_menu, selected_index });
                                load_menu(target_copy);
                                selected_index = 0;
                            }
                            break;
                        }
                        case SDLK_w: {
                            std::cout << "[W] short press (back)\n";
                            if (!menu_history.empty()) {
                                MenuState prev = menu_history.top();
                                menu_history.pop();
                                load_menu(prev.name, prev.index);
                                selected_index = prev.index;
                            }
                            break;
                        }
                        case SDLK_a: std::cout << "[A] short press\n"; break;
                        case SDLK_d: std::cout << "[D] short press\n"; break;
                        case SDLK_x: std::cout << "[X] short press\n"; break;
                    }
                }

                keyStates[key] = {};
            }
        }

        check_long_presses();

        SDL_SetRenderDrawColor(renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
        SDL_RenderClear(renderer);

        draw_buttons(renderer, font, selected_index);

        SDL_RenderPresent(renderer);

        //SDL_Delay(33); // Optional frame cap
    }

    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    TTF_Quit();
    SDL_Quit();
    return 0;
}
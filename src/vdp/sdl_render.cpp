#include "sdl_render.h"

#include <util/log.h>
#include <util/types.h>
#include <cassert>
#include "vdp_register.h"
#include "vdp.h"

#include <SDL2/SDL.h>

namespace Vdp {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* buffer = nullptr;

    constexpr int SCREEN_SCALE = 4;

    void render_init() {
        SDL_Init(SDL_INIT_VIDEO);
        window = SDL_CreateWindow("dgb sms",
                                  SDL_WINDOWPOS_UNDEFINED,
                                  SDL_WINDOWPOS_UNDEFINED,
                                  SMS_SCREEN_X * SCREEN_SCALE,
                                  SMS_SCREEN_Y * SCREEN_SCALE,
                                  SDL_WINDOW_SHOWN);
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        buffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, SMS_SCREEN_X, SMS_SCREEN_Y);
        SDL_RenderSetScale(renderer, SCREEN_SCALE, SCREEN_SCALE);
    }

    u32 fullcolor_screen[SMS_SCREEN_Y][SMS_SCREEN_X];

    u32 convert_color_channel(u8 channel) {
        switch (channel & 0b11) {
            case 0b00: return 0x00;
            case 0b01: return 0x0F;
            case 0b10: return 0xF0;
            case 0b11: return 0xFF;
        }
        logfatal("oop");
    }

    //  --BBGGRR
    inline u32 smscolor_to_sdlcolor(u8 color) {
        u32 red = convert_color_channel(color >> 0);
        u32 green = convert_color_channel(color >> 2);
        u32 blue = convert_color_channel(color >> 4);
        return (red << 24) | (green << 16) | (blue << 8);
    }

    void render_frame() {
        for (int x = 0; x < SMS_SCREEN_X; x++) {
            for (int y = 0; y < SMS_SCREEN_Y; y++) {
                fullcolor_screen[y][x] = smscolor_to_sdlcolor(screen[y][x]);
            }
        }
        SDL_UpdateTexture(buffer, nullptr, screen, SMS_SCREEN_X * 4);
        SDL_RenderCopy(renderer, buffer, nullptr, nullptr);
        SDL_RenderPresent(renderer);    SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    exit(0);
                case SDL_KEYDOWN:
                    switch (event.key.keysym.sym) {
                        case SDLK_ESCAPE:
                            exit(0);
                    }
                    break;
            }
        }
    }
}

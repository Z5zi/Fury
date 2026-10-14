#include "fury/input.hpp"
#include <SDL.h>
#include <stdexcept>
#include <iostream>
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(int, char**){try {
 require(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER|SDL_INIT_JOYSTICK)==0,"SDL init");
 SDL_VirtualJoystickDesc desc{};desc.version=SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
 desc.type=SDL_JOYSTICK_TYPE_GAMECONTROLLER;desc.naxes=SDL_CONTROLLER_AXIS_MAX;
 desc.nbuttons=SDL_CONTROLLER_BUTTON_MAX;desc.button_mask=(1u<<SDL_CONTROLLER_BUTTON_MAX)-1;
 desc.axis_mask=(1u<<SDL_CONTROLLER_AXIS_MAX)-1;desc.name="Hair settings input test";
 int index=SDL_JoystickAttachVirtualEx(&desc);require(index>=0,"Virtual device attach");
 auto* joystick=SDL_JoystickOpen(index);require(joystick,"Virtual joystick open");
 {
 fury::Input input;fury::InputState state;input.set_cinematic(true);
 auto poll=[&]{SDL_JoystickUpdate();state={};input.poll(state);};poll();
 SDL_JoystickSetVirtualButton(joystick,SDL_CONTROLLER_BUTTON_DPAD_UP,1);
 SDL_JoystickSetVirtualButton(joystick,SDL_CONTROLLER_BUTTON_DPAD_RIGHT,1);
 SDL_JoystickSetVirtualButton(joystick,SDL_CONTROLLER_BUTTON_A,1);
 SDL_JoystickSetVirtualButton(joystick,SDL_CONTROLLER_BUTTON_START,1);poll();
 require(state.menu_up&&state.menu_right&&state.menu_confirm&&state.gamepad_start_pressed,"Modal controls survive cinematic lock");
 require(!state.key_w&&!state.key_ctrl&&!state.interact_pressed&&state.move_forward==0,"Modal controls do not leak gameplay");
 poll();require(!state.gamepad_start_pressed&&state.menu_confirm,"Held Start does not repeat; held confirm remains available for app edges");
 input.set_text_entry(true);poll();require(!state.menu_confirm&&!state.menu_up,"Text-entry lock suppresses menu controls");
 input.set_text_entry(false);input.set_cinematic(false);poll();require(!state.interact_pressed,"Leaving modal while A held must not retrigger gameplay");
 SDL_JoystickSetVirtualButton(joystick,SDL_CONTROLLER_BUTTON_A,0);poll();
 SDL_JoystickSetVirtualButton(joystick,SDL_CONTROLLER_BUTTON_A,1);poll();require(state.interact_pressed,"Fresh gameplay A behavior unchanged");
 SDL_JoystickDetachVirtual(index);poll();require(!state.gamepad_connected&&!state.menu_confirm&&!state.menu_up,"Disconnect clears modal held inputs");
 }
 SDL_JoystickClose(joystick);SDL_Quit();
 std::cout<<"SDL virtual controller modal/gameplay/text-entry/hold/disconnect contract passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';SDL_Quit();return 1;}}

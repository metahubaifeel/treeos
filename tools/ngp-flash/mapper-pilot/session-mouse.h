/* Mouse actions and movement source are independent. Internal action IDs never
 * become Flash VKs. The persisted profile uses symbolic names, not these IDs. */
enum { MOUSE_LEFT_ACTION=241, MOUSE_RIGHT_ACTION=242 };
enum { PTR_NONE=0, PTR_LEFT, PTR_RIGHT, PTR_DPAD };
static const char *source_ids[]={"none","left_stick","right_stick","dpad"};
static const char *source_names[]={"无","左摇杆","右摇杆","十字键"};
static int mouse_src, draft_src;
static bool positioned;
static float mx,my;
static Sint16 axes[2],game_axes[2];static Uint32 mouse_tick,click_mask;
static unsigned dirs;
static int dir_bit(Uint8 b){switch(b){case SDL_CONTROLLER_BUTTON_DPAD_LEFT:return 1;case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:return 2;case SDL_CONTROLLER_BUTTON_DPAD_UP:return 4;case SDL_CONTROLLER_BUTTON_DPAD_DOWN:return 8;default:return 0;}}
static void mouse_stop_motion(void){axes[0]=axes[1]=0;dirs=0;mouse_tick=SDL_GetTicks();}
static bool mouse_position(void){SDL_Window *w=SDL_GL_GetCurrentWindow();if(!w)return false;if(!positioned){int width,height;SDL_GetWindowSize(w,&width,&height);mx=width/2;my=height/2;positioned=true;}return true;}
static void mouse_button(SDL_Event *e,int action,bool press){
    mouse_position();Uint8 button=action==MOUSE_LEFT_ACTION?SDL_BUTTON_LEFT:SDL_BUTTON_RIGHT;
    if(press)click_mask|=SDL_BUTTON(button);else click_mask&=~SDL_BUTTON(button);
    SDL_zero(*e);e->type=press?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;e->button.timestamp=SDL_GetTicks();SDL_Window *w=SDL_GL_GetCurrentWindow();e->button.windowID=w?SDL_GetWindowID(w):0;e->button.state=press?SDL_PRESSED:SDL_RELEASED;e->button.button=button;e->button.clicks=1;e->button.x=(int)mx;e->button.y=(int)my;
}
static bool mouse_move(SDL_Event *e){
    if(!mouse_src)return false;
    Uint32 now=SDL_GetTicks(),dt=now-mouse_tick;if(dt<12)return false;mouse_tick=now;if(dt>40)dt=40;
    int ax=axes[0],ay=axes[1];if(mouse_src==PTR_DPAD){ax=22000*(!!(dirs&2)-!!(dirs&1));ay=22000*(!!(dirs&8)-!!(dirs&4));}
    ax=abs(ax)<5500?0:ax;ay=abs(ay)<5500?0:ay;if((!ax&&!ay)||!mouse_position())return false;
    SDL_Window *w=SDL_GL_GetCurrentWindow();int width,height;SDL_GetWindowSize(w,&width,&height);if(width<1||height<1)return false;
    int oldx=(int)mx,oldy=(int)my;mx+=width*.00072f*dt*ax/32768.f;my+=width*.00072f*dt*ay/32768.f;
    mx=SDL_clamp(mx,0,width-1);my=SDL_clamp(my,0,height-1);if((int)mx==oldx&&(int)my==oldy)return false;
    SDL_WarpMouseInWindow(w,(int)mx,(int)my);SDL_zero(*e);e->type=SDL_MOUSEMOTION;e->motion.timestamp=now;e->motion.windowID=SDL_GetWindowID(w);e->motion.x=(int)mx;e->motion.y=(int)my;e->motion.xrel=(int)mx-oldx;e->motion.yrel=(int)my-oldy;e->motion.state=click_mask;return true;
}

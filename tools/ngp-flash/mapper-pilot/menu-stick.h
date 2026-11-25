/* Physical left stick only. Navigation pulses are private to the modal loop,
 * not pushed onto SDL's queue, not run through the game's remapping table. */
enum { STICK_CENTER=7000, STICK_PRESS=16000, STICK_DELAY=380, STICK_REPEAT=135 };
static struct { SDL_JoystickID joy; int x,y; bool wait_center; } raw_left;
struct StickNav { int dir; bool wait_center; Uint32 due; };
static bool centered(void){return abs(raw_left.x)<=STICK_CENTER && abs(raw_left.y)<=STICK_CENTER;}
static bool left_event(const SDL_Event *e){return e->type==SDL_CONTROLLERAXISMOTION && (e->caxis.axis==SDL_CONTROLLER_AXIS_LEFTX || e->caxis.axis==SDL_CONTROLLER_AXIS_LEFTY);}
static void track_left(const SDL_Event *e){
    if(raw_left.joy!=e->caxis.which){raw_left.x=raw_left.y=0;raw_left.wait_center=false;raw_left.joy=e->caxis.which;}
    if(e->caxis.axis==SDL_CONTROLLER_AXIS_LEFTX)raw_left.x=e->caxis.value;else raw_left.y=e->caxis.value;
}
static void arm_nav(struct StickNav *s){s->wait_center=!centered();s->dir=-1;s->due=0;}
static int nav_pulse(struct StickNav *s,Uint32 now){
    if(centered()){s->dir=-1;s->wait_center=false;return -1;}
    if(s->wait_center)return -1;
    int x=raw_left.x,y=raw_left.y,ax=abs(x),ay=abs(y);
    /* Latch one cardinal direction per tilt. No extra step for diagonal jitter. */
    if(s->dir<0){
        if(ax<STICK_PRESS && ay<STICK_PRESS)return -1;
        s->dir=ax>ay?(x<0?SDL_CONTROLLER_BUTTON_DPAD_LEFT:SDL_CONTROLLER_BUTTON_DPAD_RIGHT):(y<0?SDL_CONTROLLER_BUTTON_DPAD_UP:SDL_CONTROLLER_BUTTON_DPAD_DOWN);
        s->due=now+STICK_DELAY;return s->dir;
    }
    int drive=s->dir==SDL_CONTROLLER_BUTTON_DPAD_LEFT?-x:s->dir==SDL_CONTROLLER_BUTTON_DPAD_RIGHT?x:s->dir==SDL_CONTROLLER_BUTTON_DPAD_UP?-y:y;
    if(drive<STICK_PRESS || !SDL_TICKS_PASSED(now,s->due))return -1;
    s->due=now+STICK_REPEAT;return s->dir;
}

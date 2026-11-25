/* Isolated SDL event/persistence regression. No DRM, joystick, game or save. */
#define NGP_FREE_KEYS
#define NGP_SESSION_MOUSE
#include "quick-menu.c"
#include <assert.h>
static SDL_Event input;
static int have;
static int inject(SDL_Event *e){if(!have)return 0;*e=input;have=0;return 1;}
static void button(Uint8 b,bool on){assert(!have);SDL_zero(input);input.type=on?SDL_CONTROLLERBUTTONDOWN:SDL_CONTROLLERBUTTONUP;input.cbutton.button=b;have=1;}
static void axis(Uint8 a,int v){assert(!have);SDL_zero(input);input.type=SDL_CONTROLLERAXISMOTION;input.caxis.axis=a;input.caxis.value=v;have=1;}
static void edge(Uint32 type,SDL_Scancode sc){SDL_Event e;assert(SDL_PollEvent(&e)==1 && e.type==type && e.key.keysym.scancode==sc);}
static void none(void){SDL_Event e;assert(SDL_PollEvent(&e)==0);}
/* Exercised via the real SDL_PollEvent router, not a model of its rules. */
static void check_priority(int src){
 int draft[NKEY];memcpy(draft,idx,sizeof draft);draft_src=src;assert(save_layout(draft));
 button(SDL_CONTROLLER_BUTTON_DPAD_LEFT,true);
 if(src==PTR_DPAD){none();assert(dirs&1);}else edge(SDL_KEYDOWN,SDL_SCANCODE_A);
 axis(SDL_CONTROLLER_AXIS_LEFTX,-19000);
 if(src==PTR_LEFT){none();assert(axes[0]==-19000);}else{edge(SDL_KEYDOWN,SDL_SCANCODE_LEFT);edge(SDL_KEYDOWN,SDL_SCANCODE_J);}
 axis(SDL_CONTROLLER_AXIS_RIGHTX,-22000);
 if(src==PTR_RIGHT){none();assert(axes[0]==-22000 && !click_mask);}else{edge(SDL_KEYDOWN,SDL_SCANCODE_Z);SDL_Event click;assert(SDL_PollEvent(&click)==1 && click.type==SDL_MOUSEBUTTONDOWN);}
 /* Right-stick PRESS is distinct even when the tilt drives the cursor. */
 button(SDL_CONTROLLER_BUTTON_RIGHTSTICK,true);edge(SDL_KEYDOWN,SDL_SCANCODE_U);
 button(SDL_CONTROLLER_BUTTON_RIGHTSTICK,false);edge(SDL_KEYUP,SDL_SCANCODE_U);
 button(SDL_CONTROLLER_BUTTON_DPAD_LEFT,false);if(src!=PTR_DPAD)edge(SDL_KEYUP,SDL_SCANCODE_A);none();assert(!dirs);
 axis(SDL_CONTROLLER_AXIS_LEFTX,0);if(src!=PTR_LEFT){edge(SDL_KEYUP,SDL_SCANCODE_LEFT);edge(SDL_KEYUP,SDL_SCANCODE_J);}none();
 axis(SDL_CONTROLLER_AXIS_RIGHTX,0);if(src!=PTR_RIGHT){edge(SDL_KEYUP,SDL_SCANCODE_Z);SDL_Event click;assert(SDL_PollEvent(&click)==1 && click.type==SDL_MOUSEBUTTONUP);}none();assert(!axes[0] && !click_mask);
}
int main(int argc,char **argv){
 assert(argc==2 && free_profile());poll_next=inject;
 if(!strcmp(argv[1],"save")){
  int draft[NKEY];memcpy(draft,idx,sizeof draft);draft_src=PTR_NONE;
  for(int i=PAD_ROW;i<NKEY;i++)assert(available(i,'A') && available(i,37) && !available(i,NATIVE_SYSTEM));
  draft[PAD_ROW+2]='A';draft[STICK_ROW+2]=edit_member(37,'J',false);draft[PAD_ROW]=draft[STICK_ROW]='W';draft[0]='J';
  draft[RIGHT_ROW+2]=edit_member('Z',MOUSE_LEFT_ACTION,false);draft[9]='U';
  assert(save_layout(draft));
 }
 assert(idx[PAD_ROW+2]=='A' && idx[STICK_ROW+2]==(37|('J'<<8)));
 button(SDL_CONTROLLER_BUTTON_DPAD_LEFT,true);edge(SDL_KEYDOWN,SDL_SCANCODE_A);
 axis(SDL_CONTROLLER_AXIS_LEFTX,-18000);edge(SDL_KEYDOWN,SDL_SCANCODE_LEFT);edge(SDL_KEYDOWN,SDL_SCANCODE_J);
 button(slots[0],true);none(); /* same J as stick */
 axis(SDL_CONTROLLER_AXIS_LEFTY,-16000);edge(SDL_KEYDOWN,SDL_SCANCODE_W); /* diagonal */
 button(SDL_CONTROLLER_BUTTON_DPAD_UP,true);none();
 axis(SDL_CONTROLLER_AXIS_LEFTX,-7600);none(); /* hysteresis */
 axis(SDL_CONTROLLER_AXIS_LEFTX,0);edge(SDL_KEYUP,SDL_SCANCODE_LEFT);none();
 axis(SDL_CONTROLLER_AXIS_LEFTY,0);none();button(SDL_CONTROLLER_BUTTON_DPAD_UP,false);edge(SDL_KEYUP,SDL_SCANCODE_W);
 button(slots[0],false);edge(SDL_KEYUP,SDL_SCANCODE_J);button(SDL_CONTROLLER_BUTTON_DPAD_LEFT,false);edge(SDL_KEYUP,SDL_SCANCODE_A);none();
 for(int src=PTR_NONE;src<=PTR_DPAD;src++)check_priority(src);
 check_priority(PTR_NONE); /* pointer removed: all saved mappings resume */
 puts("PASS: both sticks + pad independent, pointer priority x 4, R3 unaffected, shared release");return 0;
}

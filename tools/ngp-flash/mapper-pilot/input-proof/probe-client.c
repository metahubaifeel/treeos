/* Scoped to the offscreen input-meter SWF in a private directory. Inject
 * into this test process's SDL return values ONLY; no uinput or evdev I/O.
 * Do not load in a user's game. Read back the SWF's centre pixel. */
#define _GNU_SOURCE
#include <SDL.h>
#include <GLES2/gl2.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int phase=-1, sent, frames, failed;
struct step { int type, key, mask; const char *name; };
static const struct step keyboard_steps[]={
    {SDL_CONTROLLERBUTTONDOWN,SDL_CONTROLLER_BUTTON_B,1,"native J down"},
    {SDL_CONTROLLERBUTTONUP,SDL_CONTROLLER_BUTTON_B,0,"native J up"},
    {SDL_KEYDOWN,SDLK_o,2,"keyboard O down"},
    {SDL_KEYDOWN,SDLK_l,6,"keyboard O+L"},
    {SDL_CONTROLLERBUTTONDOWN,SDL_CONTROLLER_BUTTON_B,7,"native J + keyboard O+L"},
    {SDL_KEYUP,SDLK_o,5,"release O only"},
    {SDL_CONTROLLERBUTTONUP,SDL_CONTROLLER_BUTTON_B,4,"release J only"},
    {SDL_KEYUP,SDLK_l,0,"release L"},
};
/* Probe a candidate for the independent R1 slot with all ordinary controls
 * still available. Test physical edges later, after finding a usable route. */
static struct step pad_steps[]={
 {SDL_CONTROLLERBUTTONDOWN,SDL_CONTROLLER_BUTTON_LEFTSHOULDER,2,"L1/O"},
 {SDL_CONTROLLERAXISMOTION,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,6,"L1/O + candidate/L"},
 {SDL_CONTROLLERBUTTONDOWN,SDL_CONTROLLER_BUTTON_B,7,"J+O+L"},
 {SDL_CONTROLLERBUTTONUP,SDL_CONTROLLER_BUTTON_LEFTSHOULDER,5,"release O only"},
 {SDL_CONTROLLERBUTTONUP,SDL_CONTROLLER_BUTTON_B,4,"release J only"},
 {SDL_CONTROLLERAXISMOTION,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,0,"release L"}
};
static const struct step l1_steps[]={
 {SDL_CONTROLLERBUTTONDOWN,SDL_CONTROLLER_BUTTON_LEFTSHOULDER,2,"L1/O down"},
 {SDL_CONTROLLERBUTTONDOWN,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,3,"A/J and L1/O together"},
 {SDL_CONTROLLERBUTTONUP,SDL_CONTROLLER_BUTTON_LEFTSHOULDER,1,"release L1 only"},
 {SDL_CONTROLLERBUTTONUP,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,0,"release A"}
};
/* Same logical row as 199408, plus physical b5 mapped to PADDLE1. */
static const struct step r1_steps[]={
 {SDL_CONTROLLERBUTTONDOWN,SDL_CONTROLLER_BUTTON_PADDLE1,4,"R1/L alone"},
 {SDL_CONTROLLERBUTTONDOWN,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,5,"R1/L + A/J"},
 {SDL_CONTROLLERBUTTONDOWN,SDL_CONTROLLER_BUTTON_LEFTSHOULDER,7,"R1/L + A/J + L1/O"},
 {SDL_CONTROLLERBUTTONUP,SDL_CONTROLLER_BUTTON_PADDLE1,3,"release only R1"},
 {SDL_CONTROLLERBUTTONUP,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,2,"release only A"},
 {SDL_CONTROLLERBUTTONUP,SDL_CONTROLLER_BUTTON_LEFTSHOULDER,0,"release L1"},
 {SDL_CONTROLLERBUTTONDOWN,SDL_CONTROLLER_BUTTON_PADDLE1,4,"tap R1 again"},
 {SDL_CONTROLLERBUTTONUP,SDL_CONTROLLER_BUTTON_PADDLE1,0,"no stuck key"}
};
#define D SDL_CONTROLLERBUTTONDOWN
#define U SDL_CONTROLLERBUTTONUP
#define L2 SDL_CONTROLLER_BUTTON_PADDLE2
#define R2 SDL_CONTROLLER_BUTTON_PADDLE3
#define L3 SDL_CONTROLLER_BUTTON_LEFTSTICK
#define R3 SDL_CONTROLLER_BUTTON_RIGHTSTICK
#define MENU_EDIT (-42)
static const struct step free_steps[]={
 {D,L2,0,"L2 unbound"},{U,L2,0,"release unbound L2"},{MENU_EDIT,0,0,"M edit four bindings and save"},
 {D,L2,1,"live L2 space"},{D,R2,3,"add R2 left"},{D,L3,7,"add L3 up"},{U,R2,5,"release R2 only"},{U,L2,4,"release L2 only"},{U,L3,0,"release L3 only"}
};
#define PHYS_A SDL_CONTROLLER_BUTTON_RIGHTSHOULDER
static const struct step overlay_steps[]={
 {MENU_EDIT,3,0,"select A, navigate to mouse left, A adds, B finishes, save"},
 {D,PHYS_A,3,"A alone holds keyboard A and mouse left"},{D,R2,3,"R2 shares mouse left"},
 {U,PHYS_A,2,"only R2 mouse left remains"},{U,R2,0,"last mouse release"},
 {D,SDL_CONTROLLER_BUTTON_X,1,"X alone holds keyboard A"},{D,PHYS_A,3,"add both"},
 {U,PHYS_A,1,"only X keyboard A remains"},{U,SDL_CONTROLLER_BUTTON_X,0,"last key release"},
 {D,PHYS_A,3,"both again"},{MENU_EDIT,4,0,"released both inside M; same game resumes idle"}
};
static const struct step multi_steps[]={
 {MENU_EDIT,7,0,"stay in picker for J, space and left click; B finishes"},
 {D,PHYS_A,7,"A holds J + space + left mouse"},{D,L2,7,"L2 shares click"},{U,PHYS_A,4,"release J and space, keep shared click"},{U,L2,0,"last click up"}
};
static const struct step drag_steps[]={
 {D,L2,1,"left held"},{D,R3,1,"second left owner"},{U,L2,1,"still dragging with R3"},
 {D,SDL_CONTROLLER_BUTTON_DPAD_RIGHT,3,"dragged movie clip past x=95"},{U,SDL_CONTROLLER_BUTTON_DPAD_RIGHT,3,"stop moving, still held"},
 {U,R3,2,"drop movie clip"},{D,SDL_CONTROLLER_BUTTON_DPAD_LEFT,6,"cursor back past x=70, dropped clip stays"},{U,SDL_CONTROLLER_BUTTON_DPAD_LEFT,6,"release dpad"}
};
#define SEL SDL_CONTROLLER_BUTTON_BACK
#define STA SDL_CONTROLLER_BUTTON_START
static int system_mode;
static const struct step system_steps[]={
 {MENU_EDIT,5,0,"use M to set Select L and Start M and save"},
 {D,SEL,0,"Select alone waits"},{U,SEL,1,"single Select release pulses L"},{SDL_USEREVENT,0,0,"L went up"},
 {D,STA,0,"Start alone waits"},{U,STA,2,"single Start release pulses M"},{SDL_USEREVENT,0,0,"M went up"},
 {D,SEL,0,"Select before chord"},{D,STA,0,"Select then Start suppressed"},{U,SEL,0,"no tap from first release"},{U,STA,0,"no tap from last release"},
 {D,STA,0,"Start before chord"},{D,SEL,0,"Start then Select suppressed"},{U,SEL,0,"no Select action"},{U,STA,0,"no Start action"},
 {D,SEL,0,"system key held entering menu"},{MENU_EDIT,6,0,"release inside M cancelled"},{D,SEL,0,"Select usable after menu"},{U,SEL,1,"Select tap still works"},{SDL_USEREVENT,0,0,"no stuck L"}
};
static const struct step native_steps[]={
 {D,SDL_CONTROLLER_BUTTON_DPAD_UP,1,"WASD up W"},{D,SDL_CONTROLLER_BUTTON_START,3,"Start Enter while W held"},{D,SDL_CONTROLLER_BUTTON_BACK,7,"logical Select Esc"},{U,SDL_CONTROLLER_BUTTON_START,5,"release Enter only"},{U,SDL_CONTROLLER_BUTTON_BACK,1,"release Esc only"},{U,SDL_CONTROLLER_BUTTON_DPAD_UP,0,"release W"}
};
static const struct step bxy_steps[]={
 {D,SDL_CONTROLLER_BUTTON_B,1,"B K"},{D,SDL_CONTROLLER_BUTTON_X,3,"X U"},{D,SDL_CONTROLLER_BUTTON_Y,7,"Y I"},{U,SDL_CONTROLLER_BUTTON_X,5,"release U"},{U,SDL_CONTROLLER_BUTTON_B,4,"release K"},{U,SDL_CONTROLLER_BUTTON_Y,0,"release I"}
};
static const struct step grid_steps[]={
 {MENU_EDIT,1,0,"choose J on QWERTY for R2 and R3 without changing A"},
 {D,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,1,"A J"},{D,R2,1,"add R2 J"},{D,R3,1,"add R3 J"},
 {U,R2,1,"release R2 still J"},{U,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,1,"release A still R3 J"},{U,R3,0,"last release clears J"},
 {MENU_EDIT,2,0,"B cancels key choice; M discards draft from keyboard"},{D,R3,1,"R3 still J after discard"},{U,R3,0,"release R3"}
};
static const struct step untouched_steps[]={
 {D,R2,0,"second game R2 unbound"},{D,R3,0,"second game R3 unbound"},{D,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,1,"second game A still J"},
 {U,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,0,"no other J owners"},{U,R2,0,"R2 up"},{U,R3,0,"R3 up"}
};
static const struct step shared_steps[]={
 {D,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,1,"A/J"},{D,R2,1,"optional R2 also J"},{U,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,1,"release A while R2 still owns J"},{U,R2,0,"last owner releases J"}
};
static const struct step reload_steps[]={
 {D,R2,1,"reload R2 left"},{D,L3,3,"reload L3 up"},{D,R3,7,"reload R3 right"},{U,L3,5,"release only L3"},{U,R3,1,"release only R3"},{U,R2,0,"release R2"}
};
static void tap_menu(Uint8 b){SDL_Event e={0};e.type=D;e.cbutton.button=b;if(SDL_PushEvent(&e)!=1)_exit(8);e.type=U;if(SDL_PushEvent(&e)!=1)_exit(8);}
#define CHOOSE SDL_CONTROLLER_BUTTON_RIGHTSHOULDER
#define BACK SDL_CONTROLLER_BUTTON_B
#define RIGHT SDL_CONTROLLER_BUTTON_DPAD_RIGHT
#define UP SDL_CONTROLLER_BUTTON_DPAD_UP
#define DOWN SDL_CONTROLLER_BUTTON_DPAD_DOWN
static void menu_script(int id){
 if(id==7){tap_menu(SDL_CONTROLLER_BUTTON_GUIDE);tap_menu(CHOOSE);tap_menu(CHOOSE);tap_menu(CHOOSE);tap_menu(DOWN);tap_menu(DOWN);tap_menu(SDL_CONTROLLER_BUTTON_DPAD_LEFT);tap_menu(CHOOSE);tap_menu(DOWN);tap_menu(CHOOSE);tap_menu(BACK);tap_menu(BACK);tap_menu(CHOOSE);return;}
 if(id==5){tap_menu(SDL_CONTROLLER_BUTTON_GUIDE);tap_menu(CHOOSE);tap_menu(UP);tap_menu(UP);tap_menu(CHOOSE);tap_menu(UP);tap_menu(CHOOSE);tap_menu(BACK);tap_menu(DOWN);tap_menu(CHOOSE);tap_menu(SDL_CONTROLLER_BUTTON_DPAD_LEFT);tap_menu(CHOOSE);tap_menu(BACK);tap_menu(BACK);tap_menu(CHOOSE);return;}
 if(id==6){tap_menu(SDL_CONTROLLER_BUTTON_GUIDE);SDL_Event e={0};e.type=U;e.cbutton.button=SEL;if(SDL_PushEvent(&e)!=1)_exit(8);tap_menu(SDL_CONTROLLER_BUTTON_GUIDE);return;}
 if(id==3){tap_menu(SDL_CONTROLLER_BUTTON_GUIDE);tap_menu(CHOOSE);tap_menu(CHOOSE);for(int i=0;i<3;i++)tap_menu(DOWN);tap_menu(CHOOSE);tap_menu(BACK);tap_menu(BACK);tap_menu(CHOOSE);return;}
 if(id==4){tap_menu(SDL_CONTROLLER_BUTTON_GUIDE);SDL_Event e={0};e.type=U;e.cbutton.button=PHYS_A;if(SDL_PushEvent(&e)!=1)_exit(8);tap_menu(SDL_CONTROLLER_BUTTON_GUIDE);return;}
 if(id){
    tap_menu(SDL_CONTROLLER_BUTTON_GUIDE);tap_menu(CHOOSE);for(int i=0;i<7;i++)tap_menu(DOWN);
    if(id==2){tap_menu(CHOOSE);tap_menu(RIGHT);tap_menu(BACK);tap_menu(DOWN);tap_menu(DOWN);tap_menu(CHOOSE);tap_menu(RIGHT);tap_menu(CHOOSE);tap_menu(CHOOSE);tap_menu(SDL_CONTROLLER_BUTTON_GUIDE);return;}
    for(int i=0;i<2;i++){tap_menu(CHOOSE);tap_menu(UP);for(int j=0;j<6;j++)tap_menu(RIGHT);tap_menu(UP);tap_menu(CHOOSE);tap_menu(BACK);if(!i){tap_menu(DOWN);tap_menu(DOWN);}}
    tap_menu(BACK);tap_menu(CHOOSE);return;
 }
 tap_menu(SDL_CONTROLLER_BUTTON_GUIDE);tap_menu(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
 for(int i=0;i<6;i++)tap_menu(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
 const int right[]={1,2,3,5};
 for(int i=0;i<4;i++){tap_menu(CHOOSE);for(int j=0;j<right[i];j++)tap_menu(RIGHT);tap_menu(CHOOSE);tap_menu(BACK);if(i<3)tap_menu(DOWN);}
 tap_menu(BACK);tap_menu(CHOOSE);
}
static const struct step *sequence=keyboard_steps;
static int count=sizeof(keyboard_steps)/sizeof(keyboard_steps[0]);
__attribute__((constructor)) static void mode(void){
 const char *test=getenv("PROBE_PAD");if(!test)return;
 if(!strcmp(test,"multi") || !strcmp(test,"multi-reload")){int skip=!strcmp(test,"multi-reload");sequence=multi_steps+skip;count=sizeof(multi_steps)/sizeof(multi_steps[0])-skip;return;}
 if(!strcmp(test,"overlay") || !strcmp(test,"overlay-reload")){int skip=!strcmp(test,"overlay-reload");sequence=overlay_steps+skip;count=sizeof(overlay_steps)/sizeof(overlay_steps[0])-skip;return;}
 if(!strcmp(test,"system") || !strcmp(test,"system-reload")){int skip=!strcmp(test,"system-reload");system_mode=1;sequence=system_steps+skip;count=sizeof(system_steps)/sizeof(system_steps[0])-skip;return;}
 if(!strcmp(test,"drag")){sequence=drag_steps;count=sizeof(drag_steps)/sizeof(drag_steps[0]);return;}
 if(!strcmp(test,"native")){sequence=native_steps;count=sizeof(native_steps)/sizeof(native_steps[0]);return;}
 if(!strcmp(test,"bxy")){sequence=bxy_steps;count=sizeof(bxy_steps)/sizeof(bxy_steps[0]);return;}
 if(!strcmp(test,"grid")){sequence=grid_steps;count=sizeof(grid_steps)/sizeof(grid_steps[0]);return;}
 if(!strcmp(test,"untouched")){sequence=untouched_steps;count=sizeof(untouched_steps)/sizeof(untouched_steps[0]);return;}
 if(!strcmp(test,"shared")){sequence=shared_steps;count=sizeof(shared_steps)/sizeof(shared_steps[0]);return;}
 if(!strcmp(test,"live")){sequence=free_steps;count=sizeof(free_steps)/sizeof(free_steps[0]);return;}
 if(!strcmp(test,"reload")){sequence=reload_steps;count=sizeof(reload_steps)/sizeof(reload_steps[0]);return;}
 if(!strcmp(test,"l1")){sequence=l1_steps;count=sizeof(l1_steps)/sizeof(l1_steps[0]);return;}
 if(!strcmp(test,"r1")){sequence=r1_steps;count=sizeof(r1_steps)/sizeof(r1_steps[0]);return;}
 sequence=pad_steps;count=sizeof(pad_steps)/sizeof(pad_steps[0]);
 if(strcmp(test,"axis")){
    pad_steps[1].type=SDL_CONTROLLERBUTTONDOWN;pad_steps[5].type=SDL_CONTROLLERBUTTONUP;
    pad_steps[1].key=pad_steps[5].key=atoi(test);
 }
}
int SDL_NumJoysticks(void){return 0;}
SDL_Window *SDL_CreateWindow(const char *title,int x,int y,int w,int h,Uint32 flags){
    (void)w;(void)h;(void)flags;
    SDL_Window *(*next)(const char*,int,int,int,int,Uint32)=dlsym(RTLD_NEXT,"SDL_CreateWindow");
    fprintf(stderr,"meter requested window %dx%d flags=%u\n",w,h,flags);
    return next(title,x,y,w>0?w:640,h>0?h:480,SDL_WINDOW_OPENGL);
}
int SDL_PollEvent(SDL_Event *e){
    static int (*next)(SDL_Event*);if(!next)next=dlsym(RTLD_NEXT,"SDL_PollEvent");
    if(!e || phase<0 || sent)return next(e);
    const struct step *s=sequence+phase;
    if(s->type==MENU_EDIT){sent=1;menu_script(s->key);return next(e);}
    SDL_zero(*e);e->type=s->type;e->common.timestamp=SDL_GetTicks();
    if(s->type==SDL_KEYDOWN || s->type==SDL_KEYUP){
        e->key.windowID=SDL_GetWindowID(SDL_GL_GetCurrentWindow());
        e->key.state=s->type==SDL_KEYDOWN?SDL_PRESSED:SDL_RELEASED;
        e->key.keysym.sym=s->key;e->key.keysym.scancode=SDL_GetScancodeFromKey(s->key);
    }else if(s->type==SDL_CONTROLLERAXISMOTION){
        e->caxis.axis=s->key;e->caxis.value=s->mask?32767:0;
    }else{
        e->cbutton.state=s->type==SDL_CONTROLLERBUTTONDOWN?SDL_PRESSED:SDL_RELEASED;
        e->cbutton.button=s->key;
    }
    sent=1;return 1;
}
void SDL_GL_SwapWindow(SDL_Window *window){
    static void (*next)(SDL_Window*);if(!next)next=dlsym(RTLD_NEXT,"SDL_GL_SwapWindow");
    static SDL_GLContext game;if(!game)game=SDL_GL_GetCurrentContext();
    if(SDL_GL_GetCurrentContext()!=game){next(window);return;}
    int w,h;Uint8 pix[4]={0};SDL_GL_GetDrawableSize(window,&w,&h);
    /* Uniform meter fill; avoid the production cursor overlay at centre. */
    glReadPixels(3*w/8,3*h/8,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pix);
    int mask=(pix[0]>110) + 2*(pix[1]>110) + 4*(pix[2]>110);
    int valid=1;for(int i=0;i<3;i++) if(abs((int)pix[i]-((mask&(1<<i))?192:32))>6)valid=0;
    frames++;
    if(frames==1 || frames==24){GLint v[4]={0},fb=-1; glGetIntegerv(GL_VIEWPORT,v);glGetIntegerv(GL_FRAMEBUFFER_BINDING,&fb);fprintf(stderr,"meter phase=%d pixel=%u,%u,%u drawable=%dx%d viewport=%d,%d,%d,%d fb=%d gl=%x\n",phase,pix[0],pix[1],pix[2],w,h,v[0],v[1],v[2],v[3],fb,glGetError());}
    if(phase<0){if(valid && mask==0){phase=0;frames=0;}else if(frames>180){fprintf(stderr,"no SWF heartbeat\n");_exit(3);}}
    else if(sent && frames>=(system_mode && sequence[phase].mask?1:4) && ((valid && mask==sequence[phase].mask)||frames>=20)){
        int ok=valid && mask==sequence[phase].mask;
        fprintf(stderr,"%s %s actual=%d rgb=%u,%u,%u\n",ok?"PASS":"FAIL",sequence[phase].name,mask,pix[0],pix[1],pix[2]);
        failed+=!ok;phase++;sent=frames=0;
        if(phase==count)_exit(failed?4:0);
    }
    next(window);
}

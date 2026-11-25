/* Private SDL queue; never reads or writes evdev. Load the production menu with LD_PRELOAD. */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s (%s)\n",__LINE__,#x,SDL_GetError());exit(3);}}while(0)
#define A SDL_CONTROLLER_BUTTON_RIGHTSHOULDER
#define B SDL_CONTROLLER_BUTTON_B
#define X SDL_CONTROLLER_BUTTON_X
#define M SDL_CONTROLLER_BUTTON_GUIDE
#define DN SDL_CONTROLLER_BUTTON_DPAD_DOWN
#define RT SDL_CONTROLLER_BUTTON_DPAD_RIGHT
#define UP SDL_CONTROLLER_BUTTON_DPAD_UP
static void btn(int b,int press){SDL_Event e={0};e.type=press?SDL_CONTROLLERBUTTONDOWN:SDL_CONTROLLERBUTTONUP;e.cbutton.button=b;e.cbutton.which=1;CHECK(SDL_PushEvent(&e)==1);}
static int analog_nav;
static void axis(int,int);
static void tap(int b){
 if(analog_nav && (b==DN || b==UP || b==RT || b==SDL_CONTROLLER_BUTTON_DPAD_LEFT)){
    int a=(b==UP||b==DN)?SDL_CONTROLLER_AXIS_LEFTY:SDL_CONTROLLER_AXIS_LEFTX;int sign=(b==UP||b==SDL_CONTROLLER_BUTTON_DPAD_LEFT)?-1:1;
    axis(a,3500*sign);axis(a,24000*sign);axis(a,22500*sign);axis(a,0);return;
 }
 btn(b,1);btn(b,0);
}
static void axis(int a,int val){SDL_Event e={0};e.type=SDL_CONTROLLERAXISMOTION;e.caxis.axis=a;e.caxis.value=val;e.caxis.which=1;CHECK(SDL_PushEvent(&e)==1);}
static int ldn,lup,rdn,rup,drags,abtn,axy,keyevents;static Uint32 dragmask;
static void drain(void){SDL_Event e;for(int n=0;n<500 && SDL_PollEvent(&e);n++){
 if(e.type==SDL_MOUSEBUTTONDOWN){ldn+=e.button.button==1;rdn+=e.button.button==3;}
 if(e.type==SDL_MOUSEBUTTONUP){lup+=e.button.button==1;rup+=e.button.button==3;}
 if(e.type==SDL_MOUSEMOTION && e.motion.state){drags++;dragmask=e.motion.state;}
 if(e.type==SDL_CONTROLLERAXISMOTION && e.caxis.value)axy++;
 if(e.type==SDL_CONTROLLERBUTTONDOWN || e.type==SDL_CONTROLLERBUTTONUP)abtn++;
 if(e.type==SDL_KEYDOWN || e.type==SDL_KEYUP)keyevents++;
 }}
static void select_source(int old,int next){tap(M);axis(SDL_CONTROLLER_AXIS_LEFTX,0);axis(SDL_CONTROLLER_AXIS_LEFTY,0);tap(DN);tap(A);for(int i=old;i<next;i++)tap(DN);for(int i=next;i<old;i++)tap(UP);tap(A);tap(A);drain();}
static void drive(int source,int on){if(source==3)btn(RT,on);else axis(source==1?SDL_CONTROLLER_AXIS_LEFTX:SDL_CONTROLLER_AXIS_RIGHTX,on?26000:0);drain();SDL_Delay(25);drain();}
static void held_test(int source){int l0=ldn,l1=lup,r0=rdn,r1=rup,n=drags,buttons=abtn,ax=axy;
 btn(A,1);btn(X,1);btn(B,1);drain();CHECK(ldn==l0+1 && rdn==r0+1);
 drive(source,1);CHECK(drags>n && dragmask==(SDL_BUTTON_LMASK|SDL_BUTTON_RMASK));btn(A,0);drain();CHECK(lup==l1);
 btn(B,0);drain();CHECK(rup==r1+1);SDL_Delay(24);drain();CHECK(dragmask==SDL_BUTTON_LMASK);drive(source,0);
 btn(X,0);drain();CHECK(lup==l1+1 && abtn==buttons && ax==axy);}
int main(int argc,char **argv){
 CHECK(argc==2);CHECK(SDL_Init(SDL_INIT_VIDEO|(!strcmp(argv[1],"direction-priority")?SDL_INIT_EVENTS:SDL_INIT_GAMECONTROLLER))==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
 SDL_Window *w=SDL_CreateWindow("boundaries",0,0,640,480,SDL_WINDOW_OPENGL);CHECK(w);SDL_GLContext ctx=SDL_GL_CreateContext(w);CHECK(ctx);drain();
 analog_nav=strstr(argv[1],"stick")!=NULL;
 if(!strcmp(argv[1],"direction-priority")){
   /* RON/profile fixture: all three rightward directions mapped to J;
    * A/X share left click, B right click. Change sources via the real M menu. */
   int prev=0;const int sources[]={1,2,3,0};
   for(unsigned i=0;i<sizeof sources/sizeof *sources;i++){
     int src=sources[i];select_source(prev,src);prev=src;
     for(int group=1;group<=3;group++){
       int before=keyevents;
       if(src==group){held_test(group);CHECK(keyevents==before);}
       else {drive(group,1);drive(group,0);CHECK(keyevents==before+2);}
     }
   }
   CHECK(SDL_GL_GetCurrentContext()==ctx);puts("PASS live menu switches, actual pointer drag from each source, mapped directions suppressed only for owner, all restored on None");SDL_Quit();return 0;
 }
 if(!strncmp(argv[1],"multi",5)){
   if(!strcmp(argv[1],"multi")){tap(M);tap(A);tap(A);tap(A);tap(DN);tap(DN);tap(SDL_CONTROLLER_BUTTON_DPAD_LEFT);tap(A);tap(DN);tap(A);tap(B);tap(B);tap(A);drain();}
   btn(A,1);drain();CHECK(keyevents==2 && ldn==1 && !lup);btn(SDL_CONTROLLER_BUTTON_PADDLE2,1);drain();btn(A,0);drain();CHECK(keyevents==4 && !lup);btn(SDL_CONTROLLER_BUTTON_PADDLE2,0);drain();CHECK(lup==1);
   /* Remove a member in the picker, discard through M, not B. */
   tap(M);tap(A);tap(A);tap(X);tap(M);drain();tap(A);drain();CHECK(keyevents==8 && ldn==2 && lup==2);puts("PASS persistent picker triple, shared release and discard");SDL_Quit();return 0;
 }
 if(!strcmp(argv[1],"system-mixed")){
   /* select_tap=mouse_left, default Start. Count every edge, not just the final state. */
   int s=SDL_CONTROLLER_BUTTON_BACK,t=SDL_CONTROLLER_BUTTON_START;
   for(int i=0;i<2;i++){int first=i?s:t,other=i?t:s;btn(first,1);drain();SDL_Delay(90);drain();btn(other,1);btn(first,0);btn(other,0);drain();CHECK(!abtn && !ldn && !lup);}
   tap(t);drain();CHECK(abtn==1);SDL_GL_SwapWindow(w);SDL_Delay(75);drain();CHECK(abtn==2);
   btn(s,1);tap(M);tap(M);btn(s,0);drain();CHECK(!ldn && !lup);
   tap(s);drain();CHECK(ldn==1 && !lup);SDL_GL_SwapWindow(w);SDL_Delay(75);drain();CHECK(lup==1 && !keyevents);
   puts("PASS mixed native/custom taps, slow chords both orders zero leaked edges, held-across-M cancelled");SDL_Quit();return 0;
 }
 if(!strncmp(argv[1],"overlay",7)){
   /* arrows-six A -> keyboard A. Add right mouse, explicitly finish the picker. */
   if(!strcmp(argv[1],"overlay")){tap(M);tap(A);tap(A);tap(DN);tap(DN);tap(DN);tap(RT);tap(A);tap(B);tap(B);tap(A);drain();}
   btn(A,1);drain();CHECK(keyevents==1 && rdn==1 && rup==0);btn(B,1);drain();CHECK(rdn==1);
   tap(M);btn(A,0);tap(M);drain();CHECK(keyevents==2 && rup==0);
   btn(B,0);drain();CHECK(rup==1);
   /* Edit to keyboard-only but discard. Next A still holds both. */
   tap(M);tap(A);tap(A);tap(A);tap(M);drain();tap(A);drain();CHECK(keyevents==4 && rdn==2 && rup==2);
   /* X removes just the right-click component; B finishes the picker. */
   if(!strcmp(argv[1],"overlay-reload")){tap(M);tap(A);tap(A);tap(DN);tap(DN);tap(DN);tap(RT);tap(X);tap(B);tap(B);tap(A);drain();tap(A);drain();CHECK(keyevents==6 && rdn==2 && rup==2);}
   CHECK(SDL_GL_GetCurrentContext()==ctx);puts("PASS explicit overlay, independent shared mouse release, menu-held release, discard, single-action replace");SDL_Quit();return 0;
 }
 if(!strncmp(argv[1],"fresh",5)){
   /* Source defaults to none. Start with unbound A/B/X. Select targets using ONLY physical menu controls. */
   tap(M);tap(A);for(int i=0;i<3;i++){tap(A);tap(DN);if(i==1)tap(RT);tap(A);tap(B);if(i<2)tap(DN);}tap(B);tap(A);drain();
   tap(A);tap(B);drain();CHECK(ldn==1 && lup==1 && rdn==1 && rup==1);
   int a=axy;axis(SDL_CONTROLLER_AXIS_LEFTX,22000);drain();CHECK(axy==a+1);select_source(0,1);held_test(1);
   select_source(1,2);held_test(2);select_source(2,3);held_test(3);
   /* Cancel a source edit via M: assignments and direction owner must stay unchanged. */
   tap(M);tap(DN);tap(A);tap(DN);tap(A);tap(M);drain();held_test(3);
 }else{held_test(3);select_source(3,0);int a=abtn;btn(RT,1);btn(RT,0);drain();CHECK(abtn==a+2);
 if(analog_nav){int ax=axy;tap(M);axis(SDL_CONTROLLER_AXIS_LEFTY,-24000);tap(M);axis(SDL_CONTROLLER_AXIS_LEFTY,-22000);axis(SDL_CONTROLLER_AXIS_LEFTY,0);drain();CHECK(axy==ax);axis(SDL_CONTROLLER_AXIS_LEFTY,24000);axis(SDL_CONTROLLER_AXIS_LEFTY,0);drain();CHECK(axy==ax+1);}}
 CHECK(keyevents==0 && SDL_GL_GetCurrentContext()==ctx);puts("PASS menu uses physical A/B/dpad; independent L/R, shared hold, drag mask, source ownership and context");SDL_Quit();return 0;
}

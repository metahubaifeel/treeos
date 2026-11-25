/* NGP 199408 pilot only. Ruffle polls SDL on its render thread. Borrow that
 * thread and window, not DRM from a second process. Closing returns from the
 * poll hook; there is no exec, relaunch, SIGSTOP or save-state trick here. */
#define _GNU_SOURCE
#include <SDL.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>

/* Public SDL_ttf ABI; dArkOS provides the runtime but not its dev headers. */
typedef struct _TTF_Font TTF_Font;
extern int TTF_Init(void);
extern void TTF_Quit(void);
extern TTF_Font *TTF_OpenFont(const char *,int);
extern void TTF_CloseFont(TTF_Font *);
extern int TTF_SizeUTF8(TTF_Font *,const char *,int *,int *);
extern SDL_Surface *TTF_RenderUTF8_Blended(TTF_Font *,const char *,SDL_Color);

static int (*poll_next)(SDL_Event *);
static SDL_AudioDeviceID audio;
SDL_AudioDeviceID SDL_OpenAudioDevice(const char *dev,int capture,const SDL_AudioSpec *want,SDL_AudioSpec *got,int flags){
    static SDL_AudioDeviceID (*open_next)(const char *,int,const SDL_AudioSpec *,SDL_AudioSpec *,int);
    if(!open_next) open_next=dlsym(RTLD_NEXT,"SDL_OpenAudioDevice");
    SDL_AudioDeviceID id=open_next(dev,capture,want,got,flags);
    if(!capture && id) audio=id;
    return id;
}

/* In the final game SDL row the engraved A uses RIGHTSHOULDER as its safe
 * RON slot. The other face slots are B/X/Y. Swap logical slots in events,
 * not the controller DB. Releases must follow the mapping used on press. */
/* Candidate only: use the independent Paddle1 slot provided by the source-built frontend. */
#ifdef NGP_FREE_KEYS
#ifdef NGP_SESSION_MOUSE
#include "session-mouse.h"
#include "menu-stick.h"
#define HOME_HELP "左摇杆 / 十字键导航   A 确定   M 返回游戏"
#define HOME_HELP_HINT "改动回首页保存才生效"
#define HOME_ROWS 7
enum { HOME_BIND=0, HOME_POINTER=1, HOME_BACKUP=2, HOME_RESTORE=3, HOME_SAVE=4, HOME_RESUME=5, HOME_EXIT=6 };
#endif
#include "free-keys.h"
#define VALUE(i) (i)
static unsigned owners[256]; /* held before a remap: release only after last owner */
#else
#define VALUE(i) vk[i]
#ifdef NGP_R1_TRIAL
#define EXTRA(value) , value
#else
#define EXTRA(value)
#endif
static const Uint8 slots[]={SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,SDL_CONTROLLER_BUTTON_B,
                            SDL_CONTROLLER_BUTTON_X,SDL_CONTROLLER_BUTTON_Y,SDL_CONTROLLER_BUTTON_LEFTSHOULDER EXTRA(SDL_CONTROLLER_BUTTON_PADDLE1)};
enum { NKEY = sizeof(slots)/sizeof(slots[0]), NHOME = 4 };
static const char *keys[NKEY]={"south","east","west","north","left_trigger" EXTRA("r1_vk")};
static const char *names[NKEY]={"A","B","X","Y","L1" EXTRA("R1")};
static int vk[NKEY],idx[NKEY]={0,1,2,3,4 EXTRA(5)};
#endif
#ifndef HOME_ROWS
#define HOME_HELP "上下选择   A 确定   Select + Start 退出游戏"
#define HOME_ROWS NHOME
#endif
static int down[SDL_CONTROLLER_BUTTON_MAX]; /* target+1, zero = not held */
static bool menu_only[SDL_CONTROLLER_BUTTON_MAX]; /* absorb menu-held releases after M */
static int face(Uint8 button){for(int i=0;i<NKEY;i++) if(slots[i]==button)return i;return -1;}
#ifdef NGP_FREE_KEYS
#define read_base free_profile
#else
static bool read_base(void){
    const char *path=getenv("RUFFLE_PROFILE"); FILE *f=path?fopen(path,"r"):NULL;
    if(!f)return false;
    char line[256],key[32];int v,mask=0;
    while(fgets(line,sizeof(line),f)) if(sscanf(line,"%31[^=]=%d",key,&v)==2 && v>0 && v<256)
        for(int i=0;i<NKEY;i++)if(!strcmp(key,keys[i])){vk[i]=v;mask|=1<<i;}
    fclose(f);return mask==(1<<NKEY)-1;
}
#endif
static bool dirty_layout(const int next[NKEY]){
#ifdef NGP_SESSION_MOUSE
    if(draft_src!=mouse_src)return true;
#endif
    return memcmp(next,idx,sizeof(idx))!=0;
}
static bool save_layout(const int next[NKEY]){
    const char *path=getenv("FLASH_PILOT_FACE_PROFILE");
    char *tmp=NULL;
    if(!path || asprintf(&tmp,"%s.tmp.XXXXXX",path)<0)return false;
    int fd=mkstemp(tmp),ok=0;
    if(fd>=0){
        bool good=true;
#ifdef NGP_SESSION_MOUSE
        if(dprintf(fd,"pointer_source=%s\n",source_ids[draft_src])<0)good=false;
#endif
        for(int i=0;i<NKEY;i++){
            int v=VALUE(next[i]);
#ifdef NGP_SESSION_MOUSE
            if(i>=SELECT_ROW && v==NATIVE_SYSTEM)continue; /* omitted = native default */
            if(v){if(dprintf(fd,"%s=",keys[i])<0)good=false;
                for(;v;v>>=8){int k=v&255;char num[8];snprintf(num,sizeof(num),"%d",k);
                    if(dprintf(fd,"%s%s",k==MOUSE_LEFT_ACTION?"mouse_left":k==MOUSE_RIGHT_ACTION?"mouse_right":num,v>255?"+":"\n")<0)good=false;}continue;}
#endif
            if(dprintf(fd,"%s=%d\n",keys[i],v)<0)good=false;
        }
        if(fsync(fd))good=false;
        if(close(fd))good=false;
        if(good && rename(tmp,path)==0){ memcpy(idx,next,sizeof(idx));ok=1;
#ifdef NGP_SESSION_MOUSE
            mouse_src=draft_src;mouse_stop_motion();
#endif
         }
    }
    if(!ok)unlink(tmp);
    free(tmp);return ok;
}
static const char *label(int n,char out[64]){
    if((n>=65 && n<=90)||(n>=48 && n<=57)){out[0]=(char)n;out[1]=0;return out;}
#ifdef NGP_SESSION_MOUSE
    if(n>255){out[0]=0;for(;n;n>>=8){int k=n&255;char b[64];size_t used=strlen(out);snprintf(out+used,64-used,"%s%s",used?"+":"",k==MOUSE_LEFT_ACTION?"左键":k==MOUSE_RIGHT_ACTION?"右键":label(k,b));}return out;}
    if(n==NATIVE_SYSTEM)return "原生默认";
    if(n==MOUSE_LEFT_ACTION)return "鼠标左键";
    if(n==MOUSE_RIGHT_ACTION)return "鼠标右键";
#endif
    switch(n){case 0:return "未绑定";case 9:return "Tab";case 16:return "Shift";case 17:return "Ctrl";case 37:return "←";case 38:return "↑";case 39:return "→";case 40:return "↓";case 32:return "空格";case 13:return "回车";case 27:return "Esc";}
    snprintf(out,16,"VK%d",n);return out;
}
/* Only scale the layout and font request. Copy the resulting glyph surface
 * 1:1 into the renderer's actual drawable, never a 640x480 text texture. */
static float ui_density;
static unsigned ui_alpha=255;
static int ui_dy;
static int ui_px(int n){return (int)(n*ui_density+0.5f);}
static bool text(SDL_Renderer *r,TTF_Font *font,int x,int y,SDL_Color col,const char *s){
    SDL_Surface *surf=TTF_RenderUTF8_Blended(font,s,col);if(!surf)return false;
    SDL_Rect rect={ui_px(x),ui_px(y),surf->w,surf->h}; SDL_Texture *t=SDL_CreateTextureFromSurface(r,surf);SDL_FreeSurface(surf);
    if(!t)return false;
    SDL_SetTextureAlphaMod(t,(Uint8)ui_alpha);
    int rc=SDL_RenderCopy(r,t,NULL,&rect);SDL_DestroyTexture(t);return rc==0;
}
#include "menu-theme.h"

/* Save snapshots are handled by a small python helper next to the launcher. */
static int saves_cmd(const char *verb, char *out, size_t n) {
    const char *helper = getenv("FLASH_SAVES"), *profile = getenv("FLASH_PILOT_FACE_PROFILE");
    if (!helper || !profile) return -1;
    char state[PATH_MAX];
    snprintf(state, sizeof(state), "%s", profile);
    char *slash = strrchr(state, '/');
    if (!slash) return -1;
    *slash = 0;
    char cmd[PATH_MAX * 3];
    snprintf(cmd, sizeof(cmd), "python3 '%s' %s '%s' 2>&1", helper, verb, state);
    FILE *p = popen(cmd, "r");
    if (!p) return -1;
    out[0] = 0;
    if (!fgets(out, (int)n, p)) out[0] = 0;
    for (char *c = out; *c; c++) if (*c == '\n') { *c = 0; break; }
    int rc = pclose(p);
    return WIFEXITED(rc) ? WEXITSTATUS(rc) : -1;
}

/* The frozen player's loaded RON actions are the live choice pool.
 * Exchange the owner of an action rather than mapping multiple buttons to
 * one logical slot (which would break independent button releases). */
enum Page { HOME, BINDINGS, KEYBOARD, POINTER_SOURCE };
#ifdef NGP_FREE_KEYS
#include "keyboard-grid.h"
static bool stage_defaults(int draft[NKEY]){
    static const int six[NKEY]={'A','Z','S','X','D','C',0,0,0,0 SYSTEM_ROWS(NATIVE_SYSTEM,NATIVE_SYSTEM)};
    memcpy(draft,arrows_template()?six:standard_free,sizeof(standard_free));
#ifdef NGP_SESSION_MOUSE
    const char *s=getenv("FLASH_KEY_TEMPLATE");const int *v=s && !strcmp(s,"wasd-six")?(const int[]){'W','S','A','D'}:(const int[]){38,40,37,39};
    for(int i=0;i<4;i++)draft[PAD_ROW+i]=draft[STICK_ROW+i]=v[i];
#endif
    return true;
}
#else
static void cycle_action(int draft[NKEY],int row,int step){
    int from=draft[row],to=(from+step+NKEY)%NKEY;
    for(int i=0;i<NKEY;i++)if(draft[i]==to){draft[i]=from;draft[row]=to;return;}
}
static bool stage_defaults(int draft[NKEY]){
    const int standard[NKEY]={74,75,85,73,79 EXTRA(76)};int next[NKEY];
    for(int i=0;i<NKEY;i++){
        int j=0;while(j<NKEY && vk[j]!=standard[i])j++;
        if(j==NKEY)return false; /* never claim an unavailable live slot */
        next[i]=j;
    }
    memcpy(draft,next,sizeof(next));return true;
}
#endif
static bool paint(SDL_Renderer *r,TTF_Font *f,TTF_Font *small,enum Page page,int row,const int draft[NKEY],const char *status){
    (void)f;
    const SDL_Color fg={232,236,244,255},dim={138,146,158,255},accent={107,231,205,255},warn={235,168,94,255};
    bool home=page==HOME;
#ifdef NGP_SESSION_MOUSE
    char pointer_label[80];snprintf(pointer_label,sizeof(pointer_label),"鼠标指针      %s",source_names[draft_src]);
#endif
    /* RetroArch-style: flat rows, hairline separators, value on the right. */
    ink(r,10,10,12,255);SDL_RenderClear(r);
    ink(r,16,26,44,255);SDL_Rect bar={0,0,ui_px(640),ui_px(42)};SDL_RenderFillRect(r,&bar);
    ink(r,42,44,50,255);SDL_Rect edge={0,ui_px(41),ui_px(640),ui_px(1)};SDL_RenderFillRect(r,&edge);
    const char *page_name=home?"设置":page==POINTER_SOURCE?"鼠标指针":NKEY>6?"按键映射":"动作交换";
    char title[64];snprintf(title,sizeof(title),"%s",game_title());
    while(tw(small,title)>392 && strlen(title)>4)title[strlen(title)-4]=0;
    bool ok=text(r,small,22,13,accent,title) && text(r,small,614-tw(small,page_name),13,dim,page_name);
    const char *items[]={"按键映射"
#ifdef NGP_SESSION_MOUSE
        ,pointer_label
#endif
        ,"备份存档","恢复存档（下次启动）","保存设置","继续游戏","退出游戏（回系统）"};
    int top=58+ui_dy,step=36;
    int count=page==POINTER_SOURCE?4:home?HOME_ROWS:NKEY;
    int first=!home && row>=8?row-7:0;
    for(int i=first;i<count && i<first+8;i++){
        int ry=top+step*(i-first);
        if(i==row){ink(r,26,28,33,255);SDL_Rect sel={0,ui_px(ry),ui_px(640),ui_px(step-1)};SDL_RenderFillRect(r,&sel);
            ink(r,107,231,205,255);SDL_Rect bar2={ui_px(6),ui_px(ry+4),ui_px(3),ui_px(step-9)};SDL_RenderFillRect(r,&bar2);}
        ink(r,34,35,40,255);SDL_Rect sep={ui_px(22),ui_px(ry+step-1),ui_px(596),ui_px(1)};SDL_RenderFillRect(r,&sep);
        if(page==POINTER_SOURCE){
            char v[80];snprintf(v,sizeof(v),"%s",source_names[i]);
            ok=ok && text(r,f,24,ry+3,fg,v);
            if(i==row)ok=ok && text(r,f,596,ry+3,accent,"●");
        }else if(home){
            const char *lbl=(i==HOME_RESUME && dirty_layout(draft))?"继续游戏（丢弃修改）":items[i];
            ink(r,i==row?107:130,i==row?231:140,i==row?205:152,255);
            icon(r,i==HOME_BIND?0:i==HOME_POINTER?1:i==HOME_BACKUP||i==HOME_SAVE?2:i==HOME_RESUME?3:4,23,ry+9,17);
            ok=ok && text(r,f,52,ry+3,i==row?accent:fg,lbl);
            if(i==HOME_BIND || i==HOME_POINTER)ok=ok && text(r,f,600,ry+3,dim,"›");
        }else{
            char v[64];const char *value=label(VALUE(draft[i]),v);
#ifdef NGP_SESSION_MOUSE
            if(pointer_owns_row(i,draft_src))ok=ok && text(r,small,183,ry+8,dim,"指针占用");
#endif
            ok=ok && text(r,f,24,ry+3,i==row?accent:fg,names[i]) && text(r,f,596-tw(f,value),ry+3,fg,value);
            if(draft[i]!=idx[i])ok=ok && text(r,small,566,ry+9,warn,"●");
        }
    }
    SDL_Color st=(strstr(status,"未保存")||strstr(status,"失败"))?warn:dim;
    ok=ok && text(r,small,24,396,st,status);
    ink(r,42,44,50,255);SDL_Rect foot={0,ui_px(424),ui_px(640),ui_px(1)};SDL_RenderFillRect(r,&foot);
    text(r,small,24,436,accent,"A");text(r,small,40,436,fg,"确定");
    text(r,small,96,436,accent,"B");text(r,small,112,436,fg,"返回");
    text(r,small,168,436,accent,"M");text(r,small,186,436,fg,home?"回到本局":NKEY>6?"回到本局（丢弃）":"回到本局");
    ok=ok && text(r,small,470,436,dim,home?HOME_HELP_HINT:"X 恢复默认 · B 保留草稿");
    if(ok)SDL_RenderPresent(r);
    return ok;
}
/* Short fade+slide only when the page changes, so navigation stays instant. */
static int ui_focus;
static bool show_page(SDL_Renderer *r,TTF_Font *f,TTF_Font *small,enum Page page,int row,const int draft[NKEY],const char *status,bool anim){
    for(int i=anim?2:5;i<=5;i++){
        ui_alpha=(unsigned)(255*i/5);ui_dy=anim?6-6*i/5:0;
#ifdef NGP_FREE_KEYS
        bool ok=page==KEYBOARD?paint_grid(r,f,small,row,draft,ui_focus):paint(r,f,small,page,row,draft,status);
#else
        bool ok=paint(r,f,small,page,row,draft,status);
#endif
        if(!ok)return false;
        if(i<5)SDL_Delay(11);
    }
    return true;
}
/* A player-owned button that was held at menu entry may be released inside
 * the menu. Deliver that release on resume instead of leaving it stuck. */
/* At most three actions per physical button, plus two axes. */
static SDL_Event pending[3*(SDL_CONTROLLER_BUTTON_MAX+8)+8];static int pend_n,pend_i;
#ifdef NGP_FREE_KEYS
static void queue_binding(SDL_Event source,int binding,bool press){
    for(;binding;binding>>=8){
        int k=binding&255;if(!k)continue;
        if(press){if(owners[k]++)continue;}else if(--owners[k])continue;
        SDL_Event edge=source;free_event(&edge,k,press);pending[pend_n++]=edge;
    }
}
#endif
#ifdef NGP_SESSION_MOUSE
/* Axis directions have their own held bindings, separate from D-pad buttons.
 * Release the old sign before pressing the new one; diagonals keep both axes. */
static int stick_down[8];
/* Track right tilt during the modal loop as well. Changing a source while
 * tilted must not send an inherited keystroke or move the pointer on resume. */
static struct {int x,y;bool wait_center;} raw_right;
static bool right_event(const SDL_Event *e){return e->type==SDL_CONTROLLERAXISMOTION && (e->caxis.axis==SDL_CONTROLLER_AXIS_RIGHTX || e->caxis.axis==SDL_CONTROLLER_AXIS_RIGHTY);}
static void track_right(const SDL_Event *e){if(e->caxis.axis==SDL_CONTROLLER_AXIS_RIGHTX)raw_right.x=e->caxis.value;else raw_right.y=e->caxis.value;}
static bool right_centered(void){return abs(raw_right.x)<=STICK_CENTER && abs(raw_right.y)<=STICK_CENTER;}
static void stick_edge(SDL_Event e,int dir,bool on){
    if(on && !stick_down[dir]){stick_down[dir]=idx[STICK_ROW+dir]+1;queue_binding(e,stick_down[dir]-1,true);}
    else if(!on && stick_down[dir]){queue_binding(e,stick_down[dir]-1,false);stick_down[dir]=0;}
}
static void stick_motion(SDL_Event e){
    int first=right_event(&e)?4:0,v=e.caxis.value;
    if(e.caxis.axis==SDL_CONTROLLER_AXIS_LEFTX || e.caxis.axis==SDL_CONTROLLER_AXIS_RIGHTX)first+=2;
    bool neg=v<-(stick_down[first]?6500:8000),pos=v>(stick_down[first+1]?6500:8000);
    if(!neg)stick_edge(e,first,false);
    if(!pos)stick_edge(e,first+1,false);
    if(neg)stick_edge(e,first,true);
    if(pos)stick_edge(e,first+1,true);
}
#include "system-taps.h"
#endif
static bool guide_held;
static int modal(SDL_Event *out,SDL_JoystickID joy){
    SDL_Window *w=SDL_GL_GetCurrentWindow();SDL_GLContext game=SDL_GL_GetCurrentContext();
    if(!w || !game || !read_base()){fprintf(stderr,"[quick-menu] no current game context/profile\n");return 0;}
    int driver=-1;
    for(int i=0;i<SDL_GetNumRenderDrivers();i++){SDL_RendererInfo info;if(!SDL_GetRenderDriverInfo(i,&info)&&!strcmp(info.name,"opengles2")){driver=i;break;}}
    if(driver<0){fprintf(stderr,"[quick-menu] no SDL opengles2 renderer\n");return 0;}
    SDL_Renderer *r=SDL_CreateRenderer(w,driver,0);
    TTF_Font *font=NULL;
#ifdef NGP_FREE_KEYS
    TTF_Font *small=NULL;
#endif
    bool ttf=false,paused=false;int forward=0,row=0,draft[NKEY];
    enum Page page=HOME,last_page=HOME;
#ifdef NGP_FREE_KEYS
    int focus=0;
#endif
    memcpy(draft,idx,sizeof(draft));
    bool held[SDL_CONTROLLER_BUTTON_MAX];
    for(int i=0;i<SDL_CONTROLLER_BUTTON_MAX;i++)held[i]=down[i]!=0 || menu_only[i];
#ifdef NGP_SESSION_MOUSE
    sys_cancel();
    struct StickNav nav;
    if(raw_left.joy!=joy){raw_left.joy=joy;raw_left.x=raw_left.y=0;}
    arm_nav(&nav);
    draft_src=mouse_src;
    /* Neutralize old game directions; navigation must never leave one stuck. */
    SDL_Event neutral;SDL_zero(neutral);neutral.common.timestamp=SDL_GetTicks();
    for(int i=0;i<8;i++)stick_edge(neutral,i,false);
    for(int i=0;i<2;i++)if(game_axes[i]){SDL_Event *z=&pending[pend_n++];SDL_zero(*z);z->type=SDL_CONTROLLERAXISMOTION;z->caxis.which=joy;z->caxis.axis=i?SDL_CONTROLLER_AXIS_LEFTY:SDL_CONTROLLER_AXIS_LEFTX;game_axes[i]=0;}
    for(int b=0;b<SDL_CONTROLLER_BUTTON_MAX;b++){
        int bit=dir_bit(b);
        if(dirs&bit)held[b]=true;
        if(!down[b])continue;
        if(bit){queue_binding(neutral,down[b]-1,false);down[b]=0;}
    }
    mouse_stop_motion();
#endif
    int width=0,height=0;
    if(!r || SDL_GetRendererOutputSize(r,&width,&height)!=0 || width<=0 || height<=0){
        fprintf(stderr,"[quick-menu] no drawable: %s\n",SDL_GetError());goto done;
    }
    ui_density=width/640.f < height/480.f ? width/640.f : height/480.f;
    SDL_Rect view={(width-ui_px(640))/2,(height-ui_px(480))/2,ui_px(640),ui_px(480)};
    fprintf(stderr,"[quick-menu] drawable=%dx%d font=%d\n",width,height,ui_px(22));
    if(SDL_RenderSetViewport(r,&view)!=0 || !(ttf=(TTF_Init()==0)) ||
       !(font=TTF_OpenFont("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",ui_px(22))) ||
#ifdef NGP_FREE_KEYS
       !(small=TTF_OpenFont("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",ui_px(15))) ||
#endif
       !show_page(r,font,small,page,row,draft,"设置仅用于这款游戏",true)){
        fprintf(stderr,"[quick-menu] open failed: %s\n",SDL_GetError());goto done;
    }
    if(audio && SDL_GetAudioDeviceStatus(audio)==SDL_AUDIO_PLAYING){ SDL_PauseAudioDevice(audio,1); paused=true; }
    fprintf(stderr,"[quick-menu] opened pid=%ld joy=%d\n",(long)getpid(),joy);
    for(;;){
        SDL_Event e;int got=poll_next(&e);bool stick=false;
#ifdef NGP_SESSION_MOUSE
        if(got && right_event(&e) && e.caxis.which==joy){track_right(&e);continue;}
        if(!got || (left_event(&e) && e.caxis.which==joy)){
            if(got)track_left(&e);
            int pulse=nav_pulse(&nav,SDL_GetTicks());
            if(pulse<0){if(!got)SDL_Delay(8);continue;}
            SDL_zero(e);e.type=SDL_CONTROLLERBUTTONDOWN;e.cbutton.which=joy;e.cbutton.button=(Uint8)pulse;stick=true;
        }
#else
        if(!got){SDL_Delay(8);continue;}
#endif
        if(e.type==SDL_QUIT || (e.type==SDL_CONTROLLERDEVICEREMOVED && e.cdevice.which==joy)){*out=e;forward=1;break;}
        if(e.type!=SDL_CONTROLLERBUTTONUP && e.type!=SDL_CONTROLLERBUTTONDOWN)continue;
        if(e.cbutton.which!=joy)continue;
        Uint8 b=e.cbutton.button;if(b>=SDL_CONTROLLER_BUTTON_MAX)continue;
#ifdef NGP_SESSION_MOUSE
        if(sys_button(e,true))continue;
#endif
        if(b==SDL_CONTROLLER_BUTTON_GUIDE){
            if(e.type==SDL_CONTROLLERBUTTONUP)guide_held=false;
            else if(!guide_held){guide_held=true;break;}
        }else if(e.type==SDL_CONTROLLERBUTTONUP){
            held[b]=false;
            if(down[b]){
#ifdef NGP_FREE_KEYS
                if(face(b)>=0){queue_binding(e,down[b]-1,false);down[b]=0;continue;}else
#endif
                {e.cbutton.button=down[b]-1;}down[b]=0;pending[pend_n++]=e; }
        }else if(stick || !held[b]){
            if(!stick)held[b]=true;
#ifdef NGP_SESSION_MOUSE
            if(b==slots[0] || b==slots[1])arm_nav(&nav); /* no held repeat across a page transition */
#endif
            const char *notice=NULL;
            int rows=page==HOME?HOME_ROWS:page==POINTER_SOURCE?4:NKEY;
#ifdef NGP_FREE_KEYS
#ifdef NGP_SESSION_MOUSE
            if(page==POINTER_SOURCE && (b==slots[0] || b==slots[1])){
                if(b==slots[0])draft_src=row;
                page=HOME;row=dirty_layout(draft)?HOME_SAVE:HOME_BIND;
            }else
#endif
            if(page==KEYBOARD){
#ifdef NGP_SESSION_MOUSE
                if(b==slots[0] || b==slots[2])draft[row]=edit_member(draft[row],caps[focus].vk,b==slots[2]);
#else
                if(b==slots[0]){draft[row]=caps[focus].vk;page=BINDINGS;}
#endif
                else if(b==slots[1])page=BINDINGS;
                else {
                    int dx=(b==SDL_CONTROLLER_BUTTON_DPAD_RIGHT)-(b==SDL_CONTROLLER_BUTTON_DPAD_LEFT);
                    int dy=(b==SDL_CONTROLLER_BUTTON_DPAD_DOWN)-(b==SDL_CONTROLLER_BUTTON_DPAD_UP);
                    if(!dx && !dy)continue;
                    focus=move_focus(row,focus,dx,dy);
                }
            }else
#endif
            if(b==SDL_CONTROLLER_BUTTON_DPAD_UP)row=(row+rows-1)%rows;
            else if(b==SDL_CONTROLLER_BUTTON_DPAD_DOWN)row=(row+1)%rows;
#ifdef NGP_FREE_KEYS
            else if(page==BINDINGS && b==slots[0]){focus=focus_vk(draft[row]&255);page=KEYBOARD;}
            else if(page==BINDINGS && b==slots[2]){
#ifdef NGP_SESSION_MOUSE
                draft_src=PTR_NONE;
#endif
                notice=stage_defaults(draft)?"已恢复默认 · 回首页保存才生效":"本局未载入全部默认动作，无法热恢复";
            }
            else if(page==BINDINGS && b==slots[1]){page=HOME;row=dirty_layout(draft)?HOME_SAVE:HOME_BIND;}
#else
            else if(page==BINDINGS && (b==SDL_CONTROLLER_BUTTON_DPAD_LEFT || b==SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
                cycle_action(draft,row,b==SDL_CONTROLLER_BUTTON_DPAD_RIGHT?1:-1);
            else if(page==BINDINGS && (b==slots[0] || b==slots[1])){
                page=HOME;row=b==slots[0]?HOME_SAVE:HOME_BIND; /* keep draft; only explicit save commits */
            }
#endif
            else if(page==HOME && b==slots[0]){
                if(row==HOME_BIND){page=BINDINGS;}
#ifdef NGP_SESSION_MOUSE
                else if(row==HOME_POINTER){row=draft_src;page=POINTER_SOURCE;}
#endif
                else if(row==HOME_BACKUP){
                    char msg[192]; int rc=saves_cmd("backup",msg,sizeof(msg));
                    notice=(rc==0 && msg[0])?msg:"备份失败";
                }
                else if(row==HOME_RESTORE){
                    const char *profile=getenv("FLASH_PILOT_FACE_PROFILE");
                    char dir[PATH_MAX * 2]; dir[0]=0;
                    if(profile){snprintf(dir,sizeof(dir),"%s",profile);char *sl=strrchr(dir,'/');if(sl)*sl=0;}
                    if(dir[0]){char path[PATH_MAX * 2];snprintf(path,sizeof(path),"%s/restore.request",dir);FILE *f=fopen(path,"w");if(f){fputs("\n",f);fclose(f);}}
                    SDL_zero(*out);out->type=SDL_QUIT;forward=1;break;
                }
                else if(row==HOME_SAVE){
                    if(!dirty_layout(draft))break;
                    bool ok=save_layout(draft);fprintf(stderr,"[quick-menu] layout saved=%d\n",ok);
                    if(ok)break;
                    notice="保存失败 · 未应用，可再试";
                }
                else if(row==HOME_EXIT){
                    /* Same orderly exit as Select + Start: the player closes,
                     * the display worker's ExecStopPost brings ES back. */
                    SDL_zero(*out);out->type=SDL_QUIT;forward=1;break;
                }
                else break;
            }else continue;
            if(!notice)notice=dirty_layout(draft)?"有未保存修改 · 保存后生效":"当前生效的键位";
#ifdef NGP_FREE_KEYS
            ui_focus=focus;
#endif
            if(!show_page(r,font,small,page,row,draft,notice,page!=last_page))break;
            last_page=page;
        }
    }
done:
#ifdef NGP_SESSION_MOUSE
    raw_right.wait_center=!right_centered();
    raw_left.wait_center=!centered(); /* don't turn a menu tilt into movement or a mouse drag on resume */
#endif
    for(int i=0;i<SDL_CONTROLLER_BUTTON_MAX;i++)menu_only[i]=held[i] && !down[i];
#ifdef NGP_FREE_KEYS
    if(small)TTF_CloseFont(small);
#endif
    if(font)TTF_CloseFont(font);
    if(ttf)TTF_Quit();
    if(r)SDL_DestroyRenderer(r);
    if(SDL_GL_MakeCurrent(w,game)<0){fprintf(stderr,"[quick-menu] restore GL failed: %s\n",SDL_GetError());out->type=SDL_QUIT;forward=1;}
    if(paused)SDL_PauseAudioDevice(audio,0);
    fprintf(stderr,"[quick-menu] closed pid=%ld\n",(long)getpid());return forward;
}
int SDL_PollEvent(SDL_Event *e){
    if(!poll_next){poll_next=dlsym(RTLD_NEXT,"SDL_PollEvent");fprintf(stderr,"[quick-menu] M listener active pid=%ld\n",(long)getpid());}
    if(!e)return poll_next(NULL);
    for(;;){
        if(pend_i<pend_n){*e=pending[pend_i++];return 1;}pend_i=pend_n=0;
#ifdef NGP_SESSION_MOUSE
        sys_tick();if(pend_n)continue;
        if(free_profile() && mouse_move(e))return 1;
#endif
        int rc=poll_next(e);if(rc<=0)return rc;
#ifdef NGP_SESSION_MOUSE
        if(left_event(e)){track_left(e);if(raw_left.wait_center){if(centered())raw_left.wait_center=false;continue;}}
        if(right_event(e)){track_right(e);if(raw_right.wait_center){if(right_centered())raw_right.wait_center=false;continue;}}
        if(e->type==SDL_MOUSEMOTION){if(mouse_src)continue;mx=e->motion.x;my=e->motion.y;positioned=true;}
        if(e->type==SDL_CONTROLLERAXISMOTION){int a=e->caxis.axis;
            bool left=a==SDL_CONTROLLER_AXIS_LEFTX||a==SDL_CONTROLLER_AXIS_LEFTY;
            bool right=a==SDL_CONTROLLER_AXIS_RIGHTX||a==SDL_CONTROLLER_AXIS_RIGHTY;
            if((left&&mouse_src==PTR_LEFT)||(right&&mouse_src==PTR_RIGHT)){axes[a==SDL_CONTROLLER_AXIS_LEFTY||a==SDL_CONTROLLER_AXIS_RIGHTY]=e->caxis.value;continue;}
            if((left||right) && free_profile()){stick_motion(*e);continue;}
            if(left)game_axes[a==SDL_CONTROLLER_AXIS_LEFTY]=e->caxis.value;
        }
#endif
        if((e->type==SDL_JOYBUTTONDOWN || e->type==SDL_JOYBUTTONUP) && e->jbutton.button==10)continue;
        if(e->type!=SDL_CONTROLLERBUTTONDOWN && e->type!=SDL_CONTROLLERBUTTONUP)return rc;
        Uint8 b=e->cbutton.button;if(b>=SDL_CONTROLLER_BUTTON_MAX)return rc;
        if(b==SDL_CONTROLLER_BUTTON_GUIDE){
            if(e->type==SDL_CONTROLLERBUTTONUP)guide_held=false;
            else if(!guide_held){ guide_held=true;if(modal(e,e->cbutton.which))return 1; }
            continue;
        }
#ifdef NGP_SESSION_MOUSE
        if(free_profile() && sys_button(*e,false))continue;
#endif
        if(menu_only[b]){if(e->type==SDL_CONTROLLERBUTTONUP)menu_only[b]=false;continue;}
#ifdef NGP_FREE_KEYS
#ifdef NGP_SESSION_MOUSE
        int bit=dir_bit(b);if(mouse_src==PTR_DPAD && bit){if(e->type==SDL_CONTROLLERBUTTONDOWN)dirs|=bit;else dirs&=~(unsigned)bit;continue;}
#endif
        int i=face(b);if(i>=0){
            bool press=e->type==SDL_CONTROLLERBUTTONDOWN;
            if(!free_profile())continue;
            if(press){if(down[b] || !idx[i])continue;down[b]=idx[i]+1;}else if(!down[b])continue;
            int key=down[b]-1;
            if(!press)down[b]=0;
            queue_binding(*e,key,press);continue;
        }
#endif
        if(e->type==SDL_CONTROLLERBUTTONDOWN){int i=face(b);if(i>=0)e->cbutton.button=slots[idx[i]];down[b]=e->cbutton.button+1;}
        else if(down[b]){e->cbutton.button=down[b]-1;down[b]=0;}
        return rc;
    }
}

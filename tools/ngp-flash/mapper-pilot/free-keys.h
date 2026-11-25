/* Ten independent digital controls on NGP. 0 means swallow, never click.
 * Keep WASD/Enter/Esc with their native owners until they too use this router. */
#ifdef NGP_SESSION_MOUSE
enum { NATIVE_SYSTEM=243, SELECT_ROW=10, START_ROW=11, PAD_ROW=12, STICK_ROW=16, RIGHT_ROW=20 };
#define RIGHT_ROWS(a,b,c,d) ,a,b,c,d
static bool pointer_owns_row(int row,int src){
 return (row>=PAD_ROW && row<STICK_ROW && src==PTR_DPAD) || (row>=STICK_ROW && row<RIGHT_ROW && src==PTR_LEFT) || (row>=RIGHT_ROW && src==PTR_RIGHT);
}
#define DIR_ROWS(a,b,c,d,e,f,g,h) ,a,b,c,d,e,f,g,h
#define SYSTEM_ROWS(a,b) ,a,b
#else
#define RIGHT_ROWS(a,b,c,d)
#define DIR_ROWS(a,b,c,d,e,f,g,h)
#define SYSTEM_ROWS(a,b)
#endif
static const Uint8 slots[]={SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,SDL_CONTROLLER_BUTTON_B,SDL_CONTROLLER_BUTTON_X,SDL_CONTROLLER_BUTTON_Y,SDL_CONTROLLER_BUTTON_LEFTSHOULDER,SDL_CONTROLLER_BUTTON_PADDLE1,SDL_CONTROLLER_BUTTON_PADDLE2,SDL_CONTROLLER_BUTTON_PADDLE3,SDL_CONTROLLER_BUTTON_LEFTSTICK,SDL_CONTROLLER_BUTTON_RIGHTSTICK SYSTEM_ROWS(SDL_CONTROLLER_BUTTON_BACK,SDL_CONTROLLER_BUTTON_START) DIR_ROWS(SDL_CONTROLLER_BUTTON_DPAD_UP,SDL_CONTROLLER_BUTTON_DPAD_DOWN,SDL_CONTROLLER_BUTTON_DPAD_LEFT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT,251,252,253,254) RIGHT_ROWS(247,248,249,250)};
enum {NKEY=sizeof(slots)/sizeof(slots[0]),NHOME=4};
static const char *keys[NKEY]={"south","east","west","north","left_trigger","r1_vk","l2_vk","r2_vk","l3_vk","r3_vk" SYSTEM_ROWS("select_tap","start_tap") DIR_ROWS("pad_up_vk","pad_down_vk","pad_left_vk","pad_right_vk","stick_up_vk","stick_down_vk","stick_left_vk","stick_right_vk") RIGHT_ROWS("right_stick_up_vk","right_stick_down_vk","right_stick_left_vk","right_stick_right_vk")};
static const char *names[NKEY]={"A","B","X","Y","L1","R1","L2","R2","L3","R3" SYSTEM_ROWS("Select","Start") DIR_ROWS("十字键 ↑","十字键 ↓","十字键 ←","十字键 →","左摇杆 ↑","左摇杆 ↓","左摇杆 ←","左摇杆 →") RIGHT_ROWS("右摇杆 ↑","右摇杆 ↓","右摇杆 ←","右摇杆 →")};
static const int standard_free[NKEY]={'J','K','U','I','O','L',0,0,0,0 SYSTEM_ROWS(NATIVE_SYSTEM,NATIVE_SYSTEM)};
static int idx[NKEY]; /* actual Flash VKs, NOT a permutation */
static const int pool[]={0,9,16,17,32,37,38,39,40,'B','C','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','T','U','V','X','Y','Z','0','1','2','3','4','5','6','7','8','9'};
static bool arrows_template(void){const char *s=getenv("FLASH_KEY_TEMPLATE");return s && (!strcmp(s,"arrows-six") || !strcmp(s,"six-keys"));}
static bool allowed_vk(int k){
#ifdef NGP_SESSION_MOUSE
 if(k==MOUSE_LEFT_ACTION||k==MOUSE_RIGHT_ACTION||(k>=37 && k<=40)||k=='W'||k=='A'||k=='S'||k=='D')return true;
#endif
 if(arrows_template()){if(k>=37 && k<=40)return false;if(k=='W'||k=='A'||k=='S'||k=='D')return true;}
 for(size_t j=0;j<sizeof(pool)/sizeof(pool[0]);j++)if(pool[j]==k)return true;
 return false;
}
#ifdef NGP_SESSION_MOUSE
/* Three packed unsigned bytes, kept in selection order. A adds idempotently;
 * X removes. Native/none are exclusive. No synthetic timing or macro. */
static int edit_member(int old,int pick,bool remove){
 if(!remove && (!pick || pick==NATIVE_SYSTEM))return pick;
 int out=0,n=0;bool found=false;
 for(int v=old;v;v>>=8){int k=v&255;if(k==pick){found=true;if(remove)continue;}
    if(k==NATIVE_SYSTEM && !remove)continue;
    out|=k<<(8*n++);}
 if(!remove && !found && n<3)out|=pick<<(8*n);
 return out;
}
#endif
static bool allowed_binding(int v){
#ifdef NGP_SESSION_MOUSE
 if(v>255){if(v>0xffffff)return false;bool used[256]={0};for(;v;v>>=8){int k=v&255;if(!k || used[k] || !allowed_vk(k))return false;used[k]=true;}return true;}
#endif
 return allowed_vk(v);
}
static bool free_profile(void){
 static bool ready=false;if(ready)return true;
 const char *path=getenv("RUFFLE_PROFILE"); FILE *f=path?fopen(path,"r"):NULL;if(!f)return false;
 char line[256],k[32];int val,mask=0,t[NKEY]={0};
 while(fgets(line,sizeof(line),f))if(sscanf(line,"%31[^=]=%d",k,&val)==2){
#ifdef NGP_SESSION_MOUSE
    if(!strcmp(k,"pointer_mode")){if(val<0||val>3){fclose(f);return false;}mouse_src=val;continue;}
#endif
    for(int i=0;i<NKEY;i++)if(!strcmp(keys[i],k)){
      bool ok=allowed_binding(val);
#ifdef NGP_SESSION_MOUSE
      if((i==SELECT_ROW || i==START_ROW) && val==NATIVE_SYSTEM)ok=true;
#endif
      if(ok && !(mask&(1<<i))){t[i]=val;mask|=1<<i;}else {fclose(f);return false;}
    }
 }
 fclose(f);if(mask!=(1<<NKEY)-1)return false;
 memcpy(idx,t,sizeof(idx));ready=true;return true;
}
static void free_event(SDL_Event *e,int vk,bool pressed){
#ifdef NGP_SESSION_MOUSE
 if(vk==MOUSE_LEFT_ACTION||vk==MOUSE_RIGHT_ACTION){mouse_button(e,vk,pressed);return;}
#endif
 SDL_Scancode s=SDL_SCANCODE_UNKNOWN;
 if(vk>='A' && vk<='Z')s=SDL_SCANCODE_A+vk-'A';else if(vk>='1' && vk<='9')s=SDL_SCANCODE_1+vk-'1';else switch(vk){case '0':s=SDL_SCANCODE_0;break;case 32:s=SDL_SCANCODE_SPACE;break;case 9:s=SDL_SCANCODE_TAB;break;case 16:s=SDL_SCANCODE_LSHIFT;break;case 17:s=SDL_SCANCODE_LCTRL;break;case 37:s=SDL_SCANCODE_LEFT;break;case 38:s=SDL_SCANCODE_UP;break;case 39:s=SDL_SCANCODE_RIGHT;break;case 40:s=SDL_SCANCODE_DOWN;break;}
 Uint32 ts=e->common.timestamp; SDL_zero(*e);e->type=pressed?SDL_KEYDOWN:SDL_KEYUP;e->common.timestamp=ts;e->key.state=pressed?SDL_PRESSED:SDL_RELEASED;e->key.keysym.scancode=s;e->key.keysym.sym=SDL_GetKeyFromScancode(s);
}

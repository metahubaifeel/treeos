/* QWERTY keycaps for the in-game ten-control editor (640x480 layout units).
 * Grey keys show the physical keyboard layout but cannot take another route's VK. */
typedef struct {int vk,row,col,width;} Cap;
#define CAP(k,r,c) {k,r,c,1}
static const Cap caps[]={
 CAP('1',0,0),CAP('2',0,1),CAP('3',0,2),CAP('4',0,3),CAP('5',0,4),CAP('6',0,5),CAP('7',0,6),CAP('8',0,7),CAP('9',0,8),CAP('0',0,9),
 CAP('Q',1,0),CAP('W',1,1),CAP('E',1,2),CAP('R',1,3),CAP('T',1,4),CAP('Y',1,5),CAP('U',1,6),CAP('I',1,7),CAP('O',1,8),CAP('P',1,9),
 CAP('A',2,0),CAP('S',2,1),CAP('D',2,2),CAP('F',2,3),CAP('G',2,4),CAP('H',2,5),CAP('J',2,6),CAP('K',2,7),CAP('L',2,8),
 CAP('Z',3,0),CAP('X',3,1),CAP('C',3,2),CAP('V',3,3),CAP('B',3,4),CAP('N',3,5),CAP('M',3,6),
 {0,4,0,2},{32,4,2,4},CAP(37,4,6),CAP(38,4,7),CAP(40,4,8),CAP(39,4,9)
#ifdef NGP_SESSION_MOUSE
 ,{MOUSE_LEFT_ACTION,5,0,2},{MOUSE_RIGHT_ACTION,5,2,2},{9,5,4,2},{16,5,6,2},{17,5,8,2},{NATIVE_SYSTEM,3,7,3}
#endif
};
#undef CAP
enum {NCAPS=sizeof(caps)/sizeof(caps[0])};
static bool available(int row,int vk){
#ifdef NGP_SESSION_MOUSE
 if(vk==NATIVE_SYSTEM)return row==SELECT_ROW || row==START_ROW;
#else
 (void)row;
#endif
 return allowed_vk(vk);
}
static int focus_vk(int vk){for(int i=0;i<NCAPS;i++)if(caps[i].vk==vk)return i;return 0;}
static int move_focus(int row,int cur,int dx,int dy){
 const Cap *c=caps+cur;int best=cur,score=100;
 for(int i=0;i<NCAPS;i++){
    const Cap *n=caps+i;if(!available(row,n->vk) || n->row!=c->row+dy)continue;
    int dist=(n->col*2+n->width)-(c->col*2+c->width);
    if(dx && dx*dist<=0)continue;
    if(abs(dist)<score){score=abs(dist);best=i;}
 }
 return best;
}
/* Invert the whole editable draft, not just the active row or currently held inputs. */
typedef struct {char list[96];const char *first;int count;} KeyOwners;
static KeyOwners cap_owners(int vk,const int draft[NKEY]){
 KeyOwners o={{0},NULL,0};if(!vk)return o;
 for(int i=0;i<NKEY;i++){
    int v=draft[i];while(v && (v&255)!=vk)v>>=8;if(!v)continue;
    if(!o.first)o.first=names[i];
    size_t n=strlen(o.list);snprintf(o.list+n,sizeof(o.list)-n,"%s%s",o.count?" / ":"",names[i]);o.count++;
 }
#ifndef NGP_SESSION_MOUSE
 if(!o.count){
    const int *dir=arrows_template()?(const int[]){38,40,37,39}:(const int[]){'W','S','A','D'};
    const char *pad[]={"十字↑","十字↓","十字←","十字→"};
    for(int j=0;j<4;j++)if(vk==dir[j]){snprintf(o.list,sizeof(o.list),"%s",pad[j]);o.first=pad[j];o.count=1;break;}
 }
#endif
 return o;
}
static bool fits(TTF_Font *font,const char *s,int units){int w=0;return !TTF_SizeUTF8(font,s,&w,NULL) && w<=ui_px(units);}
static bool paint_grid(SDL_Renderer *r,TTF_Font *f,TTF_Font *small,int row,const int draft[NKEY],int focus){
 int vk=draft[row];
 ink(r,11,18,32,255);SDL_RenderClear(r);
 SDL_Color white={237,242,252,255},mint={107,231,205,255},dim={132,147,164,255};
 /* Same header as the settings pages. */
 ink(r,16,26,44,255);SDL_Rect band={0,0,ui_px(640),ui_px(70)};SDL_RenderFillRect(r,&band);
 ink(r,38,62,86,255);SDL_Rect line={0,ui_px(69),ui_px(640),ui_px(2)};SDL_RenderFillRect(r,&line);
 char title[128],current[64];snprintf(title,sizeof(title),"%s → %s",names[row],label(vk,current));
 bool ok=text(r,small,26,12,dim,"按键映射 · 选择键盘键") && text(r,f,26,28,white,title);
 int top=76+ui_dy;
 for(int i=0;i<NCAPS;i++){
    const Cap *c=caps+i;int x=28+58*c->col,y=top+48*c->row;bool enabled=available(row,c->vk);
#ifdef NGP_SESSION_MOUSE
    if(c->vk==NATIVE_SYSTEM && !enabled)continue;
#endif
    KeyOwners o=cap_owners(c->vk,draft);int width=c->width*58-4;
    {   const int BR=i==focus?107:o.count?46:48,BG=i==focus?231:o.count?110:49,BB=i==focus?205:o.count?104:56;
        ink(r,BR,BG,BB,255);SDL_Rect box={ui_px(x),ui_px(y),ui_px(width),ui_px(44)};SDL_RenderFillRect(r,&box);
        ink(r,i==focus?14:18,i==focus?34:20,i==focus?38:26,255);SDL_Rect in={ui_px(x+1),ui_px(y+1),ui_px(width-2),ui_px(42)};SDL_RenderFillRect(r,&in);}
    char buf[64];if(!text(r,f,x+6,y-4,enabled?white:dim,label(c->vk,buf)))return false;
    if(o.count){
        char badge[24];const char *s=o.list;
        if(!fits(small,s,width-8)){snprintf(badge,sizeof(badge),"%s+%d",o.first,o.count-1);s=badge;
            if(!fits(small,s,width-8))snprintf(badge,sizeof(badge),"%d 键",o.count);}
        if(!text(r,small,x+5,y+24,enabled?mint:dim,s))return false;
    }
 }
 KeyOwners owners_here=cap_owners(caps[focus].vk,draft);char detail[180],vlabel[64];
 snprintf(detail,sizeof(detail),"%s  ←  %s",label(caps[focus].vk,vlabel),owners_here.count?owners_here.list:caps[focus].vk?"暂无绑定":"不发送动作");
 ok=ok && text(r,fits(f,detail,560)?f:small,28,366,mint,detail);
 const char *note=vk>>16?"已选 3 项；X 移除一项后再添加":"键帽下方是手柄键；+数字 表示还有其他绑定";
#ifdef NGP_SESSION_MOUSE
 if(row==SELECT_ROW||row==START_ROW)note="改绑后单放点按；Select + Start 仍直接退出";
 if(pointer_owns_row(row,draft_src))note="此组正控制鼠标；更改指针来源后使用此映射";
#endif
 ok=ok && text(r,small,28,394,dim,note);
 ink(r,42,44,50,255);SDL_Rect foot={0,ui_px(424),ui_px(640),ui_px(1)};SDL_RenderFillRect(r,&foot);
 SDL_Color key={107,231,205,255},lab={228,231,235,255};
 text(r,small,24,438,key,"A");text(r,small,40,438,lab,"添加");
 text(r,small,96,438,key,"X");text(r,small,112,438,lab,"移除");
 text(r,small,168,438,key,"B");text(r,small,184,438,lab,"选完返回");
 text(r,small,300,438,key,"X");text(r,small,316,438,lab,"恢复默认");
 ok=ok && text(r,small,470,438,dim,"M 丢弃修改");
 if(ok)SDL_RenderPresent(r);
 return ok;
}

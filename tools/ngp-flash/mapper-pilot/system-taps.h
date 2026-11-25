/* Single internal NGP pad. Never synthesise a system exit: the independent
 * read-only evdev watcher owns the physical chord even inside the M menu. */
static struct SystemKey { bool held,native_down; int pulse; Uint32 since,frame; SDL_Event source; } syskeys[2];
static bool chord_block;
static Uint32 shown_frames;
void SDL_GL_SwapWindow(SDL_Window *window){
    static void (*next)(SDL_Window *);if(!next)next=dlsym(RTLD_NEXT,"SDL_GL_SwapWindow");
    next(window);shown_frames++;
}
static void sys_emit(int i,int binding,bool press){
    SDL_Event e=syskeys[i].source;
    if(binding!=NATIVE_SYSTEM){queue_binding(e,binding,press);return;}
    e.type=press?SDL_CONTROLLERBUTTONDOWN:SDL_CONTROLLERBUTTONUP;
    e.cbutton.button=slots[SELECT_ROW+i];e.cbutton.state=press?SDL_PRESSED:SDL_RELEASED;
    pending[pend_n++]=e;
}
static void end_pulse(int i){struct SystemKey *s=syskeys+i;if(s->pulse){sys_emit(i,s->pulse,false);s->pulse=0;}}
static void sys_tick(void){
    Uint32 now=SDL_GetTicks();for(int i=0;i<2;i++){
        struct SystemKey *s=syskeys+i;if(!s->pulse)continue;
        Uint32 dt=now-s->since;
        if((shown_frames!=s->frame && dt>=60) || dt>=300)end_pulse(i);
    }
}
static void sys_cancel(void){
    for(int i=0;i<2;i++){end_pulse(i);if(syskeys[i].native_down){sys_emit(i,NATIVE_SYSTEM,false);syskeys[i].native_down=false;}}
    chord_block=syskeys[0].held || syskeys[1].held;
}
static bool sys_button(SDL_Event e,bool menu){
    int i=e.cbutton.button==slots[SELECT_ROW]?0:e.cbutton.button==slots[START_ROW]?1:-1;
    if(i<0)return false;
    struct SystemKey *s=syskeys+i;bool press=e.type==SDL_CONTROLLERBUTTONDOWN;
    if(press==s->held)return true;
    s->held=press;s->source=e;
    if(menu || chord_block){chord_block=syskeys[0].held || syskeys[1].held;return true;}
    if(idx[SELECT_ROW]==NATIVE_SYSTEM && idx[START_ROW]==NATIVE_SYSTEM){
        s->native_down=press;sys_emit(i,NATIVE_SYSTEM,press);return true;
    }
    if(press){if(syskeys[1-i].held)sys_cancel();return true;}
    /* Never start a custom action until this press is known not to be a chord.
     * Keep the pulse visible to Flash games polling Key.isDown once per frame. */
    end_pulse(i);int binding=idx[SELECT_ROW+i];if(binding){sys_emit(i,binding,true);s->pulse=binding;s->since=SDL_GetTicks();s->frame=shown_frames;}
    return true;
}

/* Isolated queue/proxy regression. No guest machine state or game code. */
#include "render_thread_proxy.h"
#include "../tests/render_worker_fixture.h"
#include <stddef.h>
typedef struct {uint32_t sequence,bytes;unsigned char data[];} Payload;
static unsigned next_sequence,checked_payloads;
static void check_payload(void *arg){
    const Payload *p=arg;assert(p->sequence==next_sequence++);
    for(uint32_t i=0;i<p->bytes;i++)assert(p->data[i]==(unsigned char)(p->sequence*13+i));
    ++checked_payloads;
}
static void barrier_job(void *arg){RenderTestGate *gate;memcpy(&gate,arg,sizeof gate);render_gate_worker_wait(gate);}
static void queue_lifetime(void){
    RenderTestGate gate;render_gate_open(&gate);RenderTestGate *pointer=&gate;
    assert(ds_rt_submit(barrier_job,&pointer,sizeof pointer)==0);render_gate_wait_entered(&gate);
    const unsigned count=12,bytes=32768;Payload *p=malloc(sizeof *p+bytes);assert(p);
    for(unsigned n=0;n<count;n++){
        p->sequence=n;p->bytes=bytes;for(unsigned i=0;i<bytes;i++)p->data[i]=(unsigned char)(n*13+i);
        assert(ds_rt_submit(check_payload,p,sizeof *p+bytes)==0);memset(p,0xa5,sizeof *p+bytes);
    }
    free(p);render_gate_release(&gate);assert(ds_rt_sync()==0);assert(checked_payloads==count);render_gate_close(&gate);
    /* More than twice the byte capacity forces repeated queue wrap/reuse. */
    unsigned packets=(unsigned)(ds_rt_capacity()/65536)*3+5;unsigned first=next_sequence;
    p=malloc(sizeof *p+65536);assert(p);
    for(unsigned n=0;n<packets;n++){
        p->sequence=first+n;p->bytes=65536;for(unsigned i=0;i<65536;i++)p->data[i]=(unsigned char)(p->sequence*13+i);
        assert(ds_rt_submit(check_payload,p,sizeof *p+65536)==0);memset(p,0x5a,sizeof *p+65536);
    }
    free(p);assert(ds_rt_sync()==0);assert(checked_payloads==count+packets);
    /* Oversized copies use the public API's bounded synchronous fallback. */
    size_t large=ds_rt_capacity()+1024;p=malloc(sizeof *p+large);assert(p);p->sequence=next_sequence;p->bytes=(uint32_t)large;
    for(size_t i=0;i<large;i++)p->data[i]=(unsigned char)(p->sequence*13+i);
    unsigned before=checked_payloads;assert(ds_rt_submit(check_payload,p,sizeof *p+large)==0);assert(checked_payloads==before+1);memset(p,0x77,sizeof *p+large);free(p);
    puts("PASS copied payload lifetime, immediate publication, byte-capacity wrap and ordered execution");
}
typedef struct{RenderTestGate *gate;SDL_atomic_t submitted,released;unsigned total;} Pressure;
static int release_pressure(void *arg){
    Pressure *p=arg;Uint32 until=SDL_GetTicks()+5000;
    while(SDL_AtomicGet(&p->submitted)<1){assert((Sint32)(until-SDL_GetTicks())>0);SDL_Delay(1);}
    /* Only the consumer's explicit gate can permit more than capacity bytes.
     * This watchdog observes bounded producer progress, then releases it. */
    SDL_Delay(1000);assert((unsigned)SDL_AtomicGet(&p->submitted)<p->total);
    SDL_AtomicSet(&p->released,1);render_gate_release(p->gate);return 0;
}
static void ignore_payload(void *arg){(void)arg;}
static void queue_backpressure(void){
    RenderTestGate gate;render_gate_open(&gate);RenderTestGate *pointer=&gate;
    assert(ds_rt_submit(barrier_job,&pointer,sizeof pointer)==0);render_gate_wait_entered(&gate);
    Pressure pressure={.gate=&gate,.total=7};SDL_AtomicSet(&pressure.submitted,0);SDL_AtomicSet(&pressure.released,0);
    SDL_Thread *thread=SDL_CreateThread(release_pressure,"backpressure-release",&pressure);assert(thread);
    size_t bytes=ds_rt_capacity()/3;unsigned char *data=malloc(bytes);assert(data);memset(data,0xaa,bytes);
    for(unsigned n=0;n<pressure.total;n++){assert(ds_rt_submit(ignore_payload,data,bytes)==0);SDL_AtomicAdd(&pressure.submitted,1);}
    free(data);assert(SDL_AtomicGet(&pressure.released));int status;SDL_WaitThread(thread,&status);assert(status==0);
    assert(ds_rt_sync()==0);render_gate_close(&gate);puts("PASS bounded backpressure and wakeup after consumer release");
}
typedef struct{uint32_t operation,id,value;} Resource;
static uint32_t resource_value;static int resource_live;static unsigned resource_reads;
static void resource_job(void *arg){
    const Resource *r=arg;assert(r->id==42);
    switch(r->operation){case 0:assert(!resource_live);resource_live=1;resource_value=r->value;break;
    case 1:assert(resource_live);resource_live=0;break;
    case 2:assert(resource_live&&resource_value==r->value);++resource_reads;break;default:abort();}
}
static void queue_resource_reuse(void){
    for(unsigned n=0;n<1000;n++){Resource r={0,42,n};assert(ds_rt_submit(resource_job,&r,sizeof r)==0);r.operation=2;assert(ds_rt_submit(resource_job,&r,sizeof r)==0);r.operation=1;assert(ds_rt_submit(resource_job,&r,sizeof r)==0);}
    assert(ds_rt_sync()==0);assert(resource_reads==1000&&!resource_live);puts("PASS ordered deletion and numeric-handle reuse through public native jobs");
}
typedef struct{SDL_threadID producer,consumer;int owns_context,value;} Borrowed;
static void borrowed_job(void *arg){Borrowed *b=arg;b->consumer=SDL_ThreadID();b->owns_context=SDL_GL_GetCurrentContext()!=NULL;b->value=0x12345678;}
static GLuint checked_shader(GLenum kind,const char *text){
    GLuint shader=glCreateShader(kind);assert(shader);glShaderSource(shader,1,&text,NULL);glCompileShader(shader);
    GLint ok=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[1024];glGetShaderInfoLog(shader,sizeof log,NULL,log);fprintf(stderr,"shader compile: %s\n",log);abort();}return shader;
}
static void proxy_buffer_uniform_lifetime(void){
    Borrowed borrowed={.producer=SDL_ThreadID()};assert(ds_rt_call(borrowed_job,&borrowed)==0);
    assert(borrowed.consumer!=borrowed.producer&&borrowed.owns_context&&borrowed.value==0x12345678);
    GLuint vertex=checked_shader(GL_VERTEX_SHADER,"attribute vec2 p;uniform mat4 m;void main(){gl_Position=m*vec4(p,0.,1.);}");
    GLuint fragment=checked_shader(GL_FRAGMENT_SHADER,"precision mediump float;uniform vec4 color;void main(){gl_FragColor=color;}");
    GLuint program=glCreateProgram();assert(program);glAttachShader(program,vertex);glAttachShader(program,fragment);glBindAttribLocation(program,0,"p");glLinkProgram(program);
    GLint linked=0;glGetProgramiv(program,GL_LINK_STATUS,&linked);assert(linked);glUseProgram(program);
    GLint color=glGetUniformLocation(program,"color"),matrix_location=glGetUniformLocation(program,"m");assert(color>=0&&matrix_location>=0);
    GLuint buffer=0;glGenBuffers(1,&buffer);assert(buffer);glBindBuffer(GL_ARRAY_BUFFER,buffer);glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,0);glEnableVertexAttribArray(0);
    assert(ds_rt_sync()==0);RenderTestGate gate;render_gate_open(&gate);RenderTestGate *pointer=&gate;
    assert(ds_rt_submit(barrier_job,&pointer,sizeof pointer)==0);render_gate_wait_entered(&gate);
    GLfloat zero[6]={0},triangle[6]={-1,-1,3,-1,-1,3},matrix[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},green[4]={0,1,0,1};
    glBufferData(GL_ARRAY_BUFFER,sizeof zero,zero,GL_DYNAMIC_DRAW);memset(zero,0xdd,sizeof zero);
    glBufferSubData(GL_ARRAY_BUFFER,0,sizeof triangle,triangle);memset(triangle,0xee,sizeof triangle);
    glUniformMatrix4fv(matrix_location,1,GL_FALSE,matrix);memset(matrix,0,sizeof matrix);
    glUniform4fv(color,1,green);memset(green,0,sizeof green);
    render_gate_release(&gate);glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,64,64);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);glDrawArrays(GL_TRIANGLES,0,3);
    unsigned char pixel[4]={0};glReadPixels(16,16,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);assert(pixel[0]<3&&pixel[1]>250&&pixel[2]<3&&pixel[3]>250);
    render_gate_close(&gate);render_gate_open(&gate);GLuint element=0;glGenBuffers(1,&element);assert(element);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,element);assert(ds_rt_sync()==0);
    assert(ds_rt_submit(barrier_job,&pointer,sizeof pointer)==0);render_gate_wait_entered(&gate);
    GLushort indices[3]={0,1,2};glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof indices,indices,GL_STREAM_DRAW);memset(indices,0xff,sizeof indices);render_gate_release(&gate);
    glClear(GL_COLOR_BUFFER_BIT);glDrawElements(GL_TRIANGLES,3,GL_UNSIGNED_SHORT,0);memset(pixel,0,sizeof pixel);glReadPixels(16,16,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);assert(pixel[0]<3&&pixel[1]>250&&pixel[2]<3&&pixel[3]>250);
    glDeleteBuffers(1,&element);glDisableVertexAttribArray(0);glDeleteBuffers(1,&buffer);glUseProgram(0);glDeleteProgram(program);glDeleteShader(vertex);glDeleteShader(fragment);
    assert(glGetError()==GL_NO_ERROR);assert(ds_rt_sync()==0);render_gate_close(&gate);
    puts("PASS borrowed synchronous call, shader queries and barrier-backed buffer/subdata/matrix/uniform source lifetimes");
}
static void proxy_gl_checks(void){
    assert(SDL_GL_GetCurrentContext()==NULL);
    glViewport(0,0,64,64);glDisable(GL_DITHER);glDisable(GL_SCISSOR_TEST);glClearColor(1,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
    GLint viewport[4]={0};glGetIntegerv(GL_VIEWPORT,viewport);assert(viewport[2]==64&&viewport[3]==64);
    unsigned char pixel[4]={0};glReadPixels(8,8,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);assert(pixel[0]>250&&pixel[1]<3&&pixel[2]<3&&pixel[3]>250);
    GLuint texture=0,framebuffer=0;glGenTextures(1,&texture);assert(texture);glBindTexture(GL_TEXTURE_2D,texture);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    unsigned char image[4*4*4];for(unsigned i=0;i<16;i++){image[i*4]=0;image[i*4+1]=255;image[i*4+2]=0;image[i*4+3]=255;}
    assert(ds_rt_sync()==0);RenderTestGate gate;render_gate_open(&gate);RenderTestGate *pointer=&gate;
    assert(ds_rt_submit(barrier_job,&pointer,sizeof pointer)==0);render_gate_wait_entered(&gate);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,4,4,0,GL_RGBA,GL_UNSIGNED_BYTE,image);memset(image,0xcc,sizeof image);render_gate_release(&gate);
    glGenFramebuffers(1,&framebuffer);assert(framebuffer);glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    memset(pixel,0,sizeof pixel);glReadPixels(1,1,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);assert(pixel[0]<3&&pixel[1]>250&&pixel[2]<3&&pixel[3]>250);
    glBindFramebuffer(GL_FRAMEBUFFER,0);glDeleteFramebuffers(1,&framebuffer);glDeleteTextures(1,&texture);
    GLuint replacement=0;glGenTextures(1,&replacement);assert(replacement);glBindTexture(GL_TEXTURE_2D,replacement);glDeleteTextures(1,&replacement);
    assert(glGetError()==GL_NO_ERROR);assert(ds_rt_sync()==0);
    render_gate_close(&gate);puts("PASS actual GLES proxy synchronous queries/readback, barrier-backed upload ownership, resource deletion/new allocation");
}
static void change_unpack_native(void *unused){(void)unused;glPixelStorei(GL_UNPACK_ALIGNMENT,8);}
static void proxy_native_unpack_layout(void){
    assert(ds_rt_call(change_unpack_native,NULL)==0);GLint alignment=0;glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment);assert(alignment==8);
    GLuint texture=0,framebuffer=0;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
    RenderTestGate gate;render_gate_open(&gate);RenderTestGate *pointer=&gate;assert(ds_rt_sync()==0);
    assert(ds_rt_submit(barrier_job,&pointer,sizeof pointer)==0);render_gate_wait_entered(&gate);
    /* Exactly12 accessible bytes: final-row trailing padding is not required. */
    unsigned char *image=malloc(12);assert(image);memset(image,0,12);image[0]=255;image[3]=255;image[9]=255;image[11]=255;
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,2,0,GL_RGBA,GL_UNSIGNED_BYTE,image);memset(image,0xdd,12);free(image);render_gate_release(&gate);
    glGenFramebuffers(1,&framebuffer);glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    unsigned char pixel[4]={0};glReadPixels(0,1,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);assert(pixel[0]<3&&pixel[1]>250&&pixel[2]<3&&pixel[3]>250);
    glBindFramebuffer(GL_FRAMEBUFFER,0);glDeleteFramebuffers(1,&framebuffer);glDeleteTextures(1,&texture);glPixelStorei(GL_UNPACK_ALIGNMENT,1);assert(glGetError()==GL_NO_ERROR);assert(ds_rt_sync()==0);render_gate_close(&gate);
    puts("PASS native-RPC pixel-unpack mutation, padded-row payload and exact last-row extent");
}
typedef struct{RenderTestGate *gate;SDL_atomic_t released;} BorrowRelease;
static int release_borrow(void *arg){BorrowRelease *r=arg;SDL_Delay(100);SDL_AtomicSet(&r->released,1);render_gate_release(r->gate);return 0;}
typedef struct{GLfloat vertices[6];GLushort indices[3];GLenum driver_error;} ClientArrays;
static void client_baseline(void *arg){
    ClientArrays *c=arg;glBindBuffer(GL_ARRAY_BUFFER,0);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);glEnableVertexAttribArray(0);glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,c->vertices);
    while(glGetError()!=GL_NO_ERROR){}glDrawElements(GL_TRIANGLES,3,GL_UNSIGNED_SHORT,c->indices);c->driver_error=glGetError();
}
static void proxy_client_pointer_fallback(void){
    ClientArrays client={.vertices={-1,-1,3,-1,-1,3},.indices={0,1,2}};
    assert(ds_rt_call(client_baseline,&client)==0);RenderTestGate gate;render_gate_open(&gate);RenderTestGate *pointer=&gate;
    assert(ds_rt_submit(barrier_job,&pointer,sizeof pointer)==0);render_gate_wait_entered(&gate);
    BorrowRelease release={.gate=&gate};SDL_AtomicSet(&release.released,0);SDL_Thread *thread=SDL_CreateThread(release_borrow,"borrow-release",&release);assert(thread);
    glDrawElements(GL_TRIANGLES,3,GL_UNSIGNED_SHORT,client.indices);assert(SDL_AtomicGet(&release.released));
    assert(glGetError()==client.driver_error);int status;SDL_WaitThread(thread,&status);assert(status==0);
    glDisableVertexAttribArray(0);assert(ds_rt_sync()==0);render_gate_close(&gate);
    printf("PASS client-pointer borrowed fallback waits for worker; original driver error=%#x preserved\n",client.driver_error);
}
static void proxy_rejected_pointer_keeps_client_state(void){
    ClientArrays client={.vertices={-1,-1,3,-1,-1,3},.indices={0,1,2}};assert(ds_rt_call(client_baseline,&client)==0);
    GLuint buffer=0;glGenBuffers(1,&buffer);assert(buffer);glBindBuffer(GL_ARRAY_BUFFER,buffer);glBufferData(GL_ARRAY_BUFFER,sizeof client.vertices,client.vertices,GL_STATIC_DRAW);
    /* Driver rejects size5, retaining the prior enabled ARRAY_BUFFER0 descriptor. */
    glVertexAttribPointer(0,5,GL_FLOAT,GL_FALSE,0,0);
    RenderTestGate gate;render_gate_open(&gate);RenderTestGate *pointer=&gate;assert(ds_rt_submit(barrier_job,&pointer,sizeof pointer)==0);render_gate_wait_entered(&gate);
    BorrowRelease release={.gate=&gate};SDL_AtomicSet(&release.released,0);SDL_Thread *thread=SDL_CreateThread(release_borrow,"rejected-pointer-release",&release);assert(thread);
    glDrawArrays(GL_TRIANGLES,0,3);assert(SDL_AtomicGet(&release.released));assert(glGetError()==GL_INVALID_VALUE);assert(glGetError()==GL_NO_ERROR);
    int status;SDL_WaitThread(thread,&status);assert(status==0);glDisableVertexAttribArray(0);glDeleteBuffers(1,&buffer);assert(ds_rt_sync()==0);render_gate_close(&gate);
    puts("PASS rejected VAP retains old client descriptor, borrowed draw wait and original GL_INVALID_VALUE");
}
static unsigned shutdown_marker;
static void mark_shutdown(void *arg){unsigned n;memcpy(&n,arg,sizeof n);shutdown_marker+=n;}
static void deferred_error(void){
    ds_rt_origin(0x00540000,1234);glEnable(0xffffffffu);
    /* Plain error query synchronizes and reports driver error, never success. */
    assert(glGetError()==GL_INVALID_ENUM);assert(glGetError()==GL_NO_ERROR);
    glEnable(0xffffffffu);assert(ds_rt_defer_draw_error()==GL_NO_ERROR);assert(ds_rt_sync()!=0);
    const DsRtError *error=ds_rt_error();assert(error&&error->code&&error->guest==0x00540000&&error->frame==1234&&error->sequence&&error->api[0]);
    DsRtError first=*error;ds_rt_origin(0x00654321,4321);glEnable(0xfffffffeu);assert(ds_rt_defer_draw_error()==GL_NO_ERROR);assert(ds_rt_sync()!=0);assert(memcmp(&first,ds_rt_error(),sizeof first)==0);
    int result=ds_rt_stop();assert(result!=0);assert(!ds_rt_active());puts("PASS forced deferred error latching, origin attribution and failed synchronization");
}
int main(int argc,char **argv){
    int errors=argc>1&&!strcmp(argv[1],"--error"),off=argc>1&&!strcmp(argv[1],"--off");RenderFixture fixture=render_fixture_open();
    printf("fixture SDL=%s GL=%s renderer=%s\n",SDL_GetCurrentVideoDriver(),glGetString(GL_VERSION),glGetString(GL_RENDERER));
    if(off){unsetenv("DS_RENDER_THREAD");assert(ds_rt_start(fixture.window,fixture.context)==0);assert(!ds_rt_active());assert(SDL_GL_GetCurrentContext()==fixture.context);
        glEnable(0xffffffffu);assert(ds_rt_defer_draw_error()==GL_INVALID_ENUM);assert(glGetError()==GL_NO_ERROR);assert(ds_rt_sync()==0);assert(ds_rt_stop()==0);assert(SDL_GL_GetCurrentContext()==fixture.context);
        render_fixture_close(&fixture);puts("render_worker_regression=PASS mode=off (main ownership and immediate driver errors)");return 0;}
    assert(!setenv("DS_RENDER_THREAD","1",1));assert(ds_rt_start(fixture.window,fixture.context)==0);assert(ds_rt_active());
    if(errors)deferred_error();else{
        queue_lifetime();queue_backpressure();queue_resource_reuse();proxy_buffer_uniform_lifetime();proxy_gl_checks();proxy_native_unpack_layout();proxy_rejected_pointer_keeps_client_state();proxy_client_pointer_fallback();
        SDL_GL_SwapWindow(fixture.window);assert(ds_rt_sync()==0);puts("PASS queued SwapWindow completion");
        for(unsigned n=0;n<1000;n++){unsigned one=1;assert(ds_rt_submit(mark_shutdown,&one,sizeof one)==0);}
        assert(ds_rt_stop()==0);assert(shutdown_marker==1000);assert(!ds_rt_active());assert(ds_rt_stop()==0);puts("PASS shutdown drains queued work and repeated stop is safe");
    }
    assert(SDL_GL_GetCurrentContext()==fixture.context);assert(glGetString(GL_VERSION));
    render_fixture_close(&fixture);printf("render_worker_regression=%s mode=%s (not gameplay/performance evidence)\n","PASS",errors?"error":"normal");return 0;
}

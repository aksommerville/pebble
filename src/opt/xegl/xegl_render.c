/* xegl_render.c
 * Implements all the OpenGL stuff.
 * We don't touch EGL or X11.
 */
 
#include "xegl_internal.h"

/* GLSL
 */

struct xegl_vertex {
  GLshort x,y;
  GLfloat tx,ty;
};
 
static const char xegl_vsrc[]=
  "#version 100\n"
  "precision mediump float;\n"
  "uniform vec2 screensize;\n"
  "attribute vec2 apos;\n"
  "attribute vec2 atexcoord;\n"
  "varying vec2 vtexcoord;\n"
  "void main() {\n"
    "vec2 npos=(apos*2.0)/screensize-1.0;\n"
    "gl_Position=vec4(npos,0.0,1.0);\n"
    "vtexcoord=atexcoord;\n"
  "}\n"
"";

static const char xegl_fsrc[]=
  "#version 100\n"
  "precision mediump float;\n"
  "uniform sampler2D sampler;\n"
  "varying vec2 vtexcoord;\n"
  "void main() {\n"
    "gl_FragColor=vec4(texture2D(sampler,vtexcoord).rgb,1.0);\n"
  "}\n"
"";

/* Compile half of one program.
 * <0 for error.
 */
 
static int xegl_render_program_compile(
  struct pblrt_video *driver,
  int pid,int type,
  const GLchar *src,GLint srcc
) {
  GLint sid=glCreateShader(type);
  if (!sid) return -1;
  glShaderSource(sid,1,&src,&srcc);
  glCompileShader(sid);
  GLint status=0;
  glGetShaderiv(sid,GL_COMPILE_STATUS,&status);
  if (status) {
    glAttachShader(pid,sid);
    glDeleteShader(sid);
    return 0;
  }
  GLuint loga=0,logged=0;
  glGetShaderiv(sid,GL_INFO_LOG_LENGTH,&loga);
  if (loga>0) {
    GLchar *log=malloc(loga+1);
    if (log) {
      GLsizei logc=0;
      glGetShaderInfoLog(sid,loga,&logc,log);
      if ((logc>0)&&(logc<=loga)) {
        while (logc&&((unsigned char)log[logc-1]<=0x20)) logc--;
        fprintf(stderr,"Failed to compile %s shader:\n%.*s\n",(type==GL_VERTEX_SHADER)?"vertex":"fragment",logc,log);
        logged=1;
      }
      free(log);
    }
  }
  if (!logged) fprintf(stderr,"Failed to compile %s shader, no further detail available.\n",(type==GL_VERTEX_SHADER)?"vertex":"fragment");
  glDeleteShader(sid);
  return -1;
}

/* Initialize one program.
 * Zero for error.
 */
 
static int xegl_render_program_init(
  struct pblrt_video *driver,
  const char *vsrc,int vsrcc,
  const char *fsrc,int fsrcc
) {
  GLint pid=glCreateProgram();
  if (!pid) return 0;
  if (xegl_render_program_compile(driver,pid,GL_VERTEX_SHADER,vsrc,vsrcc)<0) return 0;
  if (xegl_render_program_compile(driver,pid,GL_FRAGMENT_SHADER,fsrc,fsrcc)<0) return 0;
  glBindAttribLocation(pid,0,"apos");
  glBindAttribLocation(pid,1,"atexcoord");
  glLinkProgram(pid);
  GLint status=0;
  glGetProgramiv(pid,GL_LINK_STATUS,&status);
  if (status) return pid;
  GLuint loga=0,logged=0;
  glGetProgramiv(pid,GL_INFO_LOG_LENGTH,&loga);
  if (loga>0) {
    GLchar *log=malloc(loga+1);
    if (log) {
      GLsizei logc=0;
      glGetProgramInfoLog(pid,loga,&logc,log);
      if ((logc>0)&&(logc<=loga)) {
        while (logc&&((unsigned char)log[logc-1]<=0x20)) logc--;
        fprintf(stderr,"Failed to link GLSL program:\n%.*s\n",logc,log);
        logged=1;
      }
      free(log);
    }
  }
  if (!logged) fprintf(stderr,"Failed to link GLSL program, no further detail available.\n");
  glDeleteProgram(pid);
  return 0;
}

/* Cleanup.
 */
 
void xegl_render_cleanup(struct pblrt_video *driver) {
  if (DRIVER->texid) glDeleteTextures(1,&DRIVER->texid);
  glDeleteProgram(DRIVER->program);
}

/* Init.
 */
 
int xegl_render_init(struct pblrt_video *driver) {

  if (!(DRIVER->program=xegl_render_program_init(
    driver,
    xegl_vsrc,sizeof(xegl_vsrc)-1,
    xegl_fsrc,sizeof(xegl_fsrc)-1
  ))) return -1;
  
  glUseProgram(DRIVER->program);
  DRIVER->u_screensize=glGetUniformLocation(DRIVER->program,"screensize");
  DRIVER->u_sampler=glGetUniformLocation(DRIVER->program,"sampler");

  glGenTextures(1,&DRIVER->texid);
  if (!DRIVER->texid) {
    glGenTextures(1,&DRIVER->texid);
    if (!DRIVER->texid) return -1;
  }
  glBindTexture(GL_TEXTURE_2D,DRIVER->texid);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  
  return 0;
}

/* Recalculate output bounds.
 * If we're scaling down, take the largest possible space and filter linear.
 * Scaling up at least 4x, take the largest possible space and filter nearest-neighbor.
 * Between 1x and 4x, use the largest fitting multiple of framebuffer size, nearest-neighbor.
 * Framebuffer and window dimensions don't change much, so we cache the decisions.
 */
 
static void xegl_render_calculate_bounds(struct pblrt_video *driver,int fbw,int fbh) {
  DRIVER->dstww=driver->w;
  DRIVER->dstwh=driver->h;
  DRIVER->dstfw=fbw;
  DRIVER->dstfh=fbh;
  int xscale=driver->w/fbw;
  int yscale=driver->h/fbh;
  int scale=(xscale<yscale)?xscale:yscale;
  DRIVER->dstw=DRIVER->dsth=0;
  if (scale<1) {
    if (!DRIVER->texfilter) {
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
      DRIVER->texfilter=1;
    }
  } else {
    if (DRIVER->texfilter) {
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
      DRIVER->texfilter=0;
    }
    if (scale<4) {
      DRIVER->dstw=fbw*scale;
      DRIVER->dsth=fbh*scale;
    }
  }
  if (!DRIVER->dstw||!DRIVER->dsth) { // maximize
    int wforh=(fbw*driver->h)/fbh;
    if (wforh<=driver->w) {
      DRIVER->dstw=wforh;
      DRIVER->dsth=driver->h;
    } else {
      DRIVER->dstw=driver->w;
      DRIVER->dsth=(driver->w*fbh)/fbw;
    }
  }
  DRIVER->dstx=(driver->w>>1)-(DRIVER->dstw>>1);
  DRIVER->dsty=(driver->h>>1)-(DRIVER->dsth>>1);
}

/* Draw frame.
 */
 
int xegl_render_commit(struct pblrt_video *driver,const void *fb,int fbw,int fbh) {
  if ((fbw<1)||(fbw>XEGL_FRAMEBUFFER_SIZE_LIMIT)) return -1;
  if ((fbh<1)||(fbh>XEGL_FRAMEBUFFER_SIZE_LIMIT)) return -1;
  
  glViewport(0,0,driver->w,driver->h);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D,DRIVER->texid);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,fbw,fbh,0,GL_RGBA,GL_UNSIGNED_BYTE,fb);
  
  if ((driver->w!=DRIVER->dstww)||(driver->h!=DRIVER->dstwh)||(fbw!=DRIVER->dstfw)||(fbh!=DRIVER->dstfh)) {
    xegl_render_calculate_bounds(driver,fbw,fbh);
  }
  
  if ((DRIVER->dstw<driver->w)||(DRIVER->dsth<driver->h)) {
    glClearColor(0.0f,0.0f,0.0f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
  }
  
  struct xegl_vertex vtxv[]={
    {DRIVER->dstx             ,DRIVER->dsty             ,0.0f,1.0f},
    {DRIVER->dstx             ,DRIVER->dsty+DRIVER->dsth,0.0f,0.0f},
    {DRIVER->dstx+DRIVER->dstw,DRIVER->dsty             ,1.0f,1.0f},
    {DRIVER->dstx+DRIVER->dstw,DRIVER->dsty+DRIVER->dsth,1.0f,0.0f},
  };
  glUseProgram(DRIVER->program);
  glUniform2f(DRIVER->u_screensize,driver->w,driver->h);
  glUniform1i(DRIVER->u_sampler,0);
  glDisable(GL_BLEND);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(struct xegl_vertex),&vtxv[0].x);
  glVertexAttribPointer(1,2,GL_FLOAT,0,sizeof(struct xegl_vertex),&vtxv[0].tx);
  glDrawArrays(GL_TRIANGLE_STRIP,0,4);
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  
  return 0;
}

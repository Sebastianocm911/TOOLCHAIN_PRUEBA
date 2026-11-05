#include "host_api.h"
#include "fs.h"

/* ToolChain Slides: editor ligero de 5 diapositivas de texto (.tcs).
 * Guarda titulos y contenido en texto delimitado; no exporta PPTX/PDF. */
#define SLIDES 5
#define TITLE_CAP 48
#define BODY_CAP 128
#define BG 0xf1f5fb
#define PANEL 0xffffff
#define INK 0x14213a
#define MUTED 0x5a6b85
#define BLUE 0x2470e8
#define BORDER 0xc9d5e6

static host_api_t *os;
static char titles[SLIDES][TITLE_CAP];
static char bodies[SLIDES][BODY_CAP];
static int current, editing_title=1, dirty=1;
static char status_line[72]="Presentacion nueva";
static char presentation_path[FS_PATH_MAX]="/presentacion.tcs";

static int len(const char *s){int n=0;while(s&&s[n])n++;return n;}
static void copy(char *d,int cap,const char *s){int i=0;while(s&&s[i]&&i+1<cap){d[i]=s[i];i++;}d[i]=0;}
static void append(char *d,int cap,const char *s){int n=len(d);for(int i=0;s&&s[i]&&n+1<cap;i++)d[n++]=s[i];d[n]=0;}
static void load(const char *path){
    uint32_t size=0;const char *src=os->read_file(path,&size);
    for(int i=0;i<SLIDES;i++){titles[i][0]=0;bodies[i][0]=0;}
    if(src){
        int slide=0,field=0,n=0;
        for(uint32_t i=0;i<size;i++){
            char ch=src[i];
            if(ch=='|'&&field==0){titles[slide][n]=0;field=1;n=0;}
            else if(ch=='\n'){
                if(field==0)titles[slide][n]=0;else bodies[slide][n]=0;
                if(slide+1<SLIDES)slide++;
                field=0;n=0;
            }else if(ch!='\r'){
                char *dst=field?bodies[slide]:titles[slide];
                int cap=field?BODY_CAP:TITLE_CAP;
                if(n+1<cap)dst[n++]=ch;
            }
        }
        if(field==0)titles[slide][n]=0;else bodies[slide][n]=0;
        copy(status_line,(int)sizeof(status_line),"Presentacion cargada");
    }else{
        copy(titles[0],TITLE_CAP,"Mi presentacion");
        copy(bodies[0],BODY_CAP,"Escribe aqui las ideas principales.");
        copy(status_line,(int)sizeof(status_line),"Nueva; TAB cambia titulo/contenido");
    }
    dirty=1;
}
static void save(void){
    char data[FS_FILE_MAX_SIZE];data[0]=0;
    for(int i=0;i<SLIDES;i++){
        append(data,(int)sizeof(data),titles[i]);
        append(data,(int)sizeof(data),"|");
        append(data,(int)sizeof(data),bodies[i]);
        if(i<SLIDES-1)append(data,(int)sizeof(data),"\n");
    }
    if(os->write_file(presentation_path,data)){
        copy(status_line,(int)sizeof(status_line),"Guardado en /presentacion.tcs");
        os->write("slides.elf: presentacion guardada\n");
    }else{
        copy(status_line,(int)sizeof(status_line),"Error al guardar");
        os->write("slides.elf: error al guardar\n");
    }
    dirty=1;
}
static void add_char(int key){
    char *dst=editing_title?titles[current]:bodies[current];
    int cap=editing_title?TITLE_CAP:BODY_CAP,n=len(dst);
    if(key=='\r'||key=='\n')key=' ';
    if(key>=32&&key<=126&&n+1<cap){dst[n]=(char)key;dst[n+1]=0;dirty=1;}
}
static void backspace(void){
    char *dst=editing_title?titles[current]:bodies[current];
    int n=len(dst);if(n){dst[n-1]=0;dirty=1;}
}
static void draw(void){
    os->clear(BG);
    os->text(18,10,"ToolChain Slides",INK,1);
    os->text(18,38,"Presentaciones ligeras | 5 diapositivas | formato .tcs",MUTED,0);
    os->rect(16,68,os->width()-32,os->height()-142,PANEL);
    os->frame(16,68,os->width()-32,os->height()-142,BORDER);
    os->rect(32,84,os->width()-64,5,BLUE);
    os->text(52,120,titles[current][0]?titles[current]:"Titulo de la diapositiva",INK,1);
    os->text(52,164,editing_title?"Editando titulo (TAB cambia al contenido)":"Editando contenido",BLUE,0);
    int y=210;
    const char *p=bodies[current];
    while(*p&&y<os->height()-116){
        while(*p==' ')p++;
        if(!*p)break;
        char line[88];int n=0;
        while(p[n]&&p[n]!=' '&&n+1<(int)sizeof(line))n++;
        /* Ajuste simple por palabras: cada linea ocupa como maximo 86 bytes. */
        if(p[n]==' '&&n>0&&n+1<(int)sizeof(line))n++;
        for(int i=0;i<n;i++){line[i]=p[i];}
        line[n]=0;
        os->text(54,y,line,INK,0);p+=n;y+=24;
    }
    if(!bodies[current][0])os->text(54,y,"Escribe el contenido con el teclado.",MUTED,0);
    char count[24]="Diapositiva ";count[12]=(char)('1'+current);count[13]=0;
    os->text(18,os->height()-58,count,BLUE,0);
    os->text(18,os->height()-38,status_line,MUTED,0);
    os->text(18,os->height()-18,"←/→ cambiar diapositiva | TAB titulo/contenido | Ctrl+S guardar | ESC salir",MUTED,0);
    os->present();
}
int main(host_api_t *api){
    if(!api||api->abi_version!=HOST_ABI_VERSION){if(api)api->write("slides.elf: ABI incompatible\n");return 1;}
    os=api;if(!api->window("Slides - ToolChainOS",800,520))return 2;
    const char *launch=api->launch_file?api->launch_file():0;
    if(launch&&*launch)copy(presentation_path,(int)sizeof(presentation_path),launch);
    load(presentation_path);
    host_event_t ev;
    while(api->pump(&ev)){
        if(ev.type==HOST_EV_CLOSE)break;
        if(ev.type!=HOST_EV_KEY)continue;
        int k=(int)ev.key;
        if(k==27)break;
        if(k==19){save();continue;}
        if(k==HOST_KEY_LEFT){if(current>0)current--;dirty=1;}
        else if(k==HOST_KEY_RIGHT){if(current+1<SLIDES)current++;dirty=1;}
        else if(k=='\t'){editing_title=!editing_title;dirty=1;}
        else if(k=='\b'||k==127)backspace();
        else add_char(k);
        if(dirty){draw();dirty=0;}
    }
    return api->finish(0);
}

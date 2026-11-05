#include "host_api.h"
#include "fs.h"

/* Writer: editor ligero de texto UTF-8. Guarda texto plano en el FS del SO.
 * No anuncia compatibilidad DOCX/ODT: el FS actual es textual y limitado. */
#define BG 0xf4f7fc
#define PANEL 0xffffff
#define INK 0x14213a
#define MUTED 0x5a6b85
#define BLUE 0x2470e8
#define LIMIT (FS_FILE_MAX_SIZE - 1)
static char buffer[LIMIT + 1];
static int length, dirty = 1;
static host_api_t *os;
static void draw(void) {
    os->clear(BG);
    os->text(20,12,"ToolChain Writer",INK,1);
    os->text(20,40,"Editor de texto ligero | Ctrl no requerido: pulsa F2 logico (tecla S) para guardar",MUTED,0);
    os->rect(18,68,os->width()-36,os->height()-112,PANEL);
    os->frame(18,68,os->width()-36,os->height()-112,0xc9d5e6);
    int x=30,y=82;
    for(int i=0;i<length && y<os->height()-65;i++){
        if(buffer[i]=='\n'){x=30;y+=20;continue;}
        char one[2]={buffer[i],0}; os->text(x,y,one,INK,0);
        x+=8; if(x>os->width()-45){x=30;y+=20;}
    }
    os->text(20,os->height()-30,"Escribe para editar | S: guardar como /nota.txt | ESC: salir",BLUE,0);
    os->present();
}
int main(host_api_t *api){
    if(!api || api->abi_version!=HOST_ABI_VERSION){if(api)api->write("writer.elf: ABI incompatible\n");return 1;}
    os=api; if(!api->window("Writer - ToolChainOS",760,500))return 2;
    buffer[0]=0; length=0;
    host_event_t ev;
    while(api->pump(&ev)){
        if(ev.type==HOST_EV_CLOSE)break;
        if(ev.type==HOST_EV_KEY){
            int k=(int)ev.key;
            if(k==27)break;
            if(k=='s'||k=='S'){
                buffer[length]=0;
                if(api->write_file && api->write_file("/nota.txt",buffer)==0)api->write("writer.elf: guardado /nota.txt\n");
                else api->write("writer.elf: no se pudo guardar /nota.txt\n");
            } else if(k=='\b' && length>0)buffer[--length]=0;
            else if((k=='\n'||(k>=32&&k<127))&&length<LIMIT)buffer[length++]=(char)k;
            dirty=1;
        }
        if(dirty){draw();dirty=0;}
    }
    return api->finish(0);
}

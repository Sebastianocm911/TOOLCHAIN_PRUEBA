#include "host_api.h"

/* Calc Mini: calculadora de una lista de valores separados por comas.
 * Es una primera hoja de calculo textual, no reemplaza XLSX/ODS. */
#define BG 0xf4f7fc
#define INK 0x14213a
#define MUTED 0x5a6b85
#define BLUE 0x2470e8
static host_api_t *os;
static char input[256];
static int used, dirty=1;
static double total, average;
static int count;
static void calculate(void){
    total=0;average=0;count=0;
    int i=0;
    while(i<used){
        int sign=1; if(input[i]=='-'){sign=-1;i++;}
        double v=0; int digits=0;
        while(i<used&&input[i]>='0'&&input[i]<='9'){v=v*10+(input[i++]-'0');digits++;}
        if(i<used&&input[i]=='.'){i++;double place=.1;while(i<used&&input[i]>='0'&&input[i]<='9'){v+=(input[i++]-'0')*place;place*=.1;digits++;}}
        if(digits){total+=sign*v;count++;}
        while(i<used&&input[i]!=',')i++;
        if(i<used&&input[i]==',')i++;
    }
    average=count?total/count:0;
}
static void draw(void){
    os->clear(BG);os->text(22,14,"ToolChain Calc",INK,1);
    os->text(22,48,"Introduce numeros separados por coma y pulsa ENTER.",MUTED,0);
    os->rect(20,82,os->width()-40,58,0xffffff);os->frame(20,82,os->width()-40,58,0xc9d5e6);
    os->text(32,102,input,INK,0);
    os->text(22,170,"RESULTADOS",BLUE,1);
    char line[96]; int p=0;
    const char *a="Cantidad: ";while(*a)line[p++]=*a++;int n=count;char d[12];int z=0;do{d[z++]=(char)('0'+n%10);n/=10;}while(n);while(z)line[p++]=d[--z];line[p]=0;
    os->text(22,210,line,INK,0);
    /* Mostrar total/promedio con precision entera para mantener salida sin libc. */
    int t=(int)total,av=(int)average; char nums[64]="Suma: ";int q=6;
    if(t<0){nums[q++]='-';t=-t;}char rev[16];int r=0;do{rev[r++]=(char)('0'+t%10);t/=10;}while(t);while(r)nums[q++]=rev[--r];nums[q]=0;
    os->text(22,244,nums,INK,0);
    char avg[64]="Promedio (entero): ";q=19;if(av<0){avg[q++]='-';av=-av;}r=0;do{rev[r++]=(char)('0'+av%10);av/=10;}while(av);while(r)avg[q++]=rev[--r];avg[q]=0;os->text(22,274,avg,INK,0);
    os->text(22,os->height()-32,"ENTER calcular | BACKSPACE borrar | ESC salir",BLUE,0);os->present();
}
int main(host_api_t *api){
    if(!api||api->abi_version!=HOST_ABI_VERSION){if(api)api->write("calc.elf: ABI incompatible\n");return 1;}
    os=api;if(!api->window("Calc - ToolChainOS",620,390))return 2;
    input[0]=0;host_event_t ev;
    while(api->pump(&ev)){if(ev.type==HOST_EV_CLOSE)break;if(ev.type==HOST_EV_KEY){int k=(int)ev.key;if(k==27)break;if(k=='\b'&&used)input[--used]=0;else if(k=='\n')calculate();else if(((k>='0'&&k<='9')||k==','||k=='.'||k=='-')&&used<250)input[used++]=(char)k;dirty=1;}if(dirty){draw();dirty=0;}}
    return api->finish(0);
}

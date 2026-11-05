#include "host_api.h"

/* Calc Mini: suma y promedio entero de una lista separada por comas. */
#define BG 0xf4f7fc
#define INK 0x14213a
#define MUTED 0x5a6b85
#define BLUE 0x2470e8
static host_api_t *os;
static char input[256];
static int used, dirty=1, total, average, count;
static void calculate(void){
    total=0;average=0;count=0;int i=0;
    while(i<used){
        int sign=1;if(input[i]=='-'){sign=-1;i++;}
        int v=0,digits=0;
        while(i<used&&input[i]>='0'&&input[i]<='9'){v=v*10+(input[i++]-'0');digits++;}
        if(digits){total+=sign*v;count++;}
        while(i<used&&input[i]!=',')i++;
        if(i<used&&input[i]==',')i++;
    }
    average=count?total/count:0;
}
static void number(char *out,int value){
    int i=0;if(value<0){out[i++]='-';value=-value;}
    char rev[16];int n=0;do{rev[n++]=(char)('0'+value%10);value/=10;}while(value&&n<15);
    while(n)out[i++]=rev[--n];out[i]=0;
}
static void draw(void){
    os->clear(BG);os->text(22,14,"ToolChain Calc",INK,1);
    os->text(22,48,"Introduce enteros separados por coma y pulsa ENTER.",MUTED,0);
    os->rect(20,82,os->width()-40,58,0xffffff);os->frame(20,82,os->width()-40,58,0xc9d5e6);
    os->text(32,102,input,INK,0);os->text(22,170,"RESULTADOS",BLUE,1);
    char line[80]="Cantidad: ";char n[24];number(n,count);int p=10;for(int i=0;n[i];i++)line[p++]=n[i];line[p]=0;
    os->text(22,210,line,INK,0);
    char sum[24],avg[24];number(sum,total);number(avg,average);
    char row[80]="Suma: ";p=6;for(int i=0;sum[i];i++)row[p++]=sum[i];row[p]=0;os->text(22,244,row,INK,0);
    char row2[80]="Promedio entero: ";p=17;for(int i=0;avg[i];i++)row2[p++]=avg[i];row2[p]=0;os->text(22,274,row2,INK,0);
    os->text(22,os->height()-32,"ENTER calcular | BACKSPACE borrar | ESC salir",BLUE,0);os->present();
}
int main(host_api_t *api){
    if(!api||api->abi_version!=HOST_ABI_VERSION){if(api)api->write("calc.elf: ABI incompatible\n");return 1;}
    os=api;if(!api->window("Calc - ToolChainOS",620,390))return 2;
    input[0]=0;host_event_t ev;
    while(api->pump(&ev)){if(ev.type==HOST_EV_CLOSE)break;if(ev.type==HOST_EV_KEY){int k=(int)ev.key;if(k==27)break;if(k=='\b'&&used)input[--used]=0;else if(k=='\n')calculate();else if(((k>='0'&&k<='9')||k==','||k=='-')&&used<250)input[used++]=(char)k;dirty=1;}if(dirty){draw();dirty=0;}}
    return api->finish(0);
}

#include "host_api.h"
#include "fs.h"

/* ToolChain Calc: hoja de calculo ligera de 8 x 6 celdas.
 * Formato CSV nativo; formulas compatibles: =A1+B2, =A1-B2 y =SUM(A1:B3). */
#define ROWS 8
#define COLS 6
#define CELL 18
#define BG 0xf1f5fb
#define PANEL 0xffffff
#define INK 0x14213a
#define MUTED 0x5a6b85
#define BLUE 0x2470e8
#define BORDER 0xc9d5e6
#define SOFT 0xdbe9ff

static host_api_t *os;
static char cells[ROWS][COLS][CELL];
static char edit[CELL];
static int row, col, edit_len, editing, dirty=1;
static char status_line[72]="Nueva hoja: escribe en una celda";
static char sheet_path[FS_PATH_MAX]="/hoja.csv";

static int len(const char *s){int n=0;while(s&&s[n])n++;return n;}
static void copy(char *d,int cap,const char *s){int i=0;while(s&&s[i]&&i+1<cap){d[i]=s[i];i++;}d[i]=0;}
static void append(char *d,int cap,const char *s){int n=len(d);for(int i=0;s&&s[i]&&n+1<cap;i++)d[n++]=s[i];d[n]=0;}
static int plain_number(const char *s,int *value){
    int sign=1,i=0,n=0,digits=0;
    if(s[i]=='-'){sign=-1;i++;}
    else if(s[i]=='+')i++;
    while(s[i]>='0'&&s[i]<='9'){
        int d=s[i++]-'0';if(n>(1000000-d)/10) return 0;
        n=n*10+d;digits++;
    }
    if(!digits||s[i])return 0;
    *value=sign*n;return 1;
}
static int parse_ref(const char *s,int *r,int *c,int *used){
    char ch=s[0];if(ch>='a'&&ch<='z')ch=(char)(ch-32);
    if(ch<'A'||ch>='A'+COLS)return 0;
    int i=1,n=0,digits=0;
    while(s[i]>='0'&&s[i]<='9'){n=n*10+(s[i]-'0');i++;digits++;}
    if(!digits||n<1||n>ROWS)return 0;
    *r=n-1;*c=ch-'A';*used=i;return 1;
}
static int cell_raw_number(int r,int c){
    int n=0;if(r<0||r>=ROWS||c<0||c>=COLS)return 0;
    plain_number(cells[r][c],&n);return n;
}
static int cell_value(int r,int c){
    const char *s=cells[r][c];int n=0;
    if(s[0]!='='){plain_number(s,&n);return n;}
    s++;
    if(s[0]=='S'&&s[1]=='U'&&s[2]=='M'&&s[3]=='('){
        int r1,c1,r2,c2,a,b,u=0,v=0;
        if(!parse_ref(s+4,&r1,&c1,&u)||s[4+u]!=':'||
           !parse_ref(s+5+u,&r2,&c2,&v)||s[5+u+v]!=')')return 0;
        int sum=0;
        for(int rr=r1;rr<=r2;rr++)for(int cc=c1;cc<=c2;cc++)sum+=cell_raw_number(rr,cc);
        return sum;
    }
    int r1,c1,r2,c2,u=0,v=0;
    if(!parse_ref(s,&r1,&c1,&u))return 0;
    char op=s[u];
    if(op!='+'&&op!='-')return 0;
    if(!parse_ref(s+u+1,&r2,&c2,&v)||s[u+1+v])return 0;
    return op=='+'?cell_raw_number(r1,c1)+cell_raw_number(r2,c2):
                     cell_raw_number(r1,c1)-cell_raw_number(r2,c2);
}
static void number_text(int value,char *out,int cap){
    char reverse[16];int n=0,p=0;
    if(value<0){out[p++]='-';value=-value;}
    do{reverse[n++]=(char)('0'+value%10);value/=10;}while(value&&n<(int)sizeof(reverse));
    while(n&&p+1<cap)out[p++]=reverse[--n];out[p]=0;
}
static void display_cell(int r,int c,char *out,int cap){
    if(cells[r][c][0]=='=')number_text(cell_value(r,c),out,cap);
    else copy(out,cap,cells[r][c]);
}
static void load_csv(const char *path){
    uint32_t size=0;const char *src=os->read_file(path,&size);
    for(int r=0;r<ROWS;r++)for(int c=0;c<COLS;c++)cells[r][c][0]=0;
    if(src){
        int r=0,c=0,n=0;
        for(uint32_t i=0;i<size&&r<ROWS;i++){
            char ch=src[i];
            if(ch==','||ch=='\n'||i+1==size){
                if(ch!=','&&ch!='\n'&&n+1<CELL)edit[n++]=ch;
                edit[n]=0;copy(cells[r][c],CELL,edit);n=0;
                if(ch=='\n'){r++;c=0;}else if(c+1<COLS)c++;
            }else if(n+1<CELL)edit[n++]=ch;
        }
        copy(status_line,(int)sizeof(status_line),"CSV cargado");
    }else copy(status_line,(int)sizeof(status_line),"Hoja nueva; Ctrl+S para guardar");
    copy(edit,(int)sizeof(edit),"");edit_len=0;editing=0;dirty=1;
}
static void save_csv(void){
    char csv[FS_FILE_MAX_SIZE];csv[0]=0;
    for(int r=0;r<ROWS;r++){
        for(int c=0;c<COLS;c++){
            append(csv,(int)sizeof(csv),cells[r][c]);
            if(c<COLS-1)append(csv,(int)sizeof(csv),",");
        }
        if(r<ROWS-1)append(csv,(int)sizeof(csv),"\n");
    }
    if(os->write_file(sheet_path,csv)){
        copy(status_line,(int)sizeof(status_line),"Guardado en /hoja.csv");
        os->write("calc.elf: hoja CSV guardada\n");
    }else{
        copy(status_line,(int)sizeof(status_line),"Error al guardar CSV");
        os->write("calc.elf: error al guardar\n");
    }
    dirty=1;
}
static void commit_edit(void){
    if(!editing)return;
    copy(cells[row][col],CELL,edit);
    editing=0;edit_len=0;edit[0]=0;dirty=1;
}
static void move_cell(int dr,int dc){
    commit_edit();
    row+=dr;col+=dc;
    if(row<0)row=0;if(row>=ROWS)row=ROWS-1;
    if(col<0)col=0;if(col>=COLS)col=COLS-1;
    dirty=1;
}
static void draw(void){
    os->clear(BG);
    os->text(18,10,"ToolChain Calc",INK,1);
    os->text(18,38,"Hoja CSV | formulas: =A1+B2, =A1-B2, =SUM(A1:B3)",MUTED,0);
    os->rect(16,62,os->width()-32,30,PANEL);os->frame(16,62,os->width()-32,30,BORDER);
    char label[8]={(char)('A'+col),(char)('1'+row),0};
    os->text(26,70,label,BLUE,0);
    os->text(70,70,editing?edit:cells[row][col],INK,0);
    int cellw=(os->width()-80)/COLS;if(cellw>112)cellw=112;
    int x0=48,y0=112,ch=34;
    for(int c=0;c<COLS;c++){
        int x=x0+c*cellw;
        os->rect(x,y0,cellw,ch,SOFT);os->frame(x,y0,cellw,ch,BORDER);
        char head[2]={(char)('A'+c),0};os->text(x+cellw/2-4,y0+10,head,INK,0);
    }
    for(int r=0;r<ROWS;r++){
        int y=y0+(r+1)*ch;
        char number[4];number_text(r+1,number,(int)sizeof(number));
        os->text(20,y+10,number,MUTED,0);
        for(int c=0;c<COLS;c++){
            int x=x0+c*cellw;
            os->rect(x,y,cellw,ch,(r==row&&c==col)?SOFT:PANEL);
            os->frame(x,y,cellw,ch,(r==row&&c==col)?BLUE:BORDER);
            char value[24];display_cell(r,c,value,(int)sizeof(value));
            os->text(x+6,y+10,value,INK,0);
        }
    }
    char result[32]="Valor calculado: ";char number[16];
    number_text(cell_value(row,col),number,(int)sizeof(number));append(result,(int)sizeof(result),number);
    os->text(18,os->height()-56,result,BLUE,0);
    os->text(18,os->height()-36,status_line,MUTED,0);
    os->text(18,os->height()-17,"Flechas mover | escribe editar | Enter confirmar | Ctrl+S guardar | ESC salir",MUTED,0);
    os->present();
}
int main(host_api_t *api){
    if(!api||api->abi_version!=HOST_ABI_VERSION){if(api)api->write("calc.elf: ABI incompatible\n");return 1;}
    os=api;if(!api->window("Calc - ToolChainOS",760,520))return 2;
    const char *launch=api->launch_file?api->launch_file():0;
    if(launch&&*launch)copy(sheet_path,(int)sizeof(sheet_path),launch);
    load_csv(sheet_path);
    host_event_t ev;
    while(api->pump(&ev)){
        if(ev.type==HOST_EV_CLOSE)break;
        if(ev.type!=HOST_EV_KEY)continue;
        int k=(int)ev.key;
        if(k==27)break;
        if(k==19){commit_edit();save_csv();continue;}
        if(k==HOST_KEY_LEFT){move_cell(0,-1);}
        else if(k==HOST_KEY_RIGHT){move_cell(0,1);}
        else if(k==HOST_KEY_UP){move_cell(-1,0);}
        else if(k==HOST_KEY_DOWN){move_cell(1,0);}
        else if(k=='\r'||k=='\n'){if(editing)commit_edit();else{editing=1;edit_len=0;edit[0]=0;}dirty=1;}
        else if(k=='\b'||k==127){if(editing&&edit_len){edit[--edit_len]=0;dirty=1;}}
        else if(k==HOST_KEY_DELETE){editing=0;edit_len=0;edit[0]=0;cells[row][col][0]=0;dirty=1;}
        else if(k>=32&&k<=126&&edit_len<CELL-1){
            if(!editing){editing=1;edit_len=0;edit[0]=0;}
            edit[edit_len++]=(char)k;edit[edit_len]=0;dirty=1;
        }
        if(dirty){draw();dirty=0;}
    }
    commit_edit();
    return api->finish(0);
}

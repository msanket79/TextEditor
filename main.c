/*** ------------------------------Includes------------------------ ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include<ctype.h>
#include<errno.h>
#include<stdio.h>
#include<stdlib.h>
#include<stdarg.h>
#include<fcntl.h>
#include<termios.h>
#include<string.h>
#include<time.h>
#include<sys/ioctl.h>
#include<unistd.h>

/*** ------------------------------Defines------------------------ ***/
#define TAB_STOP 8
#define CTRL_KEY(k) ((k) & 0x1f)   //ctrl does this 
#define QUIT_TIMES 3

enum editorKey{
    BACKSPACE=127,
    ARROW_LEFT=1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    PAGE_UP,
    PAGE_DOWN,
    HOME_KEY,
    END_KEY
};

/*** ------------------------------Data------------------------ ***/
struct termios orig_termios; //this will store the original terminal attributes f
typedef struct erow{
    int size;
    char *chars;
    int rsize;
    char*render;
    
}erow;

struct editor_configs{
    int cx,cy; //current curson position
    int rx;
    int screenRows;
    int screenCols;
    int rowoff;
    int coloff;
    int numrows;
    char*filename;
    char statusmsg[80];
    time_t statusmsg_time;
    erow *row;
    int dirty;
    
    struct termios orig_termios;
};

struct editor_configs E;

/*** ------------------------------Func Prototypes------------------------ ***/
void editorSetStatusMessage(const char*fmt,...);
void editorRefreshScreen();
char* editorPrompt(char*prompt);


/*** ------------------------------Terminal------------------------ ***/

void die(const char*s){ //prints error message and exits the program

    write(STDOUT_FILENO,"\x1b[2J",4);   //this will clear the screen
    write(STDOUT_FILENO,"\x1b[H",4);     //this will move our cursor to 1row and 1st colmn

    perror(s);
    exit(1);
}

void disableRawMode(){
    if( tcsetattr(STDIN_FILENO,TCSAFLUSH,&E.orig_termios)==-1 ) die("tcsetattr"); //we will set back the orig attr before exiting
}

void enableRawMode() {
    
        
    if( tcgetattr(STDIN_FILENO,&E.orig_termios)==-1 ) die("tcgetattr");  // to get the terminal atributes 
    atexit(disableRawMode); //this will call the disbalerawmode function on exiting the program 
    struct termios raw=orig_termios;
    raw.c_lflag&=~(ECHO | ICANON | ISIG | IEXTEN); //modifying the attributes, setting the echo feature off   c_lflag -->local flags(miscallenous flag) 
    //now ICANON is a flag for canonical attribute we are turning canonical mode off also (input flags start with I but this is a local flag)
    //ISIG-- used to turn off ctrl+c,z
    // IEXTEN --used to turn off ctrl+v which is used to type any character ctrl+c,ctrkl+q without causing them to send signal
    //IXON-- used to turn off ctrl+s,q -- it is a input flag --(ctrl s is used to stop sending transmission(input,output) ctnr q rsume sit)
    // ICRNL -- the carriage return (13,\r) is written as (10,\n) so we are chaning it to 13
    raw.c_iflag&=~(IXON | ICRNL);
    //OPOST ---while printing the ouput the complier print (\n) ad (\r\n) we have to turn it off \r moves the curser to first line and \n moves curser down so now we will also have to add \r in our printf
    raw.c_oflag&=~(OPOST);

    // now some miscallenous flags that need to be turn off (which are already turned off in modern terminals but it is a trandition)
    raw.c_iflag&=~(BRKINT | INPCK | ISTRIP);
    raw.c_cflag |=(CS8);
    // currently read waits infititly to for input we will change the wait time and also the no of input characters before read time outs
    raw.c_cc[VMIN]=0; //min characters to raed before timeout
    raw.c_cc[VTIME]=1; //min time to wait before timeout 100ms

    if( tcsetattr(STDIN_FILENO,TCSAFLUSH,&raw)==-1 ) die("tcsetattr"); //setting back the new attributes to the terminal  TCSAFLUSH tells us to finish all the output before resetiing and ignore all the unread input



}


int editorReadKey() { //waits for one key and then returns it
    int nread;
    char c;
    while( (nread=read(STDIN_FILENO,&c,1)) !=1){
        if(nread==-1 && errno != EAGAIN ) die("read");
    }
    if(c=='\x1b'){
        char buff[3];
        if(read(STDOUT_FILENO,&buff[0],1)!=1) return '\x1b'; //if no key pressed then escape
        if(read(STDOUT_FILENO,&buff[1],1)!=1) return '\x1b'; //if again no key pressed then escape sequence
        if(buff[0]=='['){

            if(buff[1]>='0' && buff[1]<='9'){
                if(read(STDIN_FILENO,&buff[2],1)!=1) return '\x1b';
                if(buff[2]=='~'){
                    switch(buff[1]) {
                        case '1':
                            return HOME_KEY;
                        case '3':
                            return DEL_KEY;
                        case '4':
                            return END_KEY;
                        case '5':
                            return PAGE_UP;
                        case '6':
                            return PAGE_DOWN;
                        case '7':
                            return HOME_KEY;
                        case '8':
                            return END_KEY;
                    }
                }

            }
            else{
                switch (buff[1])
                {
                case 'A': return ARROW_UP;
                case 'B': return ARROW_DOWN;
                case 'C': return ARROW_RIGHT;
                case 'D': return ARROW_LEFT;
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
        
                default:
                    break;
                }
            }
        }
        else if(buff[0]=='O'){
            switch(buff[1]){
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }
            return '\x1b'; 
    }
    else{
        return c;
    }
}

int getCursorPostion(int*rows,int *cols){
    char buf[32];
    unsigned int i=0;
    if(write(STDOUT_FILENO,"\x1b[6n",4)!=4) return -1;

    while(i<sizeof(buf)-1){
        if(read(STDIN_FILENO,&buf[i],1)!=1) break;
        if(buf[i]=='R') break;
        i++;

    }
    buf[i]='\0';
    if(buf[0]!='\x1b' || buf[1]!='[') return -1;
    if(sscanf(&buf[2],"%d;%d",rows,cols)!=2) return -1;
    return 0;

}

int getWindowSize(int *rows,int *cols){ //this will extract the window size of the terminal from ioctl call x1b==27
    struct winsize ws;
    if(ioctl(STDOUT_FILENO,TIOCGWINSZ,&ws)==-1 || ws.ws_col==0){
        if(write(STDOUT_FILENO,"\x1b[999C\x1b[999B",12)!=12) return -1;
        editorReadKey();
        return getCursorPostion(rows,cols);
    }
    else{
        *cols=ws.ws_col;
        *rows=ws.ws_row;
        return 0;
    }
}

/*** ------------------------------row operations------------------------ ***/
int editorRowCxTORx(erow *row,int cx){
    int rx=0;
    int j=0;
    for(;j<cx;j++){
        if(row->chars[j]=='\t'){
            rx+=(TAB_STOP-1)-(rx%TAB_STOP);
        }
        rx++;

    }
    return rx;

}
void editorUpdateRow(erow* row){
    int tabs=0;
    int j=0;
    for(;j<row->size;j++){
        if(row->chars[j]=='\t') tabs++;
    }
    free(row->render);
    row->render=malloc(row->size+tabs*(TAB_STOP-1)+1);
    j=0;
    int idx=0;
    for(j=0;j<row->size;j++){
        if(row->chars[j]=='\t'){
            while(idx%TAB_STOP!=0) row->render[idx++]=' ';
        }
        else{
        row->render[idx++]=row->chars[j];
        }
    }
    row->render[idx]='\0';
    row->rsize=idx;
    
}
void editorInsertRow(int at,const char*s,size_t len){
    if(at<0 || at>E.numrows) return;
    E.row=realloc(E.row,sizeof(erow)*(E.numrows+1));
    memmove(&E.row[at+1],&E.row[at],sizeof(erow)*(E.numrows-at));
    E.row[at].size=len;
    E.row[at].chars=malloc(sizeof(char)*(len+1));
    memcpy(E.row[at].chars,s,sizeof(char)*len);
    E.row[at].chars[len]='\0';
    E.row[at].rsize=0;
    E.row[at].render=NULL;
    E.numrows++;
    editorUpdateRow(&E.row[at]);
    E.dirty++;


}
void editorRowInsertCharacter(erow*row,int at, int c){
    if(at<0 || at >row->size) at=row->size;
    row->chars=realloc(row->chars,row->size+2);
    memmove(&row->chars[at+1],&row->chars[at],row->size-at+1);
    row->size++;
    row->chars[at]=c;
    editorUpdateRow(row);
    E.dirty++;
}
void editorRowDeleteCharacter(erow*row,int at){
    if(at<0 || at>=row->size) return;
    memmove(&row->chars[at],&row->chars[at+1],row->size-at);
    row->size--;
    editorUpdateRow(row);
    E.dirty++;
}

void editorRowInsertString(erow*row,const char*s,int len){
    row->chars=realloc(row->chars,row->size+len+1);
    memcpy(&row->chars[row->size],s,len);
    row->size+=len;
    row->chars[row->size]='\0';
    editorUpdateRow(row);
    E.dirty++;
}
void editorDeleteRow(int at){
    if(at<0||at>=E.numrows) return;
    free(E.row[at].chars);
    free(E.row[at].render);
    memmove(&E.row[at],&E.row[at+1],sizeof(erow)*(E.numrows-at-1));
    E.numrows--;
    E.dirty++;

}
/*** ------------------------------Editor operations------------------------ ***/

void editorInsertCharacter(int c){
    if(E.cy==E.numrows){
        editorInsertRow(E.numrows,"",0); //appending a new empty row
    }
    editorRowInsertCharacter(&E.row[E.cy],E.cx,c);
    E.cx++;
}

void editorDeleteCharacter(){
    if(E.cy==E.numrows) return;
    if(E.cy==0 && E.cx==0) return;
    erow*row=&E.row[E.cy];
    if(E.cx>0){
        editorRowDeleteCharacter(row,E.cx-1);
        E.cx--;
    }
    else{
        editorRowInsertString(&E.row[E.cy-1],E.row[E.cy].chars,E.row[E.cy].size);
        editorDeleteRow(E.cy);
        E.cy--;
        E.cx=E.row[E.cy-1].size;
    }
}

void editorInsertNewline(){
    if(E.cx==0){
        editorInsertRow(E.cy,"",0);
    }
    else{
        erow*row=&E.row[E.cy];
        editorInsertRow(E.cy+1,&E.row[E.cy].chars[E.cx],row->size-E.cx);
        row=&E.row[E.cy]; //we reassign the value becuase above function calls realloc which may cause the pointer to move
        row->size=E.cx;
        row->chars[row->size]='\0';
        editorUpdateRow(row);
    }
    E.cy++;
    E.cx=0;
}
/*** ------------------------------File I/O------------------------ ***/

char* editorRowsToString(int*buflen){
    int tolen=0;
    for(int i=0;i<E.numrows;i++){
        tolen+=E.row[i].size+1;

    }
    *buflen=tolen;
    char*text=(char*)malloc(tolen);
    int j=0;
    for(int i=0;i<E.numrows;i++){
        memcpy(text+j,E.row[i].chars,E.row[i].size);
        j+=E.row[i].size;
        *(text+j)='\n';
        j++;

    }
    return text;

}

void editorOpen(char*filename){
    free(E.filename);
    E.filename=strdup(filename);
    FILE*fp=fopen(filename,"r");
    if(!fp) die("fopen");
    char *line=NULL;
    size_t linecap=0;
    ssize_t linelen;
    while(((linelen=getline(&line,&linecap,fp))!=-1)){
        while(linelen>0 && (line[linelen-1]=='\n' || line[linelen-1]=='\r')) linelen--;
        editorInsertRow(E.numrows,line,linelen);

    }
    fclose(fp);
    free(line);
    E.dirty=0;

}

void editorSave(){
    if(E.filename==NULL){
        E.filename=editorPrompt("Save as: %s");
    }
    int len;
    if(E.dirty==0) return;
    char*text=editorRowsToString(&len);
    int fd=open(E.filename,O_RDWR | O_CREAT,0644);

    if(fd!=-1){
        
        if( ftruncate(fd,len)!=-1){
            if( write(fd,text,len)!=-1){
                free(text);
                close(fd);
                E.dirty=0;
                editorSetStatusMessage("%d bytes written to the disk",len);
                return;

            }
        }
    close(fd);
    }
   
   
    free(text);
    editorSetStatusMessage("can't save! I/O error : %d",strerror(errno));
    
}

/*** ------------------------------Append Buffer------------------------ ***/

struct appendbuff
{
    char *b;
    int len;
};
#define APPENDBUFF_INIT {NULL,0};

void appendbuffAppend(struct appendbuff *ab,const char *s,int len){
    char *new_appendbuff=realloc(ab->b,ab->len+len);
    if(new_appendbuff==NULL) return;
    memcpy(&new_appendbuff[ab->len],s,len);
    ab->b=new_appendbuff;
    ab->len=ab->len+len;
}

void appendbuffFree(struct appendbuff *ab){
    free(ab->b);
}




/*** ------------------------------Output------------------------ ***/
void editorScroll(){
    E.rx=0;
    if(E.cy<E.numrows){
        E.rx=editorRowCxTORx(&E.row[E.cy],E.cx);
    }
    if(E.cy<E.rowoff){
        E.rowoff=E.cy;
    }
    if(E.cy>=E.rowoff+E.screenRows){
        E.rowoff=E.cy-E.screenRows+1;
    }
    // if(E.cx<E.coloff){
    //     E.coloff=E.cx;
    // }
    // if(E.cx>=E.coloff+E.screenCols){
    //     E.coloff=E.cx-E.screenCols+1;
    // }
    if(E.rx<E.coloff){
        E.coloff=E.rx;
    }
    if(E.rx>E.coloff+E.screenCols){
        E.coloff=E.rx-E.screenCols+1;
    }
}
void editorDrawRows(struct appendbuff*ab){
    int y;
    for( y=0;y<E.screenRows;y++){
        int filerow=y+E.rowoff;
        if(filerow>=E.numrows){
        if(E.numrows==0 && y==E.screenRows/3){
            // appendbuffAppend(ab,"sanket",6);
            char welcome_message[80];
            int welcome_len=snprintf(welcome_message,sizeof(welcome_message),"Sanket's Editor %s","1.0.0.1");
            
          

            if(welcome_len>E.screenCols) welcome_len=E.screenCols; //if the messgae goes out of scrren trim the length
            int padding=(E.screenCols-welcome_len)/2; //for centering the message
           int rpadding=padding;
            if(padding){
                 appendbuffAppend(ab,"~",1);
                 padding--;
            }

            while(padding--) appendbuffAppend(ab,"~",1);
            appendbuffAppend(ab,welcome_message,welcome_len);
            if(rpadding){
                 appendbuffAppend(ab,"~",1);
                 rpadding--;
            }

            while(rpadding--) appendbuffAppend(ab,"~",1);
            
        }
        else{
            appendbuffAppend(ab,"~",1);
        }
    }
    else{
        int len=E.row[filerow].rsize-E.coloff;
        if(len<0) len=0;
        if(len>E.screenCols) len=E.screenCols;
        appendbuffAppend(ab,&E.row[filerow].render[E.coloff],len);
    }
        appendbuffAppend(ab,"\x1b[K",3); //clears the current line

            appendbuffAppend(ab,"\r\n",2);
    }

}
void editorDrawStatusBar(struct appendbuff *ab){
    appendbuffAppend(ab,"\x1b[7m",4); //to invert the colours of status bar
    char status[80],rstatus[80];
    int len=snprintf(status,sizeof(status),"%.20s - %d lines %s",E.filename?E.filename:"[No Name]",E.numrows,
    (E.dirty? "(modified)":""));
    int rlen=snprintf(rstatus,sizeof(rstatus),"%d/%d",E.cy+1,E.numrows);
    if(len>E.screenCols) len=E.screenCols;
    appendbuffAppend(ab,status,len);
    while(len<E.screenCols){
        if(E.screenCols-len==rlen){
            appendbuffAppend(ab,rstatus,rlen);
            break;
        }
        else{
        appendbuffAppend(ab," ",1);
        len++;
        }
    }
    appendbuffAppend(ab,"\x1b[m",3); //to change the colour back to normal
    // appendbuffAppend(ab,"\r\n",2); 

}
void editorDrawMessageBar(struct appendbuff*ab){
    appendbuffAppend(ab, "\x1b[K", 3);
    int msglen=strlen(E.statusmsg);
    if(msglen>E.screenCols) msglen=E.screenCols;
    if(msglen && time(NULL)-E.statusmsg_time<5)
        appendbuffAppend(ab,E.statusmsg,msglen);
    appendbuffAppend(ab,"\r\n",2);
}
void editorRefreshScreen(){ 
    editorScroll();
    struct appendbuff ab=APPENDBUFF_INIT;
    appendbuffAppend(&ab,"\x1b[?25l",6); //to hide the cursor while refreshing
    // appendbuffAppend(&ab,"\x1b[2J",4);//this will clear the screen
    appendbuffAppend(&ab,"\x1b[H",3);   //this will move our cursor to 1row and 1st colmn

    editorDrawRows(&ab);
    editorDrawMessageBar(&ab);
    editorDrawStatusBar(&ab);
    char buff[32];
    snprintf(buff,sizeof(buff),"\x1b[%d;%dH",(E.cy-E.rowoff+1),(E.rx-E.coloff)+1);
    appendbuffAppend(&ab,buff,strlen(buff));
    // appendbuffAppend(&ab,"\x1b[H",3);   //this will move our cursor to 1row and 1st colmn
    appendbuffAppend(&ab,"\x1b[?25h",6); //to hide the cursor while refreshing
    write(STDOUT_FILENO,ab.b,ab.len);
    appendbuffFree(&ab);
}
void editorSetStatusMessage(const char*fmt,...){
    va_list args;
    va_start(args,fmt);//it tells to point at the first argument after fmt
    vsnprintf(E.statusmsg,sizeof(E.statusmsg),fmt,args);
    va_end(args); //to clean the list
    E.statusmsg_time=time(NULL);
}


/*** ------------------------------Input------------------------ ***/
char* editorPrompt(char*prompt){
    size_t bufsize=128;
    char*buff=(char*)malloc(bufsize);
    size_t buflen=0;
    while(1){
        editorSetStatusMessage(prompt,buff);
        editorRefreshScreen();
        int c=editorReadKey();
        if(c==DEL_KEY || c== CTRL_KEY('h') || c==BACKSPACE){
            if(buflen!=0) buff[--buflen]='\0';
            
        }
        else if(c=='\x1b'){
            editorSetStatusMessage("");
            free(buff);
            return NULL;
        }
        else if(c=='\r'){
            if(buflen!=0){
                editorSetStatusMessage("");
                return buff;
            }
        }
        else if(!iscntrl(c) && c<128){
            if(buflen==bufsize-1){
                bufsize*=2;
                buff=realloc(buff,bufsize);
            }
            buff[buflen++]=c;
            buff[buflen]='\0';
        }


    }
}

void editorMoveCursor(int key){
    erow* row=(E.cy>E.numrows) ?NULL:&E.row[E.cy];
    switch (key)
    {
    case ARROW_LEFT:
        if(E.cx!=0) E.cx--;
        else if(E.cy>0){
            E.cy--;
            E.cx=E.row[E.cy].size;
        }
        break;
    case ARROW_RIGHT:
        if(row && E.cx<row->size)
         E.cx++;
        else if(row && E.cx==row->size){
            E.cy++;
            E.cx=0;
        }
        break;
    case ARROW_UP:
        if(E.cy!=0) E.cy--;
        break;
    case ARROW_DOWN:
        if(E.cy<E.numrows) E.cy++;
        break;
    }
    row=(E.cy>=E.numrows) ?NULL :&E.row[E.cy];
    int rowlen=row?row->size:0;
    if(E.cx>rowlen){
        E.cx=rowlen;
    }
}
void editorProcessKey(){
    static int quit_times=QUIT_TIMES;
    int c=editorReadKey();

    switch(c){
        case '\r':
            editorInsertNewline();
            break;
        case CTRL_KEY('q'):
            if(E.dirty && quit_times>0){
                editorSetStatusMessage("Warning!!! File has unsaved changes. Press ctrl-Q %d more times to quit",quit_times);
                quit_times--;
                return;
            }
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
        case CTRL_KEY('s'):
            editorSave();
            break;
        case HOME_KEY:
            E.cx=0;
            break;
        case END_KEY:
        if(E.cy<E.numrows)
            E.cx=E.row[E.cy].size;
        break;
        case BACKSPACE:
        case CTRL_KEY('h'):
        case DEL_KEY:
            if(c==DEL_KEY) editorMoveCursor(ARROW_RIGHT);
            editorDeleteCharacter();
            break;
        case PAGE_UP:
        case PAGE_DOWN:
            {
                if(c==PAGE_UP){
                    E.cy=E.rowoff;
                }
                else if(c==PAGE_DOWN){
                    E.cy=E.rowoff+E.screenRows-1;
                    if(E.cy>E.numrows) E.cy=E.numrows;
                }

                int times=E.screenRows;
                while(times--){
                    editorMoveCursor(c==PAGE_UP?ARROW_UP:ARROW_DOWN);
                }
            }
            break;
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;
        case CTRL_KEY('l'):
            break;
        default:
            editorInsertCharacter(c);
            break;
    }
    quit_times=QUIT_TIMES;

}
    // while( read(STDIN_FILENO,c,1)==1); // STDIN_FILENO 0 is a define for stdin, read returns the number of bytes it read 
    // By default the terminal starts in canonical __mode
        // canonical mode -- input is sent when the user hits enter
        //raw mode-- in this each keypress is sent to the program when user hits -- we want this mode because we want live editing

        
/*** ------------------------------Intialization------------------------ ***/
void initEditor(){
    E.cx=0;
    E.cy=0;
    E.rx=0;
    E.numrows=0;
    E.row=NULL;
    E.rowoff=0;
    E.coloff=0;
    if(getWindowSize(&E.screenRows,&E.screenCols)==-1) die("getWindowSize");
    E.screenRows-=2;
    E.filename=NULL;
    E.statusmsg[0]='\0';
    E.statusmsg_time=0;
    E.dirty=0;
}


int main(int argc,char*argv[]){
    initEditor();
    enableRawMode();
    if(argc>=2){
    editorOpen(argv[1]);
    }
    editorSetStatusMessage("HELP: Ctrl-S =save | Ctrl-Q =quit");
    while(1){
        editorRefreshScreen();
        editorProcessKey();
    }

    return 0;
}
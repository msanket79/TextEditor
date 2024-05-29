/*** ------------------------------Includes------------------------ ***/
#include<ctype.h>
#include<errno.h>
#include<stdio.h>
#include<stdlib.h>
#include<termios.h>
#include<unistd.h>
#include<string.h>
#include<sys/ioctl.h>

/*** ------------------------------Defines------------------------ ***/
#define CTRL_KEY(k) ((k) & 0x1f)   //ctrl does this 

enum editorKey{
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

struct editor_configs{
    int cx,cy; //current curson position
    struct termios orig_termios;
    int screenRows;
    int screenCols;
};

struct editor_configs E;



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
void editorDrawRows(struct appendbuff*ab){
    int y;
    for( y=0;y<E.screenRows;y++){
        if(y==E.screenRows/3){
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
        appendbuffAppend(ab,"\x1b[K",3); //clears the current line
        if(y<E.screenRows-1)
        {
            appendbuffAppend(ab,"\r\n",2);
        }
    }

}
void editorRefreshScreen(){ 
    struct appendbuff ab=APPENDBUFF_INIT;
    appendbuffAppend(&ab,"\x1b[?25l",6); //to hide the cursor while refreshing
    // appendbuffAppend(&ab,"\x1b[2J",4);//this will clear the screen
    appendbuffAppend(&ab,"\x1b[H",3);   //this will move our cursor to 1row and 1st colmn

    editorDrawRows(&ab);
    char buff[32];
    snprintf(buff,sizeof(buff),"\x1b[%d;%dH",E.cy+1,E.cx+1);
    appendbuffAppend(&ab,buff,strlen(buff));
    // appendbuffAppend(&ab,"\x1b[H",3);   //this will move our cursor to 1row and 1st colmn
    appendbuffAppend(&ab,"\x1b[?25h",6); //to hide the cursor while refreshing
    write(STDOUT_FILENO,ab.b,ab.len);
    appendbuffFree(&ab);
}


/*** ------------------------------Input------------------------ ***/
void editorMoveCursor(int key){
    switch (key)
    {
    case ARROW_LEFT:
        if(E.cx!=0) E.cx--;
        break;
    case ARROW_RIGHT:
        if(E.cx!=E.screenCols-1) E.cx++;
        break;
    case ARROW_UP:
        if(E.cy!=0) E.cy--;
        break;
    case ARROW_DOWN:
        if(E.cy!=E.screenRows-1) E.cy++;
        break;
    }
}
void editorProcessKey(){
    int c=editorReadKey();

    switch(c){
        case CTRL_KEY('q'):
            exit(0);
            break;
        case HOME_KEY:
            E.cx=0;
            break;
        case END_KEY:
            E.cx=E.screenCols-1;
            break;
        case PAGE_UP:
        case PAGE_DOWN:
            {
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
    }

}
    // while( read(STDIN_FILENO,c,1)==1); // STDIN_FILENO 0 is a define for stdin, read returns the number of bytes it read 
    // By default the terminal starts in canonical __mode
        // canonical mode -- input is sent when the user hits enter
        //raw mode-- in this each keypress is sent to the program when user hits -- we want this mode because we want live editing

        
/*** ------------------------------Intialization------------------------ ***/
void initEditor(){
    if(getWindowSize(&E.screenRows,&E.screenCols)==-1) die("getWindowSize");
}


int main(){
    initEditor();
    enableRawMode();

    while(1){
        editorRefreshScreen();
        editorProcessKey();
    }

    return 0;
}
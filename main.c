/*** ------------------------------Includes------------------------ ***/
#include<ctype.h>
#include<errno.h>
#include<stdio.h>
#include<stdlib.h>
#include<termios.h>
#include<unistd.h>

/*** ------------------------------Data------------------------ ***/
struct termios orig_termios; //this will store the original terminal attributes f



/*** ------------------------------Terminal------------------------ ***/
void die(const char*s){ //prints error message and exits the program
    perror(s);
    exit(1);
}
void disableRawMode(){
    if( tcsetattr(STDIN_FILENO,TCSAFLUSH,&orig_termios)==-1 ) die("tcsetattr"); //we will set back the orig attr before exiting
}

void enableRawMode() {
    
        
    if( tcgetattr(STDIN_FILENO,&orig_termios)==-1 ) die("tcgetattr");  // to get the terminal atributes 
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

    // while( read(STDIN_FILENO,c,1)==1); // STDIN_FILENO 0 is a define for stdin, read returns the number of bytes it read 
    // By default the terminal starts in canonical __mode
        // canonical mode -- input is sent when the user hits enter
        //raw mode-- in this each keypress is sent to the program when user hits -- we want this mode because we want live editing

        
/*** ------------------------------Intialization------------------------ ***/
int main(){
    enableRawMode();


    // we will make our program exit whenever it reads q
    while(1){
        char c='\0';
        if( read(STDIN_FILENO,&c,1)==-1 && errno!= EAGAIN ) die("read"); //eagain is thrown after timeout
        if(iscntrl(c)){ //if the charater is control character we will only print ascii
            printf("%d\r\n", c);

        }
        else{
            printf("%d (%c)\r\n",c,c);
        }
        if(c=='q') break;
        
    }
    return 0;
}
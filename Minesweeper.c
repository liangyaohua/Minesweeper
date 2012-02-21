#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <ncurses.h>

/* 雷区的范围 */
#define MINEAREA_WIDTH      9
#define MINEAREA_LENGTH     9
/* 雷的个数 */
#define MINE_NUMBER         10
 
/*
* 将每个方块的状态分类：
* 1.初始状态
* 2.判定为雷（也就是通常的插旗）
* 3.排除（若周边有n个雷则显示为n，0则显示为空，用-1来表示雷）
*/
#define SQUARE_INIT         0
#define SQUARE_FLAG         1
#define SQUARE_CLEAN        2
#define SQUARE_ZERO         0
#define SQUARE_MINE         -1

/* 显示图形 */
#define GRAPH_INIT          '.'
#define GRAPH_MINE          '@'
#define GRAPH_NULL          ' '
#define GRAPH_FLAG          'F'
 
#define NEWLINE             addch('\n')
#define _W(y)               (y * 2 + 3)
#define _L(x)               (x * 3 + 1)
/* 设置光标 */
#define SET_CURSOR(y, x)    mvchgat(_W(y), _L(x), 2, A_REVERSE, 0, NULL)
#define CLEAN_CURSOR(y, x)  mvchgat(_W(y), _L(x), 2, A_NORMAL,  0, NULL)
 
#define WPRINT_NUMBER(y, x, v)   \
     mvprintw(y, x, "%d", v)
#define WPRINT_CHAR(y, x, c)     \
     mvaddch(y, x, c)
 
/* 光标的位置 */
int g_cur_y = 0;
int g_cur_x = 0;
 
struct square_t {
     int type;
     int mine;
};
 
/* timer process function */
int timer_p();
void sig_refresh_time(int signum);
  
int init_mine(struct square_t square[MINEAREA_WIDTH][MINEAREA_LENGTH]);
int check_yx(int y, int x);
int game_loop(struct square_t square[MINEAREA_WIDTH][MINEAREA_LENGTH]);
int clean_zero_squares(struct square_t square[MINEAREA_WIDTH][MINEAREA_LENGTH], int cur_y, int cur_x);
  
/* window functions */
int win_init(int width, int length, int mine_num);
int win_refresh(struct square_t square[MINEAREA_WIDTH][MINEAREA_LENGTH], int width, int length, int mines);
int win_refresh_remine_mines(int mines);
int win_refresh_secs(int secs);
int win_mv_cursor(int delta_y, int delta_x, int width, int length);
int win_destroy();
int win_bang();
int win_win();
int win_game_over();
 
int main()
{
	   printf("\nThis program will demonstrate curses.  It will enter curses\n");
     char response;
   scanf(%c,&response);
     int pid_timer;
     int pid_main;
 
     switch (pid_timer = fork()) {
     case 0:
         /* timer进程，用作计时器 */
         timer_p();
         _exit(0);
     case -1:
         perror("fork() error!");
         return -1;
     default:
         /* main process */
         break;
     }
 
     pid_main = getpid();
 
     /* SIGUSR1信号用来刷新显示时间 */
     if (signal(SIGUSR1, sig_refresh_time) == SIG_ERR)
         return -1;
 
     struct square_t square[MINEAREA_WIDTH][MINEAREA_LENGTH];
 
     if (init_mine(square) == -1) return -1;
 
     win_init(MINEAREA_WIDTH, MINEAREA_LENGTH, MINE_NUMBER);
 
     /* 主循环 */
     game_loop(square);
 
     win_game_over();
 
     /* 主进程结束前需要结束timer子进程 */
     kill(pid_timer, SIGKILL);
 
     int key = -1;
     do {
         key = getch();
     }
     while (key != 'y' && key != 'Y');
 
     wait(NULL);
     win_destroy();
 
     return 0;
 }
 
 /* 初始化雷区信息 */
 int init_mine(struct square_t square[MINEAREA_WIDTH][MINEAREA_LENGTH])
 {
     if (square == NULL)
         return -1;
 
     printf("waiting...\n");
 
     int n,m;
     for (n = 0; n < 9; ++n) {
         for (m = 0; m < 9; ++m) {
             square[n][m].type = 0;
             square[n][m].mine = 0;
         }
     }
 
     int i;
     int y, x;
 
     srandom((int)time(NULL));
 
     for (i = 0; i < MINE_NUMBER; ++i) {
         y = random() % MINEAREA_WIDTH;
         x = random() % MINEAREA_LENGTH;
 
         if (square[y][x].mine == SQUARE_MINE) {
             --i;
         }
         else {
             square[y][x].mine = SQUARE_MINE;
 
             if (check_yx(y-1, x  ) == 0 && square[y-1][x  ].mine != SQUARE_MINE)
                 ++square[y-1][x  ].mine;
 
             if (check_yx(y+1, x  ) == 0 && square[y+1][x  ].mine != SQUARE_MINE)
                 ++square[y+1][x  ].mine;
 
             if (check_yx(y  , x-1) == 0 && square[y  ][x-1].mine != SQUARE_MINE)
                 ++square[y  ][x-1].mine;
 
             if (check_yx(y  , x+1) == 0 && square[y  ][x+1].mine != SQUARE_MINE)
                 ++square[y  ][x+1].mine;
 
             if (check_yx(y-1, x-1) == 0 && square[y-1][x-1].mine != SQUARE_MINE)
                 ++square[y-1][x-1].mine;
 
             if (check_yx(y+1, x-1) == 0 && square[y+1][x-1].mine != SQUARE_MINE)
                 ++square[y+1][x-1].mine;
 
             if (check_yx(y-1, x+1) == 0 && square[y-1][x+1].mine != SQUARE_MINE)
                 ++square[y-1][x+1].mine;
 
             if (check_yx(y+1, x+1) == 0 && square[y+1][x+1].mine != SQUARE_MINE)
                 ++square[y+1][x+1].mine;
         }
     }
 
     return 0;
 }
 
 int check_yx(int y, int x)
 {
     if (y >= 0
         && y < MINEAREA_WIDTH
         && x >= 0
         && x < MINEAREA_LENGTH)
     {
         return 0;
     }
 
     return -1;
 }
 
 /* 主循环 */
 int game_loop(struct square_t square[MINEAREA_WIDTH][MINEAREA_LENGTH])
 {
     fd_set rfd;
     FD_ZERO(&rfd);
     FD_SET(0, &rfd);
 
     static int sweeped_mines = 0; /* 判定正确的雷数 */
 
     int ret;
     int input;
     int remain_mines = MINE_NUMBER;
 
     while (1) {
         if ((ret = select(1, &rfd, NULL, NULL, NULL)) <= 0) {
             //return -1;    //当程序被信号中断时select可能会返回-1
             continue;
         }

         switch (input = getch()) {
         /* w,s,a,d方向键 */
         case 'w':
         case 'W':
             win_mv_cursor(-1, 0, MINEAREA_WIDTH, MINEAREA_LENGTH);
             break;
         case 's':
         case 'S':
             win_mv_cursor(+1, 0, MINEAREA_WIDTH, MINEAREA_LENGTH);
             break;
         case 'a':
         case 'A':
             win_mv_cursor(0, -1, MINEAREA_WIDTH, MINEAREA_LENGTH);
             break;
         case 'd':
         case 'D':
             win_mv_cursor(0, +1, MINEAREA_WIDTH, MINEAREA_LENGTH);
             break;
 
         /* 插旗 */
         case 'j':
         case 'J':
             if (square[g_cur_y][g_cur_x].type == SQUARE_INIT) {
                 square[g_cur_y][g_cur_x].type = SQUARE_FLAG;
                 --remain_mines;
 
                 if (square[g_cur_y][g_cur_x].mine == SQUARE_MINE)
                     ++sweeped_mines;
             }
             else if (square[g_cur_y][g_cur_x].type == SQUARE_FLAG) {
                 square[g_cur_y][g_cur_x].type = SQUARE_INIT;
                 ++remain_mines;
 
                 if (square[g_cur_y][g_cur_x].mine == SQUARE_MINE)
                     --sweeped_mines;
             }
             else
                 break;
 
             if (sweeped_mines == MINE_NUMBER) {
                 win_win();
                 goto GAME_OVER;
             }

             win_refresh(square, MINEAREA_WIDTH, MINEAREA_LENGTH, remain_mines);
             break;
 
         /* 打开方块 */
         case 'k':
         case 'K':
             if (square[g_cur_y][g_cur_x].type == SQUARE_CLEAN)
                 break;
             else if (square[g_cur_y][g_cur_x].mine == SQUARE_MINE) {
                 win_bang();
 
                 int n, m;
                 for (n = 0; n < MINEAREA_WIDTH; ++n) {
                     for (m = 0; m < MINEAREA_LENGTH; ++m) {
                         square[n][m].type = SQUARE_CLEAN;
                     }
                 }
 
                 win_refresh(square, MINEAREA_WIDTH, MINEAREA_LENGTH, remain_mines);
                 goto GAME_OVER;
             }
 
             square[g_cur_y][g_cur_x].type = SQUARE_CLEAN;
 
             if (square[g_cur_y][g_cur_x].mine == SQUARE_ZERO)
                 clean_zero_squares(square, g_cur_y, g_cur_x);
 
             win_refresh(square, MINEAREA_WIDTH, MINEAREA_LENGTH, remain_mines);
             break;
 
         /* 退出 */
         case 'q':
         case 'Q':
             goto GAME_OVER;
 
         default:
             break;
         }
     }
 
 GAME_OVER:
     return 0;
 }
 
 /* 如果打开的方块下面是0，则自动打开所有周围为0的方块 */
 int clean_zero_squares(struct square_t square[MINEAREA_WIDTH][MINEAREA_LENGTH], int cur_y, int cur_x)
 {
     if (check_yx(cur_y - 1, cur_x) == 0
         && square[cur_y - 1][cur_x].mine == SQUARE_ZERO
         && square[cur_y - 1][cur_x].type != SQUARE_CLEAN)
     {
         square[cur_y - 1][cur_x].type = SQUARE_CLEAN;
         clean_zero_squares(square, cur_y - 1, cur_x);
     }
 
     if (check_yx(cur_y + 1, cur_x) == 0
         && square[cur_y + 1][cur_x].mine == SQUARE_ZERO
         && square[cur_y + 1][cur_x].type != SQUARE_CLEAN)
     {
         square[cur_y + 1][cur_x].type = SQUARE_CLEAN;
         clean_zero_squares(square, cur_y + 1, cur_x);
     }
 
     if (check_yx(cur_y, cur_x - 1) == 0
         && square[cur_y][cur_x - 1].mine == SQUARE_ZERO
         && square[cur_y][cur_x - 1].type != SQUARE_CLEAN)
     {
         square[cur_y][cur_x - 1].type = SQUARE_CLEAN;
         clean_zero_squares(square, cur_y, cur_x - 1);
     }
 
     if (check_yx(cur_y, cur_x + 1) == 0
         && square[cur_y][cur_x + 1].mine == SQUARE_ZERO
         && square[cur_y][cur_x + 1].type != SQUARE_CLEAN)
     {
         square[cur_y][cur_x + 1].type = SQUARE_CLEAN;
         clean_zero_squares(square, cur_y, cur_x + 1);
     }
 
     return 0;
 }
 
 /*****************************************************************************/
 /* 初始化显示界面 */
 int win_init(int width, int length, int mine_num)
 {
     initscr();
     raw();
     noecho();
     keypad(stdscr, TRUE);
     curs_set(0);
     refresh();
 
     win_refresh_remine_mines(MINE_NUMBER);
     win_refresh_secs(0);
 
     int frame_width  = width  * 2 + 1;
     int frame_length = length * 3 + 1;
     char *line = NULL;
     line = (char*)malloc((frame_length + 1) * sizeof(char));
     memset(line, '-', frame_length);
     *(line + frame_length) = '\0';
     mvprintw(2, 0, line);NEWLINE;
 
     int i, j;
     for (j = 0; j < frame_width - 2; ++j) {
         addch('|');
         for (i = 0; i < length * 2 + 1 - 2; ++i) {
             if (j % 2 == 0) {
                 if (i % 2 == 0) {
                     addch(GRAPH_INIT);addch(' ');
                 }
                 else {
                     addch('|');
                 }
             }
             else {
                 if (i % 2 == 0) {
                     addch('-');addch('-');
                 }
                 else {
                     addch('+');
                 }
             }
         }
         addch('|');NEWLINE;
     }
 
     printw(line);NEWLINE;
 
     /* set cursor position */
     SET_CURSOR(g_cur_y, g_cur_x);
 
     refresh();
     return 0;
 }
 
 /* 刷新显示界面 */
int win_refresh(struct square_t square[MINEAREA_WIDTH][MINEAREA_LENGTH], int width, int length, int mines)
 {
     if (square == NULL)
         return -1;
 
     win_refresh_remine_mines(mines);
 
     int j, i;
     for (j = 0; j < width; ++j) {
         for (i = 0; i < length; ++i) {
             switch (square[j][i].type) {
             case SQUARE_INIT:
                 WPRINT_CHAR(_W(j), _L(i), GRAPH_INIT);
                 break;
             case SQUARE_FLAG:
                 WPRINT_CHAR(_W(j), _L(i), GRAPH_FLAG);
                 break;
             case SQUARE_CLEAN:
                 switch (square[j][i].mine) {
                 case SQUARE_MINE:
                     WPRINT_CHAR(_W(j), _L(i), GRAPH_MINE);
                     break;
                 case SQUARE_ZERO:
                     WPRINT_CHAR(_W(j), _L(i), GRAPH_NULL);
                     break;
                 default:
                     WPRINT_NUMBER(_W(j), _L(i), square[j][i].mine);
                     break;
                 }
                 break;
             default:
                 break;
             }
         }
     }
 
     refresh();
 
     return 0;
 }
 
 int win_refresh_remine_mines(int mines)
 {
     mvprintw(0, 0, "Mines: %d", mines);
     mvprintw(1, 0, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
     refresh();
     return 0;
 }
 
 int win_refresh_secs(int secs)
 {
     mvprintw(0, 15, "Seconds: %d", secs);
     refresh();
     return 0;
 }
 
 
 int win_mv_cursor(int delta_y, int delta_x, int width, int length)
 {
     CLEAN_CURSOR(g_cur_y, g_cur_x);
 
     if (g_cur_y + delta_y < width && g_cur_y + delta_y >= 0)
         g_cur_y += delta_y;
 
     if (g_cur_x + delta_x < length && g_cur_x + delta_x >= 0)
         g_cur_x += delta_x;
 
     SET_CURSOR(g_cur_y, g_cur_x);
 
     refresh();
 
     return 0;
 }
 
 int win_destroy()
 {
     endwin();
 
     return 0;
 }
 
 int win_bang()
 {
     mvprintw(0, 0, "BANG!!!!");
     refresh();
     return 0;
 }
 
 int win_win()
 {
     mvprintw(0, 0, "WIN!!!!");
     refresh();
     return 0;
 }
 
 int win_game_over()
 {
     mvprintw(1, 0, "Game Over!");
     mvprintw(1, 0, "Press 'y' or 'Y' to end.");
     refresh();
     return 0;
 }
 
/*****************************************************************************/
 int timer_p()
 {
     /* 每秒钟给主进程发一次信号 */
     do {
         sleep(1);
         kill(getppid(), SIGUSR1);
     }
     while (1);
 
     return 0;
 }
 
 void sig_refresh_time(int signum)
 {
     static int secs = 0;
     win_refresh_secs(++secs);
 }
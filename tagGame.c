#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "tagGame.h"           // 鬼ごっこモジュールヘッダファイル

#define MAINWIN_LINES   20     // メインウィンドウの高さ(行数)
#define MAINWIN_COLUMS  40     // メインウィンドウの横幅(桁数)
#define MAINWIN_SX      2      // メインウィンドウの左上座標
#define MAINWIN_SY      1      // メインウィンドウの左上座標



#define SUBWIN_LINES   10     // メインウィンドウの高さ(行数)
#define SUBWIN_COLUMS  40     // メインウィンドウの横幅(桁数)
#define SUBWIN_SX      44      // メインウィンドウの左上座標
#define SUBWIN_SY      1      // メインウィンドウの左上座標



#define MOVE_UP         'i'    // 上に移動するキー
#define MOVE_LEFT       'j'    // 左に移動するキー
#define MOVE_DOWN       'k'    // 下に移動するキー
#define MOVE_RIGHT      'l'    // 右に移動するキー

// サーバーが送信するメッセージの最大長さ
// 自分の座標情報(8byte) + 相手の座標情報(8byte) + 自分のいるマップ(8byte) + 相手のいるマップ(8byte) +'\0'
#define SERVER_MSG_LEN   (8 + 8 + 8 + 8 + 1)

// クライアントが送信するメッセージの最大長さ
// 押しているキー情報 + '\0'
#define CLIENT_MSG_LEN   (4 + 1)

//Map関係
int** MAP;//2次元配列 (->マップ用)
int** SUBMAP;

//--------------------------------------------------------------------
//  鬼ごっこゲームモジュール内部で使用する構造体の定義
//--------------------------------------------------------------------

// サーバーに届く入力データ
typedef struct {
  int myKey;                   // ユーザが押しているキー
  int itKey;                   // 相手の押しているキー(クライアントから届く)
  int quit;                    // ゲームを終了させるメッセージが届いた場合に TRUE
} ServerInputData;

// クライアントに届く入力データ
typedef struct {
  int myKey;                   // ユーザが押しているキー
  int myX;                     // 自分の X 座標(サーバーから届く)
  int myY;                     // 自分の Y 座標(サーバーから届く)
  int itX;                     // 相手の X 座標(サーバーから届く)
  int itY;                     // 相手の Y 座標(サーバーから届く)
  int quit;                    // ゲームを終了させるメッセージが届いた場合に TRUE
  int myInMainMap;       // 自分がメインマップにいるか
  int itInMainMap;       // 相手がメインマップにいるか
} ClientInputData;

//--------------------------------------------------------------------
//  鬼ごっこゲームモジュール内部で使用する関数のプロトタイプ宣言
//--------------------------------------------------------------------
static void getServerInputData(TagGame *game, ServerInputData *serverData);
static void getClientInputData(TagGame *game, ClientInputData *clientData);
static void updatePlayerStatus(TagGame *game, ServerInputData *serverData);
static void copyGameState(TagGame *game, ClientInputData *clientData);
static void printGame(TagGame *game);
static void sendGameInfo(TagGame *game);
static void sendMyPressedKey(TagGame *game, ClientInputData *clietData);
static void die();

WINDOW* chooseWin(TagGame *game,Player *character);
int** chooseMap(TagGame *game,Player *character);
//--------------------------------------------------------------------
//  外部に公開する関数の定義
//--------------------------------------------------------------------

/*
 * 鬼ごっこゲームの初期化
 * 引数 :
 *   myChara - 自分を表すキャラクタ
 *   mySX    - 自分の開始 X 座標
 *   mySY    - 自分の開始 Y 座標
 *   itChara - 相手を表すキャラクタ
 *   itSX    - 相手の開始 X 座標
 *   itSY    - 相手の開始 Y 座標
 * 戻値 :
 *   鬼ごっこゲームオブジェクトへのポインタ
 */
TagGame* initTagGame(char myChara, int mySX, int mySY,
                     char itChara, int itSX, int itSY)
{
  TagGame* game = (TagGame *)malloc(sizeof(TagGame));

  // すべてのメンバを 0 で初期化
  bzero(game, sizeof(TagGame));

  //
  // ゲームの論理的データの初期化
  //
  game->my.chara = myChara;
  game->my.x     = mySX;
  game->my.y     = mySY;
  game->it.chara = itChara;
  game->it.x     = itSX;
  game->it.y     = itSY;
  game->my.inMainMap = true;
  game->it.inMainMap = true;

  // 前回のプレイヤー情報を初期化(現在のプレイヤー情報と同じにする)
  memcpy(&game->preMy, &game->my, sizeof(Player));
  memcpy(&game->preIt, &game->it, sizeof(Player));

  //
  // 画面の初期化
  //
  initscr();               // curses ライブラリの初期化
  signal(SIGINT, die);     // Ctrl-C 時に端末を復旧する関数 die を登録
  signal(SIGTERM, die);    // kill 時に端末を復旧する関数 die を登録
  noecho();                // エコーバックの中止
  cbreak();                // キーボードバッファリングの中止

  // ゲーム画面の作成
  game->mainWin = newwin(MAINWIN_LINES, MAINWIN_COLUMS, MAINWIN_SY, MAINWIN_SX);
  game->subWin = newwin(SUBWIN_LINES, SUBWIN_COLUMS, SUBWIN_SY, SUBWIN_SX);

  // 画面が小さい場合
  if (game->mainWin == NULL || game->subWin == NULL) {
    endwin();
    fprintf(stderr, "Error: terminal size is too small\n");
    exit(1);
  }

  // 各制御キーはエスケープシーケンスで表現されている
  // それを論理キーコードに変換する
  keypad(game->mainWin, TRUE);

  return game;
}

/*
 * 鬼ごっこゲームの準備
 * 引数 :
 *   game - 鬼ごっこゲームオブジェクトへのポインタ
 *   s    - 相手との会話用ファイルデスクリプタ
 */
void setupTagGame(TagGame *game, int s)
{
  //
  // データ入力のための準備
  //
  game->s = s;                    // 相手との会話用ファイルデスクリプタを登録
  FD_ZERO(&(game->fdset));        // 入力を監視するファイルデスクリプタマスクを初期化
  FD_SET(0, &(game->fdset));      // 標準入力(キーボード)を監視する
  FD_SET(s, &(game->fdset));      // 相手との会話用デスクリプタを監視する
  game->fdsetWidth = s + 1;       // fdset のビット幅(=最大デスクリプタ番号＋１)
  game->watchTime.tv_sec  = 0;    // 監視時間を 100 msec にセット
  game->watchTime.tv_usec = 100 * 1000;

  //
  // 画面の準備
  //

  // 画面の静的要素の描画
  box(game->mainWin, ACS_VLINE, ACS_HLINE);
  box(game->subWin, ACS_VLINE, ACS_HLINE);

  //マップ生成
  createMap(game,game->mainWin,"O-map.txt");
  createMap(game,game->subWin,"T-map.txt");

  // 物理画面に描画
  wrefresh(game->mainWin);
  wrefresh(game->subWin);
}

/*
 * サーバー側鬼ごっこゲームの開始
 * 引数 :
 *   game - 鬼ごっこゲームオブジェクトへのポインタ
 */
void playServerTagGame(TagGame *game)
{
  ServerInputData serverData;

  while (1) {
    
    // ユーザのキー入力と相手から届いたキー入力データを読む
    getServerInputData(game, &serverData);

    if(game->my.x == game->it.x && game->my.y == game->it.y && game->my.inMainMap == game->it.inMainMap){//鬼が逃げる役に追いついたとき

      showText(game,"You Win",5,15,2);
      showText(game,"Thank you for playing!!",5,8,2);

      serverData.quit = 1;

      break;
    }

    // ユーザもしくは相手から終了のメッセージが届いた場合,終了する
    if (serverData.quit){

      showText(game,"QUIT",5,18,1);

      break;
    }

    // プレイヤーの状態を更新する
    updatePlayerStatus(game, &serverData);

    // 表示する
    printGame(game);

    // ゲームの状態を相手に知らせる
    sendGameInfo(game);
  }

  // 相手も終了するようメッセージを送る
  write(game->s, "quit", 5);  
}


/*
 * クライアント側鬼ごっこゲームの開始
 * 引数 :
 *   game - 鬼ごっこゲームオブジェクトへのポインタ
 */
void playClientTagGame(TagGame *game)
{
  ClientInputData clientData;

  while (1) {
    
    // ユーザのキー入力と, 相手から届いたゲームの状態を読む
    getClientInputData(game, &clientData);

    if(game->my.x == game->it.x && game->my.y == game->it.y && game->my.inMainMap == game->it.inMainMap){//鬼が逃げる役に追いついたとき

      showText(game,"You Lose",5,15,3);
      showText(game,"Thank you for playing!!",5,8,3);

      clientData.quit = 1;

      break;
    }

    // ユーザもしくは相手から終了のメッセージが届いた場合,終了する
    if (clientData.quit){

      showText(game,"QUIT",5,18,1);

      break;
    }

    // ゲームの状態を更新する
    copyGameState(game, &clientData);

    // 表示する
    printGame(game);

    // 自分の押しているキーを相手に送る
    sendMyPressedKey(game, &clientData);
  }

  // 相手も終了するようメッセージを送る
  write(game->s, "quit", 5);  
}

/*
 * 鬼ごっこゲームの後始末
 * 引数 :
 *   game - 鬼ごっこゲームオブジェクトへのポインタ
 */
void destroyTagGame(TagGame *game)
{
  // ウィンドウを廃棄
  delwin(game->mainWin);
  // ファイルデスクリプタを閉じる
  close(game->s);
  // オブジェクトを解放する
  free(game);
  // 端末を元に戻して終了
  die();
}




//--------------------------------------------------------------------
//  外部に公開しない関数の定義
//--------------------------------------------------------------------

/*
 * サーバー側: データが届いているファイルデスクリプタからデータを読む
 * 引数 :
 *   game       - 鬼ごっこゲームオブジェクトへのポインタ
 *   serverData - 届いたデータを格納する ServerInputData 構造体へのポインタ(出力)
 */
static void getServerInputData(TagGame *game, ServerInputData *serverData)
{
  fd_set  arrived   = game->fdset;        // データが届いたファイルデスクリプタの集合
  TimeVal watchTime = game->watchTime;    // ファイルデスクリプタの監視時間
  char    msg[CLIENT_MSG_LEN];            // 相手から届いたメッセージ

  // すべてのメンバを０で初期化
  // データが届いていない場合, メンバの値は０
  bzero(serverData, sizeof(ServerInputData));

  //
  // データが届いているファイルデスクリプタを調べる
  //
  select(game->fdsetWidth, &arrived, NULL, NULL, &watchTime);
  
  //
  // 標準入力 (キーボード, ０番) にデータが届いている場合
  //
  if (FD_ISSET(0, &arrived)) {
    serverData->myKey = wgetch(game->mainWin);    // 押されているキーを読み取る
    // 終了するかどうかチェック
    if (serverData->myKey == 'q')
      serverData->quit = TRUE;
  }

  //
  // 相手との会話用ファイルデスクリプタにデータが届いている場合
  //
  if (FD_ISSET(game->s, &arrived)) {
    read(game->s, msg, CLIENT_MSG_LEN);   // メッセージを読み取る
    
    // 終了するかどうかチェック
    if (strcmp(msg, "quit") == 0)
      serverData->quit = TRUE;
    // 届いたメッセージから押下情報を抽出
    else 
      sscanf(msg, "%3d ", &serverData->itKey);
  }

  // すでにデータが届いていた場合, select() は直ちに終了する
  // そこで余った監視時間だけ休止する(Linux でのみ有効)
  usleep(watchTime.tv_usec); 
  // 休止中にたまったキー入力をクリア
  if (serverData->myKey != 0)  
    flushinp(); 
}

/*
 * クライアント側: データが届いているファイルデスクリプタからデータを読む
 * 引数 :
 *   game       - 鬼ごっこゲームオブジェクトへのポインタ
 *   clientData - 届いたデータを格納する ClientInputData 構造体へのポインタ(出力)
 */
static void getClientInputData(TagGame *game, ClientInputData *clientData)
{
  fd_set  arrived   = game->fdset;        // データが届いたファイルデスクリプタの集合
  TimeVal watchTime = game->watchTime;    // ファイルデスクリプタの監視時間
  char    msg[SERVER_MSG_LEN];            // 相手から届いたメッセージ

  // すべてのメンバを０で初期化
  // データが届いていない場合, メンバの値は０
  bzero(clientData, sizeof(ClientInputData));

  //
  // データが届いているファイルデスクリプタを調べる
  //
  select(game->fdsetWidth, &arrived, NULL, NULL, &watchTime);
  
  //
  // 標準入力 (キーボード, ０番) にデータが届いている場合
  //
  if (FD_ISSET(0, &arrived)) {
    clientData->myKey = wgetch(game->mainWin);    // 押されているキーを読み取る
    // 終了するかどうかチェック
    if (clientData->myKey == 'q')
      clientData->quit = TRUE;
  }

  //
  // 相手との会話用ファイルデスクリプタにデータが届いている場合
  //
  if (FD_ISSET(game->s, &arrived)) {
    read(game->s, msg, SERVER_MSG_LEN);          // メッセージを読み取る
    lseek(game->s, 0, SEEK_END);

    // 終了するかどうかチェック
    if (strcmp(msg, "quit") == 0)
      clientData->quit = TRUE;
    // 届いたメッセージから座標を抽出
    else {
      // 自分と相手の座標情報を抽出
      sscanf(msg, "%3d %3d %3d %3d %3d %3d", &clientData->itX, &clientData->itY, 
                                      &clientData->myX, &clientData->myY, &clientData->itInMainMap, &clientData->myInMainMap);
    }
  }

  // すでにデータが届いていた場合, select() は直ちに終了する
  // そこで余った監視時間だけ休止する(Linux でのみ有効)
  usleep(watchTime.tv_usec); 
  // 休止中にたまったキー入力をクリア
  if (clientData->myKey != 0)  
    flushinp(); 
}

/*
 * プレイヤーの状態を更新する
 * 引数 :
 *   game       - 鬼ごっこゲームオブジェクトへのポインタ
 *   serverData - 鬼ごっこゲームに対する入力データ
 */
static void updatePlayerStatus(TagGame *game, ServerInputData *serverData)
{
  Player *my = &game->my;    // ショートカット
  Player *it = &game->it;    // ショートカット

  // 前回のプレイヤー情報を保存
  memcpy(&game->preMy, &game->my, sizeof(Player));
  memcpy(&game->preIt, &game->it, sizeof(Player));
  
  int **myMap = chooseMap(game,my);
  int **itMap = chooseMap(game,it);

  int myLines = chooseMap_Lines(game,my);
  int myColums = chooseMap_Colums(game,my);
  int itLines = chooseMap_Lines(game,it);
  int itColums = chooseMap_Colums(game,it);


// キーに応じて処理
  switch (serverData->myKey) {
  case KEY_UP:
    if(my->y > 2 && myMap[my->y - 1][my->x] == 3 && myMap[my->y - 2][my->x] == 0) my->y -= 2;//上方向に飛び越え壁があり,２つ上方向に足場がある場合
    break;
  case MOVE_UP: 
    if(myMap[my->y - 1][my->x] == 1 || myMap[my->y - 1][my->x] == 3)//上方向に壁があった場合
    break;
    if(myMap[my->y - 1][my->x] == 2){ //上方向にワープポイントがあった場合
      warp(game,my);
      break;
    }
    if (my->y > 1) my->y--;
    break;

  case KEY_DOWN: 
    if(my->y < myLines - 2 - 1 && myMap[my->y + 1][my->x] == 3 && myMap[my->y + 2][my->x] == 0) my->y += 2;//下方向に飛び越え壁があり,2つ下方向に足場がある場合
    break;
  case MOVE_DOWN:
    if(myMap[my->y + 1][my->x] == 1 || myMap[my->y + 1][my->x] == 3)//下方向に壁があった場合
    break;
    if(myMap[my->y + 1][my->x] == 2){//舌方向にワープポイントがあった場合
      warp(game,my);
      break;
    }
    if (my->y < myLines - 2) my->y++;
    break;

  case KEY_LEFT: 
    if(my->x > 2 && myMap[my->y][my->x - 1] == 3 && myMap[my->y][my->x - 2] == 0) my->x -= 2; //左方向に飛び越え壁があり,2左方向に足場がある場合
    break;
  case MOVE_LEFT:
    if(myMap[my->y][my->x - 1] == 1 || myMap[my->y][my->x - 1] == 3)//左方向に壁があった場合
    break;
    if(myMap[my->y][my->x - 1] == 2){//左方向にワープポイントがあった場合
      warp(game,my);
      break;
    }
    if (my->x > 1) my->x--;
    break;

  case KEY_RIGHT: 
    if(my->x < myColums - 2 - 1 && myMap[my->y][my->x + 1] == 3 && myMap[my->y][my->x + 2] == 0) my->x += 2; //右方向に飛び越え壁があり,2つ右方向に足場がある場合
    break;
  case MOVE_RIGHT:
    if(myMap[my->y][my->x + 1] == 1 || myMap[my->y][my->x + 1] == 3)//右方向に壁があった場合
    break;
    if(myMap[my->y][my->x + 1] == 2){//右方向にワープポイントがあった場合
      warp(game,my);
      break;
    }
    if (my->x < myColums - 2) my->x++;
    break;

  }

  // キーに応じて処理
  switch (serverData->itKey) {
  case KEY_UP:
    if(it->y > 2 && itMap[it->y - 1][it->x] == 3 && itMap[it->y - 2][it->x] == 0) it->y -= 2;//上方向に飛び越え壁があり,2つ上方向に足場ががある場合
    break;
  case MOVE_UP: 
    if(itMap[it->y - 1][it->x] == 1 || itMap[it->y - 1][it->x] == 3)//上方向に壁があった場合
    break;
    if(itMap[it->y - 1][it->x] == 2){ //上方向にワープポイントがあった場合
      warp(game,it);
      break;
    }
    if (it->y > 1) it->y--;
    break;

  case KEY_DOWN: 
    if(it->y < itLines - 2 - 1 && itMap[it->y + 1][it->x] == 3 && itMap[it->y + 2][it->x] == 0) it->y += 2;//下方向に壁があり,2つ下方向に足場がある場合
    break;
  case MOVE_DOWN:
    if(itMap[it->y + 1][it->x] == 1 || itMap[it->y + 1][it->x] == 3)//下方向に壁があった場合
    break;
    if(itMap[it->y + 1][it->x] == 2){//舌方向にワープポイントがあった場合
      warp(game,it);
      break;
    }
    if (it->y < itLines - 2) it->y++;
    break;

  case KEY_LEFT: 
    if(it->x > 2 && itMap[it->y][it->x - 1] == 3 && itMap[it->y][it->x - 2] == 0) it->x -= 2;
    break;
  case MOVE_LEFT:
    if(itMap[it->y][it->x - 1] == 1 || itMap[it->y][it->x - 1] == 3)//左方向に壁があった場合
    break;
    if(itMap[it->y][it->x - 1] == 2){//左方向にワープポイントがあった場合
      warp(game,it);
      break;
    }
    if (it->x > 1) it->x--;
    break;

  case KEY_RIGHT: 
    if(it->x < itColums - 2 - 1 && itMap[it->y][it->x + 1] == 3 && itMap[it->y][it->x + 2] == 0) it->x += 2;
    break;
  case MOVE_RIGHT:
    if(itMap[it->y][it->x + 1] == 1 || itMap[it->y][it->x + 1] == 3)//右方向に壁があった場合
    break;
    if(itMap[it->y][it->x + 1] == 2){//右方向にワープポイントがあった場合
      warp(game,it);
      break;
    }
    if (it->x < itColums - 2) it->x++;
    break;
  }
}

/*
 * ゲームの状態を更新する
 * 引数 :
 *   game       - 鬼ごっこゲームオブジェクトへのポインタ
 *   clientData - 鬼ごっこゲームに対する入力データ
 */
static void copyGameState(TagGame *game, ClientInputData *clientData)
{
  Player *my = &game->my;    // ショートカット
  Player *it = &game->it;    // ショートカット

  // データが届いていなければ, 何もする必要はない
  if (clientData->myX == 0) 
    return; 

  // 前回のプレイヤー情報を保存
  memcpy(&game->preMy, &game->my, sizeof(Player));
  memcpy(&game->preIt, &game->it, sizeof(Player));
  
  // 位置を更新
  my->x = clientData->myX;
  my->y = clientData->myY;
  it->x = clientData->itX;
  it->y = clientData->itY;
  my->inMainMap = clientData->myInMainMap;
  it->inMainMap = clientData->itInMainMap;

}

/*
 * ゲーム画面を表示する
 * 引数 :
 *   game - 鬼ごっこゲームオブジェクトへのポインタ
 */
static void printGame(TagGame *game)
{
  WINDOW *mw    = game->mainWin;    // ショートカット
  Player *my    = &game->my;        // ショートカット
  Player *preMy = &game->preMy;     // ショートカット
  Player *it    = &game->it;        // ショートカット
  Player *preIt = &game->preIt;     // ショートカット
  bool IinMainMap = game->my.inMainMap;
  bool ItinMainMap = game->it.inMainMap;

  WINDOW *myWin = chooseWin(game,my);//自分がいるウィンドウ
  WINDOW *itWin = chooseWin(game,it);//相手がいるウィンドウ
  WINDOW *preMyWin = chooseWin(game,preMy);//自分がいたウィンドウ
  WINDOW *preItWin = chooseWin(game,preIt);//相手がいたウィンドウ

  // 相手の描画 (もし自分と重なった場合, 自分を上に描画したいので相手が先)
  mvwaddch(preItWin, preIt->y, preIt->x, ' ');    // 消去
  mvwaddch(itWin, it->y, it->x, it->chara);    // 表示

  // 自分の描画
  mvwaddch(preMyWin, preMy->y, preMy->x, ' ');    // 消去
  mvwaddch(myWin, my->y, my->x, my->chara);    // 表示

  // 物理画面へ描画
  wrefresh(game->mainWin);
  wrefresh(game->subWin);
}



/*
 * ゲームの状態を相手に知らせる
 * 引数 :
 *   game - 鬼ごっこゲームオブジェクトへのポインタ
 */
static void sendGameInfo(TagGame *game)
{
  Player *my = &game->my;        // ショートカット
  Player *it = &game->it;        // ショートカット
  char    msg[SERVER_MSG_LEN];   // 相手に送るメッセージ

  // 自分の情報が変化していなければ送る必要はない
  if (memcmp(&game->my, &game->preMy, sizeof(Player)) == 0 &&
      memcmp(&game->it, &game->preIt, sizeof(Player)) == 0)
    return;

  //
  // 位置座標をメッセージに変換
  //

  // プレイヤーの座標情報
  sprintf(msg, "%3d %3d %3d %3d %3d %3d", my->x, my->y, it->x, it->y, my->inMainMap, it->inMainMap);

  // 送る
  write(game->s, msg, SERVER_MSG_LEN);    
}

/*
 * 自分の押しているキーを相手に送る
 * 引数 :
 *   game      - 鬼ごっこゲームオブジェクトへのポインタ
 *   clietData - クライアントに対する入力データ
 */
static void sendMyPressedKey(TagGame *game, ClientInputData *clietData)
{
  char    msg[CLIENT_MSG_LEN];   // 相手に送るメッセージ

  // 何も押されていなければ送る必要はない
  if (clietData->myKey == 0)
    return;

  //
  // 位置座標をメッセージに変換
  //

  // プレイヤーの座標情報
  sprintf(msg, "%d", clietData->myKey);

  // 送る
  write(game->s, msg, CLIENT_MSG_LEN);    
}

/*
 * 端末を復元し終了する
 */
static void die()
{
  endwin();
  exit(1);
}



//--------------------------------------------------------------------
//  自作関数
//--------------------------------------------------------------------

void showText(TagGame *game,char *text,int WinX,int WinY,int penID){

  start_color();//カラールーチンを初期化
  init_pair(1,COLOR_WHITE,COLOR_BLACK);//ペンの色を設定
  init_pair(2,COLOR_RED,COLOR_WHITE);
  init_pair(3,COLOR_BLUE,COLOR_WHITE);

  wattron(game->mainWin,COLOR_PAIR(penID));//使うペンを設定
  wbkgd(game->mainWin,COLOR_PAIR(penID));//背景色を塗る

  wclear(game->mainWin);//メインウィンドウのマップを消去
  werase(game->subWin);//サブウィンドウ消去

  box(game->mainWin, ACS_VLINE, ACS_HLINE); // 画面の静的要素の描画
  wmove(game->mainWin,WinX,WinY); //カーソルの位置を指定
  wprintw(game->mainWin,text);  //カーソルの位置に文字出力

  wrefresh(game->mainWin);  //物理画面へ描写
  wrefresh(game->subWin);

  sleep(3);

}

//テキストファイルであるMAPを読み込んで２次元配列に落とし込む
int** readLine(char *mapName){
  /* macros */
  int N = 256;

  FILE *fp;
  char *filename = mapName;
  char readline[N];
  int** map;//2次元配列のマップ
	int i, j = 0;
	int lines, columes;

	// マップ用のメモリ領域の確保
	map = (int**)malloc(sizeof(int*) * N);
	for(i = 0; i < N; i++){
		map[i] = malloc(sizeof(int) * N);
	}
	
    /* ファイルのオープン */
  if ((fp = fopen(filename, "r")) == NULL) {
    fprintf(stderr, "cannot open %s.\n", filename);
    exit(1);
  }
	
	// 1行目の読み込み
	if(fscanf(fp, "%d, %d", &lines, &columes) != EOF){
		//printf("line: %d, columns: %d\n", lines, columes);
	}
	else {
		fprintf(stderr, "format error: %s.\n", filename);
		exit(1);
	}
	
	i = 0;
  /* ファイルの終端まで文字を読み取り表示する */
  while ( fgets(readline, N, fp) != NULL ) {
	// 1行目はスキップ
	if(i > 0){

		// 2行目以降をマップに読み込む
    for(j = 0; j < columes; j++){

      if(readline[j] == ' '){
        map[i-1][j] = 0;
      }
      else if(readline[j] == 'W'){
        map[i-1][j] = 2;
      }
      else if(readline[j] == '+'){
        map [i-1][j] = 3;
      }
      else{
        map[i-1][j] = 1;
      }

    } 

	}
	i++;
}
	
  /*ファイルの表示
	for(i = 0; i < lines; i++){
		for(j = 0; j < columes; j++){
			printf("%d", map[i][j]);
		}
		printf("\n");
	}
	*/

    /* ファイルのクローズ */
    fclose(fp);

    return map;
}


void createMap(TagGame *game,WINDOW *Win,char *mapName) {
  int WinLinesIndex = 0;//縦の描画ループ回数の変数
  int WinColumsIndex = 0;//横のびょがループの回数の変数

  int** map;//描画のためのマップ

  if(Win == game->mainWin){//メインウィンドウを描画するとき
    WinLinesIndex = MAINWIN_LINES;
    WinColumsIndex = MAINWIN_COLUMS;
    MAP = readLine(mapName);
    map = MAP;
  }
  else if(Win == game->subWin){//サブウィンドウを描画するとき
    WinLinesIndex = SUBWIN_LINES;
    WinColumsIndex = SUBWIN_COLUMS;
    SUBMAP = readLine(mapName);
    map = SUBMAP;
  }

  // マップの開始位置を設定
  wmove(Win, 1, 1);//マップの外周は表示させないため(1,1)スタート

  // マップを描画
  for (int i = 1; i < WinLinesIndex - 1; i++) {//縦の描画ループ
    for (int j = 1; j < WinColumsIndex - 1; j++) {//横の描画ループ
      if (map[i][j] == 0){//なにもない場合
        wprintw(Win, " ");
      }
      else if(map[i][j] == 2){//ワープポイントがある場合
        wprintw(Win,"W");
      }
      else if(map[i][j] == 3){
        wprintw(Win,"+");
      }
      else {//壁がある場合
        wprintw(Win, "#");
      }
    }
    // 次の行へカーソルを移動
    wmove(Win, i + 1, 1);
  }

  // ウィンドウの更新
  wrefresh(Win);

}



void warp(TagGame *game,Player *character){

 if(character->inMainMap){//メインウィンドウにいるとき
    character->inMainMap = false;
    character->x = 2;
    character->y = 2;
  }
  else{//サブウィンドウにいるとき
    character->inMainMap = true;
    character->x = 37;
    character->y = 17;
  }

}

//キャラクターがいるウィンドウを取得する
WINDOW* chooseWin(TagGame *game,Player *character){

  if(character->inMainMap == true){//キャラクターがメインマップにいるなら
    return game->mainWin;
  }
  else{
    return game->subWin;
  }

}

//キャラクターがいるマップを取得する
int** chooseMap(TagGame *game,Player *character){

  if(character->inMainMap == true){//キャラクターがメインマップにいるなら
    return MAP;
  }
  else{
    return SUBMAP;
  }

}

//キャラクターがいるマップの行数を取得する
int chooseMap_Lines(TagGame *game,Player *character){

  if(character->inMainMap == true){//キャラクターがメインマップにいるなら
    return MAINWIN_LINES;
  }
  else{
    return SUBWIN_LINES;
  }

}


//キャラクターがいるマップの列数を取得する
int chooseMap_Colums(TagGame *game,Player *character){

  if(character->inMainMap == true){//キャラクターがメインマップにいるなら
    return MAINWIN_COLUMS;
  }
  else{
    return SUBWIN_COLUMS;
  }

}
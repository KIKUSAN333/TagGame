/********************************************************************
                       鬼ごっこゲームモジュール
                            ヘッダファイル
 ********************************************************************/ 
#include <curses.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

//--------------------------------------------------------------------
//   鬼ごっこゲームモジュールにおける型の定義
//--------------------------------------------------------------------
typedef struct timeval TimeVal;  // 簡便のために構造体に別名を定義

/*
 * プレーヤーデータ構造体の定義
 */
typedef struct {
  char    chara;                 // 自分を表すキャラクタ
  int     x;                     // 自分の X 座標
  int     y;                     // 自分の Y 座標
  int    inMainMap;
} Player;

/*
 * 鬼ごっこゲーム構造体の定義
 */
typedef struct {
  // ゲームの論理的データ
  Player  my;                    // 自分のデータ
  Player  preMy;                 // 前回の自分のデータ
  Player  it;                    // 相手のデータ
  Player  preIt;                 // 前回の相手のデータ

  // 画面関連のデータ
  WINDOW *mainWin;               // メインウィンドウ
  WINDOW *subWin;


  // 入力関連のデータ
  int     s;                     // 相手との会話用ファイルデスクリプタ
  fd_set  fdset;                 // 入力を監視するファイルデスクリプタの集合
  int     fdsetWidth;            // fdset のビット幅(=最大デスクリプタ番号＋１)
  TimeVal watchTime;             // 監視時間
} TagGame;


//--------------------------------------------------------------------
//   鬼ごっこゲームモジュールが外部に公開する関数のプロトタイプ宣言
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
                     char itChara, int itSX, int itSY);

/*
 * 鬼ごっこゲームの準備
 * 引数 :
 *   game - 鬼ごっこゲームオブジェクトへのポインタ
 *   s    - 相手との会話用ファイルデスクリプタ
 */
void setupTagGame(TagGame *game, int s);

/*
 * サーバー側鬼ごっこゲームの開始
 * 引数 :
 *   game - 鬼ごっこゲームオブジェクトへのポインタ
 */
void playServerTagGame(TagGame *game);

/*
 * クライアント側鬼ごっこゲームの開始
 * 引数 :
 *   game - 鬼ごっこゲームオブジェクトへのポインタ
 */
void playClientTagGame(TagGame *game);

/*
 * 鬼ごっこゲームの後始末
 * 引数 :
 *   game - 鬼ごっこゲームオブジェクトへのポインタ
 */
void destroyTagGame(TagGame *game);
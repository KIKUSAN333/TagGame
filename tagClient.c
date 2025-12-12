#include <string.h>
#include <unistd.h>

#include "snet.h"           // 一対一通信ライブラリ
#include "tagGame.h"        // 鬼ごっこモジュール

#define PORT       10000    // デフォルトのサーバー側ポート番号
#define HOST_LEN   64       // ホスト名の最大長
#define MY_CHARA   'o'      // 自分を表すキャラクタ
#define MY_SX      10       // 自分の開始 X 座標
#define MY_SY      10       // 自分の開始 Y 座標
#define IT_CHARA   'x'      // 相手を表すキャラクタ
#define IT_SX      1       // 相手の開始 X 座標
#define IT_SY      1       // 相手の開始 Y 座標

int main(int argc, char *argv[]) 
{ 
  char     serverName[HOST_LEN];    // サーバーのホスト名
  int      s;                       // クライアントとの会話用デスクリプタ
  TagGame *game;                    // 鬼ごっこゲーム

  // 鬼ごっこゲームの初期化
  game = initTagGame(MY_CHARA, MY_SX, MY_SY, IT_CHARA, IT_SX, IT_SY);

  // 引数で指定されたホスト名をサーバとする
  // もし引数がなければ自分自身をサーバーとして仮定し，アクセスを試みる
  if (argc == 2) 
    strcpy(serverName, argv[1]);
  else
    gethostname(serverName, HOST_LEN);

  // サーバーを準備する。指定のサーバーの指定のポートに接続すると,サーバー
  // と会話するためのデスクリプタを返す
  s = setupClient(serverName, PORT);

  // 鬼ごっこゲームの準備
  setupTagGame(game, s);

  // 鬼ごっこゲームの開始
  playClientTagGame(game);

  // 鬼ごっこゲームの後始末
  destroyTagGame(game);

  return 0;
} 

#include "snet.h"           // 一対一通信ライブラリ
#include "tagGame.h"        // 鬼ごっこモジュール

#define PORT       10000    // デフォルトのサーバー側ポート番号
#define MY_CHARA   'o'      // 自分を表すキャラクタ
#define MY_SX      1        // 自分の開始 X 座標
#define MY_SY      1        // 自分の開始 Y 座標
#define IT_CHARA   'x'      // 相手を表すキャラクタ
#define IT_SX      10       // 相手の開始 X 座標
#define IT_SY      10       // 相手の開始 Y 座標

int main(int argc, char *argv[]) 
{ 
  int      s;       // クライアントとの会話用デスクリプタ
  TagGame *game;    // 鬼ごっこゲーム

  // 鬼ごっこゲームの初期化
  game = initTagGame(MY_CHARA, MY_SX, MY_SY, IT_CHARA, IT_SX, IT_SY);

  // サーバーを準備する。クライアントが指定のポートに接続すると,
  // クライアントと会話するためのデスクリプタを返す
  s = setupServer(PORT);

  // 鬼ごっこゲームの準備
  setupTagGame(game, s);

  // 鬼ごっこゲームの開始
  playServerTagGame(game);

  // クライアントから切断しないと "can't bind" になるので、少し待つ
  sleep(1);

  // 鬼ごっこゲームの後始末
  destroyTagGame(game);

  return 0;
}

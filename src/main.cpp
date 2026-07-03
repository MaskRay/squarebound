// ============================================================================
//  Squarebound · 方格征途
//  —— 致童年：在五子棋盘上，用黑白棋子与四色标识摆出的战棋梦。
//
//  绿子=主角  蓝/红/黄=英雄  白子正面=我方小兵  白子反面=敌方小兵  黑子=障碍
//  15x15 棋盘（五子棋规格，含星位）。英雄血量以二进制棋子展示（致敬）。
//
//  操作：左键 选择/移动/攻击   右键 待命/取消   空格 结束回合(敌方回合按住加速)
//        Shift+F1~F9 存档     F1~F9 读档       Esc 取消选择/返回标题
// ============================================================================

#include <raylib.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>
#include <vector>

// ------------------------------------------------------------------ 配置
static const int n = 15;           // 棋盘 15x15
static const int cell = 46;        // 格子像素
static const int bx = 30, by = 35; // 棋盘原点
static const int winW = 1280, winH = 760;
static const int panelX = bx + n * cell + 26; // 侧栏起点
static const int panelW = winW - panelX - 24;
static const int healAmt = 4;      // 牧师治疗量
static const int herbAmt = 4;      // 药草回复量
static const int minDmg = 1;       // 最低伤害：攻击不低于此值（防御再高也会掉血）
static const float aiStep = 0.55f; // 敌方单位行动间隔(秒)

// ------------------------------------------------------------------ 调色
static const Color cDesk = {34, 29, 26, 255};
static const Color cWood = {211, 166, 105, 255};
static const Color cWoodE = {188, 141, 82, 255};
static const Color cGrid = {96, 66, 38, 255};
static const Color cGold = {235, 195, 90, 255};
static const Color cText = {235, 228, 215, 255};
static const Color cDim = {168, 158, 143, 255};
static const Color cPanel = {46, 40, 36, 255};
static const Color cGreen = {74, 165, 92, 255};
static const Color cBlue = {76, 116, 205, 255};
static const Color cRed = {203, 84, 71, 255};
static const Color cYellow = {222, 178, 64, 255};
static const Color cIvory = {242, 234, 216, 255};
static const Color cIvoryD = {205, 193, 170, 255};
static const Color cBlackpc = {38, 33, 40, 255};
static const Color cEnemy = {170, 52, 46, 255};
static const Color cPurple = {150, 92, 200, 255};

// ------------------------------------------------------------------ 兵种
enum UType { UGreen, UBlue, URed, UYellow, USoldier, UEsoldier, UEcaptain, UEmage, UEboss, UCount };

struct UnitDef {
  const char *name;
  const char *glyph;
  int hp, atk, def, mov, rng;
  bool enemy, healer, aoe;
  Color body, ring, glyphCol;
};

// clang-format off
//    名称          字  Hp Atk Def Mov Rng 敌   治疗   范围
static const UnitDef defs[UCount] = {
    {"主角·远征者", "主", 12, 4, 1, 3, 3, false, false, false, cGreen,   {32, 92, 48, 255},   RAYWHITE},
    {"蓝骑士",     "骑", 14, 5, 2, 4, 1, false, false, false, cBlue,    {30, 56, 120, 255},  RAYWHITE},
    {"红法师",     "法",  8, 6, 0, 2, 3, false, false, false, cRed,     {120, 40, 32, 255},  RAYWHITE},
    {"黄牧师",     "牧",  9, 2, 0, 3, 2, false, true,  false, cYellow,  {140, 104, 26, 255}, {60, 44, 10, 255}},
    {"义勇兵",     "兵",  1, 2, 0, 2, 2, false, false, false, cIvory,   {110, 120, 150, 255},{50, 60, 90, 255}},
    {"敌方小兵",   "卒",  1, 2, 0, 2, 2, true,  false, false, cIvoryD,  {140, 44, 40, 255},  {130, 36, 32, 255}},
    {"敌将",       "将",  9, 4, 1, 3, 1, true,  false, false, cBlackpc, cEnemy,              {236, 120, 110, 255}},
    {"敌巫",       "巫",  6, 5, 0, 2, 3, true,  false, false, cBlackpc, cPurple,             {200, 160, 240, 255}},
    {"魔王",       "王", 20, 5, 2, 2, 2, true,  false, true,  cBlackpc, cGold,               cGold},
};
// clang-format on

// ------------------------------------------------------------------ 关卡
enum Objective { ObjWipe, ObjReach, ObjBoss };

struct LevelDef {
  const char *name;
  const char *intro;
  Objective obj;
  const char *rows[n];
};

// 图例: . 空地  X 黑子障碍  E 旗帜(终点)  h 药草
//       G 主角  B 蓝骑士  R 红法师  Y 黄牧师  s 我方小兵
//       e 敌兵  c 敌将    m 敌巫    K 魔王
static const LevelDef levels[] = {
    {"初阵",
     "哨探来报：不只是零星敌兵，还有个百夫长压阵。带上蓝骑士，给他们一个教训！",
     ObjWipe,
     {
         "...............",
         "...e...e...e...",
         "..e...e...e....",
         ".......c.......",
         "....e.....e....",
         ".....X...X.....",
         "...............",
         "...............",
         "...............",
         ".....X...X.....",
         "......h........",
         "......s.s......",
         ".....G...B.....",
         "...............",
         "...............",
     }},
    {"石林",
     "乱石如林，两员敌将据险而守。善用黑石遮挡远程攻击的视线。",
     ObjWipe,
     {
         "...............",
         "...e...c...e...",
         ".e.....e.....e.",
         "...X.X...X.X...",
         ".....e.c.e.....",
         "..X....e....X..",
         "....X.....X....",
         "...e...X...e...",
         "....X.....X....",
         "..X.........X..",
         ".....h...h.....",
         "...X.......X...",
         "....s.....s....",
         "....G.B.R......",
         "...............",
     }},
    {"突围",
     "敌军重重合围，兵力悬殊，硬拼难以全歼。护送主角杀出重围，抵达东北角的旗帜！",
     ObjReach,
     {
         ".............E.",
         "........Xe.c...",
         "....e.....e....",
         "....e...e.Xe...",
         ".....X.e.......",
         ".e.......X..e..",
         "....X..e..e....",
         ".......h...e...",
         "....e..e..X....",
         "..X.....e......",
         ".....X....e....",
         ".h......e......",
         "..s.Y..........",
         ".G.B...........",
         "..s............",
     }},
    {"双将",
     "两员敌将互为犄角，敌巫压阵，敌兵蜂拥而至。逐个击破，勿陷重围！",
     ObjWipe,
     {
         "...............",
         "....c.....c....",
         ".......m.......",
         "..e...e.e...e..",
         "....e.....e....",
         ".....X.X.X.....",
         "....e.....e....",
         "..X..........X.",
         "......e.e......",
         "...............",
         "....h.....h....",
         ".....X...X.....",
         "....s..s..s....",
         "...G.B.R.Y.....",
         "...............",
     }},
    {"隘口",
     "石墙横贯战场，只有两处隘口，墙后敌巫冷箭封锁。是坚守，还是强攻？",
     ObjWipe,
     {
         "...............",
         "...e...c...e...",
         "......m........",
         "..e..e...e..e..",
         "...e...c...e...",
         "..e.........e..",
         "...............",
         "XXXX.XXXXX.XXXX",
         "...............",
         "...............",
         "....h.....h....",
         "..s...s...s....",
         "...G.B.R.Y.....",
         "...............",
         "...............",
     }},
    {"法阵",
     "四名敌巫布下法阵，箭雨覆盖全场，敌将率兵合围。切勿在开阔地久留！",
     ObjWipe,
     {
         "...............",
         "...m.......m...",
         "....X.....X....",
         "...X..m....X...",
         "....e..c..e....",
         "..X.........X..",
         "...e...c...e...",
         "...X.......X...",
         "....X.....X....",
         "...e..e.e..e...",
         ".....X...X.....",
         "...............",
         "...s.......s...",
         "....G.B.R.Y....",
         "......s........",
     }},
    {"王座",
     "魔王端坐王座，麾下精锐尽出。它的攻击会波及范围内所有人——击败它，终结远征！",
     ObjBoss,
     {
         "......XKX......",
         "...............",
         "....m.....m....",
         "..X..........X.",
         ".....c...c.....",
         "....e.....e....",
         "..e..........e.",
         ".......h.......",
         "....e.....e....",
         "..X.........X..",
         "...............",
         ".h...........h.",
         "...s.s...s.....",
         "....G.B.R.Y....",
         "...............",
     }},
};
static const int nlevels = (int)(sizeof(levels) / sizeof(levels[0]));

// 所有 UI 文案（用于收集字体码点，务必包含程序中出现的全部汉字）
static const char *uiAll =
    "方格征途 致童年——五子棋盘上的战棋梦 第关 回合 我方行动 敌方行动 选择关卡 "
    "开始游戏 按住空格可加速敌方回合 目标：歼灭所有敌人 主角抵达旗帜 击败魔王 "
    "结束回合 已存档至槽位 读取槽位 槽位为空 只能在我方回合存档 存档失败 读档失败 "
    "胜利！败北……按回车进入下一关 按回车重试本关，退出键返回标题 恭喜通关！ "
    "远征者踏过十五路棋盘，黑白之间再无敌手。 那些年用棋子摆出的幻想，如今都成真了。 "
    "感谢游玩 按回车返回标题 血量 二进制 攻击 射程 移动 治疗友军 范围攻击 触死 "
    "左键：选择切换移动攻击 右键：待命取消 点击自身原地待命 再点则 存读档 操作说明 "
    "旗草已有存档无请用方向键选关然后回车出击単位卡尚未行动待命中我military "
    "药草恢复生命值 抵达旗帜即胜利 敌人当前回合全部阵亡自动结束 名称 阵营 中立 "
    "友军单位 敌军单位 障碍黑子不可通行且阻挡视线 提示 （）开始战斗继续按住可加速 防御 "
    "触死（被击中即阵亡） ";

// ------------------------------------------------------------------ 状态
enum Mode { MTitle, MIntro, MPlay, MVictory, MDefeat, MEnding };
enum Phase { PhPlayer, PhEnemy };

struct Unit {
  int type = 0;
  int x = 0, y = 0;
  int hp = 1;
  bool acted = false, alive = true;
  float vx = 0, vy = 0; // 视觉位置（格坐标，插值）
  float hitT = 0, deathT = 0, bumpT = 0;
  float bdx = 0, bdy = 0; // 攻击前扑方向
  const UnitDef &def() const { return defs[type]; }
};

struct Item {
  int x, y;
  bool taken;
};
struct FText {
  float x, y, t;
  std::string s;
  Color c;
};
struct Toast {
  std::string s;
  float t;
};

struct Game {
  Mode gameMode = MTitle;
  Phase turnPhase = PhPlayer;
  int levelIdx = 0, turn = 1;
  int titleSel = 0;

  std::vector<Unit> units;
  std::vector<Item> items;
  bool obst[n][n] = {};
  int exitX = -1, exitY = -1;

  // 交互
  int sel = -1;
  bool moved = false;
  int reach[n][n];                           // -1 不可达
  std::vector<std::pair<int, bool>> targets; // (单位下标, 是否治疗)
  float autoEndT = -1;
  float aiT = 0;

  std::string banner;
  float bannerT = 0;
  std::vector<FText> floats;
  std::vector<Toast> toasts;
};

static Game g;
static Font gFont, gFontBig;

// ------------------------------------------------------------------ CLI 记谱
// 坐标记谱：列 a~o（左→右），行 1~15（上→下），如 f13。
// 单位字母与关卡图例一致：G 主角 B 蓝骑士 R 红法师 Y 黄牧师 s 义勇兵
//                          e 敌兵 c 敌将 m 敌巫 K 魔王
static bool cliMode = false;
static bool aiSim = false;               // 置位时抑制动画/日志副作用（AI 前瞻模拟用）
static bool benchScriptedPlayer = false; // 置位时玩家方改用固定的朴素贪心策略（独立强度标尺）
static int scoreJitter = 0;              // selfplay 打分抖动（模拟多局差异）
static std::vector<std::string> cliLog;  // 战斗事件（伤害/治疗）记录

static const char typeChar[UCount] = {'G', 'B', 'R', 'Y', 's', 'e', 'c', 'm', 'K'};

static std::string sqName(int x, int y) { return TextFormat("%c%d", 'a' + x, y + 1); }
static bool parseSq(const std::string &s, int *x, int *y) {
  if (s.size() < 2 || s[0] < 'a' || s[0] > 'o')
    return false;
  int row = atoi(s.c_str() + 1);
  if (row < 1 || row > n)
    return false;
  *x = s[0] - 'a';
  *y = row - 1;
  return true;
}
static std::string unitTag(const Unit &u) {
  return TextFormat("%c@%s", typeChar[u.type], sqName(u.x, u.y).c_str());
}

// ------------------------------------------------------------------ 小工具
static int manh(int x0, int y0, int x1, int y1) { return abs(x0 - x1) + abs(y0 - y1); }
static bool inBoard(int x, int y) { return x >= 0 && x < n && y >= 0 && y < n; }
static Vector2 cellCenter(float cx, float cy) {
  return {bx + cx * cell + cell * 0.5f, by + cy * cell + cell * 0.5f};
}

static void addFloat(int cx, int cy, const std::string &s, Color c) {
  if (aiSim)
    return;
  Vector2 p = cellCenter((float)cx, (float)cy);
  g.floats.push_back({p.x, p.y - 14, 0, s, c});
}
static void addToast(const std::string &s) {
  if (cliMode) {
    printf("[%s]\n", s.c_str());
    return;
  }
  g.toasts.push_back({s, 0});
}
static void setBanner(const std::string &s) {
  g.banner = s;
  g.bannerT = 1.4f;
}

static int unitAt(int x, int y) {
  for (int i = 0; i < (int)g.units.size(); i++)
    if (g.units[i].alive && g.units[i].x == x && g.units[i].y == y)
      return i;
  return -1;
}

// 视线：黑子阻挡（Bresenham，端点除外）
static bool los(int x0, int y0, int x1, int y1) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy, x = x0, y = y0;
  while (true) {
    if (!(x == x0 && y == y0) && !(x == x1 && y == y1) && g.obst[y][x])
      return false;
    if (x == x1 && y == y1)
      break;
    int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y += sy;
    }
  }
  return true;
}

// 可达范围 BFS：黑子与敌对单位阻挡，友军可穿行、不可停留
static void computeReach(int ui, int dist[n][n], int maxMov = -1) {
  const Unit &u = g.units[ui];
  if (maxMov < 0)
    maxMov = u.def().mov;
  for (int y = 0; y < n; y++)
    for (int x = 0; x < n; x++)
      dist[y][x] = -1;
  std::queue<std::pair<int, int>> q;
  dist[u.y][u.x] = 0;
  q.push({u.x, u.y});
  const int dx4[] = {1, -1, 0, 0}, dy4[] = {0, 0, 1, -1};
  while (!q.empty()) {
    auto [cx, cy] = q.front();
    q.pop();
    if (dist[cy][cx] >= maxMov)
      continue;
    for (int d = 0; d < 4; d++) {
      int nx = cx + dx4[d], ny = cy + dy4[d];
      if (!inBoard(nx, ny) || g.obst[ny][nx] || dist[ny][nx] >= 0)
        continue;
      int o = unitAt(nx, ny);
      if (o >= 0 && g.units[o].def().enemy != u.def().enemy)
        continue; // 敌人阻挡
      dist[ny][nx] = dist[cy][cx] + 1;
      q.push({nx, ny});
    }
  }
}
static bool canStand(int ui, int x, int y) {
  int o = unitAt(x, y);
  return o < 0 || o == ui;
}

// 从 (x,y) 出发能攻击/治疗到的目标
static void findTargets(int ui, int x, int y, std::vector<std::pair<int, bool>> &out) {
  out.clear();
  const Unit &u = g.units[ui];
  const UnitDef &d = u.def();
  for (int i = 0; i < (int)g.units.size(); i++) {
    if (!g.units[i].alive || i == ui)
      continue;
    const Unit &t = g.units[i];
    int md = manh(x, y, t.x, t.y);
    if (md < 1 || md > d.rng)
      continue;
    if (d.rng > 1 && !los(x, y, t.x, t.y))
      continue;
    if (t.def().enemy != d.enemy)
      out.push_back({i, false});
    else if (d.healer && t.hp < t.def().hp)
      out.push_back({i, true});
  }
}

// ------------------------------------------------------------------ 逻辑
static void checkOutcome();

// 实际伤害 = 攻击 - 防御，保底 MinDmg
static int effDmg(int atk, const Unit &t) { return std::max(minDmg, atk - t.def().def); }

static void damageUnit(int ti, int atk) {
  Unit &t = g.units[ti];
  int dmg = effDmg(atk, t);
  t.hp -= dmg;
  t.hitT = 1.0f;
  addFloat(t.x, t.y, TextFormat("-%d", dmg), (Color){255, 120, 100, 255});
  if (t.hp <= 0) {
    t.hp = 0;
    t.alive = false;
    t.deathT = 1.0f;
  }
  if (cliMode && !aiSim)
    cliLog.push_back(TextFormat("x%s -%d%s", unitTag(t).c_str(), dmg, t.alive ? "" : " 阵亡"));
}

static void doAction(int ai, int ti, bool heal) {
  Unit &a = g.units[ai];
  Unit &t = g.units[ti];
  a.bumpT = 1.0f;
  float len = (float)manh(a.x, a.y, t.x, t.y);
  a.bdx = (t.x - a.x) / std::max(1.0f, len);
  a.bdy = (t.y - a.y) / std::max(1.0f, len);
  if (heal) {
    int amt = std::min(healAmt, t.def().hp - t.hp);
    t.hp += amt;
    addFloat(t.x, t.y, TextFormat("+%d", amt), (Color){130, 230, 140, 255});
    if (cliMode && !aiSim)
      cliLog.push_back(TextFormat("+%s +%d", unitTag(t).c_str(), amt));
  } else if (a.def().aoe) {
    // 魔王：波及范围内所有我方单位
    for (int i = 0; i < (int)g.units.size(); i++) {
      Unit &p = g.units[i];
      if (!p.alive || p.def().enemy == a.def().enemy)
        continue;
      int md = manh(a.x, a.y, p.x, p.y);
      if (md >= 1 && md <= a.def().rng && los(a.x, a.y, p.x, p.y))
        damageUnit(i, a.def().atk);
    }
  } else {
    damageUnit(ti, a.def().atk);
  }
  checkOutcome();
}

static void moveUnit(int ui, int x, int y) {
  Unit &u = g.units[ui];
  u.x = x;
  u.y = y;
  if (!u.def().enemy) {
    for (auto &it : g.items)
      if (!it.taken && it.x == x && it.y == y) {
        it.taken = true;
        int amt = std::min(herbAmt, u.def().hp - u.hp);
        u.hp += amt;
        addFloat(x, y, TextFormat("+%d", amt), (Color){130, 230, 140, 255});
      }
    if (u.type == UGreen && x == g.exitX && y == g.exitY)
      checkOutcome();
  }
}

static void checkOutcome() {
  if (g.gameMode != MPlay)
    return;
  bool greenAlive = false, anyEnemy = false, bossAlive = false;
  for (auto &u : g.units) {
    if (!u.alive)
      continue;
    if (u.type == UGreen) {
      greenAlive = true;
      if (levels[g.levelIdx].obj == ObjReach && u.x == g.exitX && u.y == g.exitY) {
        g.gameMode = MVictory;
        return;
      }
    }
    if (u.def().enemy)
      anyEnemy = true;
    if (u.type == UEboss)
      bossAlive = true;
  }
  if (!greenAlive) {
    g.gameMode = MDefeat;
    return;
  }
  Objective ob = levels[g.levelIdx].obj;
  // 全歼敌军永远算胜利；此外弑君关击杀魔王即可（残兵不影响）
  if (!anyEnemy || (ob == ObjBoss && !bossAlive))
    g.gameMode = MVictory;
}

static void loadLevel(int idx) {
  g.levelIdx = idx;
  g.units.clear();
  g.items.clear();
  g.exitX = g.exitY = -1;
  memset(g.obst, 0, sizeof(g.obst));
  const LevelDef &l = levels[idx];
  for (int y = 0; y < n; y++)
    for (int x = 0; x < n; x++) {
      char c = l.rows[y][x];
      int t = -1;
      switch (c) {
      case 'X':
        g.obst[y][x] = true;
        break;
      case 'E':
        g.exitX = x;
        g.exitY = y;
        break;
      case 'h':
        g.items.push_back({x, y, false});
        break;
      case 'G':
        t = UGreen;
        break;
      case 'B':
        t = UBlue;
        break;
      case 'R':
        t = URed;
        break;
      case 'Y':
        t = UYellow;
        break;
      case 's':
        t = USoldier;
        break;
      case 'e':
        t = UEsoldier;
        break;
      case 'c':
        t = UEcaptain;
        break;
      case 'm':
        t = UEmage;
        break;
      case 'K':
        t = UEboss;
        break;
      }
      if (t >= 0) {
        Unit u;
        u.type = t;
        u.x = x;
        u.y = y;
        u.vx = (float)x;
        u.vy = (float)y;
        u.hp = defs[t].hp;
        g.units.push_back(u);
      }
    }
  g.turn = 1;
  g.turnPhase = PhPlayer;
  g.sel = -1;
  g.moved = false;
  g.targets.clear();
  g.autoEndT = -1;
  g.floats.clear();
}

static void beginPlayerTurn(bool advance) {
  if (advance)
    g.turn++;
  g.turnPhase = PhPlayer;
  for (auto &u : g.units)
    u.acted = false;
  g.sel = -1;
  g.moved = false;
  g.targets.clear();
  g.autoEndT = -1;
  setBanner(TextFormat("第 %d 回合 · 我方行动", g.turn));
  checkOutcome();
}

static void endPlayerTurn() {
  g.sel = -1;
  g.moved = false;
  g.targets.clear();
  g.autoEndT = -1;
  // 敌方是否还有人
  bool any = false;
  for (auto &u : g.units)
    if (u.alive && u.def().enemy)
      any = true;
  if (!any) {
    beginPlayerTurn(true);
    return;
  }
  g.turnPhase = PhEnemy;
  for (auto &u : g.units)
    u.acted = false;
  g.aiT = 0.6f;
  setBanner("敌方行动");
}

static void finishUnit(int ui) {
  g.units[ui].acted = true;
  g.sel = -1;
  g.moved = false;
  g.targets.clear();
  if (g.gameMode != MPlay || g.turnPhase != PhPlayer)
    return;
  for (auto &u : g.units)
    if (u.alive && !u.def().enemy && !u.acted)
      return;
  g.autoEndT = 0.45f; // 全部行动完毕，稍后自动结束回合
}

// ------------------------------------------------------------------ 单位 AI（敌我通用）
// GUI 敌方回合与 CLI selfplay 共用。ScoreJitter>0 时打分加入随机抖动。
static int jitter() { return scoreJitter > 0 ? rand() % scoreJitter : 0; }

// 从若干源点出发的距离场（黑子阻挡、无视单位），用于逼近引导
static void distField(const std::vector<std::pair<int, int>> &srcs, int df[n][n]) {
  for (int y = 0; y < n; y++)
    for (int x = 0; x < n; x++)
      df[y][x] = -1;
  std::queue<std::pair<int, int>> q;
  for (auto &[sx, sy] : srcs) {
    df[sy][sx] = 0;
    q.push({sx, sy});
  }
  const int dx4[] = {1, -1, 0, 0}, dy4[] = {0, 0, 1, -1};
  while (!q.empty()) {
    auto [cx, cy] = q.front();
    q.pop();
    for (int k = 0; k < 4; k++) {
      int nx = cx + dx4[k], ny = cy + dy4[k];
      if (!inBoard(nx, ny) || g.obst[ny][nx] || df[ny][nx] >= 0)
        continue;
      df[ny][nx] = df[cy][cx] + 1;
      q.push({nx, ny});
    }
  }
}

// 沿距离场朝目标挪动：在可达格中选 Df 最小者。
// 传入 Threat 时只考虑威胁为 0 的格子（谨慎集结，不进入对方火力圈）。
static void moveAlong(int ui, int dist[n][n], int df[n][n], int (*threat)[n] = nullptr) {
  Unit &u = g.units[ui];
  int bestD = df[u.y][u.x] < 0 ? 9999 : df[u.y][u.x];
  int mx = u.x, my = u.y;
  for (int y = 0; y < n; y++)
    for (int x = 0; x < n; x++) {
      if (dist[y][x] < 0 || !canStand(ui, x, y) || df[y][x] < 0)
        continue;
      if (threat && threat[y][x] > 0)
        continue;
      if (df[y][x] < bestD || (df[y][x] == bestD && dist[y][x] < dist[my][mx])) {
        bestD = df[y][x];
        mx = x;
        my = y;
      }
    }
  if (mx != u.x || my != u.y)
    moveUnit(ui, mx, my);
}

// FromSide 一方的威胁图：每格 = 对方下回合可能打到这里的总伤害。
// 不考虑视线（偏保守估计），供敌方规避与集结判断用。
static void threatMap(bool fromSide, int threat[n][n]) {
  for (int y = 0; y < n; y++)
    for (int x = 0; x < n; x++)
      threat[y][x] = 0;
  static int dist[n][n];
  static bool mark[n][n];
  for (int i = 0; i < (int)g.units.size(); i++) {
    const Unit &p = g.units[i];
    if (!p.alive || p.def().enemy != fromSide)
      continue;
    computeReach(i, dist);
    memset(mark, 0, sizeof(mark));
    int r = p.def().rng;
    for (int y = 0; y < n; y++)
      for (int x = 0; x < n; x++) {
        if (dist[y][x] < 0 || !canStand(i, x, y))
          continue;
        for (int dy = -r; dy <= r; dy++)
          for (int dx = -(r - abs(dy)); dx <= r - abs(dy); dx++)
            if (inBoard(x + dx, y + dy))
              mark[y + dy][x + dx] = true;
      }
    for (int y = 0; y < n; y++)
      for (int x = 0; x < n; x++)
        if (mark[y][x])
          threat[y][x] += p.def().atk;
  }
}

// ---- 一步前瞻：用真实规则模拟候选动作，再用一个统一评估函数打分 ----
// 单位价值（厘点）。击杀/集火/优先脆皮/护住主角，都由"价值 × 剩余血量比例"自然涌现，
// 不再需要一堆手调常数。
static int pieceValue(int type) {
  switch (type) {
  case UGreen:
    return 7000; // 主角：阵亡即败，最高优先
  case UBlue:
    return 4600; // 蓝骑士（坦克）
  case URed:
    return 4400; // 红法师（脆皮高伤）
  case UYellow:
    return 4200; // 黄牧师（治疗，需优先除去）
  case USoldier:
  case UEsoldier:
    return 800; // 小兵
  case UEcaptain:
    return 3400;
  case UEmage:
    return 4000;
  case UEboss:
    return 12000;
  }
  return 1000;
}

// 局面评估（站在 side 视角）：己方价值 − 敌方价值，价值按剩余血量比例缩放；
// 己方单位若站在对方火力（oppThreat）下，按预期损失折价。
static int evaluateFor(bool side, int oppThreat[n][n]) {
  int v = 0;
  for (auto &u : g.units) {
    if (!u.alive)
      continue;
    int maxhp = std::max(1, u.def().hp);
    bool mine = (u.def().enemy == side);
    v += (mine ? 1 : -1) * pieceValue(u.type) * u.hp / maxhp;
    if (mine && oppThreat[u.y][u.x] > 0) { // 预期下回合会被打掉的价值
      int dmg = std::min(u.hp, oppThreat[u.y][u.x]);
      v -= pieceValue(u.type) * dmg / maxhp;
    }
  }
  return v;
}

// 模拟「移动到 (sx,sy) 并可选攻击 ti」，返回结果局面对 side 的评估值；随后完全回滚。
static int simulateAction(int ui, int sx, int sy, int ti, bool side, int oppThreat[n][n]) {
  std::vector<Unit> ub = g.units;
  std::vector<Item> ib = g.items;
  Mode mb = g.gameMode;
  aiSim = true;
  moveUnit(ui, sx, sy);
  if (ti >= 0)
    doAction(ui, ti, false);
  int v = evaluateFor(side, oppThreat);
  aiSim = false;
  g.units = ub;
  g.items = ib;
  g.gameMode = mb;
  return v;
}

static void aiActFor(int ui) {
  Unit &u = g.units[ui];
  const UnitDef &d = u.def();
  bool side = d.enemy;
  static int dist[n][n];
  computeReach(ui, dist);

  // 对方威胁图：既供评估函数折价，也供防守方的撤退/集结判断
  static int oppThreat[n][n];
  threatMap(!side, oppThreat);

  auto inRangeLos = [&](int x, int y, const Unit &t) {
    int md = manh(x, y, t.x, t.y);
    return md >= 1 && md <= d.rng && (d.rng == 1 || los(x, y, t.x, t.y));
  };

  // 1) 牧师：优先治疗缺血 >=3 的友军
  if (d.healer) {
    int bestScore = -1, bx = u.x, by = u.y, bt = -1;
    for (int y = 0; y < n; y++)
      for (int x = 0; x < n; x++) {
        if (dist[y][x] < 0 || !canStand(ui, x, y))
          continue;
        for (int i = 0; i < (int)g.units.size(); i++) {
          const Unit &t = g.units[i];
          if (!t.alive || i == ui || t.def().enemy != side)
            continue;
          int missing = t.def().hp - t.hp;
          if (missing < 3 || !inRangeLos(x, y, t))
            continue;
          int score = missing * 10 - dist[y][x] + jitter();
          if (score > bestScore) {
            bestScore = score;
            bx = x;
            by = y;
            bt = i;
          }
        }
      }
    if (bt >= 0) {
      moveUnit(ui, bx, by);
      doAction(ui, bt, true);
      return;
    }
  }

  // 群胆：附近（曼哈顿 4 格内）有 2 个以上同伴时更敢冒险。
  int nearAllies = 0;
  for (int j = 0; j < (int)g.units.size(); j++) {
    const Unit &p = g.units[j];
    if (j != ui && p.alive && p.def().enemy == side && manh(p.x, p.y, u.x, u.y) <= 4)
      nearAllies++;
  }
  bool courage = nearAllies >= 2;
  bool defensive = side && levels[g.levelIdx].obj != ObjReach;

  // 2) 一步前瞻：模拟每个「落点 + 攻击目标」，用统一评估函数取对己方最有利者。
  //    击杀/集火/优先脆皮/避开火力，都从评估里自然涌现，无需手调分项常数。
  int baseline = evaluateFor(side, oppThreat); // 按兵不动的局面值
  int bestVal = -1000000000, bx = u.x, by = u.y, bt = -1;
  for (int y = 0; y < n; y++)
    for (int x = 0; x < n; x++) {
      if (dist[y][x] < 0 || !canStand(ui, x, y))
        continue;
      for (int i = 0; i < (int)g.units.size(); i++) {
        const Unit &t = g.units[i];
        if (!t.alive || t.def().enemy == side || !inRangeLos(x, y, t))
          continue;
        int val = simulateAction(ui, x, y, i, side, oppThreat) - dist[y][x] + jitter() * 50;
        if (val > bestVal) {
          bestVal = val;
          bx = x;
          by = y;
          bt = i;
        }
      }
    }
  bool bestKills = bt >= 0 && (d.aoe || effDmg(d.atk, g.units[bt]) >= g.units[bt].hp);

  // 3) 主角在夺旗关：能攻则攻（击杀优先），否则全速冲旗
  if (!side && u.type == UGreen && levels[g.levelIdx].obj == ObjReach && g.exitX >= 0) {
    if (bt >= 0 && (bestKills || bestVal - baseline >= 800)) { // 只为可观收益停留
      moveUnit(ui, bx, by);
      doAction(ui, bt, false);
      return;
    }
    static int df[n][n];
    distField({{g.exitX, g.exitY}}, df);
    moveAlong(ui, dist, df);
    if (g.gameMode != MPlay)
      return; // 已抵达旗帜
    std::vector<std::pair<int, bool>> ts;
    findTargets(ui, u.x, u.y, ts);
    for (auto &[ti, heal] : ts)
      if (!heal) {
        doAction(ui, ti, false);
        break;
      }
    return;
  }

  // 出手判定：进攻若能改善局面（或击杀 / 群胆 / 夺旗关），就打；孤立单位不做纯亏损的
  // 送死换命。因 2) 已在所有可达落点里搜过，凑不到攻击(bt<0)即本回合确实打不到人。
  bool engage = bt >= 0 && (!defensive || courage || bestKills || bestVal >= baseline);
  if (engage) {
    moveUnit(ui, bx, by);
    doAction(ui, bt, false);
    return;
  }

  // 4) 打不到（或不值得打）：朝对方机动
  std::vector<std::pair<int, int>> srcs;
  for (auto &p : g.units)
    if (p.alive && p.def().enemy != side)
      srcs.push_back({p.x, p.y});
  if (srcs.empty())
    return;
  static int df[n][n];
  distField(srcs, df);
  if (defensive) {
    // 只有孤立（同伴<2）且站在必死火力下的单位才撤到火力圈边缘；
    // 有同伴壮胆或伤害尚可承受时顶着火力压上；不在火力圈内时到圈外集结。
    if (oppThreat[u.y][u.x] > 0) {
      if (oppThreat[u.y][u.x] >= u.hp && nearAllies < 2) {
        int mx = u.x, my = u.y;
        int bt2 = oppThreat[u.y][u.x];
        int bd2 = df[u.y][u.x] < 0 ? 9999 : df[u.y][u.x];
        for (int y = 0; y < n; y++)
          for (int x = 0; x < n; x++) {
            if (dist[y][x] < 0 || !canStand(ui, x, y) || df[y][x] < 0)
              continue;
            if (oppThreat[y][x] < bt2 || (oppThreat[y][x] == bt2 && df[y][x] < bd2)) {
              bt2 = oppThreat[y][x];
              bd2 = df[y][x];
              mx = x;
              my = y;
            }
          }
        if (mx != u.x || my != u.y)
          moveUnit(ui, mx, my);
      } else {
        moveAlong(ui, dist, df); // 压上接敌
      }
    } else {
      moveAlong(ui, dist, df, oppThreat); // 在火力圈外集结推进
    }
  } else {
    moveAlong(ui, dist, df);
  }
}

// 固定的"朴素贪心"玩家策略：作为独立、不随敌方 AI 演化的强度标尺。
// 有能攻则攻（击杀 > 残血），否则朝最近敌人（夺旗关主角朝旗帜）推进；
// 刻意不做暴露规避 / 前瞻评估，因此与 aiActFor 相互独立，可暴露"自走同源偏差"。
static void scriptedPlayerAct(int ui) {
  Unit &u = g.units[ui];
  const UnitDef &d = u.def();
  static int dist[n][n];
  computeReach(ui, dist);
  auto inRangeLos = [&](int x, int y, const Unit &t) {
    int md = manh(x, y, t.x, t.y);
    return md >= 1 && md <= d.rng && (d.rng == 1 || los(x, y, t.x, t.y));
  };
  // 牧师：治疗缺血最多的友军
  if (d.healer) {
    int best = -1, bx = u.x, by = u.y, bt = -1;
    for (int y = 0; y < n; y++)
      for (int x = 0; x < n; x++) {
        if (dist[y][x] < 0 || !canStand(ui, x, y))
          continue;
        for (int i = 0; i < (int)g.units.size(); i++) {
          const Unit &t = g.units[i];
          if (!t.alive || i == ui || t.def().enemy)
            continue;
          int miss = t.def().hp - t.hp;
          if (miss < 3 || !inRangeLos(x, y, t))
            continue;
          int s = miss * 10 - dist[y][x];
          if (s > best) {
            best = s;
            bx = x;
            by = y;
            bt = i;
          }
        }
      }
    if (bt >= 0) {
      moveUnit(ui, bx, by);
      doAction(ui, bt, true);
      return;
    }
  }
  // 贪心攻击：可达落点里选最优（击杀优先，其次残血），无视自身暴露
  int best = -1, bx = u.x, by = u.y, bt = -1;
  for (int y = 0; y < n; y++)
    for (int x = 0; x < n; x++) {
      if (dist[y][x] < 0 || !canStand(ui, x, y))
        continue;
      for (int i = 0; i < (int)g.units.size(); i++) {
        const Unit &t = g.units[i];
        if (!t.alive || !t.def().enemy || !inRangeLos(x, y, t))
          continue;
        int dmg = effDmg(d.atk, t);
        int s = (dmg >= t.hp ? 1000 : 0) + dmg * 10 + (t.def().hp - t.hp) - dist[y][x];
        if (s > best) {
          best = s;
          bx = x;
          by = y;
          bt = i;
        }
      }
    }
  if (bt >= 0) {
    moveUnit(ui, bx, by);
    doAction(ui, bt, false);
    return;
  }
  // 打不到：夺旗关主角冲旗，否则朝最近敌人推进
  static int df[n][n];
  if (u.type == UGreen && levels[g.levelIdx].obj == ObjReach && g.exitX >= 0) {
    distField({{g.exitX, g.exitY}}, df);
    moveAlong(ui, dist, df);
    return;
  }
  std::vector<std::pair<int, int>> srcs;
  for (auto &p : g.units)
    if (p.alive && p.def().enemy)
      srcs.push_back({p.x, p.y});
  if (srcs.empty())
    return;
  distField(srcs, df);
  moveAlong(ui, dist, df);
}

// ------------------------------------------------------------------ 存/读档
static std::string slotPath(int slot) { return TextFormat("saves/slot%d.txt", slot + 1); }

static bool saveGame(int slot) {
  if (g.gameMode != MPlay || g.turnPhase != PhPlayer) {
    addToast("只能在我方回合存档");
    return false;
  }
  std::error_code ec;
  std::filesystem::create_directories("saves", ec);
  std::ofstream f(slotPath(slot));
  if (!f) {
    addToast("存档失败");
    return false;
  }
  f << "SQB1\n" << g.levelIdx << " " << g.turn << "\n";
  int n = 0;
  for (auto &u : g.units)
    if (u.alive)
      n++;
  f << n << "\n";
  for (auto &u : g.units)
    if (u.alive)
      f << u.type << " " << u.x << " " << u.y << " " << u.hp << " " << (int)u.acted << "\n";
  f << g.items.size() << "\n";
  for (auto &it : g.items)
    f << (int)it.taken << " ";
  f << "\n";
  addToast(TextFormat("已存档至槽位 %d", slot + 1));
  return true;
}

static bool loadGame(int slot) {
  std::ifstream f(slotPath(slot));
  if (!f) {
    addToast(TextFormat("槽位 %d 为空", slot + 1));
    return false;
  }
  std::string magic;
  f >> magic;
  int lvl, turn, n;
  if (magic != "SQB1" || !(f >> lvl >> turn >> n) || lvl < 0 || lvl >= nlevels) {
    addToast("读档失败");
    return false;
  }
  loadLevel(lvl);
  g.turn = turn;
  g.units.clear();
  for (int i = 0; i < n; i++) {
    int t, x, y, hp, acted;
    if (!(f >> t >> x >> y >> hp >> acted) || t < 0 || t >= UCount) {
      addToast("读档失败");
      loadLevel(lvl);
      return false;
    }
    Unit u;
    u.type = t;
    u.x = x;
    u.y = y;
    u.hp = hp;
    u.acted = acted;
    u.vx = (float)x;
    u.vy = (float)y;
    g.units.push_back(u);
  }
  size_t ni;
  f >> ni;
  for (size_t i = 0; i < ni && i < g.items.size(); i++) {
    int tk;
    f >> tk;
    g.items[i].taken = tk;
  }
  g.gameMode = MPlay;
  g.turnPhase = PhPlayer;
  addToast(TextFormat("读取槽位 %d", slot + 1));
  setBanner(TextFormat("第 %d 回合 · 我方行动", g.turn));
  return true;
}

static void handleSaveLoadKeys() {
  bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
  for (int i = 0; i < 9; i++) {
    if (IsKeyPressed(KEY_F1 + i)) {
      if (shift)
        saveGame(i);
      else
        loadGame(i);
    }
  }
}

// ------------------------------------------------------------------ 输入
static void selectUnit(int ui) {
  g.sel = ui;
  g.moved = false;
  computeReach(ui, g.reach);
  findTargets(ui, g.units[ui].x, g.units[ui].y, g.targets);
}

static bool isTarget(int ui, bool *heal = nullptr) {
  for (auto &t : g.targets)
    if (t.first == ui) {
      if (heal)
        *heal = t.second;
      return true;
    }
  return false;
}

static void updatePlay(float dt) {
  handleSaveLoadKeys();
  if (g.bannerT > 0)
    g.bannerT -= dt;

  if (g.turnPhase == PhEnemy) {
    g.aiT -= dt * (IsKeyDown(KEY_SPACE) ? 4.0f : 1.0f);
    if (g.aiT <= 0) {
      int next = -1;
      for (int i = 0; i < (int)g.units.size(); i++)
        if (g.units[i].alive && g.units[i].def().enemy && !g.units[i].acted) {
          next = i;
          break;
        }
      if (next < 0) {
        beginPlayerTurn(true);
        return;
      }
      aiActFor(next);
      g.units[next].acted = true;
      if (g.gameMode != MPlay)
        return;
      g.aiT = aiStep;
    }
    return;
  }

  // ---- 我方回合
  if (g.autoEndT > 0) {
    g.autoEndT -= dt;
    if (g.autoEndT <= 0)
      endPlayerTurn();
    return;
  }
  if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
    endPlayerTurn();
    return;
  }
  if (IsKeyPressed(KEY_W) && g.sel >= 0) { // 原地待命
    finishUnit(g.sel);
    return;
  }
  if (IsKeyPressed(KEY_ESCAPE)) {
    if (g.sel >= 0) {
      g.sel = -1;
      g.moved = false;
      g.targets.clear();
    } else
      g.gameMode = MTitle;
    return;
  }

  Vector2 m = GetMousePosition();
  // 结束回合按钮
  Rectangle btn = {(float)panelX, 596, (float)panelW, 46};
  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(m, btn)) {
    endPlayerTurn();
    return;
  }

  int mcx = (int)floorf((m.x - bx) / cell), mcy = (int)floorf((m.y - by) / cell);
  bool onBoard = inBoard(mcx, mcy) && m.x >= bx && m.y >= by;

  if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
    if (g.sel >= 0) {
      if (g.moved)
        finishUnit(g.sel);
      else {
        g.sel = -1;
        g.targets.clear();
      }
    }
    return;
  }
  if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || !onBoard)
    return;

  int cu = unitAt(mcx, mcy);
  if (g.sel < 0) {
    if (cu >= 0 && !g.units[cu].def().enemy && !g.units[cu].acted)
      selectUnit(cu);
    return;
  }

  Unit &su = g.units[g.sel];
  // 点击目标：攻击 / 治疗
  bool heal = false;
  if (cu >= 0 && cu != g.sel && isTarget(cu, &heal)) {
    int s = g.sel;
    doAction(s, cu, heal);
    if (g.gameMode == MPlay)
      finishUnit(s);
    return;
  }
  // 点击自身：未移动时取消选择（防止误选后被锁死），已移动时确认待命
  if (cu == g.sel) {
    if (g.moved)
      finishUnit(g.sel);
    else {
      g.sel = -1;
      g.targets.clear();
    }
    return;
  }
  // 移动
  if (!g.moved && g.reach[mcy][mcx] >= 0 && canStand(g.sel, mcx, mcy)) {
    moveUnit(g.sel, mcx, mcy);
    if (g.gameMode != MPlay)
      return;
    g.moved = true;
    for (int y = 0; y < n; y++)
      for (int x = 0; x < n; x++)
        g.reach[y][x] = -1;
    findTargets(g.sel, su.x, su.y, g.targets);
    if (g.targets.empty())
      finishUnit(g.sel);
    return;
  }
  // 切换选择其他未行动友军
  if (cu >= 0 && !g.units[cu].def().enemy && !g.units[cu].acted && !g.moved) {
    selectUnit(cu);
    return;
  }
  // 其他：已移动则待命，否则取消
  if (g.moved)
    finishUnit(g.sel);
  else {
    g.sel = -1;
    g.targets.clear();
  }
}

// ------------------------------------------------------------------ 渲染
static void txt(const std::string &s, float x, float y, float size, Color c) {
  DrawTextEx(gFont, s.c_str(), {x, y}, size, 1.0f, c);
}
static void txtC(const std::string &s, float cx, float y, float size, Color c) {
  Vector2 m = MeasureTextEx(gFont, s.c_str(), size, 1.0f);
  DrawTextEx(gFont, s.c_str(), {cx - m.x / 2, y}, size, 1.0f, c);
}
static void txtBigC(const std::string &s, float cx, float y, float size, Color c) {
  Vector2 m = MeasureTextEx(gFontBig, s.c_str(), size, 1.0f);
  DrawTextEx(gFontBig, s.c_str(), {cx - m.x / 2, y}, size, 1.0f, c);
}

static const char *objText(Objective o) {
  switch (o) {
  case ObjWipe:
    return "目标：歼灭所有敌人";
  case ObjReach:
    return "目标：抵达旗帜或全歼敌军";
  default:
    return "目标：击败魔王";
  }
}

static void drawPiece(const Unit &u) {
  const UnitDef &d = u.def();
  Vector2 p = cellCenter(u.vx, u.vy);
  if (u.bumpT > 0) {
    float k = sinf(u.bumpT * PI) * 8.0f;
    p.x += u.bdx * k;
    p.y += u.bdy * k;
  }
  float r = 17.0f;
  float alpha = u.alive ? 1.0f : std::max(0.0f, u.deathT);
  if (alpha <= 0)
    return;

  DrawCircleV({p.x + 2, p.y + 3}, r, Fade(BLACK, 0.30f * alpha));
  DrawCircleV(p, r, Fade(d.body, alpha));
  DrawCircleV({p.x - 4, p.y - 5}, r * 0.45f, Fade(WHITE, (d.body.r > 150 ? 0.45f : 0.22f) * alpha));
  DrawRing(p, r - 2.0f, r + 0.6f, 0, 360, 40, Fade(d.ring, alpha));
  // 敌方小兵 = 翻面白子：加一道内圈提示
  if (u.type == UEsoldier)
    DrawRing(p, r - 6.0f, r - 4.5f, 0, 360, 32, Fade(d.ring, 0.7f * alpha));

  Vector2 gm = MeasureTextEx(gFont, d.glyph, 19, 0);
  DrawTextEx(gFont, d.glyph, {p.x - gm.x / 2, p.y - gm.y / 2 - 1}, 19, 0, Fade(d.glyphCol, alpha));

  if (u.alive && d.hp > 1) {
    float w = 28, h = 4;
    float px = p.x - w / 2, py = p.y + r + 3;
    DrawRectangle((int)px, (int)py, (int)w, (int)h, Fade(BLACK, 0.55f));
    float pct = (float)u.hp / d.hp;
    Color hc = pct > 0.5f    ? (Color){110, 205, 100, 255}
               : pct > 0.25f ? (Color){230, 190, 70, 255}
                             : (Color){225, 90, 70, 255};
    DrawRectangle((int)px, (int)py, (int)(w * pct), (int)h, hc);
  }
  if (u.alive && u.acted && g.turnPhase == PhPlayer && !d.enemy)
    DrawCircleV(p, r, Fade((Color){60, 60, 60, 255}, 0.55f));
  if (u.hitT > 0)
    DrawRing(p, r + 2, r + 5 + (1 - u.hitT) * 6, 0, 360, 40, Fade(RAYWHITE, u.hitT * 0.8f));
}

// 悬停预览：指针所指单位的移动范围（填充）与攻击覆盖（描边）。
// 友军蓝、敌军红，与选中单位的绿色叠层区分开。
static void drawHoverRange(int ui) {
  const Unit &u = g.units[ui];
  const UnitDef &d = u.def();
  static int dist[n][n];
  computeReach(ui, dist);
  static bool atk[n][n];
  for (int y = 0; y < n; y++)
    for (int x = 0; x < n; x++)
      atk[y][x] = false;
  // 攻击覆盖 = 从任一可站立格出发、射程内（远程需视线）的所有格子
  for (int sy = 0; sy < n; sy++)
    for (int sx = 0; sx < n; sx++) {
      if (dist[sy][sx] < 0 || !canStand(ui, sx, sy))
        continue;
      for (int ty = 0; ty < n; ty++)
        for (int tx = 0; tx < n; tx++) {
          int md = manh(sx, sy, tx, ty);
          if (md < 1 || md > d.rng || (d.rng > 1 && !los(sx, sy, tx, ty)))
            continue;
          atk[ty][tx] = true;
        }
    }
  Color moveCol = d.enemy ? (Color){210, 95, 80, 255} : (Color){86, 150, 224, 255};
  Color atkCol = (Color){238, 92, 70, 255};
  for (int y = 0; y < n; y++)
    for (int x = 0; x < n; x++) {
      bool canMove = dist[y][x] >= 0 && canStand(ui, x, y);
      if (canMove) {
        DrawRectangle(bx + x * cell + 1, by + y * cell + 1, cell - 2, cell - 2,
                      Fade(moveCol, 0.24f));
        DrawRectangleLinesEx({(float)(bx + x * cell + 1), (float)(by + y * cell + 1),
                              (float)cell - 2, (float)cell - 2},
                             1, Fade(moveCol, 0.4f));
      } else if (atk[y][x]) {
        // 移动范围之外的攻击覆盖：红色描边（可移动后打到的额外威胁）
        DrawRectangleLinesEx({(float)(bx + x * cell + 3), (float)(by + y * cell + 3),
                              (float)cell - 6, (float)cell - 6},
                             1.5f, Fade(atkCol, 0.55f));
      }
    }
}

static void drawBoard() {
  // 木质棋盘
  DrawRectangle(bx - 14, by - 14, n * cell + 28, n * cell + 28, cWoodE);
  DrawRectangle(bx - 8, by - 8, n * cell + 16, n * cell + 16, cWood);
  DrawRectangleLinesEx({(float)bx - 14, (float)by - 14, (float)n * cell + 28, (float)n * cell + 28},
                       3, (Color){70, 46, 26, 255});
  for (int i = 0; i <= n; i++) {
    DrawLineEx({(float)(bx + i * cell), (float)by},
               {(float)(bx + i * cell), (float)(by + n * cell)}, 1.0f, Fade(cGrid, 0.65f));
    DrawLineEx({(float)bx, (float)(by + i * cell)},
               {(float)(bx + n * cell), (float)(by + i * cell)}, 1.0f, Fade(cGrid, 0.65f));
  }
  // 五子棋星位
  const int stars[5][2] = {{3, 3}, {3, 11}, {7, 7}, {11, 3}, {11, 11}};
  for (auto &s : stars)
    DrawCircleV(cellCenter((float)s[0], (float)s[1]), 3.0f, Fade(cGrid, 0.8f));

  // 旗帜（终点）
  if (g.exitX >= 0) {
    Vector2 p = cellCenter((float)g.exitX, (float)g.exitY);
    float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 3);
    DrawRectangle(bx + g.exitX * cell + 2, by + g.exitY * cell + 2, cell - 4, cell - 4,
                  Fade(cGold, 0.25f + 0.15f * pulse));
    Vector2 gm = MeasureTextEx(gFont, "旗", 24, 0);
    DrawTextEx(gFont, "旗", {p.x - gm.x / 2, p.y - gm.y / 2}, 24, 0,
               Fade((Color){180, 60, 40, 255}, 0.85f + 0.15f * pulse));
  }
  // 药草
  for (auto &it : g.items) {
    if (it.taken)
      continue;
    Vector2 p = cellCenter((float)it.x, (float)it.y);
    DrawCircleV(p, 11, Fade((Color){96, 180, 96, 255}, 0.9f));
    Vector2 gm = MeasureTextEx(gFont, "草", 14, 0);
    DrawTextEx(gFont, "草", {p.x - gm.x / 2, p.y - gm.y / 2}, 14, 0, (Color){24, 66, 28, 255});
  }

  // 移动范围
  if (g.sel >= 0 && !g.moved && g.turnPhase == PhPlayer) {
    for (int y = 0; y < n; y++)
      for (int x = 0; x < n; x++)
        if (g.reach[y][x] >= 0 && canStand(g.sel, x, y)) {
          DrawRectangle(bx + x * cell + 1, by + y * cell + 1, cell - 2, cell - 2,
                        Fade((Color){90, 190, 120, 255}, 0.30f));
          DrawRectangleLinesEx({(float)(bx + x * cell + 1), (float)(by + y * cell + 1),
                                (float)cell - 2, (float)cell - 2},
                               1, Fade((Color){90, 190, 120, 255}, 0.45f));
        }
  }
  // 悬停预览：指针所指单位（非当前选中）的移动/攻击范围
  {
    Vector2 hm = GetMousePosition();
    int hx = (int)floorf((hm.x - bx) / cell), hy = (int)floorf((hm.y - by) / cell);
    if (g.gameMode == MPlay && inBoard(hx, hy) && hm.x >= bx && hm.y >= by) {
      int hu = unitAt(hx, hy);
      if (hu >= 0 && hu != g.sel)
        drawHoverRange(hu);
    }
  }
  // 悬停格
  Vector2 m = GetMousePosition();
  int mcx = (int)floorf((m.x - bx) / cell), mcy = (int)floorf((m.y - by) / cell);
  if (inBoard(mcx, mcy) && m.x >= bx && m.y >= by && g.gameMode == MPlay)
    DrawRectangleLinesEx(
        {(float)(bx + mcx * cell), (float)(by + mcy * cell), (float)cell, (float)cell}, 2,
        Fade(RAYWHITE, 0.5f));

  // 黑子障碍
  for (int y = 0; y < n; y++)
    for (int x = 0; x < n; x++)
      if (g.obst[y][x]) {
        Vector2 p = cellCenter((float)x, (float)y);
        DrawCircleV({p.x + 2, p.y + 3}, 18, Fade(BLACK, 0.35f));
        DrawCircleV(p, 18, (Color){24, 22, 24, 255});
        DrawCircleV({p.x - 5, p.y - 6}, 7, Fade(WHITE, 0.18f));
      }

  // 单位：先画阵亡渐隐，再画存活
  for (auto &u : g.units)
    if (!u.alive && u.deathT > 0)
      drawPiece(u);
  for (auto &u : g.units)
    if (u.alive)
      drawPiece(u);

  // 目标圈
  float pulse = 2.0f * sinf((float)GetTime() * 6);
  if (g.sel >= 0 && g.turnPhase == PhPlayer) {
    for (auto &[ti, heal] : g.targets) {
      const Unit &t = g.units[ti];
      Vector2 p = cellCenter(t.vx, t.vy);
      Color c = heal ? (Color){110, 230, 130, 255} : (Color){240, 80, 60, 255};
      DrawRing(p, 20 + pulse, 23 + pulse, 0, 360, 40, c);
    }
    const Unit &su = g.units[g.sel];
    DrawRing(cellCenter(su.vx, su.vy), 19 + pulse * 0.5f, 21.5f + pulse * 0.5f, 0, 360, 40,
             RAYWHITE);
  }

  // 漂浮文字
  for (auto &ft : g.floats) {
    float a = 1.0f - ft.t;
    Vector2 mm = MeasureTextEx(gFont, ft.s.c_str(), 24, 1);
    DrawTextEx(gFont, ft.s.c_str(), {ft.x - mm.x / 2 + 1, ft.y - ft.t * 34 + 1}, 24, 1,
               Fade(BLACK, a * 0.6f));
    DrawTextEx(gFont, ft.s.c_str(), {ft.x - mm.x / 2, ft.y - ft.t * 34}, 24, 1, Fade(ft.c, a));
  }
}

// 二进制血量（致敬：棋盘外用正反面棋子记血量）
static void drawBinaryHP(float x, float y, int hp, int maxhp) {
  int bits = 1;
  while ((1 << bits) <= maxhp)
    bits++;
  txt("二进制", x, y - 2, 16, cDim);
  float cx = x + 62;
  for (int b = bits - 1; b >= 0; b--) {
    bool on = (hp >> b) & 1;
    Vector2 c = {cx + 9, y + 8};
    DrawCircleV({c.x + 1, c.y + 1.5f}, 8, Fade(BLACK, 0.4f));
    if (on) {
      DrawCircleV(c, 8, (Color){30, 28, 30, 255});
      DrawCircleV({c.x - 2, c.y - 2.5f}, 3, Fade(WHITE, 0.25f));
    } else {
      DrawCircleV(c, 8, cIvory);
      DrawRing(c, 6.5f, 8, 0, 360, 24, (Color){150, 140, 120, 255});
    }
    cx += 21;
  }
}

static void drawUnitCard(const Unit &u, float x, float y) {
  const UnitDef &d = u.def();
  DrawRectangleRounded({x, y, (float)panelW, 158}, 0.08f, 6, (Color){56, 49, 44, 255});
  DrawRectangleRoundedLinesEx({x, y, (float)panelW, 158}, 0.08f, 6, 2, Fade(d.body, 0.8f));
  DrawCircleV({x + 30, y + 32}, 16, d.body);
  DrawRing({x + 30, y + 32}, 14, 16.5f, 0, 360, 32, d.ring);
  Vector2 gm = MeasureTextEx(gFont, d.glyph, 17, 0);
  DrawTextEx(gFont, d.glyph, {x + 30 - gm.x / 2, y + 32 - gm.y / 2}, 17, 0, d.glyphCol);
  txt(d.name, x + 56, y + 14, 24, cText);
  txt(d.enemy ? "敌军单位" : "友军单位", x + 56, y + 42, 15,
      d.enemy ? (Color){230, 130, 110, 255} : (Color){140, 200, 150, 255});

  // 血条
  float bx = x + 16, by = y + 68, bw = panelW - 32;
  txt(TextFormat("血量 %d / %d", u.hp, d.hp), bx, by - 4, 18, cText);
  DrawRectangle((int)bx, (int)(by + 20), (int)bw, 9, Fade(BLACK, 0.5f));
  float pct = (float)u.hp / d.hp;
  Color hc = pct > 0.5f    ? (Color){110, 205, 100, 255}
             : pct > 0.25f ? (Color){230, 190, 70, 255}
                           : (Color){225, 90, 70, 255};
  DrawRectangle((int)bx, (int)(by + 20), (int)(bw * pct), 9, hc);

  if (d.hp > 1)
    drawBinaryHP(bx, by + 38, u.hp, d.hp);
  else
    txt("触死（被击中即阵亡）", bx, by + 36, 16, cDim);

  std::string stats = TextFormat("攻击 %d  防御 %d  射程 %d  移动 %d", d.atk, d.def, d.rng, d.mov);
  txt(stats, bx, by + 62, 18, cText);
  if (d.healer)
    txt(TextFormat("治疗友军 +%d", healAmt), bx + 310, by + 62, 18, (Color){130, 230, 140, 255});
  if (d.aoe)
    txt("范围攻击", bx + 310, by + 62, 18, (Color){240, 140, 90, 255});
}

static void drawPanel() {
  DrawRectangleRounded({(float)panelX - 12, 20, (float)panelW + 24, winH - 40.0f}, 0.03f, 6,
                       cPanel);
  float x = panelX;
  txt("方格征途", x, 36, 34, cGold);
  txt("SQUAREBOUND", x + 168, 48, 16, cDim);
  const LevelDef &l = levels[g.levelIdx];
  txt(TextFormat("第 %d 关 · %s", g.levelIdx + 1, l.name), x, 84, 24, cText);
  txt(objText(l.obj), x, 114, 19, (Color){225, 180, 120, 255});
  txt(TextFormat("回合 %d", g.turn), x, 142, 19, cText);
  txt(g.turnPhase == PhPlayer ? "我方行动" : "敌方行动", x + 100, 142, 19,
      g.turnPhase == PhPlayer ? (Color){140, 210, 150, 255} : (Color){230, 130, 110, 255});
  DrawLineEx({x, 172}, {x + panelW, 172}, 1, Fade(cDim, 0.4f));

  // 单位卡：优先悬停，其次选中
  Vector2 m = GetMousePosition();
  int mcx = (int)floorf((m.x - bx) / cell), mcy = (int)floorf((m.y - by) / cell);
  int show = -1;
  if (inBoard(mcx, mcy) && m.x >= bx && m.y >= by)
    show = unitAt(mcx, mcy);
  if (show < 0)
    show = g.sel;
  if (show >= 0)
    drawUnitCard(g.units[show], x, 186);
  else {
    DrawRectangleRounded({x, 186, (float)panelW, 158}, 0.08f, 6, (Color){52, 46, 41, 255});
    txtC("将鼠标移到棋子上查看详情", x + panelW / 2.0f, 250, 18, cDim);
  }

  // 选中状态提示
  if (g.sel >= 0) {
    std::string tip =
        g.moved ? "已移动：点击目标攻击，或右键待命" : "点绿格移动 · 点红圈攻击 · W 待命";
    txtC(tip, x + panelW / 2.0f, 356, 16, (Color){200, 190, 160, 255});
  }

  // 结束回合按钮
  Rectangle btn = {x, 596, (float)panelW, 46};
  bool hov = CheckCollisionPointRec(m, btn) && g.turnPhase == PhPlayer;
  DrawRectangleRounded(btn, 0.25f, 6, hov ? (Color){120, 84, 44, 255} : (Color){86, 62, 38, 255});
  DrawRectangleRoundedLinesEx(btn, 0.25f, 6, 2, Fade(cGold, hov ? 1.0f : 0.55f));
  txtC("结 束 回 合", x + panelW / 2.0f, 606, 24, hov ? cGold : cText);

  // 操作说明
  float hy = 652;
  const char *helps[] = {
      "左键 选择/移动/攻击      W 原地待命",
      "右键/再点自身 取消选择（已移动则待命）",
      "空格 结束回合（敌方回合按住可加速）",
      "Shift+F1~F9 存档       F1~F9 读档",
  };
  for (int i = 0; i < 4; i++)
    txt(helps[i], x, hy + i * 22, 16, cDim);
}

static void drawBannerToasts(float dt) {
  if (g.bannerT > 0) {
    float a = std::min(1.0f, g.bannerT / 0.4f);
    float bw = n * cell;
    DrawRectangle(bx, by + n * cell / 2 - 36, (int)bw, 72, Fade(BLACK, 0.55f * a));
    Vector2 mm = MeasureTextEx(gFontBig, g.banner.c_str(), 40, 2);
    DrawTextEx(gFontBig, g.banner.c_str(),
               {bx + bw / 2 - mm.x / 2, by + n * cell / 2.0f - mm.y / 2}, 40, 2, Fade(cGold, a));
  }
  float ty = winH - 44;
  for (int i = (int)g.toasts.size() - 1; i >= 0; i--) {
    Toast &t = g.toasts[i];
    t.t += dt;
    if (t.t > 2.2f) {
      g.toasts.erase(g.toasts.begin() + i);
      continue;
    }
    float a = t.t > 1.7f ? (2.2f - t.t) / 0.5f : 1.0f;
    Vector2 mm = MeasureTextEx(gFont, t.s.c_str(), 20, 1);
    DrawRectangleRounded({36, ty - 4, mm.x + 24, 30}, 0.4f, 6, Fade(BLACK, 0.6f * a));
    txt(t.s, 48, ty, 20, Fade(cGold, a));
    ty -= 36;
  }
}

static void drawOverlayBase() { DrawRectangle(0, 0, winW, winH, Fade(BLACK, 0.62f)); }

static void drawTitle() {
  ClearBackground(cDesk);
  // 装饰棋子
  struct {
    float x, y, r;
    Color c;
  } deco[] = {
      {150, 600, 46, cGreen},   {260, 660, 34, cBlue}, {1120, 620, 40, cRed},
      {1010, 680, 30, cYellow}, {90, 140, 30, cIvory}, {1180, 130, 34, (Color){24, 22, 24, 255}},
  };
  for (auto &d : deco) {
    DrawCircleV({d.x + 3, d.y + 4}, d.r, Fade(BLACK, 0.3f));
    DrawCircleV({d.x, d.y}, d.r, Fade(d.c, 0.85f));
    DrawCircleV({d.x - d.r * 0.25f, d.y - d.r * 0.3f}, d.r * 0.4f, Fade(WHITE, 0.2f));
  }
  txtBigC("方格征途", winW / 2.0f, 130, 84, cGold);
  txtC("SQUAREBOUND", winW / 2.0f, 232, 22, cDim);
  txtC("—— 致童年：五子棋盘上的战棋梦 ——", winW / 2.0f, 276, 22, cText);

  const LevelDef &l = levels[g.titleSel];
  txtC("←  选择关卡  →", winW / 2.0f, 370, 22, cDim);
  txtC(TextFormat("第 %d 关 · %s", g.titleSel + 1, l.name), winW / 2.0f, 404, 32, cText);
  txtC(objText(l.obj), winW / 2.0f, 448, 18, (Color){225, 180, 120, 255});

  float blink = 0.6f + 0.4f * sinf((float)GetTime() * 4);
  txtC("按 回车 出击", winW / 2.0f, 512, 26, Fade(cGold, blink));

  // 存档槽位状态
  std::string slots = "存档槽位：";
  bool any = false;
  for (int i = 0; i < 9; i++)
    if (std::filesystem::exists(slotPath(i))) {
      slots += TextFormat(" %d", i + 1);
      any = true;
    }
  if (!any)
    slots += " 无";
  txtC(slots, winW / 2.0f, 580, 18, cDim);
  txtC("F1~F9 读档    Shift+F1~F9 存档（战斗中）", winW / 2.0f, 610, 18, cDim);
  txtC("左键 选择/移动/攻击    右键 待命/取消    空格 结束回合", winW / 2.0f, 680, 18,
       Fade(cDim, 0.8f));
}

static void updateTitle() {
  handleSaveLoadKeys();
  if (IsKeyPressed(KEY_LEFT))
    g.titleSel = (g.titleSel + nlevels - 1) % nlevels;
  if (IsKeyPressed(KEY_RIGHT))
    g.titleSel = (g.titleSel + 1) % nlevels;
  if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
    loadLevel(g.titleSel);
    g.gameMode = MIntro;
  }
}

static void drawIntro() {
  drawBoard();
  drawPanel();
  drawOverlayBase();
  const LevelDef &l = levels[g.levelIdx];
  txtBigC(TextFormat("第 %d 关", g.levelIdx + 1), winW / 2.0f, 200, 40, cDim);
  txtBigC(l.name, winW / 2.0f, 260, 72, cGold);
  txtC(l.intro, winW / 2.0f, 380, 24, cText);
  txtC(objText(l.obj), winW / 2.0f, 430, 22, (Color){225, 180, 120, 255});
  float blink = 0.6f + 0.4f * sinf((float)GetTime() * 4);
  txtC("按 回车 开始战斗", winW / 2.0f, 510, 24, Fade(cGold, blink));
}

static void drawEndOverlay(bool win) {
  drawBoard();
  drawPanel();
  drawOverlayBase();
  if (win) {
    txtBigC("胜 利！", winW / 2.0f, 260, 84, cGold);
    txtC(g.levelIdx + 1 < nlevels ? "按 回车 进入下一关" : "按 回车 继续", winW / 2.0f, 400, 24,
         cText);
  } else {
    txtBigC("败 北……", winW / 2.0f, 260, 84, (Color){200, 90, 80, 255});
    txtC("主角阵亡。按 回车 重试本关，Esc 返回标题", winW / 2.0f, 400, 24, cText);
  }
}

static void drawEnding() {
  ClearBackground(cDesk);
  txtBigC("恭 喜 通 关！", winW / 2.0f, 160, 72, cGold);
  txtC("远征者踏过十五路棋盘，黑白之间再无敌手。", winW / 2.0f, 300, 26, cText);
  txtC("那些年用棋子摆出的幻想，如今都成真了。", winW / 2.0f, 345, 26, cText);
  txtC("感谢游玩 · SQUAREBOUND", winW / 2.0f, 440, 20, cDim);
  float blink = 0.6f + 0.4f * sinf((float)GetTime() * 4);
  txtC("按 回车 返回标题", winW / 2.0f, 520, 22, Fade(cGold, blink));
}

// ------------------------------------------------------------------ 动画
static void updateAnims(float dt) {
  for (auto &u : g.units) {
    u.vx += ((float)u.x - u.vx) * std::min(1.0f, dt * 12);
    u.vy += ((float)u.y - u.vy) * std::min(1.0f, dt * 12);
    if (u.hitT > 0)
      u.hitT -= dt * 3.0f;
    if (u.bumpT > 0)
      u.bumpT -= dt * 3.5f;
    if (!u.alive && u.deathT > 0)
      u.deathT -= dt * 1.6f;
  }
  for (int i = (int)g.floats.size() - 1; i >= 0; i--) {
    g.floats[i].t += dt * 1.1f;
    if (g.floats[i].t >= 1.0f)
      g.floats.erase(g.floats.begin() + i);
  }
}

// ------------------------------------------------------------------ 字体
static std::vector<int> buildCodepoints() {
  std::string all = uiAll;
  for (auto &d : defs) {
    all += d.name;
    all += d.glyph;
  }
  for (auto &l : levels) {
    all += l.name;
    all += l.intro;
  }
  all += "旗草已移动：点击目标攻击，或右键待命 点击绿色格移动 / 点击红圈敌人攻击 "
         "将鼠标移到棋子上查看详情";
  int count = 0;
  int *cps = LoadCodepoints(all.c_str(), &count);
  std::vector<int> v(cps, cps + count);
  UnloadCodepoints(cps);
  for (int c = 32; c < 127; c++)
    v.push_back(c);
  v.push_back(0x2190);
  v.push_back(0x2192); // ← →
  std::sort(v.begin(), v.end());
  v.erase(std::unique(v.begin(), v.end()), v.end());
  return v;
}

// 注意：raylib(stb_truetype) 无法解析 .ttc 字体合集，只收集 .ttf/.otf 候选。
static std::vector<std::string> cjkFontCandidates() {
  std::vector<std::string> v;
  if (const char *env = getenv("SQUAREBOUND_FONT"))
    v.push_back(env);
  v.push_back("assets/font.ttf");
  std::vector<std::string> fc;
  if (FILE *f = popen("fc-list -f '%{file}\\n' :lang=zh 2>/dev/null", "r")) {
    char buf[1024];
    while (fgets(buf, sizeof(buf), f)) {
      buf[strcspn(buf, "\n")] = 0;
      std::string s = buf;
      if (s.size() < 4)
        continue;
      std::string ext = s.substr(s.size() - 4);
      for (auto &c : ext)
        c = (char)tolower(c);
      if (ext == ".ttf" || ext == ".otf")
        fc.push_back(s);
    }
    pclose(f);
  }
  // 黑体/Sans 类更适合 UI，排前面
  auto pref = [](const std::string &s) {
    return s.find("黑") != std::string::npos || s.find("Hei") != std::string::npos ||
           s.find("hei") != std::string::npos || s.find("Sans") != std::string::npos ||
           s.find("sans") != std::string::npos;
  };
  std::stable_sort(fc.begin(), fc.end(),
                   [&](const std::string &a, const std::string &b) { return pref(a) > pref(b); });
  v.insert(v.end(), fc.begin(), fc.end());
  return v;
}

// 失败时 LoadFontEx 会静默返回内置字体，必须用 glyph 覆盖率判断是否真的加载成功
static bool loadValidatedFont(const std::string &path, int size, std::vector<int> &cps, Font *out) {
  if (!std::filesystem::exists(path))
    return false;
  Font f = LoadFontEx(path.c_str(), size, cps.data(), (int)cps.size());
  if (f.texture.id == 0 || f.texture.id == GetFontDefault().texture.id ||
      f.glyphCount < (int)cps.size() * 3 / 5) {
    if (f.texture.id != 0 && f.texture.id != GetFontDefault().texture.id)
      UnloadFont(f);
    return false;
  }
  *out = f;
  return true;
}

// ------------------------------------------------------------------ CLI 模式
// 记谱：列 a~o、行 1~15（自上而下），如 mv f13 f10 / mva f10 f8 g6 / atk f10 g8。
static void cliPrintStatus() {
  const LevelDef &l = levels[g.levelIdx];
  printf("第 %d 关 · %s | %s | 回合 %d · %s\n", g.levelIdx + 1, l.name, objText(l.obj), g.turn,
         g.turnPhase == PhPlayer ? "我方行动" : "敌方行动");
}

static void cliPrintBoard() {
  printf("   ");
  for (int x = 0; x < n; x++)
    printf("%c ", 'a' + x);
  printf("\n");
  for (int y = 0; y < n; y++) {
    printf("%2d ", y + 1);
    for (int x = 0; x < n; x++) {
      char c = '.';
      if (g.obst[y][x])
        c = '#';
      if (x == g.exitX && y == g.exitY)
        c = '@';
      for (auto &it : g.items)
        if (!it.taken && it.x == x && it.y == y)
          c = '*';
      int ui = unitAt(x, y);
      if (ui >= 0)
        c = typeChar[g.units[ui].type];
      printf("%c ", c);
    }
    printf("%d\n", y + 1);
  }
  printf("  (# 黑子障碍  * 药草  @ 旗帜  大小写字母=单位, 见 units)\n");
}

static void cliPrintUnitLine(const Unit &u) {
  const UnitDef &d = u.def();
  printf("  %c %-4s %s hp%d/%d atk%d def%d rng%d mov%d%s%s\n", typeChar[u.type],
         sqName(u.x, u.y).c_str(), d.name, u.hp, d.hp, d.atk, d.def, d.rng, d.mov,
         d.healer ? " 治疗" : (d.aoe ? " 范围" : ""),
         (!d.enemy && g.turnPhase == PhPlayer) ? (u.acted ? " [已行动]" : " [可行动]") : "");
}

static void cliPrintUnits() {
  printf("我方:\n");
  for (auto &u : g.units)
    if (u.alive && !u.def().enemy)
      cliPrintUnitLine(u);
  printf("敌方:\n");
  for (auto &u : g.units)
    if (u.alive && u.def().enemy)
      cliPrintUnitLine(u);
}

static bool cliCheckEnd() {
  if (g.gameMode == MVictory) {
    printf("== 胜利！(回合 %d) 输入 level %d 进入下一关，或 restart 重玩 ==\n", g.turn,
           std::min(g.levelIdx + 2, nlevels));
    return true;
  }
  if (g.gameMode == MDefeat) {
    printf("== 败北……(回合 %d) 输入 restart 重试 ==\n", g.turn);
    return true;
  }
  return false;
}

static int cliOwnUnit(const std::string &sq) {
  int x, y;
  if (!parseSq(sq, &x, &y)) {
    printf("! 坐标无效: %s\n", sq.c_str());
    return -1;
  }
  int ui = unitAt(x, y);
  if (ui < 0) {
    printf("! %s 上没有单位\n", sq.c_str());
    return -1;
  }
  if (g.units[ui].def().enemy) {
    printf("! %s 是敌方单位\n", sq.c_str());
    return -1;
  }
  if (g.units[ui].acted) {
    printf("! %s 本回合已行动\n", unitTag(g.units[ui]).c_str());
    return -1;
  }
  return ui;
}

static bool cliMoveTo(int ui, const std::string &sq) {
  int x, y;
  if (!parseSq(sq, &x, &y)) {
    printf("! 坐标无效: %s\n", sq.c_str());
    return false;
  }
  static int dist[n][n];
  computeReach(ui, dist);
  if (dist[y][x] < 0 || !canStand(ui, x, y)) {
    printf("! %s 不可达 (移动力 %d)\n", sq.c_str(), g.units[ui].def().mov);
    return false;
  }
  std::string from = sqName(g.units[ui].x, g.units[ui].y);
  moveUnit(ui, x, y);
  printf("  %c %s-%s\n", typeChar[g.units[ui].type], from.c_str(), sq.c_str());
  return true;
}

static bool cliActOn(int ui, const std::string &sq) {
  int x, y;
  if (!parseSq(sq, &x, &y)) {
    printf("! 坐标无效: %s\n", sq.c_str());
    return false;
  }
  int ti = unitAt(x, y);
  if (ti < 0) {
    printf("! %s 上没有目标\n", sq.c_str());
    return false;
  }
  std::vector<std::pair<int, bool>> ts;
  findTargets(ui, g.units[ui].x, g.units[ui].y, ts);
  for (auto &[cand, heal] : ts)
    if (cand == ti) {
      cliLog.clear();
      doAction(ui, ti, heal);
      for (auto &l : cliLog)
        printf("  %s\n", l.c_str());
      return true;
    }
  printf("! %s 不在射程/视线内（或不是合法目标）\n", sq.c_str());
  return false;
}

static void cliEnemyTurn() {
  g.turnPhase = PhEnemy;
  printf("-- 敌方行动 --\n");
  for (int i = 0; i < (int)g.units.size(); i++) {
    Unit &u = g.units[i];
    if (!u.alive || !u.def().enemy)
      continue;
    int ox = u.x, oy = u.y;
    cliLog.clear();
    aiActFor(i);
    std::string line = TextFormat("  %c %s", typeChar[u.type], sqName(ox, oy).c_str());
    if (u.x != ox || u.y != oy)
      line += TextFormat("-%s", sqName(u.x, u.y).c_str());
    for (auto &l : cliLog)
      line += " " + l;
    printf("%s\n", line.c_str());
    if (g.gameMode != MPlay)
      return;
  }
  g.turn++;
  g.turnPhase = PhPlayer;
  for (auto &u : g.units)
    u.acted = false;
  printf("-- 第 %d 回合 · 我方行动 --\n", g.turn);
}

// AI 双方自走一局：1 胜 / 0 负 / -1 超时
static int cliPlayOneGame(int maxTurns, bool verbose) {
  while (g.gameMode == MPlay) {
    if (g.turn > maxTurns)
      return -1;
    for (int i = 0; i < (int)g.units.size() && g.gameMode == MPlay; i++) {
      Unit &u = g.units[i];
      if (!u.alive || u.def().enemy)
        continue;
      int ox = u.x, oy = u.y;
      cliLog.clear();
      if (benchScriptedPlayer)
        scriptedPlayerAct(i);
      else
        aiActFor(i);
      if (verbose) {
        std::string line =
            TextFormat("T%-2d 我 %c %s", g.turn, typeChar[u.type], sqName(ox, oy).c_str());
        if (u.x != ox || u.y != oy)
          line += TextFormat("-%s", sqName(u.x, u.y).c_str());
        for (auto &l : cliLog)
          line += " " + l;
        printf("%s\n", line.c_str());
      }
    }
    for (int i = 0; i < (int)g.units.size() && g.gameMode == MPlay; i++) {
      Unit &u = g.units[i];
      if (!u.alive || !u.def().enemy)
        continue;
      int ox = u.x, oy = u.y;
      cliLog.clear();
      aiActFor(i);
      if (verbose) {
        std::string line =
            TextFormat("T%-2d 敌 %c %s", g.turn, typeChar[u.type], sqName(ox, oy).c_str());
        if (u.x != ox || u.y != oy)
          line += TextFormat("-%s", sqName(u.x, u.y).c_str());
        for (auto &l : cliLog)
          line += " " + l;
        printf("%s\n", line.c_str());
      }
    }
    if (g.gameMode == MPlay)
      g.turn++;
  }
  return g.gameMode == MVictory ? 1 : 0;
}

struct SelfplayStats {
  int wins = 0, losses = 0, timeouts = 0;
  double sumWinTurns = 0, sumGreenHp = 0, sumSurvivors = 0;
};

static SelfplayStats cliSelfplay(int level, int games, int maxTurns, bool verbose) {
  SelfplayStats st;
  for (int s = 0; s < games; s++) {
    srand(1234 + s * 7919);
    scoreJitter = games > 1 ? 8 : 0;
    loadLevel(level);
    g.gameMode = MPlay;
    int r = cliPlayOneGame(maxTurns, verbose);
    if (r == 1) {
      st.wins++;
      st.sumWinTurns += g.turn;
      for (auto &u : g.units)
        if (u.alive && !u.def().enemy) {
          st.sumSurvivors += 1;
          if (u.type == UGreen)
            st.sumGreenHp += u.hp;
        }
    } else if (r == 0)
      st.losses++;
    else
      st.timeouts++;
  }
  scoreJitter = 0;
  return st;
}

static int cliAnalyze(int games, int maxTurns) {
  printf("关卡强度分析：每关 AI 双方自走 %d 局（回合上限 %d）\n\n", games, maxTurns);
  printf("关卡        胜   负  超时  均胜回合  主角均余血  均存活\n");
  for (int l = 0; l < nlevels; l++) {
    SelfplayStats st = cliSelfplay(l, games, maxTurns, false);
    printf("%d %-8s %3d  %3d  %3d  %7.1f  %9.1f  %5.1f\n", l + 1, levels[l].name, st.wins,
           st.losses, st.timeouts, st.wins ? st.sumWinTurns / st.wins : 0.0,
           st.wins ? st.sumGreenHp / st.wins : 0.0, st.wins ? st.sumSurvivors / st.wins : 0.0);
  }
  return 0;
}

static const char *cliHelp = "命令（坐标=列a~o+行1~15，如 f13）：\n"
                             "  show/b            打印棋盘        units/u        列出单位\n"
                             "  sel <sq>          查看可达与目标   obj            查看目标与状态\n"
                             "  mv  <from> <to>   移动（放弃攻击） atk <from> <t> 原地攻击/治疗\n"
                             "  mva <from> <to> <t> 移动后攻击/治疗 wait <sq>     原地待命\n"
                             "  end/e             结束回合（敌方行动并记谱）\n"
                             "  save <1-9> / load <1-9>            存档/读档\n"
                             "  level <n> / restart                跳关/重开本关\n"
                             "  selfplay [局数] [回合上限]          AI 双方自走当前关\n"
                             "  analyze [局数]                     全关卡强度分析\n"
                             "  quit/q            退出\n";

static int cliMain(int startLevel) {
  if (startLevel < 0 || startLevel >= nlevels)
    startLevel = 0;
  loadLevel(startLevel);
  g.gameMode = MPlay;
  printf("Squarebound · 方格征途 CLI（help 查看命令）\n");
  cliPrintStatus();
  cliPrintBoard();
  std::string line;
  while (printf("> "), fflush(stdout), std::getline(std::cin, line)) {
    std::istringstream ss(line);
    std::string cmd, a1, a2, a3;
    ss >> cmd >> a1 >> a2 >> a3;
    if (cmd.empty())
      continue;
    if (cmd == "quit" || cmd == "q")
      break;
    if (cmd == "help" || cmd == "h" || cmd == "?") {
      printf("%s", cliHelp);
      continue;
    }
    if (cmd == "show" || cmd == "b") {
      cliPrintStatus();
      cliPrintBoard();
      continue;
    }
    if (cmd == "units" || cmd == "u") {
      cliPrintUnits();
      continue;
    }
    if (cmd == "obj") {
      cliPrintStatus();
      continue;
    }
    if (cmd == "save") {
      saveGame(atoi(a1.c_str()) - 1);
      continue;
    }
    if (cmd == "load") {
      if (loadGame(atoi(a1.c_str()) - 1)) {
        cliPrintStatus();
        cliPrintBoard();
      }
      continue;
    }
    if (cmd == "level") {
      int l = atoi(a1.c_str()) - 1;
      if (l < 0 || l >= nlevels) {
        printf("! 关卡 1~%d\n", nlevels);
        continue;
      }
      loadLevel(l);
      g.gameMode = MPlay;
      cliPrintStatus();
      cliPrintBoard();
      continue;
    }
    if (cmd == "restart") {
      loadLevel(g.levelIdx);
      g.gameMode = MPlay;
      cliPrintStatus();
      cliPrintBoard();
      continue;
    }
    if (cmd == "selfplay") {
      int games = a1.empty() ? 1 : atoi(a1.c_str());
      int maxT = a2.empty() ? 40 : atoi(a2.c_str());
      SelfplayStats st = cliSelfplay(g.levelIdx, games, maxT, games == 1);
      printf("结果：%d 胜 %d 负 %d 超时", st.wins, st.losses, st.timeouts);
      if (st.wins)
        printf("，均胜回合 %.1f，主角均余血 %.1f", st.sumWinTurns / st.wins,
               st.sumGreenHp / st.wins);
      printf("\n");
      loadLevel(g.levelIdx);
      g.gameMode = MPlay;
      continue;
    }
    if (cmd == "analyze") {
      cliAnalyze(a1.empty() ? 20 : atoi(a1.c_str()), 40);
      loadLevel(g.levelIdx);
      g.gameMode = MPlay;
      continue;
    }
    // ---- 以下为对局操作
    if (g.gameMode != MPlay) {
      printf("! 对局已结束，restart 或 level <n>\n");
      continue;
    }
    if (cmd == "sel") {
      int x, y;
      if (!parseSq(a1, &x, &y) || unitAt(x, y) < 0) {
        printf("! 无效坐标或空格\n");
        continue;
      }
      int ui = unitAt(x, y);
      cliPrintUnitLine(g.units[ui]);
      static int dist[n][n];
      computeReach(ui, dist);
      printf("  可达:");
      for (int yy = 0; yy < n; yy++)
        for (int xx = 0; xx < n; xx++)
          if (dist[yy][xx] >= 0 && canStand(ui, xx, yy))
            printf(" %s", sqName(xx, yy).c_str());
      printf("\n  目标:");
      std::vector<std::pair<int, bool>> ts;
      findTargets(ui, x, y, ts);
      for (auto &[ti, heal] : ts)
        printf(" %s%s", heal ? "+" : "x", unitTag(g.units[ti]).c_str());
      printf("\n");
      continue;
    }
    if (cmd == "mv" || cmd == "mva" || cmd == "atk" || cmd == "wait") {
      int ui = cliOwnUnit(a1);
      if (ui < 0)
        continue;
      bool ok = true;
      if (cmd == "mv")
        ok = cliMoveTo(ui, a2);
      else if (cmd == "atk")
        ok = cliActOn(ui, a2);
      else if (cmd == "mva") {
        ok = cliMoveTo(ui, a2);
        if (ok && g.gameMode == MPlay)
          cliActOn(ui, a3); // 已经移动，攻击失败也视为已行动
      }
      if (ok) {
        g.units[ui].acted = true;
        if (cliCheckEnd())
          continue;
        bool allActed = true;
        for (auto &u : g.units)
          if (u.alive && !u.def().enemy && !u.acted)
            allActed = false;
        if (allActed) {
          cliEnemyTurn();
          cliCheckEnd();
        }
      }
      continue;
    }
    if (cmd == "end" || cmd == "e") {
      cliEnemyTurn();
      cliCheckEnd();
      continue;
    }
    printf("! 未知命令，help 查看\n");
  }
  return 0;
}

// 用固定的朴素贪心玩家（scriptedPlayerAct）迎战当前敌方 AI —— 一个不随 AI 演化的
// 独立强度标尺。若自走(--analyze)与本表对同一关难度判断相反，多半是"自走同源偏差"。
static int cliBench(int games, int maxTurns) {
  printf("固定朴素玩家 vs 当前敌方 AI（%d 局/关，回合上限 %d）——独立强度标尺\n\n", games, maxTurns);
  printf("关卡        胜   负  超时  均胜回合  主角均余血\n");
  benchScriptedPlayer = true;
  for (int l = 0; l < nlevels; l++) {
    SelfplayStats st = cliSelfplay(l, games, maxTurns, false);
    printf("%d %-8s %3d  %3d  %3d  %7.1f  %9.1f\n", l + 1, levels[l].name, st.wins, st.losses,
           st.timeouts, st.wins ? st.sumWinTurns / st.wins : 0.0,
           st.wins ? st.sumGreenHp / st.wins : 0.0);
  }
  benchScriptedPlayer = false;
  return 0;
}

// ------------------------------------------------------------------ 战术回归测试
// 给定精确局面 → 断言 AI 的决策。确定性、不依赖自走，是 AI 正确性的地面真值
// （防止改动悄悄踩坏旧能力）。
static int testFails = 0, testTotal = 0;
static void tReset(int lvl) {
  loadLevel(lvl);
  g.units.clear();
  g.gameMode = MPlay;
  scoreJitter = 0;
  aiSim = false;
  benchScriptedPlayer = false;
}
static int tAdd(int type, int x, int y, int hp) {
  Unit u;
  u.type = type;
  u.x = x;
  u.y = y;
  u.hp = hp;
  u.vx = (float)x;
  u.vy = (float)y;
  g.units.push_back(u);
  return (int)g.units.size() - 1;
}
static void tCheck(const char *name, bool ok, const std::string &detail) {
  testTotal++;
  if (!ok)
    testFails++;
  printf("  [%s] %s%s\n", ok ? "PASS" : "FAIL", name, ok ? "" : ("  <- " + detail).c_str());
}

static int runTests() {
  printf("战术回归测试（确定性，独立于自走）：\n");

  // T1 有必杀就取——远程小兵射程内可秒杀红法师
  tReset(0);
  tAdd(UGreen, 0, 0, 12);
  int a = tAdd(UEsoldier, 7, 7, 1);
  int tgt = tAdd(URed, 7, 9, 2);
  aiActFor(a);
  tCheck("lethal-kill", !g.units[tgt].alive, "敌兵未秒杀射程内的红法师");

  // T2 集火脆皮——敌将同时够到红法师(def0)与蓝骑士(def2)，应打红法师
  tReset(0);
  tAdd(UGreen, 0, 0, 12);
  a = tAdd(UEcaptain, 7, 7, 9);
  int mage = tAdd(URed, 7, 8, 8);
  int kn = tAdd(UBlue, 6, 7, 14);
  aiActFor(a);
  tCheck("focus-squishy", g.units[mage].hp < 8 && g.units[kn].hp == 14,
         TextFormat("红法师hp=%d 蓝骑士hp=%d", g.units[mage].hp, g.units[kn].hp));

  // T3 远程放风筝——敌巫应隔空射击而非贴脸
  tReset(0);
  tAdd(UGreen, 0, 0, 12);
  a = tAdd(UEmage, 7, 5, 6);
  kn = tAdd(UBlue, 7, 9, 14);
  aiActFor(a);
  tCheck("ranged-kite", g.units[kn].hp < 14 && manh(g.units[a].x, g.units[a].y, 7, 9) >= 2,
         TextFormat("蓝骑士hp=%d 距离=%d", g.units[kn].hp, manh(g.units[a].x, g.units[a].y, 7, 9)));

  // T4 群胆压上——有 2 名同伴壮胆时，敌将贴脸攻击蓝骑士（历史"贴脸不打"回归）
  tReset(0);
  tAdd(UGreen, 0, 0, 12);
  kn = tAdd(UBlue, 7, 4, 14);
  a = tAdd(UEcaptain, 7, 7, 9);
  tAdd(UEsoldier, 6, 8, 1);
  tAdd(UEsoldier, 8, 8, 1);
  aiActFor(a);
  tCheck("grouped-commit", g.units[kn].hp < 14, "群胆敌将未攻击蓝骑士（贴脸不打）");

  // T5 优先取价值——小兵射程内既能秒红法师又能蹭主角，应取红法师的人头
  tReset(0);
  tAdd(UGreen, 7, 9, 12);
  int red = tAdd(URed, 7, 5, 2);
  a = tAdd(UEsoldier, 7, 7, 1);
  aiActFor(a);
  tCheck("secure-kill", !g.units[red].alive, "小兵没取射程内红法师的人头");

  // T6 牧师治疗——应治疗相邻缺血的主角
  tReset(0);
  tAdd(UEsoldier, 0, 0, 1);
  a = tAdd(UYellow, 7, 7, 9);
  int green = tAdd(UGreen, 7, 8, 5);
  aiActFor(a);
  tCheck("healer-heals", g.units[green].hp > 5,
         TextFormat("牧师未治疗缺血主角，hp=%d", g.units[green].hp));

  // T7 魔王范围——同时波及范围内两个小兵
  tReset(0);
  tAdd(UGreen, 0, 14, 12);
  a = tAdd(UEboss, 7, 7, 20);
  int s1 = tAdd(USoldier, 7, 8, 1);
  int s2 = tAdd(USoldier, 8, 7, 1);
  aiActFor(a);
  tCheck("boss-aoe", !g.units[s1].alive && !g.units[s2].alive, "魔王范围攻击未同时命中两个小兵");

  // T8 夺旗关主角朝旗帜推进
  tReset(2);
  tAdd(UEsoldier, 0, 0, 1);
  green = tAdd(UGreen, 2, 13, 12);
  int before = manh(2, 13, g.exitX, g.exitY);
  aiActFor(green);
  int after = manh(g.units[green].x, g.units[green].y, g.exitX, g.exitY);
  tCheck("hero-marches-flag", after < before,
         TextFormat("到旗帜距离 %d->%d 未缩短", before, after));

  printf("\n结果：%d/%d 通过%s\n", testTotal - testFails, testTotal,
         testFails ? "（有失败！）" : "");
  return testFails ? 1 : 0;
}

// ------------------------------------------------------------------ 主循环
int main(int Argc, char **Argv) {
  if (Argc > 1 && strcmp(Argv[1], "--cli") == 0) {
    cliMode = true;
    return cliMain(Argc > 2 ? atoi(Argv[2]) - 1 : 0);
  }
  if (Argc > 1 && strcmp(Argv[1], "--analyze") == 0) {
    cliMode = true;
    return cliAnalyze(Argc > 2 ? atoi(Argv[2]) : 20, Argc > 3 ? atoi(Argv[3]) : 40);
  }
  if (Argc > 1 && strcmp(Argv[1], "--bench") == 0) {
    cliMode = true;
    return cliBench(Argc > 2 ? atoi(Argv[2]) : 100, Argc > 3 ? atoi(Argv[3]) : 40);
  }
  if (Argc > 1 && strcmp(Argv[1], "--test") == 0) {
    cliMode = true;
    return runTests();
  }
  SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
  InitWindow(winW, winH, "Squarebound · 方格征途");
  SetExitKey(KEY_NULL);
  SetTargetFPS(60);

  std::vector<int> cps = buildCodepoints();
  gFont = GetFontDefault();
  gFontBig = GetFontDefault();
  for (const std::string &path : cjkFontCandidates()) {
    Font f36, f84;
    if (!loadValidatedFont(path, 36, cps, &f36))
      continue;
    if (!loadValidatedFont(path, 84, cps, &f84)) {
      UnloadFont(f36);
      continue;
    }
    gFont = f36;
    gFontBig = f84;
    SetTextureFilter(gFont.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(gFontBig.texture, TEXTURE_FILTER_BILINEAR);
    TraceLog(LOG_INFO, "SQB: CJK font loaded: %s", path.c_str());
    break;
  }

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();
    if (dt > 0.1f)
      dt = 0.1f;
    updateAnims(dt);

    switch (g.gameMode) {
    case MTitle:
      updateTitle();
      break;
    case MIntro:
      handleSaveLoadKeys();
      if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        g.gameMode = MPlay;
        beginPlayerTurn(false);
      }
      if (IsKeyPressed(KEY_ESCAPE))
        g.gameMode = MTitle;
      break;
    case MPlay:
      updatePlay(dt);
      break;
    case MVictory:
      handleSaveLoadKeys();
      if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        if (g.levelIdx + 1 < nlevels) {
          loadLevel(g.levelIdx + 1);
          g.gameMode = MIntro;
        } else
          g.gameMode = MEnding;
      }
      break;
    case MDefeat:
      handleSaveLoadKeys();
      if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        loadLevel(g.levelIdx);
        g.gameMode = MIntro;
      }
      if (IsKeyPressed(KEY_ESCAPE))
        g.gameMode = MTitle;
      break;
    case MEnding:
      if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE))
        g.gameMode = MTitle;
      break;
    }

    BeginDrawing();
    ClearBackground(cDesk);
    switch (g.gameMode) {
    case MTitle:
      drawTitle();
      break;
    case MIntro:
      drawIntro();
      break;
    case MPlay:
      drawBoard();
      drawPanel();
      break;
    case MVictory:
      drawEndOverlay(true);
      break;
    case MDefeat:
      drawEndOverlay(false);
      break;
    case MEnding:
      drawEnding();
      break;
    }
    drawBannerToasts(dt);
    EndDrawing();
  }

  if (gFont.texture.id != GetFontDefault().texture.id)
    UnloadFont(gFont);
  if (gFontBig.texture.id != GetFontDefault().texture.id)
    UnloadFont(gFontBig);
  CloseWindow();
  return 0;
}

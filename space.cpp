//#include"sdk_AI_Client.hpp"
#include "ai_client.h"
#include <algorithm>
#include <cstring>
#include <ctime>
#include <fstream>
#include <random>
#include <set>
#include <stack>
#include <string>
#include <utility>
#include <vector>
#define fakepos Pos(-1, -1, -1)
using namespace std;

// typedef tuple<int, int, int> Pos;
// typedef map<string, vector<vector<int>>> Trapbag;

ofstream fout;
int totalround = 0; //轮数
Player player[4];   //玩家
vector<int> others; //同层玩家
/*
struct Player {            // 玩家类，存储玩家的相关信息
    int hp = 200;          // 玩家的血量
    STATUS status = ALIVE; // 玩家现在的状态
    int id = -1;           //玩家的id
    vector<int> keys; // 玩家当前拿到的钥匙
    Pos pos;               // 玩家坐标
    Tool toolbag;          //工具栏
    bool has_entered_escape_layer = false; //对于other player有意义，对于自己是无意义的，表示是否曾经到达逃生舱那层
    int left;//剩余挨刀数
};*/
int myid;                         // id
Pos mypos;                        //位置
Node mynode;                      //位置
Tool myitem;                      //工具
vector<int> keys;                 //钥匙
bool endturn = 0;                 //结束回合
bool action = 0;                  //主要行动
int mainstrategy = 0;             //主策略,0照常抢钥匙，1顶层拦截
int strategy = 0;                 //策略
Pos dst = make_tuple(-1, -1, -1); //目标位置
int v;
int staycount = 0;   //连续不动的回合数
int movecount = 0;   //连续移动的回合数
vector<Pos> lastpos; //保存曾经的位置
Pos escapepos;

struct All0f { // 0层所有点
    Pos pos;
    All0f() = default;
    All0f(int x, int y, int z) : pos(Pos(x, y, z)) {}
};
vector<All0f> all0f;
struct All1f { // 1层所有点
    Pos pos;
    All1f() = default;
    All1f(int x, int y, int z) : pos(Pos(x, y, z)) {}
};
vector<All1f> all1f;
struct All2f { // 2层所有点
    Pos pos;
    All2f() = default;
    All2f(int x, int y, int z) : pos(Pos(x, y, z)) {}
};
vector<All2f> all2f;
struct Inv { //物资点类
    Pos pos;
    int left = 2;
    int cd = 0;
    Inv() = default;
    Inv(int x, int y, int z) : pos(x, y, z), left(2), cd(0) {}
};
vector<Inv> inv;
struct Box { //死亡掉落物类
    Pos pos;
    int val; //价值
    Box() = default;
    Box(Pos p, int v) : pos(p), val(v) {}
    Box(int x, int y, int z) : pos(x, y, z) { val = 0; }
};
vector<Box> box;
struct Ele { //电梯类
    Pos pos;
    Ele() = default;
    Ele(int x, int y, int z) : pos(x, y, z) {}
};
vector<Ele> ele;
struct Dan { //缩圈点类
    Pos pos;
    int totalround;
    Dan(int x, int y, int z, int r) : pos(Pos(x, y, z)), totalround(r) {}
};
vector<Dan> dan;
struct Buf { //上下电梯的缓存
    Pos pos;
    int saveval;
    int left;
    Buf() = default;
    Buf(Pos p, int sv) : pos(p), saveval(sv), left(3) {}
};
vector<Buf> buf;
struct EnemyPos { //敌人占据的位置
    Pos pos;
    int left;
    EnemyPos() = default;
    EnemyPos(Pos p) : pos(p), left(4) {}
};
vector<EnemyPos> epos;
int detectval = 0; //探查指数

pair<Pos, Pos> getkeypos(int id);
int dis(Pos s, Pos t, AI_Client &client);
int possave(Pos pos, AI_Client &client, bool active);
int restrictdanval(Pos pos);

void detect(Pos pos, AI_Client &client) {
    if (client.get_check_cd() > 0) //探查cd没到
        return;
    if (pos == escapepos) //逃生点不探查
        return;
    for (auto c : ele) { //电梯不探查
        if (c.pos == pos)
            return;
    }
    for (int i = 0; i <= 3; i++) //钥匙机不探查
        if (getkeypos(i).first == pos || getkeypos(i).second == pos)
            return;
    bool should = 0;
    for (auto c : epos) {
        if (c.pos == pos) {
            should = 1;
            break;
        }
    }
    if (should == 0) //没敌人曾经到达
        return;
    cerr << "detect" << endl;
    if (detectval > 0) { //最近追人或正在追人
        client.detect(pos);
        return;
    } else {
        for (auto c : inv) { //物资点探查
            if (c.pos == pos) {
                client.detect(pos);
                return;
            }
        }
        if (dis(pos, escapepos, client) == 1) { //在终点旁边则探查
            client.detect(pos);
            return;
        }
    }
    cerr << "not detect" << endl;
}

void printpos(Pos pos) { //调试用，输出一个位置的坐标
    cerr << get<0>(pos) << ' ' << get<1>(pos) << ' ' << get<2>(pos) << endl;
}

int dis(Pos s, Pos t, AI_Client &client) { //可跨层两点间距离，若没有路径则返回-1
    if (s == t)
        return 0;
    return client.find_shortest_path(s, t).size() - 1;
}
int planedis(Pos s, Pos t, AI_Client &client) { //同层两点间距离，若没有路径则返回-1
    if (s == t)
        return 0;
    if (get<2>(s) != get<2>(t)) //跨层
        return client.find_shortest_path(s, t).size() + 1;
    return client.find_shortest_path(s, t).size() - 1;
}
int centerdis(Pos s, AI_Client &client) { //离中心曼哈顿距离
    return abs(get<0>(s) - 3) + abs(get<1>(s) - 3);
}
string getFirstTrap() {
    if (myitem.trapbag["LandMine"][0][0])
        return "LandMine";
    if (myitem.trapbag["Sticky"][0][0])
        return "Sticky";
    return "None";
}

string nextToInspect(Tool &inventory) {
    if (player[myid].hp <= 140 && myitem.kits_num == 0) //血低则拿医疗包
        return "Kit";
    if (inventory.blinks_num < 1) //优先拿闪现
        return "Transport";
    if (inventory.kits_num < 1) //拿医疗包
        return "Kit";
    if (inventory.blinks_num < 2) //优先拿满闪现
        return "Transport";
    if (inventory.kits_num < 2) //再拿满医疗包
        return "Kit";
    if (inventory.trapbag["LandMine"][0][0] < 2) //拿取地雷
        return "LandMine";
    return "Sticky";
}

int getboxval(int enemyid) {
    int ret = 0;
    for (auto c : player[enemyid].keys) {
        if (find(keys.begin(), keys.end(), c) == keys.end()) { //自己没有这把钥匙
            ret++;
        }
    }
    return ret;
}

void updatestatus(AI_Client &client) {
    mypos = client.get_player_pos();
    mynode = client.pos2node(mypos);
    myitem = client.get_tools();
    keys = client.get_keys();
    player[myid] = client.player;
    others.clear();
    for (auto c : client.others) {
        if (c.id >= 0 && get<0>(player[c.id].pos) >= 0 && player[c.id].status == ALIVE && c.status == DEAD) //看见刚死
            box.push_back(Box(player[c.id].pos, getboxval(c.id)));
        if (c.id >= 0 && player[c.id].status != ESCAPED && c.status == ESCAPED) //看见刚逃脱
            mainstrategy = 0;
        if (c.id >= 0) {
            int tmp = player[c.id].escapeleft;
            player[c.id] = c;
            player[c.id].escapeleft = tmp;
        }
        if (c.id >= 0 && get<2>(c.pos) == get<2>(mypos) && c.status != ESCAPED && c.status != DEAD &&
            c.status != AI_ERROR) { //他人真实在视野内
            others.push_back(c.id);
        }
    }
    for (auto c : player) {
        player[c.id].left = (player[c.id].hp - 1) / 70 + 1;
    }
    if (player[myid].hp >= 120 && myitem.kits_num > 0) { //如果有医疗包
        player[myid].left = min(3, player[myid].left + 1);
    }

    //更新物资点
    auto iter = inv.begin();
    while (iter != inv.end()) {
        Pos pos = (*iter).pos; //物资点的位置
        bool flag = 0;
        for (auto c : dan) {
            if (c.totalround <= totalround && c.pos == pos) { //被缩圈
                flag = 1;
                break;
            }
        }
        if (flag) {
            inv.erase(iter);
        } else {
            iter++;
        }
    }
    //更新盒子
    auto iter2 = box.begin();
    while (iter2 != box.end()) {
        Pos pos = (*iter2).pos; //盒子的位置
        bool flag = 0;
        for (auto c : dan) {
            if (c.totalround <= totalround && c.pos == pos) { //被缩圈
                flag = 1;
                break;
            }
        }
        if (flag) {
            box.erase(iter2);
        } else {
            iter2++;
        }
    }
}
void updateinv(AI_Client &client) { //更新一回合仅发生一次的一些重要信息
                                    //更新物资点
    for (int i = 0; i < inv.size(); i++) {
        if (inv[i].left != 2) {
            inv[i].cd -= 1;
        }
        if (inv[i].cd == 0) {
            if (inv[i].left != 2) {
                inv[i].left++;
                inv[i].cd = inv[i].left == 2 ? 0 : 8;
            }
        }
    }

    //更新地图点
    auto iter = all0f.begin();
    while (iter != all0f.end()) {
        for (auto c : dan) {
            if (c.pos == (*iter).pos && totalround > c.totalround) {
                all0f.erase(iter);
                continue;
            }
        }
        iter++;
    }
    auto iter2 = all1f.begin();
    while (iter2 != all1f.end()) {
        for (auto c : dan) {
            if (c.pos == (*iter2).pos && totalround > c.totalround) {
                all1f.erase(iter2);
                continue;
            }
        }
        iter2++;
    }
    auto iter3 = all2f.begin();
    while (iter3 != all2f.end()) {
        for (auto c : dan) {
            if (c.pos == (*iter3).pos && totalround > c.totalround) {
                all2f.erase(iter3);
                continue;
            }
        }
        iter3++;
    }

    //更新掉落物
    if (get<2>(mypos) == 0) {
        auto it = box.begin();
        while (it != box.end()) {
            if (get<2>((*it).pos) == 0 && (client.judge_if_can_pick(client.pos2node((*it).pos))).second.size() == 0 ||
                (client.judge_if_can_pick(client.pos2node((*it).pos))).second.size() == 1 &&
                    client.judge_if_can_pick(client.pos2node((*it).pos)).second[0] != "Box") {
                box.erase(it);
            } else {
                it++;
            }
        }
        for (auto c : all0f) {
            auto res = client.judge_if_can_pick(client.pos2node(c.pos));
            if (res.second.size() == 2) {
                box.push_back(Box(c.pos, 1));
            } else if (res.second.size() == 1) {
                if (res.second[0] == "Box") {
                    box.push_back(Box(c.pos, 1));
                }
            }
        }
    } else if (get<2>(mypos) == 1) {
        auto it = box.begin();
        while (it != box.end()) {
            if (get<2>((*it).pos) == 1 && (client.judge_if_can_pick(client.pos2node((*it).pos))).second.size() == 0 ||
                (client.judge_if_can_pick(client.pos2node((*it).pos))).second.size() == 1 &&
                    client.judge_if_can_pick(client.pos2node((*it).pos)).second[0] != "Box") {
                box.erase(it);
            } else {
                it++;
            }
        }
        for (auto c : all1f) {
            auto res = client.judge_if_can_pick(client.pos2node(c.pos));
            if (res.second.size() == 2) {
                box.push_back(Box(c.pos, 1));
            } else if (res.second.size() == 1) {
                if (res.second[0] == "Box") {
                    box.push_back(Box(c.pos, 1));
                }
            }
        }
    } else {
        auto it = box.begin();
        while (it != box.end()) {
            if (get<2>((*it).pos) == 2 && (client.judge_if_can_pick(client.pos2node((*it).pos))).second.size() == 0 ||
                (client.judge_if_can_pick(client.pos2node((*it).pos))).second.size() == 1 &&
                    client.judge_if_can_pick(client.pos2node((*it).pos)).second[0] != "Box") {
                box.erase(it);
            } else {
                it++;
            }
        }
        for (auto c : all2f) {
            auto res = client.judge_if_can_pick(client.pos2node(c.pos));
            if (res.second.size() == 2) {
                box.push_back(Box(c.pos, 1));
            } else if (res.second.size() == 1) {
                if (res.second[0] == "Box") {
                    box.push_back(Box(c.pos, 1));
                }
            }
        }
    }

    //更新换层缓存
    for (int i = 0; i < buf.size(); i++) {
        buf[i].left--;
    }
    auto iter4 = buf.begin();
    while (iter4 != buf.end()) {
        if ((*iter4).left == 0) { //缓存到期
            buf.erase(iter4);
        } else
            iter4++;
    }

    //更新探查
    detectval = max(0, detectval - 1);
    for (int i = 0; i < epos.size(); i++) {
        epos[i].left--;
    }
    auto iter5 = epos.begin();
    while (iter5 != epos.end()) {
        if ((*iter5).left == 0) { //缓存到期
            epos.erase(iter5);
        } else
            iter5++;
    }
    for (auto c : others) { //记录敌人的位置
        epos.push_back(EnemyPos(player[c].pos));
    }
}
int valuable(int enemyid, AI_Client &client) { //判断一个敌人对自己是否有价值，看钥匙，返回值越大价值越高
    int ret = 0;
    if (player[enemyid].keys.size() == 4)
        ret += 2;
    if (player[enemyid].keys.size() == 3)
        ret += 1;
    for (auto key : player[enemyid].keys) {
        if (find(keys.begin(), keys.end(), key) == keys.end()) { //敌人拥有我没有的钥匙
            ret++;
        }
    }
    return ret;
}
pair<int, vector<int>> nearenemies(Pos pos, AI_Client &client) { //返回pos位置相邻的敌人数量和位置
    int num = 0;
    vector<int> tmp;
    tmp.clear();
    for (auto c : others) {
        if (player[c].hp > 0 &&
            (planedis(player[c].pos, pos, client) == 0 || planedis(player[c].pos, pos, client) == 1)) {
            num++;
            tmp.push_back(c);
        }
    }

    return make_pair(num, tmp);
}
pair<int, vector<int>> twoblockenemies(Pos pos, AI_Client &client) { //返回pos位置两格远的敌人数量和位置
    int num = 0;
    vector<int> tmp;
    tmp.clear();
    for (auto c : others) {
        if (player[c].hp > 0 && planedis(player[c].pos, pos, client) == 2) {
            num++;
            tmp.push_back(c);
        }
    }
    return make_pair(num, tmp);
}
pair<int, vector<int>> threeblockenemies(Pos pos, AI_Client &client) { //返回pos位置三格远的敌人数量和位置
    int num = 0;
    vector<int> tmp;
    tmp.clear();
    for (auto c : others) {
        if (player[c].hp > 0 && planedis(player[c].pos, pos, client) == 3) {
            num++;
            tmp.push_back(c);
        }
    }
    return make_pair(num, tmp);
}

bool invjudge(Pos pos) { //判断一个物资点是否应该捡
    for (auto c : inv) {
        if (c.pos == pos) {
            if (c.left != 0) //有剩余
                return 1;
            else
                return 0;
        }
    }
    return 0;
}

bool isele(Pos pos) { //判断pos位置是否是电梯
    for (int i = 0; i < ele.size(); i++)
        if (ele[i].pos == pos)
            return 1;

    return 0;
}
bool movetopos(Pos pos, AI_Client &client) { //执行移动指令
    if (restrictdanval(pos) == 1) {
        return 0;
    }
    detect(pos, client);
    movecount++;
    if (get<2>(pos) != get<2>(mypos))
        buf.push_back(Buf(mypos, possave(mypos, client, 1)));
    return client.move(pos);
}
bool placetrap(AI_Client &client) { //设置陷阱
    if (getFirstTrap() != "None")
        client.place_trap(getFirstTrap());
}
bool isrestricted(AI_Client &client) { //观察上几回合的位置判断自己是否被局限住了
    if (lastpos.size() < 6 || movecount < 6)
        return 0;
    for (int i = lastpos.size() - 6; i < lastpos.size() - 1; i++) {
        for (int j = i + 1; j < lastpos.size(); j++) {
            if (dis(lastpos[i], lastpos[j], client) > 2) {
                return 0;
            }
        }
    }
    return 1;
}

bool watch(AI_Client &client) { //监视其他玩家的钥匙情况
    for (int i = 0; i <= 3; i++) {
        if (i != myid) {
            if (player[i].keys.size() == 4 && player[i].status != ESCAPED) { //发现有人拿了4把钥匙
                return 1;
            }
        }
    }
    return 0;
}
int getdegree(Pos pos, AI_Client &client) { //获得一个点的度数
    return client.get_neighbors(pos).size();
}
pair<Pos, Pos> getkeypos(int id) { //返回钥匙机的位置
    if (id == 0) {
        return make_pair(Pos(0, 0, 1), Pos(0, 0, 2));
    } else if (id == 1) {
        return make_pair(Pos(6, 0, 1), Pos(6, 0, 2));
    } else if (id == 2) {
        return make_pair(Pos(6, 6, 1), Pos(6, 6, 2));
    } else {
        return make_pair(Pos(0, 6, 1), Pos(0, 6, 2));
    }
}
pair<int, Pos> getnearestinv(Pos pos, AI_Client &client) { //获得最近的有效物资点位置
    Pos targetpos = fakepos;
    int mindis = 100;
    for (auto c : box) { //掉落物
        if (dis(c.pos, pos, client) < mindis) {
            mindis = dis(c.pos, pos, client);
            targetpos = c.pos;
        }
    }
    if (targetpos != fakepos)
        return make_pair(mindis, targetpos);
    for (auto c : inv) {  //物资点
        if (c.left > 0) { //还有物资
            if (dis(c.pos, pos, client) < mindis) {
                mindis = dis(c.pos, pos, client);
                targetpos = c.pos;
            }
        }
    }
    return make_pair(mindis, targetpos);
}
pair<int, Pos> getnearestkey(Pos pos, AI_Client &client) { //获得最近的有效钥匙位置
    Pos targetpos = fakepos;
    int mindis = 100;
    for (int i = 0; i <= 3; i++) {
        if (find(keys.begin(), keys.end(), i) == keys.end()) {
            Pos p1 = getkeypos(i).first;
            Pos p2 = getkeypos(i).second;
            if (dis(pos, p1, client) < mindis) {
                mindis = dis(pos, p1, client);
                targetpos = p1;
            } else if (dis(pos, p1, client) == mindis && get<2>(pos) == get<2>(p1)) { //优先同层
                mindis = dis(pos, p1, client);
                targetpos = p1;
            }
            if (dis(pos, p2, client) < mindis) {
                mindis = dis(pos, p2, client);
                targetpos = p2;
            } else if (dis(pos, p2, client) == mindis && get<2>(pos) == get<2>(p2)) { //优先同层
                mindis = dis(pos, p2, client);
                targetpos = p2;
            }
        }
    }
    return make_pair(mindis, targetpos);
}
pair<int, Pos> getnearestele(Pos pos, AI_Client &client) { //获得最近的电梯位置
    Pos targetpos = fakepos;
    int mindis = 100;
    for (auto c : ele) {
        if (dis(c.pos, pos, client) < mindis) {
            mindis = dis(c.pos, pos, client);
            targetpos = c.pos;
        }
    }
    return make_pair(mindis, targetpos);
}
pair<int, Pos> getnearestvalenemy(Pos pos, AI_Client &client) { //获得最近的有价值敌人位置
    Pos targetpos = fakepos;
    int mindis = 100;
    int numkeys = -1;
    for (auto c : others) {
        if (valuable(c, client) >= 4 && player[c].pos != escapepos && player[c].keys.size() > numkeys) { //高价值敌人
            mindis = dis(player[c].pos, pos, client);
            targetpos = player[c].pos;
            numkeys = player[c].keys.size();
        } else if (dis(player[c].pos, pos, client) < mindis && valuable(c, client) == 4 && player[c].pos != escapepos &&
                   player[c].keys.size() == numkeys) {
            mindis = dis(player[c].pos, pos, client);
            targetpos = player[c].pos;
            numkeys = player[c].keys.size();
        }
    }
    if (targetpos != fakepos)
        return make_pair(mindis, targetpos);
    mindis = 100;
    numkeys = -1;
    for (auto c : others) {
        if (valuable(c, client) == 3 && player[c].pos != escapepos && player[c].keys.size() > numkeys) { //高价值敌人
            mindis = dis(player[c].pos, pos, client);
            targetpos = player[c].pos;
            numkeys = player[c].keys.size();
        } else if (dis(player[c].pos, pos, client) < mindis && valuable(c, client) == 3 && player[c].pos != escapepos &&
                   player[c].keys.size() > numkeys) {
            mindis = dis(player[c].pos, pos, client);
            targetpos = player[c].pos;
            numkeys = player[c].keys.size();
        }
    }
    if (targetpos != fakepos)
        return make_pair(mindis, targetpos);
    mindis = 100;
    numkeys = -1;
    for (auto c : others) {
        if (valuable(c, client) == 2 && player[c].pos != escapepos && player[c].keys.size() > numkeys) { //高价值敌人
            mindis = dis(player[c].pos, pos, client);
            targetpos = player[c].pos;
            numkeys = player[c].keys.size();
        } else if (dis(player[c].pos, pos, client) < mindis && valuable(c, client) == 2 && player[c].pos != escapepos &&
                   player[c].keys.size() > numkeys) {
            mindis = dis(player[c].pos, pos, client);
            targetpos = player[c].pos;
            numkeys = player[c].keys.size();
        }
    }
    return make_pair(mindis, targetpos);
    /* mindis = 100;
    for (auto c : others) {
        if (dis(player[c].pos, pos, client) < mindis && valuable(c, client) && player[c].pos != escapepos) {
            mindis = dis(player[c].pos, pos, client);
            targetpos = player[c].pos;
        }
    }
    return make_pair(mindis, targetpos);*/
}
int restrictdanval(Pos pos) { //返回缩圈系数，越小越危险
    for (auto c : dan) {
        if (c.totalround >= totalround && c.pos == pos) {
            if (totalround == c.totalround)
                return 1;
            if (c.totalround - totalround == 1)
                return 2;
            if (c.totalround - totalround == 2)
                return 4;
        }
    }
    return 10;
}
int posdanval(Pos pos) { // 位置系数，越小越危险
    for (int i = (int)(buf.size() - 1); i >= 0; i--) {
        if (buf[i].pos == pos) {
            return buf[i].saveval;
        }
    }
    return 10;
}
int possave(Pos pos, AI_Client &client,
            bool active) { //判断一个位置是否安全,返回1到6，越小代表越不安全,active代表是否先手
    int danval = min(restrictdanval(pos), posdanval(pos)); //危险系数
    if (nearenemies(pos, client).first == 0 && twoblockenemies(pos, client).first == 0 &&
        threeblockenemies(pos, client).first == 0) {
        cerr << "safe 6" << endl;
        return min(danval, 6);
    } else if (nearenemies(pos, client).first == 0 && twoblockenemies(pos, client).first == 0) { //三格有敌人
        auto enemies = threeblockenemies(pos, client).second; //所有隔三格的敌人
        auto iter = enemies.begin();
        while (iter != enemies.end()) {
            if (player[*iter].status == WAIT_FOR_ESCAPE) {
                enemies.erase(iter);
            } else {
                iter++;
            }
        }
        if (enemies.size() == 0) {
            cerr << "safe 5" << endl;
            return min(danval, 5);
        }
        int totalleft = 0;
        for (auto c : enemies) {
            totalleft += player[c].left;
        }
        if (enemies.size() >= 2) {
            if (player[myid].left == 1) {
                cerr << "safe 2" << endl;
                return 2;
            }
            if (player[myid].left == 2) {
                cerr << "safe 3" << endl;
                return min(danval, 3);
            }
            if (player[myid].left == 3) {
                cerr << "safe 4" << endl;
                return min(danval, 4);
            }
        }
        if (player[myid].left == 1) {
            cerr << "safe 3" << endl;
            return 3;
        }
        cerr << "safe 5" << endl;
        return min(danval, 5);
    } else if (nearenemies(pos, client).first == 0) {       //两格有敌人
        auto enemies = twoblockenemies(pos, client).second; //所有隔两格的敌人
        auto iter = enemies.begin();
        while (iter != enemies.end()) {
            if (player[*iter].status == WAIT_FOR_ESCAPE) {
                enemies.erase(iter);
            } else {
                iter++;
            }
        }
        if (enemies.size() == 0) {
            cerr << "safe 5" << endl;
            return min(danval, 5);
        }
        int totalleft = 0;
        for (auto c : enemies) {
            totalleft += player[c].left;
        }
        if (enemies.size() >= 2) {
            if (player[myid].left == 1) {
                cerr << "safe 1" << endl;
                return 1;
            }
            if (player[myid].left == 2 && totalleft <= 3) {
                cerr << "safe 3" << endl;
                return min(danval, 3);
            }
            if (player[myid].left == 2 && totalleft > 3) {
                cerr << "safe 2" << endl;
                return min(danval, 2);
            }
            if (player[myid].left == 3 && totalleft <= 5) {
                cerr << "safe 4" << endl;
                return min(danval, 4);
            }
            if (player[myid].left == 3 && totalleft > 5) {
                cerr << "safe 3" << endl;
                return min(danval, 3);
            }
        }
        if (enemies.size() == 1) {
            if (player[myid].left == 1 && active && totalleft == 1 && myitem.blinks_num) {
                cerr << "safe 4" << endl;
                return min(danval, 4);
            }
            if (player[myid].left == 1) {
                cerr << "safe 2" << endl;
                return min(danval, 2);
            }
            if (player[myid].left == 2 && totalleft == 3) {
                cerr << "safe 3" << endl;
                return min(danval, 3);
            }
            if (player[myid].left == 2 && totalleft == 2) {
                if (active) {
                    cerr << "safe 4" << endl;
                    return min(danval, 4);
                } else {
                    cerr << "safe 3" << endl;
                    return min(danval, 3);
                }
            }
            if (player[myid].left == 2 && totalleft < 2) {
                cerr << "safe 4" << endl;
                return min(danval, 4);
            }
            if (player[myid].left == 3 && totalleft == 3 && myitem.blinks_num > 0) {
                cerr << "safe 4" << endl;
                return min(danval, 4);
            }
            if (player[myid].left == 3 && totalleft == 3) {
                cerr << "safe 3" << endl;
                return min(danval, 3);
            }
            if (player[myid].left == 3 && totalleft <= 2) {
                cerr << "safe 5" << endl;
                return min(danval, 5);
            }
        }
    } else {                                            //一格有敌人
        auto enemies = nearenemies(pos, client).second; //所有隔一格以内的敌人
        auto iter = enemies.begin();
        while (iter != enemies.end()) {
            if (player[*iter].status == WAIT_FOR_ESCAPE) {
                enemies.erase(iter);
            } else {
                iter++;
            }
        }
        int twoval = twoblockenemies(pos, client).first;
        if (enemies.size() == 0) {
            cerr << "safe 5" << endl;
            return min(danval, 5);
        }
        int totalleft = 0;
        for (auto c : enemies) {
            totalleft += player[c].left;
        }
        if (enemies.size() >= 2) {
            if (active) {
                if (player[myid].left >= totalleft + enemies.size() - 1) {
                    cerr << "safe 4" << endl;
                    return 4;
                }
                if (player[myid].left == totalleft + enemies.size() - 2) {
                    cerr << "safe 2" << endl;
                    return 2;
                }
                cerr << "safe 1" << endl;
                return 1;
            } else {
                cerr << "safe 1" << endl;
                return 1;
            }
        }
        if (active) { //先手
            if (totalleft > player[myid].left) {
                cerr << "safe 1" << endl;
                return 1;
            }
            if (totalleft == player[myid].left) {
                cerr << "safe 4" << endl;
                return min(danval, 4);
            }
            if (totalleft == player[myid].left - 1 && twoval > 0) {
                cerr << "safe 4" << endl;
                return min(danval, 4);
            }
            if (totalleft == player[myid].left - 1 && twoval == 0) {
                cerr << "safe 5" << endl;
                return min(danval, 5);
            }
            cerr << "safe 5" << endl;
            return min(danval, 5);
        } else {
            if (totalleft == 3 && player[myid].left == 3) {
                cerr << "safe 3" << endl;
                return 3;
            }
            if (totalleft >= player[myid].left) {
                cerr << "safe 1" << endl;
                return 1;
            }
            if (totalleft == player[myid].left - 1) {
                cerr << "safe 4" << endl;
                return min(danval, 4);
            }
            if (twoval > 1) {
                cerr << "safe 4" << endl;
                return min(danval, 4);
            }
            cerr << "safe 5" << endl;
            return min(danval, 5);
        }
    }
}
pair<int, Pos> findsafistmove(Pos pos, AI_Client &client) { //找到最安全且价值最高的移动方式，如果当前位置最安全则不动
    auto allneighbor = client.get_neighbors(pos, false);
    auto eleneighbor = client.get_neighbors(pos, true); //考虑电梯

    int maxsaveval = possave(pos, client, 1);
    int minescapeval = centerdis(pos, client);
    int mininvval = getnearestinv(pos, client).first;
    int minkeyval = getnearestkey(pos, client).first;
    int maxdegree = getdegree(pos, client);
    Pos maxvalpos = pos;
    for (auto c : allneighbor) {                   //所有邻点
        int val1 = possave(c, client, 0);          //安全度
        int val2 = centerdis(c, client);           //离中心的距离
        int val3 = getnearestinv(c, client).first; //最近的物资点距离
        int val4 = getnearestkey(c, client).first; //最近的钥匙距离
        int val5 = getdegree(c, client);           //度数
        if (val1 > maxsaveval) {
            maxsaveval = val1;
            minescapeval = val2;
            mininvval = val3;
            minkeyval = val4;
            maxdegree = val5;
            maxvalpos = c;
        } else if (val1 < maxsaveval) {
            continue;
        } else if (val2 < minescapeval) {
            maxsaveval = val1;
            minescapeval = val2;
            mininvval = val3;
            minkeyval = val4;
            maxdegree = val5;
            maxvalpos = c;
        } else if (val2 > minescapeval) {
            continue;
        } else if (val3 < mininvval) {
            maxsaveval = val1;
            minescapeval = val2;
            mininvval = val3;
            minkeyval = val4;
            maxdegree = val5;
            maxvalpos = c;
        } else if (val3 > mininvval) {
            continue;
        } else if (val4 < minkeyval) {
            maxsaveval = val1;
            minescapeval = val2;
            mininvval = val3;
            minkeyval = val4;
            maxdegree = val5;
            maxvalpos = c;
        } else if (val4 > minkeyval) {
            continue;
        } else if (val5 > maxdegree) {
            maxsaveval = val1;
            minescapeval = val2;
            mininvval = val3;
            minkeyval = val4;
            maxdegree = val5;
            maxvalpos = c;
        }
    }
    if (allneighbor.size() != eleneighbor.size()) {                          //有电梯可乘
        if (get<2>(pos) == 0 && maxsaveval <= 2 && myitem.blinks_num == 0) { //在0层且很危险且没闪现
            if (get<0>(pos) == 3)                                            //只能跑2层
                return make_pair(3, Pos(get<0>(pos), get<1>(pos), 2));
            else if (possave(Pos(get<0>(pos), get<1>(pos), 2), client, 0) >=
                     possave(Pos(get<0>(pos), get<1>(pos), 2), client, 0)) // 2层更安全
                return make_pair(3, Pos(get<0>(pos), get<1>(pos), 2));
            else
                return make_pair(3, Pos(get<0>(pos), get<1>(pos), 1));
        } else if (get<2>(pos) == 1) { //在1层
            if (player[myid].keys.size() == 4 &&
                possave(Pos(get<0>(pos), get<1>(pos), 0), client, 0) >= 3) //已经拿满钥匙
                return make_pair(3, Pos(get<0>(pos), get<1>(pos), 0));
            else if (maxsaveval <= 3 && possave(Pos(get<0>(pos), get<1>(pos), 0), client, 2) >= 3) //不太安全
                return make_pair(3, Pos(get<0>(pos), get<1>(pos), 2));
        } else if (get<2>(pos) == 2) { //在2层
            if (player[myid].keys.size() == 4 &&
                possave(Pos(get<0>(pos), get<1>(pos), 0), client, 0) >= 3) //已经拿满钥匙
                return make_pair(3, Pos(get<0>(pos), get<1>(pos), 0));
            else if (maxsaveval <= 3 && get<0>(pos) != 3 && possave(Pos(get<0>(pos), get<1>(pos), 0), client, 1) >= 3 &&
                     get<0>(pos) != 3) //不太安全
                return make_pair(3, Pos(get<0>(pos), get<1>(pos), 1));
        }
    }
    return make_pair(maxsaveval, maxvalpos);
}
int findinv(Pos pos) { //返回pos位置的物资点编号
    for (int i = 0; i < inv.size(); i++)
        if (inv[i].pos == pos)
            return i;
    return -1;
}
int findbox(Pos pos) { //返回pos位置的掉落物编号
    for (int i = 0; i < box.size(); i++)
        if (box[i].pos == pos && box[i].val > 0)
            return i;
    return -1;
}

//主要策略行动
bool move(Pos pos, int status, AI_Client &client) { //移动，status：0跑酷，1逃跑，2闪移
    if (status == 0) {                              //跑酷
        if (mainstrategy == 0) {                    //正常情况
            if (((box.size() != 0 && keys.size() < 4) || myitem.blinks_num < 1 ||
                 (player[myid].hp <= 140 && myitem.kits_num == 0)) &&
                getnearestinv(pos, client).second != fakepos) { //物资不足且有物资可拾取
                if (get<2>(mypos) == 0 && keys.size() == 4) {   //跳过
                } else {
                    cerr << "move_inv" << endl;
                    Pos invpos = getnearestinv(pos, client).second;
                    printpos(pos);
                    printpos(invpos);
                    if (pos != invpos && invpos != fakepos) {
                        auto path = client.find_shortest_path(pos, invpos);
                        Pos next = path[1];
                        if (possave(next, client, 0) >= 3) { //安全
                            dst = next;
                            return 1;
                        } else { //不安全
                            Pos savemove = findsafistmove(pos, client).second;
                            if (savemove != pos) { //合适的移动
                                dst = savemove;
                                return 1;
                            }
                        }
                    }
                }
            }
            if (player[myid].keys.size() == 4) { //已经拿满钥匙
                cerr << "move_finish" << endl;
                Pos escapepos = client.get_escape_pos();
                if (pos != escapepos) {
                    auto path = client.find_shortest_path(pos, escapepos);
                    Pos next = path[1];
                    if (possave(next, client, 0) >= 3) { //安全
                        dst = next;
                        return 1;
                    } else {                                //不安全
                        if (possave(pos, client, 1) >= 3) { //原地不动最好
                            return 0;
                        }
                        Pos savemove = findsafistmove(pos, client).second;
                        if (savemove != pos) { //合适的移动
                            dst = savemove;
                            return 1;
                        }
                    }
                }
            }
            if (player[myid].keys.size() < 4 && isrestricted(client) == 0) { //如果没被追人局限住则追人
                cerr << "move_enemy" << endl;
                Pos enemypos = getnearestvalenemy(pos, client).second;
                if (enemypos != fakepos) {
                    if (pos != enemypos) {
                        auto path = client.find_shortest_path(pos, enemypos);
                        Pos next = path[1];
                        if (possave(next, client, 0) >= 3) { //安全
                            dst = next;
                            return 1;
                        } else { //不安全
                            Pos savemove = findsafistmove(pos, client).second;
                            if (savemove != pos) { //合适的移动
                                dst = savemove;
                                return 1;
                            }
                        }
                    }
                }
            }
            if (player[myid].keys.size() < 4) {
                cerr << "move_normal" << endl;
                Pos keypos = getnearestkey(pos, client).second;
                if (pos != keypos) {
                    auto path = client.find_shortest_path(pos, keypos);
                    Pos next = path[1];
                    if (possave(next, client, 0) >= 3) { //安全
                        dst = next;
                        return 1;
                    } else {                                //不安全
                        if (possave(pos, client, 1) >= 3) { //原地不动最好
                            return 0;
                        }
                        Pos savemove = findsafistmove(pos, client).second;
                        if (savemove != pos) { //合适的移动
                            dst = savemove;
                            return 1;
                        }
                    }
                }
            }
        } else {                           //拦截
            bool shouldgo = watch(client); //是否应该上三楼
            if (shouldgo) {                //准备上楼杀人
                if (myitem.blinks_num == 0 ||
                    (myitem.kits_num == 0 && player[myid].hp <= 190)) { //缓一步，拿物资提升实力
                    cerr << "move_gokill_prepareinv" << endl;
                    Pos invpos = getnearestinv(pos, client).second;
                    printpos(pos);
                    printpos(invpos);
                    if (pos != invpos && invpos != fakepos) {
                        auto path = client.find_shortest_path(pos, invpos);
                        Pos next = path[1];
                        if (possave(next, client, 0) >= 3) { //安全
                            dst = next;
                            return 1;
                        } else { //不安全
                            Pos savemove = findsafistmove(pos, client).second;
                            if (savemove != pos) { //合适的移动
                                dst = savemove;
                                return 1;
                            }
                        }
                    }
                } else { //直接上
                    cerr << "move_gokill" << endl;
                    Pos escapepos = client.get_escape_pos();
                    if (pos != escapepos) {
                        auto path = client.find_shortest_path(pos, escapepos);
                        Pos next = path[1];
                        if (possave(next, client, 0) >= 3) { //安全
                            dst = next;
                            return 1;
                        } else {                                //不安全
                            if (possave(pos, client, 1) >= 3) { //原地不动最好
                                return 0;
                            }
                            Pos savemove = findsafistmove(pos, client).second;
                            if (savemove != pos) { //合适的移动
                                dst = savemove;
                                return 1;
                            }
                        }
                    }
                }

            } else {
                if (((box.size() != 0 && keys.size() < 4) || myitem.blinks_num < 2 || myitem.kits_num < 2) &&
                    getnearestinv(pos, client).second != fakepos) { //多拿物资
                    cerr << "move_prepareinv" << endl;
                    Pos invpos = getnearestinv(pos, client).second;
                    printpos(pos);
                    printpos(invpos);
                    if (pos != invpos && invpos != fakepos) {
                        auto path = client.find_shortest_path(pos, invpos);
                        Pos next = path[1];
                        if (possave(next, client, 0) >= 3) { //安全
                            dst = next;
                            return 1;
                        } else { //不安全
                            Pos savemove = findsafistmove(pos, client).second;
                            if (savemove != pos) { //合适的移动
                                dst = savemove;
                                return 1;
                            }
                        }
                    }
                }
                if (player[myid].keys.size() < 4 && isrestricted(client) == 0) { //如果没被追人局限住则追人
                    cerr << "move_prepareenemy" << endl;
                    Pos enemypos = getnearestvalenemy(pos, client).second;
                    if (enemypos != fakepos) {
                        if (pos != enemypos) {
                            auto path = client.find_shortest_path(pos, enemypos);
                            Pos next = path[1];
                            if (possave(next, client, 0) >= 3) { //安全
                                dst = next;
                                return 1;
                            } else { //不安全
                                Pos savemove = findsafistmove(pos, client).second;
                                if (savemove != pos) { //合适的移动
                                    dst = savemove;
                                    return 1;
                                }
                            }
                        }
                    }
                }
                if (player[myid].keys.size() < 4) {
                    cerr << "move_preparenormal" << endl;
                    Pos keypos = getnearestkey(pos, client).second;
                    if (pos != keypos) {
                        auto path = client.find_shortest_path(pos, keypos);
                        Pos next = path[1];
                        if (possave(next, client, 0) >= 3) { //安全
                            dst = next;
                            return 1;
                        } else {                                //不安全
                            if (possave(pos, client, 1) >= 3) { //原地不动最好
                                return 0;
                            }
                            Pos savemove = findsafistmove(pos, client).second;
                            if (savemove != pos) { //合适的移动
                                dst = savemove;
                                return 1;
                            }
                        }
                    }
                }
            }
        }

        return 0;
    } else if (status == 1) { //逃跑
        int saveval = findsafistmove(pos, client).first;
        Pos savemove = findsafistmove(pos, client).second;
        cerr << "escape_move:" << saveval << endl;
        if (saveval <= 2 && myitem.blinks_num > 0) { //考虑闪出
            auto nei = client.get_neighbors(pos);
            int nowval = saveval;
            int toescapedis = dis(savemove, client.get_escape_pos(), client);
            Pos nowmove = savemove;
            for (auto c : nei) {
                printpos(c);
                auto tmp = client.get_neighbors(c, true);
                for (auto a : tmp) {
                    if (possave(a, client, 0) > nowval) { //闪出后更安全
                        nowval = possave(a, client, 0);
                        toescapedis = dis(a, client.get_escape_pos(), client);
                        nowmove = a;
                    } else if (possave(a, client, 0) == nowval &&
                               dis(a, client.get_escape_pos(), client) < toescapedis &&
                               keys.size() == 4) { //拿满钥匙，且闪出后离出口更近
                        nowval = possave(a, client, 0);
                        toescapedis = dis(a, client.get_escape_pos(), client);
                        nowmove = a;
                    } else if (possave(a, client, 0) == nowval && get<2>(a) != get<2>(pos) && get<2>(a) != 0 &&
                               keys.size() < 4) { //未拿满钥匙，且闪出后换到非0层
                        nowval = possave(a, client, 0);
                        toescapedis = dis(a, client.get_escape_pos(), client);
                        nowmove = a;
                    }
                }
            }
            if (nowmove != savemove && nowmove != pos) {
                auto path = client.find_shortest_path(pos, nowmove);
                detect(path[1], client);
                client.blink(path[1]);
                updatestatus(client);
                placetrap(client);
                strategy = 7;
                return 1;
            }
        }
        if (savemove != pos) { //合适的移动
            placetrap(client);
            dst = savemove;
            return 1;
        }
        return 0;
    } else if (status == 2) { //闪移
    }
}
bool attack(Pos pos, Pos enemypos, int enemyid, AI_Client &client) { //尝试攻击，如果成功则返回1
    detectval = 3;
    if (dis(pos, enemypos, client) <= 1) { //可以直接攻击
        movecount = 0;
        bool suc = client.attack(enemypos, enemyid);
        if (suc && player[enemyid].left == 1) { //打死了
            box.push_back(Box(enemypos, getboxval(enemyid)));
        }
        return suc;
    } else { //隔一格
        if (myitem.blinks_num > 0) {
            auto path = client.find_shortest_path(pos, enemypos);
            Pos next = path[1];
            detect(path[1], client);
            client.blink(next);
            movecount = 0;
            bool suc = client.attack(enemypos, enemyid);
            if (suc && player[enemyid].left == 1) { //打死了
                box.push_back(Box(enemypos, getboxval(enemyid)));
            }
            return suc;
        } else {
            auto path = client.find_shortest_path(pos, enemypos);
            Pos next = path[1];
            movetopos(next, client);
            return 1;
        }
    }
}
bool pick(Pos pos, AI_Client &client) { //拾取
    auto canpick = client.judge_if_can_pick(client.pos2node(pos));
    if (canpick.first == 0 || possave(pos, client, 1) < 3)
        return 0;
    if (canpick.second.size() == 2) { //两种东西都有
        if (findbox(pos) != -1)
            return 1;
        if (mainstrategy == 0) { //正常
            if ((player[myid].hp <= 140 && myitem.kits_num == 0) ||
                myitem.blinks_num < 2) { //若应该耗回合来拿物资，且是物资点，进行判断
                bool can = invjudge(pos);
                if (can) {
                    return 1;
                }
            }
        } else {                                              //拦截
            if (myitem.kits_num < 1 || myitem.blinks_num < 1) //应该拿
                return invjudge(pos);
            if (watch(client) == 0 && (myitem.kits_num < 2 || myitem.blinks_num < 2)) //有余裕拿
                return invjudge(pos);
        }
    } else if (canpick.second.size() == 1) {                    //只有一种东西
        if (canpick.second[0] == "Box" && findbox(pos) != -1) { //若是掉落物则拾取
            strategy = 2;
            return 1;
        } else if (mainstrategy == 0) { //正常
            if ((player[myid].hp <= 140 && myitem.kits_num == 0) ||
                myitem.blinks_num < 2) { //若应该耗回合来拿物资，且是物资点，进行判断
                bool can = invjudge(pos);
                if (can) {
                    return 1;
                }
            }
        } else {                                              //拦截
            if (myitem.kits_num < 1 || myitem.blinks_num < 1) //应该拿
                return invjudge(pos);
            if (watch(client) == 0 && (myitem.kits_num < 2 || myitem.blinks_num < 2)) //有余裕拿
                return invjudge(pos);
        }
    }
    return 0;
}
bool keypick(Pos pos) {            //钥匙拾取
    for (int i = 0; i <= 3; i++) { //能拾取未获得的钥匙
        if (find(keys.begin(), keys.end(), i) == keys.end() &&
            (pos == getkeypos(i).first || pos == getkeypos(i).second))
            return 1;
    }
    return 0;
}
bool heal(Pos pos, AI_Client &client) { //治疗
    if (player[myid].hp <= 120 && myitem.kits_num) {
        return 1;
    }
    if (player[myid].hp <= 140 && myitem.kits_num && nearenemies(pos, client).first == 0)
        return 1;
    return 0;
}
bool escape(Pos pos, AI_Client &client) { //逃离
    if (pos == client.get_escape_pos() && client.get_keys().size() == 4) {
        if (possave(pos, client, 0) >= 4)
            return 1;
    }
    return 0;
}

void blinkto(Pos from, Pos to, AI_Client &client) { //向某个位置闪
    auto path = client.find_shortest_path(from, to);
    detect(path[1], client);
    client.blink(path[1]);
    updatestatus(client);
}
void moveto(Pos from, Pos to, AI_Client &client) { //向某个位置移动
    auto path = client.find_shortest_path(from, to);
    if (restrictdanval(path[1]) == 1) {
        return;
    }
    detect(path[1], client);
    movecount++;
    if (get<2>(path[1]) != get<2>(from))
        buf.push_back(Buf(mypos, possave(mypos, client, 1)));
    client.move(path[1]);
}
bool decideonthird(AI_Client &client) {           //进行在三层的特殊处理
    if (player[myid].status == WAIT_FOR_ESCAPE) { // TODO正在逃生
        strategy = 8;
        return 1;
    }
    if (mainstrategy == 0) {     //正常操作
        vector<int> towinenemy;  //已经拿满钥匙的敌人
        vector<int> dangerenemy; //未拿满钥匙的敌人
        for (auto c : others) {
            if (player[c].keys.size() == 4) {
                towinenemy.push_back(c);
            } else {
                dangerenemy.push_back(c);
            }
        }
        if (towinenemy.size() == 0 && dangerenemy.size() == 0) { //没有敌人
            cerr << "no enemy" << endl;
            if (keys.size() == 4) {       //已经拿满钥匙
                if (mypos == escapepos) { //直接跑
                    strategy = 4;
                    return 1;
                } else {
                    if (dis(mypos, escapepos, client) <= myitem.blinks_num) { //能直接闪过去
                        blinkto(mypos, escapepos, client);
                        strategy = 7;
                        return 1;
                    } else { //没法直接闪过去
                        moveto(mypos, escapepos, client);
                        strategy = 8;
                        return 1;
                    }
                }
            } else { // TODO没拿满钥匙
                return 0;
            }
        } else if (towinenemy.size() == 1 && dangerenemy.size() == 0) { //只有一个将胜利的敌人
            cerr << "one towin enemy" << endl;
            int enemy = towinenemy[0];
            Pos enemypos = player[enemy].pos;
            if (keys.size() == 4) { //已经拿满钥匙
                if (player[enemy].status == WAIT_FOR_ESCAPE && player[enemy].left <= player[enemy].escapeleft &&
                    dis(mypos, enemypos, client) - 1 <=
                        myitem.blinks_num + player[enemy].escapeleft - player[enemy].left) { //有可能打死
                    cerr << "possible kill" << endl;
                    detectval = 3;
                    if (dis(mypos, enemypos, client) <= 1) { //可以直接打
                        movecount = 0;
                        client.attack(enemypos, enemy);
                        strategy = 8;
                        return 1;
                    } else if (dis(mypos, enemypos, client) - 1 <= myitem.blinks_num) { //能闪过去
                        blinkto(mypos, enemypos, client);
                        strategy = 7;
                        return 1;
                    } else {
                        moveto(mypos, enemypos, client);
                        strategy = 8;
                        return 1;
                    }
                } else if (player[enemy].status == WAIT_FOR_ESCAPE) { //不管敌人
                    cerr << "enemy wait for escape" << endl;
                    if (mypos == escapepos) { //直接跑
                        strategy = 4;
                        return 1;
                    } else if (dis(mypos, escapepos, client) <= myitem.blinks_num) { //能闪过去
                        blinkto(mypos, escapepos, client);
                        strategy = 7;
                        return 1;
                    } else {
                        moveto(mypos, escapepos, client);
                        strategy = 8;
                        return 1;
                    }
                } else { //敌人不在逃生状态
                    cerr << "normal" << endl;
                    if (player[myid].left == 3 && dis(escapepos, enemypos, client) >= 3) { //安全
                        if (mypos == escapepos) {                                          //跑
                            strategy = 4;
                            return 1;
                        } else if (dis(mypos, escapepos, client) <= myitem.blinks_num) { //能闪过去
                            blinkto(mypos, escapepos, client);
                            strategy = 7;
                            return 1;
                        } else { //移过去
                            moveto(mypos, escapepos, client);
                            strategy = 8;
                            return 1;
                        }
                    } else if (player[myid].left >= player[enemy].left &&
                               (dis(mypos, enemypos, client) - 1 <= myitem.blinks_num ||
                                player[myid].left >= 2)) { //开打
                        detectval = 3;
                        if (dis(mypos, enemypos, client) <= 1) { //可以直接打
                            movecount = 0;
                            client.attack(enemypos, enemy);
                            strategy = 8;
                            return 1;
                        } else if (dis(mypos, enemypos, client) - 1 <= myitem.blinks_num &&
                                   (dis(mypos, enemypos, client) <= 2 || player[enemy].left == 1)) { //能闪过去
                            blinkto(mypos, enemypos, client);
                            strategy = 7;
                            return 1;
                        } else if (dis(mypos, enemypos, client) == 2) { // 2格距离，保持不动
                            if (pick(mypos, client))
                                strategy = 2;
                            else
                                strategy = 5;
                            return 1;
                        } else { //移过去
                            moveto(mypos, enemypos, client);
                            strategy = 8;
                            return 1;
                        }
                    } else { //打不过，跑
                        if (heal(mypos, client)) {
                            strategy = 3;
                            return 1;
                        } else if (dis(mypos, enemypos, client) > 3) {
                            moveto(mypos, enemypos, client);
                            strategy = 8;
                            return 1;
                        } else if (dis(mypos, enemypos, client) == 3) {
                            if (pick(mypos, client))
                                strategy = 2;
                            else
                                strategy = 8;
                            return 1;
                        } else if (move(mypos, 1, client)) {
                            if (strategy == -1)
                                strategy = 0;
                            return 1;
                        } else {
                            return 0;
                        }
                    }
                }
            } else { //未拿满钥匙
                if (player[enemy].status == WAIT_FOR_ESCAPE && player[enemy].left <= player[enemy].escapeleft &&
                    dis(mypos, enemypos, client) - 1 <=
                        myitem.blinks_num + player[enemy].escapeleft - player[enemy].left) { //有可能打死
                    cerr << "possible kill" << endl;
                    detectval = 3;
                    if (dis(mypos, enemypos, client) <= 1) { //可以直接打
                        movecount = 0;
                        client.attack(enemypos, enemy);
                        strategy = 8;
                        return 1;
                    } else if (dis(mypos, enemypos, client) - 1 <= myitem.blinks_num) { //能闪过去
                        blinkto(mypos, enemypos, client);
                        strategy = 7;
                        return 1;
                    } else {
                        moveto(mypos, enemypos, client);
                        strategy = 8;
                        return 1;
                    }
                } else if (player[enemy].status == WAIT_FOR_ESCAPE) { //自己干自己的
                    cerr << "enemy wait for escape" << endl;
                    if (heal(mypos, client)) {
                        strategy = 3;
                        return 1;
                    } else if (pick(mypos, client)) {
                        strategy = 2;
                        return 1;
                    } else if (move(mypos, 0, client)) {
                        if (strategy == -1)
                            strategy = 0;
                        return 1;
                    } else {
                        return 0;
                    }
                } else { //敌人不在逃生状态
                    cerr << "normal" << endl;
                    if (player[myid].left >= player[enemy].left &&
                        (dis(mypos, enemypos, client) - 1 <= myitem.blinks_num || player[myid].left >= 2)) { //开打
                        detectval = 3;
                        if (dis(mypos, enemypos, client) <= 1) { //可以直接打
                            movecount = 0;
                            client.attack(enemypos, enemy);
                            strategy = 8;
                            return 1;
                        } else if (dis(mypos, enemypos, client) - 1 <= myitem.blinks_num &&
                                   (dis(mypos, enemypos, client) <= 2 || player[enemy].left == 1)) { //能闪过去
                            blinkto(mypos, enemypos, client);
                            strategy = 7;
                            return 1;
                        } else if (dis(mypos, enemypos, client) == 2) { // 2格距离，保持不动
                            if (pick(mypos, client))
                                strategy = 2;
                            else
                                strategy = 5;
                            return 1;
                        } else { //移过去
                            moveto(mypos, enemypos, client);
                            strategy = 8;
                            return 1;
                        }
                    } else { //打不过，跑
                        if (heal(mypos, client)) {
                            strategy = 3;
                            return 1;
                        } else if (dis(mypos, enemypos, client) > 2 && player[myid].left > 1) {
                            moveto(mypos, enemypos, client);
                            strategy = 8;
                            return 1;
                        } else if (dis(mypos, enemypos, client) == 2 && player[myid].left > 1) {
                            if (pick(mypos, client))
                                strategy = 2;
                            else
                                strategy = 8;
                            return 1;
                        } else if (move(mypos, 1, client)) {
                            if (strategy == -1)
                                strategy = 0;
                            return 1;
                        } else {
                            return 0;
                        }
                    }
                }
            }
        } else if (towinenemy.size() == 0 && dangerenemy.size() == 1) { //只有一个未胜利的敌人
            cerr << "one danger enemy" << endl;
            int enemy = dangerenemy[0];
            Pos enemypos = player[enemy].pos;
            bool value = valuable(enemy, client);
            if (keys.size() == 4) {                                                    //已经拿满钥匙
                if (player[myid].left == 3 && dis(escapepos, enemypos, client) >= 3) { //安全
                    if (mypos == escapepos) {                                          //跑
                        strategy = 4;
                        return 1;
                    } else if (dis(mypos, escapepos, client) <= myitem.blinks_num) { //能闪过去
                        blinkto(mypos, escapepos, client);
                        strategy = 7;
                        return 1;
                    } else { //移过去
                        moveto(mypos, escapepos, client);
                        strategy = 8;
                        return 1;
                    }
                } else { //跑
                    if (heal(mypos, client)) {
                        strategy = 3;
                        return 1;
                    } else if (move(mypos, 1, client)) {
                        if (strategy == -1)
                            strategy = 0;
                        return 1;
                    } else {
                        return 0;
                    }
                }
            } else { //未拿满钥匙
                if (player[myid].left >= player[enemy].left && value &&
                    (dis(mypos, enemypos, client) - 1 <= myitem.blinks_num || player[myid].left >= 2)) { //有价值，开打
                    detectval = 3;
                    if (dis(mypos, enemypos, client) <= 1) { //可以直接打
                        movecount = 0;
                        client.attack(enemypos, enemy);
                        strategy = 8;
                        return 1;
                    } else if (dis(mypos, enemypos, client) - 1 <= myitem.blinks_num &&
                               (dis(mypos, enemypos, client) <= 2 || player[enemy].left == 1)) { //能闪过去
                        blinkto(mypos, enemypos, client);
                        strategy = 7;
                        return 1;
                    } else if (dis(mypos, enemypos, client) == 2) { // 2格距离，保持不动
                        if (pick(mypos, client))
                            strategy = 2;
                        else
                            strategy = 5;
                        return 1;
                    } else { //移过去
                        moveto(mypos, enemypos, client);
                        strategy = 8;
                        return 1;
                    }
                } else { //打不过，跑
                    if (heal(mypos, client)) {
                        strategy = 3;
                        return 1;
                    } else if (move(mypos, 1, client)) {
                        if (strategy == -1)
                            strategy = 0;
                        return 1;
                    } else {
                        return 0;
                    }
                }
            }
        } else { // TODO敌人较多
            return 0;
        }
    } else {                     //拦截
        vector<int> towinenemy;  //已经拿满钥匙的敌人
        vector<int> dangerenemy; //未拿满钥匙的敌人
        for (auto c : others) {
            if (player[c].keys.size() == 4) {
                towinenemy.push_back(c);
            } else {
                dangerenemy.push_back(c);
            }
        }
        if (towinenemy.size() == 0 && dangerenemy.size() == 0) { //没有敌人
            cerr << "no enemy" << endl;
            if (pick(mypos, client)) {
                strategy = 2;
                return 1;
            }
            if (get<2>(getnearestinv(mypos, client).second) == 0) { //有东西可捡
                if (get<2>(mypos) == 0 && keys.size() == 4) {       //跳过
                } else {
                    cerr << "move_inv" << endl;
                    Pos invpos = getnearestinv(mypos, client).second;
                    printpos(mypos);
                    printpos(invpos);
                    if (mypos != invpos && invpos != fakepos) {
                        auto path = client.find_shortest_path(mypos, invpos);
                        Pos next = path[1];
                        if (possave(next, client, 0) >= 3) { //安全
                            dst = next;
                            strategy = 0;
                            return 1;
                        } else { //不安全
                            Pos savemove = findsafistmove(mypos, client).second;
                            if (savemove != mypos) { //合适的移动
                                dst = savemove;
                                strategy = 0;
                                return 1;
                            }
                        }
                    }
                }
            }

            if (dis(mypos, escapepos, client) == 1 && possave(mypos, client, 1) > 1) { //已经等好位置且安全
                placetrap(client);
                strategy = 5;
                return 1;
            } else if (mypos == escapepos) { //在中间
                auto neighbors = client.get_neighbors(mypos);
                bool complete = 0;
                for (auto c : neighbors) {
                    if (rand() % neighbors.size() == 0) {
                        movetopos(c, client);
                        complete = 1;
                        break;
                    }
                }
                if (!complete) {
                    movetopos(neighbors[neighbors.size() - 1], client);
                }
                strategy = 8;
                return 1;
            } else { //往中间走
                moveto(mypos, escapepos, client);
                strategy = 8;
                return 1;
            }
        } else if (towinenemy.size() == 1 && dangerenemy.size() == 0) { //只有一个将胜利的敌人
            cerr << "one towin enemy" << endl;
            int enemy = towinenemy[0];
            Pos enemypos = player[enemy].pos;
            if (player[enemy].status == WAIT_FOR_ESCAPE && player[enemy].left <= player[enemy].escapeleft &&
                dis(mypos, enemypos, client) - 1 <=
                    myitem.blinks_num + player[enemy].escapeleft - player[enemy].left) { //有可能打死
                cerr << "possible kill" << endl;
                detectval = 3;
                if (dis(mypos, enemypos, client) <= 1) { //可以直接打
                    movecount = 0;
                    client.attack(enemypos, enemy);
                    strategy = 8;
                    return 1;
                } else if (dis(mypos, enemypos, client) - 1 <= myitem.blinks_num &&
                           (dis(mypos, enemypos, client) <= 2 || player[enemy].left == 1)) { //能闪过去
                    blinkto(mypos, enemypos, client);
                    strategy = 7;
                    return 1;
                } else {
                    moveto(mypos, enemypos, client);
                    strategy = 8;
                    return 1;
                }
            } else if (player[enemy].status == WAIT_FOR_ESCAPE) { //自己干自己的
                cerr << "enemy wait for escape" << endl;
                if (heal(mypos, client)) {
                    strategy = 3;
                    return 1;
                } else if (pick(mypos, client)) {
                    strategy = 2;
                    return 1;
                } else if (move(mypos, 0, client)) {
                    if (strategy == -1)
                        strategy = 0;
                    return 1;
                } else {
                    return 0;
                }
            } else { //敌人不在逃生状态
                cerr << "normal" << endl;
                if (player[myid].left >= player[enemy].left &&
                    (dis(mypos, enemypos, client) - 1 <= myitem.blinks_num || player[myid].left >= 2)) { //开打
                    detectval = 3;
                    if (dis(mypos, enemypos, client) <= 1) { //可以直接打
                        movecount = 0;
                        client.attack(enemypos, enemy);
                        strategy = 8;
                        return 1;
                    } else if (dis(mypos, enemypos, client) - 1 <= myitem.blinks_num &&
                               (dis(mypos, enemypos, client) <= 2 || player[enemy].left == 1)) { //能闪过去
                        blinkto(mypos, enemypos, client);
                        strategy = 7;
                        return 1;
                    } else if (dis(mypos, enemypos, client) == 2) { // 2格距离，保持不动
                        if (pick(mypos, client))
                            strategy = 2;
                        else
                            strategy = 5;
                        return 1;
                    } else { //移过去
                        moveto(mypos, enemypos, client);
                        strategy = 8;
                        return 1;
                    }
                } else { //打不过，跑
                    if (heal(mypos, client)) {
                        strategy = 3;
                        return 1;
                    } else if (dis(mypos, enemypos, client) > 2 && player[myid].left > 1) {
                        moveto(mypos, enemypos, client);
                        strategy = 8;
                        return 1;
                    } else if (dis(mypos, enemypos, client) == 2 && player[myid].left > 1) {
                        if (pick(mypos, client))
                            strategy = 2;
                        else
                            strategy = 8;
                        return 1;
                    } else if (move(mypos, 1, client)) {
                        if (strategy == -1)
                            strategy = 0;
                        return 1;
                    } else {
                        return 0;
                    }
                }
            }
        } else if (towinenemy.size() == 0 && dangerenemy.size() == 1) { //只有一个未胜利的敌人
            cerr << "one danger enemy" << endl;
            int enemy = dangerenemy[0];
            Pos enemypos = player[enemy].pos;
            bool value = valuable(enemy, client);
            if (player[myid].left >= player[enemy].left && value &&
                (dis(mypos, enemypos, client) - 1 <= myitem.blinks_num || player[myid].left >= 2)) { //有价值，开打
                detectval = 3;
                if (dis(mypos, enemypos, client) <= 1) { //可以直接打
                    movecount = 0;
                    client.attack(enemypos, enemy);
                    strategy = 8;
                    return 1;
                } else if (dis(mypos, enemypos, client) - 1 <= myitem.blinks_num &&
                           (dis(mypos, enemypos, client) <= 2 || player[enemy].left == 1)) { //能闪过去
                    blinkto(mypos, enemypos, client);
                    strategy = 7;
                    return 1;
                } else if (dis(mypos, enemypos, client) == 2) { // 2格距离，保持不动
                    if (pick(mypos, client))
                        strategy = 2;
                    else
                        strategy = 5;
                    return 1;
                } else { //移过去
                    moveto(mypos, enemypos, client);
                    strategy = 8;
                    return 1;
                }
            } else { //打不过，跑
                if (heal(mypos, client)) {
                    strategy = 3;
                    return 1;
                } else if (dis(mypos, enemypos, client) > 3) {
                    moveto(mypos, enemypos, client);
                    strategy = 8;
                    return 1;
                } else if (dis(mypos, enemypos, client) == 3) {
                    if (pick(mypos, client))
                        strategy = 2;
                    else
                        strategy = 8;
                    return 1;
                } else if (move(mypos, 1, client)) {
                    if (strategy == -1)
                        strategy = 0;
                    return 1;
                } else {
                    return 0;
                }
            }
        } else { // TODO敌人较多
            return 0;
        }
    }
}

void decideMainStrategy(AI_Client &client) { //决定当前主策略
    if (keys.size() == 4 || player[myid].left == 1) {
        mainstrategy = 0;
        return;
    }
    int target = -1;
    for (int i = 0; i <= 3; i++) {
        if (i != myid) {
            if (player[i].keys.size() > keys.size() && player[i].keys.size() >= 3 &&
                getnearestkey(mypos, client).first >= 2 && player[i].status != ESCAPED &&
                player[i].status != AI_ERROR) { //发现有人比自己先拿了钥匙且拿了很多钥匙
                mainstrategy = 1;
                target = i;
            }
        }
    }
    if (target != -1) {
        if (player[target].status == WAIT_FOR_ESCAPE &&
            (get<2>(mypos) != 0 || dis(player[target].pos, mypos, client) - 1 >
                                       player[target].escapeleft + myitem.blinks_num - player[target].left))
            mainstrategy = 0;
    }
    bool ret0 = 0;
    for (int i = 0; i <= 3; i++) {
        if (i != myid) {
            if (player[i].keys.size() >= keys.size() && player[i].keys.size() >= 3 && player[i].status != ESCAPED &&
                player[i].status != AI_ERROR) { //有人仍不比自己钥匙少
                ret0 = 1;
            }
        }
    }
    if (ret0 == 0)
        mainstrategy = 0;
}
void decideStrategy(
    AI_Client &client) //选择策略  0移动，1攻击，2拾取，3治疗，4逃离,5不动,6捡钥匙,7继续行动,8已经行动完,9跳过
{
    updatestatus(client);
    decideMainStrategy(client);
    cerr << "mainstrategy:" << mainstrategy << endl;
    if (player[myid].status == DEAD || player[myid].status == SKIP) { //如果没法行动则跳过
        strategy = 9;
        return;
    }
    int i, j;
    strategy = -1;
    if (mainstrategy == 0 || mainstrategy == 1) { //正常操作
        if (get<2>(mypos) == 0) {                 //在三层，进行三层的特殊处理
            if (decideonthird(client) == 1)
                return;
        }
        pair<int, vector<int>> nearenemy = nearenemies(mypos, client);
        pair<int, vector<int>> twoblockenemy = twoblockenemies(mypos, client); //两格远敌人探测
        if (nearenemy.first == 0 && twoblockenemy.first == 0) {                //当前位置一格两格均安全
            cerr << "s1" << endl;
            if (action == 0) {               //尚未主要行动
                if (escape(mypos, client)) { //可逃离
                    strategy = 4;
                } else if (heal(mypos, client)) { //应当治疗
                    strategy = 3;
                } else if (pick(mypos, client)) { //有东西可捡
                    strategy = 2;
                } else if (keypick(mypos)) { //捡钥匙
                    strategy = 6;
                } else if (move(mypos, 0, client)) { //移动
                    strategy = 0;
                } else if (staycount >= 2 && move(mypos, 1, client)) { //避免多回合不动
                    if (strategy == -1)
                        strategy = 0;
                } else { //不动
                    strategy = 5;
                }
            } else { //已经主要行动
            }
        } else if (nearenemy.first == 0 && twoblockenemy.first != 0) { //当前位置一格安全，两格不安全
            cerr << "s2" << endl;
            int saveval = possave(mypos, client, 1);
            if (action == 0) { //尚未主要行动
                if (escape(mypos, client)) {
                    strategy = 4;
                    return;
                }
                if (pick(mypos, client)) { //有东西可捡
                    strategy = 2;
                    return;
                }
                if (heal(mypos, client)) { //应当治疗
                    strategy = 3;
                    return;
                }
                if (saveval >= 4 && saveval != 6 && isrestricted(client) == 0 &&
                    !(watch(client) == 1 && mainstrategy == 1 && get<2>(mypos) != 0)) { //高安全度且可攻击，说明应攻击
                    auto enemies = twoblockenemies(mypos, client).second;
                    if (valuable(enemies[0], client) >= 2) {
                        bool successattack = attack(mypos, player[enemies[0]].pos, player[enemies[0]].id, client);
                        if (successattack) {
                            strategy = 8;
                            return;
                        } else {
                            cerr << "attack fail" << endl;
                            strategy = 5;
                            return;
                        }
                    }
                }
                if (keypick(mypos)) { //捡钥匙
                    strategy = 6;
                } else if (move(mypos, 0, client)) { //移动
                    if (strategy == -1)
                        strategy = 0;
                } else if (staycount >= 1 && move(mypos, 1, client)) { //避免多回合不动
                    if (strategy == -1)
                        strategy = 0;
                } else { //不动
                    strategy = 5;
                }
            } else { //已经主要行动
            }
        } else { //当前位置不安全
            cerr << "s3" << endl;
            int saveval = possave(mypos, client, 1);
            if (action == 0) { //尚未主要行动
                if (escape(mypos, client)) {
                    strategy = 4;
                    return;
                }
                if (saveval >= 4 && saveval != 6 &&
                    !(watch(client) == 1 && mainstrategy == 1 && get<2>(mypos) != 0)) { //高安全度且可攻击，说明应攻击
                    auto enemies = nearenemies(mypos, client).second;
                    if (valuable(enemies[0], client) >= 1) {
                        bool successattack = attack(mypos, player[enemies[0]].pos, player[enemies[0]].id, client);
                        if (successattack) {
                            strategy = 8;
                            return;
                        } else {
                            cerr << "attack fail" << endl;
                            strategy = 5;
                            return;
                        }
                    }
                }
                if (heal(mypos, client)) { //应当治疗
                    strategy = 3;
                } else if (move(mypos, 1, client)) { //移动
                    if (strategy == -1)
                        strategy = 0;
                } else { //不动
                    auto enemies = nearenemies(mypos, client).second;
                    bool successattack = attack(mypos, player[enemies[0]].pos, player[enemies[0]].id, client);
                    if (successattack) {
                        strategy = 8;
                        return;
                    } else {
                        cerr << "attack fail" << endl;
                        strategy = 5;
                        return;
                    }
                }
            } else { //已经主要行动
            }
        }
    } else if (mainstrategy == 1) {
    }
}

Edge *findEdge(Node &s, Pos t) //返回对应的边
{
    for (Edge *edge : s.edges) {
        Pos other = (edge->stpos == mypos) ? edge->edpos : edge->stpos;
        if (other == t)
            return edge;
    }
}

void play(AI_Client &client) {
    if (totalround == 0) { //一开始执行
        escapepos = client.get_escape_pos();
        box.clear();
        lastpos.clear();
        srand(time(0));
        if (rand() % 1 == 0) {
            mainstrategy = 0;
        } else {
            mainstrategy = 1;
        }

        //输出文件
        string name = "0.txt";
        name[0] += client.get_id();
        fout.open(name, ios::out);

        //初始化物资点、电梯、缩圈点
        inv = {Inv(3, 2, 1), Inv(3, 4, 1), Inv(0, 3, 1), Inv(6, 3, 1), Inv(0, 3, 2), Inv(6, 3, 2),
               Inv(2, 2, 2), Inv(3, 2, 2), Inv(4, 2, 2), Inv(2, 4, 2), Inv(3, 4, 2), Inv(4, 4, 2)};
        ele = {Ele(3, 0, 0), Ele(3, 6, 0), Ele(1, 3, 0), Ele(5, 3, 0), Ele(1, 3, 1),
               Ele(5, 3, 1), Ele(3, 0, 2), Ele(3, 6, 2), Ele(1, 3, 2), Ele(5, 3, 2)};
        dan = {Dan(2, 2, 1, 40), Dan(3, 2, 1, 40), Dan(4, 2, 1, 40), Dan(2, 3, 1, 40), Dan(4, 3, 1, 40),
               Dan(2, 4, 1, 40), Dan(3, 4, 1, 40), Dan(4, 4, 1, 40), Dan(1, 2, 2, 50), Dan(2, 1, 2, 50),
               Dan(4, 1, 2, 50), Dan(5, 2, 2, 50), Dan(1, 4, 2, 50), Dan(2, 5, 2, 50), Dan(5, 4, 2, 50),
               Dan(4, 5, 2, 50), Dan(2, 2, 0, 60), Dan(2, 4, 0, 60), Dan(4, 2, 0, 60), Dan(4, 4, 0, 60),
               Dan(0, 2, 1, 70), Dan(0, 3, 1, 70), Dan(0, 4, 1, 70), Dan(6, 2, 1, 70), Dan(6, 3, 1, 70),
               Dan(6, 4, 1, 70), Dan(2, 3, 2, 80), Dan(3, 2, 2, 80), Dan(4, 3, 2, 80), Dan(3, 4, 2, 80)};
        all0f = {All0f(3, 0, 0), All0f(3, 1, 0), All0f(2, 1, 0), All0f(4, 1, 0), All0f(1, 2, 0), All0f(3, 2, 0),
                 All0f(5, 2, 0), All0f(2, 2, 0), All0f(4, 2, 0), All0f(1, 3, 0), All0f(3, 3, 0), All0f(5, 3, 0),
                 All0f(1, 4, 0), All0f(2, 4, 0), All0f(3, 4, 0), All0f(4, 4, 0), All0f(5, 4, 0), All0f(2, 5, 0),
                 All0f(3, 5, 0), All0f(4, 5, 0), All0f(3, 6, 0)};
        all1f = {All1f(0, 0, 1), All1f(0, 1, 1), All1f(0, 2, 1), All1f(0, 3, 1), All1f(0, 4, 1), All1f(0, 5, 1),
                 All1f(0, 6, 1), All1f(1, 0, 1), All1f(1, 1, 1), All1f(1, 2, 1), All1f(1, 3, 1), All1f(1, 4, 1),
                 All1f(1, 5, 1), All1f(1, 6, 1), All1f(2, 1, 1), All1f(2, 2, 1), All1f(2, 3, 1), All1f(2, 4, 1),
                 All1f(2, 5, 1), All1f(3, 1, 1), All1f(3, 2, 1), All1f(3, 4, 1), All1f(3, 5, 1), All1f(4, 1, 1),
                 All1f(4, 2, 1), All1f(4, 3, 1), All1f(4, 4, 1), All1f(4, 5, 1), All1f(5, 0, 1), All1f(5, 1, 1),
                 All1f(5, 2, 1), All1f(5, 3, 1), All1f(5, 4, 1), All1f(5, 5, 1), All1f(5, 6, 1), All1f(6, 0, 1),
                 All1f(6, 1, 1), All1f(6, 2, 1), All1f(6, 3, 1), All1f(6, 4, 1), All1f(6, 5, 1), All1f(6, 6, 1)};
        for (int i = 0; i <= 6; i++)
            for (int j = 0; j <= 6; j++)
                if (!(i == 3 && (j == 1 || j == 5)))
                    all2f.push_back(All2f(i, j, 2));
    }

    cerr << "play start" << endl;
    //保存并同步信息
    endturn = 0;
    action = 0;
    dst = fakepos;
    totalround = client.get_state();
    myid = client.get_id();
    lastpos.push_back(client.get_player_pos());

    updatestatus(client);
    for (auto c : client.others) { //更新逃脱状态
        if (c.id >= 0 && c.status == WAIT_FOR_ESCAPE) {
            if (player[c.id].escapeleft == 0) {
                player[c.id].escapeleft = 3;
            } else {
                player[c.id].escapeleft--;
            }
        }
    }
    updateinv(client);
    cerr << "totalround:" << totalround << endl;
    cerr << "mypos:";
    printpos(player[myid].pos);
    cerr << "myhp:" << player[myid].hp << endl;

    if (player[myid].status == DEAD || player[myid].status == SKIP || player[myid].status == AI_ERROR ||
        player[myid].status == ESCAPED) { //立即结束
        client.finish();
        return;
    }
    if (totalround == 1 && myid == 0) {
        if (rand() % 2 == 0)
            movetopos(Pos(1, 0, 1), client);
        cerr << "finish" << endl << endl;
        client.finish();
        return;
    }
    if (totalround == 1 && myid == 1) {
        movetopos(Pos(5, 0, 1), client);
        cerr << "finish" << endl << endl;
        client.finish();
        return;
    }
    strategy = 7;
    while (strategy == 7) // 7继续行动
        decideStrategy(client);
    cerr << "strategy" << strategy << endl;
    if (strategy != 5) //动了
        staycount = 0;

    switch (strategy) { // 0移动，1攻击，2拾取，3治疗，4逃离,5不动,6捡钥匙,8已经行动完,9跳过回合
    case 0:             //移动
    {
        if (client.get_player_copy(client.get_id()).status == DEAD &&
            client.get_player_copy(client.get_id()).status == SKIP) {
            cerr << "finish skip" << endl << endl;
            return;
        }
        movetopos(dst, client);
        if (client.get_player_copy(client.get_id()).status == DEAD &&
            client.get_player_copy(client.get_id()).status == SKIP) {
            cerr << "finish skip" << endl << endl;
            return;
        }
        endturn = true;
        break;
    }
    case 1: //攻击
    {
    }
    case 2: //拾取
    {
        if (client.get_player_copy(client.get_id()).status == DEAD &&
            client.get_player_copy(client.get_id()).status == SKIP) {
            cerr << "finish skip" << endl << endl;
            return;
        }
        auto canpick = client.judge_if_can_pick(client.pos2node(mypos));
        if (canpick.second.size() == 2) { //两种东西都有
            client.inspect_box();
            cerr << box.size() << endl;
            auto iter = box.begin();
            while (iter != box.end()) { //删除该位置的box
                if ((*iter).pos == mypos) {
                    box.erase(iter);
                } else
                    iter++;
            }
        } else if (canpick.second.size() == 1) { //只有一种东西
            if (canpick.second[0] == "Box") {    //若是掉落物则拾取
                client.inspect_box();
                cerr << box.size() << endl;
                auto iter = box.begin();
                while (iter != box.end()) { //删除该位置的box
                    if ((*iter).pos == mypos) {
                        box.erase(iter);
                    } else
                        iter++;
                }
            } else { //若是物资点，进行判断
                bool success = client.inspect_materials(nextToInspect(myitem));
                int targetinvid = findinv(mypos);
                if (success) {
                    inv[targetinvid].left = max(0, inv[targetinvid].left - 1);
                    if (inv[targetinvid].cd == 0)
                        inv[targetinvid].cd = 8;
                } else {
                    inv[targetinvid].left = 0;
                    inv[targetinvid].cd = 8;
                }
            }
        }
        movecount = 0;
        endturn = true;
        break;
    }
    case 3: //治疗
    {
        client.use_kit();
        movecount = 0;
        endturn = true;
        break;
    }
    case 4: //逃离
    {
        client.escape(true);
        movecount = 0;
        endturn = true;
        break;
    }
    case 5: //不动
    {
        if (client.get_player_copy(client.get_id()).status == DEAD &&
            client.get_player_copy(client.get_id()).status == SKIP) {
            cerr << "finish skip" << endl << endl;
            return;
        }
        staycount++; //没动
        endturn = true;
        break;
    }
    case 6: //捡钥匙
    {
        client.get_key();
        movecount = 0;
        cerr << "finish key" << endl << endl;
        return;
    }
    case 8: { //已经行动完
        endturn = true;
        break;
    }
    case 9: { //跳过回合
        cerr << "finish skip" << endl << endl;
        return;
    }
    }

    if (endturn || (client.get_player_copy(client.get_id()).status != DEAD &&
                    client.get_player_copy(client.get_id()).status != SKIP)) //若当前操作计入行动次数，则结束回合
    {
        cerr << "finish" << endl << endl;
        client.finish();
    } else //若当前操作不计入行动次数，则可以继续行动
    {
        play(client);
    }
}

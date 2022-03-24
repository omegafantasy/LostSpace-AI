//
// Created by Emily Jia on 2020/12/22.
//

#ifndef AI_SDK_AI_CLIENT_H
#define AI_SDK_AI_CLIENT_H

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "jsoncpp/json/json.h"

constexpr int LAYER = 3;
constexpr int LENGTH = 7;
constexpr int INF = 0x3f3f3f3f;
enum MapReaderStatus { NODE, EDGE, MATERIAL, ELEVATOR, BLOCK };

// key: "Sticky", "LandMine"
// 有多少个, 有x个埋了，x个埋的地方的坐标（3元组）
using Trapbag = std::map<std::string, std::vector<std::vector<int>>>;
using Pos = std::tuple<int, int, int>;
Pos json_to_pos(const Json::Value &json);
enum STATUS { ALIVE, DEAD, ESCAPED, SKIP, WAIT_FOR_ESCAPE, AI_ERROR };

// 道具类型
struct {
    std::string LandMine = "LandMine"; // 地雷
    std::string Slime = "Sticky";      // 粘弹，策划案中的官方名称为Slime，游戏逻辑中对应为Sticky
    std::string MedKit = "Kit";        // 医疗包，官方名称为MedKit，逻辑中为Kit
    std::string Blink = "Transport";   // 闪现，官方名称为Blink，逻辑中为Transport
} const ToolType;

// 交互道具类型
struct {
    std::string KeyMachine = "KeyMachine";
    std::string Materials = "Materials";
    std::string EscapeCapsule = "EscapeCapsule";
    std::string Elevator = "Elevator";
    std::string Box = "Box";
} const InterpropsType;

struct Edge {
    Pos stpos, edpos;           // 起始坐标，终点坐标
    bool elevator_only = false; // 为elevator添加的"虚边"
    Edge() = default;
    Edge(const Pos &s, const Pos &t) : stpos(s), edpos(t) {}
};

struct Node {
    Pos pos;                             // 坐标
    std::vector<int> playerid;           // 节点上的玩家序号的list
    std::vector<std::string> interprops; // 节点上的交互道具集合，目前最多1个
    std::vector<Edge *> edges;           // 与该点相邻的边
    bool box = false;                    // 是否有掉的包
    bool has_materials = false;          // 是否有物资点
    int block_turn = INF;                // 缩圈回合数
    bool able = true;                    // 是否被缩圈
    Node() = default;
    Node(const Pos &p) : pos(p) {}
    Node(const Node &another_node);
};

struct Tool {
    int kits_num;    // 剩余血包数量
    int blinks_num;  // 闪现道具数量
    Trapbag trapbag; // 陷阱的相关信息，格式见上
};

struct Player {            // 玩家类，存储玩家的相关信息
    int hp = 200;          // 玩家的血量
    STATUS status = ALIVE; // 玩家现在的状态
    int id = -1;           //玩家的id
    std::vector<int> keys; // 玩家当前拿到的钥匙
    Pos pos;               // 玩家坐标
    Tool toolbag;          //工具栏
    bool has_entered_escape_layer = false; //对于other player有意义，对于自己是无意义的，表示是否曾经到达逃生舱那层
    int left;
    int escapeleft = 0;
    Player() = default;
    Player(const Player &another_player) = default;
};

using Nodeset = std::vector<std::vector<std::vector<Node *>>>;
struct Map {                                             // 地图类
    Nodeset nodeset;                                     // 地图上的点集，支持三元组下标访问
    std::vector<Edge> edgeset = std::vector<Edge>(1000); //边集，初始化防止扩容后地址发生变化
    Map();
    Map(const Map &another_map);
    ~Map();
};

class AI_Client {
  public:
    Map map;
    Player player;              // 自己玩家的信息
    int state = -1;             // 大回合数
    std::vector<Player> others; // 可以得到其他玩家的部分消息
    int detectCD = 0;           // 探查技能cd

    // 仅用于通讯和地图读取
    MapReaderStatus cur_reader; //表示当前在读取地图的哪一部分，仅用于读地图
    Json::Reader reader;
    Json::Value root;

    void _handle_block(); // 处理缩圈
    bool _listen();
    void _init_game();
    void _start_turn();
    void _in_turn();
    void _off_turn();

    void _send_len(const std::string &s);
    void _send_msg(const std::string &s);
    Json::Value _get_json_with_list(const std::string &type, const std::vector<std::string> &action);
    Json::Value _get_json_with_list_and_pos(const std::string &type, const std::vector<std::string> &action,
                                            const Pos &pos);
    void _write_and_send_json(const Json::Value &json_msg);
    void _read_map(const char *map_config_path);
    Player &_player(int id); // id 可以为自己
    bool _wait_for_action_response();

    // todo 选手可填写以下函数，处理自己陷阱被触发后的存储等内容，也可选择不填写
    void _trigger_trap(std::string trap, Pos pos);
    void _destroy_trap(std::string trap, Pos pos);

    explicit AI_Client(const char *map_config_path);
    ~AI_Client() = default;

    // 得到拷贝
    AI_Client(const AI_Client &another_client) = default;
    Player get_player_copy(int id);
    std::pair<AI_Client *, std::vector<Player *>> get_copy();

    // Node 与 Pos 的转换函数
    Node &pos2node(const Pos &p); // 从位置获得相应节点
    Pos node2pos(const Node &n);  // 从节点获得位置

    // 查询函数
    int get_state() const;                    // 获得当前的回合数
    int get_id() const;                       // 获得自己的id
    Pos get_escape_pos() const;               // 获得撤离点的位置坐标，返回值三元组
    Pos get_spawn_pos(int id) const;          // 获得出生点位置坐标 num是玩家的编号
    Pos get_player_pos() const;               // 获得自己目前的位置
    Pos get_others_pos(int id);               // 获得别人的位置
    int get_player_hp() const;                // 获得自己hp
    int get_others_hp(int id);                // 获得别人的hp
    int get_check_cd() const;                 // 获得自己探查(detect)技能的cd，注意返回值可能小于0
    Tool get_tools() const;                   // 获得自己的工具栏内容
    std::vector<int> get_keys() const;        // 获得自己的钥匙
    std::vector<int> get_others_keys(int id); // 获得别人的钥匙

    // 返回值表示[是否有可拾取的交互道具，交互道具类型]，交互道具类型包括 Materials(物资点)和Box(箱子)
    std::pair<bool, std::vector<std::string>> judge_if_can_pick(const Node &n);

    // 查询某一节点的邻接节点集，第2个参数表示考不考虑上下层的邻接
    std::vector<Pos> get_neighbors(const Pos &p, bool consider_elevator = false);

    // 寻找最短路径，返回路径上各位置组成的vector，默认允许跨层查询
    std::vector<Pos> find_shortest_path(const Pos &s, const Pos &t);

    // 行动函数。除非特殊说明，以下函数的bool返回值表示操作是否成功
    bool move(const Pos &tar);                 // 发送移动指令
    bool attack(const Pos &pos, int playerid); // 攻击，pos为要打的玩家的坐标，playeridWie要打的玩家
    bool inspect_materials(const std::string &type); // 检视物资点，type的合法取值为: LandMine, Sticky, Kit, Transport
    bool inspect_box();                              // 检视掉的包
    bool place_trap(const std::string &type); // type的合法取值: LandMine, Sticky
    bool use_kit();                           // 使用医疗包
    bool detect(const Pos &tar); // 探查，注意如果操作失败，则throw runtime_error，否则返回该节点上是否有陷阱
    bool get_key(); // 与钥匙机互动，下一回合将获得钥匙
    bool escape(bool to_escape); // 进行逃生操作，to_escape为true则进入逃脱等待状态，为false则终止逃脱等待状态
    bool blink(Pos target); // 闪现
    void finish();          // 发送回合结束指令，发送了此指令之后停止回合的计时

    void run();
};

// todo 选手必须填写此函数
void play(AI_Client &client);

#endif // AI_SDK_AI_CLIENT_H

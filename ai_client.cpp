/**
 * update 2021.2.28
 * 修复了_listen()中的问题，修复了在他人回合调用play的问题，更改了map文件不存在时的报错方式
 */
#include "ai_client.h"

#include <cstring>
#include <exception>

#include "jsoncpp/json/json.h"

using std::vector;

Pos json_to_pos(const Json::Value &json) {
    if (!json.isArray())
        throw std::logic_error("invalid json");
    if (json.size() != 3)
        throw std::range_error("invalid json");
    return {json[0].asInt(), json[1].asInt(), json[2].asInt()};
}

Node::Node(const Node &another_node) {
    pos = another_node.pos;
    playerid = another_node.playerid;
    interprops = another_node.interprops;
    this->edges.clear();
    for (auto edge_ptr : another_node.edges) {
        Edge *copied_ptr = new Edge(*edge_ptr);
        edges.push_back(copied_ptr);
    }
    box = another_node.box;
    has_materials = another_node.has_materials;
    block_turn = another_node.block_turn;
    able = another_node.able;
}

Map::Map() : nodeset(LENGTH, std::vector<std::vector<Node *>>(LENGTH, std::vector<Node *>(LAYER))) {
    for (std::vector<std::vector<Node *>> &x : nodeset)
        for (std::vector<Node *> &y : x)
            for (Node *&z : y)
                z = nullptr;
}

Map::Map(const Map &another_map)
    : nodeset(LENGTH, std::vector<std::vector<Node *>>(LENGTH, std::vector<Node *>(LAYER))) {
    for (int x = 0; x < LENGTH; x++)
        for (int y = 0; y < LENGTH; y++)
            for (int z = 0; z < LAYER; z++) {
                if (nodeset[x][y][z])
                    delete nodeset[x][y][z];
                nodeset[x][y][z] = new Node(*another_map.nodeset[x][y][z]);
            }
}

Map::~Map() {
    for (int x = 0; x < LENGTH; x++)
        for (int y = 0; y < LENGTH; y++)
            for (int z = 0; z < LAYER; z++)
                if (nodeset[x][y][z])
                    delete nodeset[x][y][z];
}

void AI_Client::_handle_block() {
    for (auto &v1 : map.nodeset) {
        for (auto &v2 : v1) {
            for (Node *node : v2) {
                if (node && state > node->block_turn) {
                    node->able = false;
                }
            }
        }
    }
}

AI_Client::AI_Client(const char *map_config_path) {
    std::setbuf(stdout, nullptr);
    cur_reader = NODE;
    _read_map(map_config_path);
    player.hp = 200;
    others = vector<Player>(5);
}

bool AI_Client::_listen() {
    char header[4];
    int info_len = 0;
    while (true) {
        scanf("%4c", header);
        info_len = (header[0] - '0') * 1000 + (header[1] - '0') * 100 + (header[2] - '0') * 10 + (header[3] - '0');
        if (info_len > 0)
            break;
    }
    char *info = new char[info_len + 2];
    fgets(info, info_len + 1, stdin);
    if (!reader.parse(info, root)) {
        fprintf(stderr, "Parse Error\nError Info:\n%s\n", info);
        return false;
    }
    return true;
}
void AI_Client::_init_game() {
    player.id = root["id"].asInt();
    player.keys = vector<int>({player.id});
    player.pos = get_spawn_pos(player.id);
    for (int i = 0; i < 4; ++i) {
        if (i == player.id)
            continue;
        others[i].id = i;
        others[i].keys = vector<int>({i});
        others[i].pos = get_spawn_pos(i);
    }
}

// 回合开始，解析开始信息
void AI_Client::_start_turn() {
    state = root["state"].asInt();
    if (root["inturn"] != player.id) {
        return;
    }
    _handle_block();
    if (detectCD)
        --detectCD;
    player.status = STATUS(root["status"].asInt());
    player.pos = Pos(root["pos"][0].asInt(), root["pos"][1].asInt(), root["pos"][2].asInt());
    player.hp = root["hp"].asInt();
    player.keys.clear();
    Json::Value key_arr = root["keys"];
    for (int i = 0; i < key_arr.size(); ++i) {
        player.keys.push_back(key_arr[i].asInt());
    } // 更新钥匙信息
    player.toolbag.trapbag.clear();
    Json::Value tool_arr = root["tools"];
    player.toolbag.kits_num = tool_arr[ToolType.MedKit].asInt();
    player.toolbag.blinks_num = tool_arr[ToolType.Blink].asInt();
    std::string trap_type[2] = {ToolType.LandMine, ToolType.Slime};
    for (int i = 0; i < 2; ++i) {
        Json::Value cur_tool = tool_arr[trap_type[i]];
        int left = cur_tool[0].asInt(), used = cur_tool[1].asInt();
        player.toolbag.trapbag[trap_type[i]].push_back(std::vector<int>({left}));
        player.toolbag.trapbag[trap_type[i]].push_back(std::vector<int>({used}));
        for (int j = 0; j < used; ++j) {
            player.toolbag.trapbag[trap_type[i]].push_back(
                std::vector<int>({cur_tool[j + 2][0].asInt(), cur_tool[j + 2][1].asInt(), cur_tool[j + 2][2].asInt()}));
        }
    }
    for (int i = 0; i < 3; ++i) {
        Json::Value cur_player = root["others"][i];
        int id = cur_player["player_id"].asInt();
        Player &another = this->_player(id);
        STATUS previous = another.status;
        another.status = STATUS(cur_player["status"].asInt());

        if (another.status == DEAD) {
            another.pos = Pos(-1, -1, -1);
        } else if (previous == DEAD && another.status == ALIVE) {
            another.pos = get_spawn_pos(another.id);
        }

        Json::Value player_keys = cur_player["keys"];
        another.keys.clear();
        for (int j = 0; j < player_keys.size(); ++j) {
            another.keys.push_back(player_keys[j].asInt());
        }
        another.hp = cur_player["hp"].asInt();
    }
    if (player.status == ALIVE || player.status == WAIT_FOR_ESCAPE)
        play(*this);
}

void AI_Client::_in_turn() {
    if (root["type"].asString() == "escaped") {
        player.status = ESCAPED;
    } else if (root["type"].asString() == "other_death") {
        int died_id = root["playerid"].asInt();
        for (auto &another_player : others) {
            if (another_player.id == died_id) {
                another_player.status = DEAD;
                another_player.pos = Pos(-1, -1, -1);
                break;
            }
        }
    } else if (root["type"].asString() == "getkey") {
        Json::Value key_arr = root["keys"];
        for (int i = 0; i < key_arr.size(); ++i) {
            player.keys.push_back(key_arr[i].asInt());
        } // 更新钥匙信息
    } else if (root["type"].asString() == "death") {
        player.status = DEAD;
        pos2node(player.pos).box = root["box"].asBool();
    } else if (root["type"].asString() == "ai_error") {
        if (root["content"]) {
            Json::Value error_ids = root["content"];
            for (int i = 0; i < error_ids.size(); i++)
                this->_player(error_ids[i].asInt()).status = AI_ERROR;
        } else {
            player.status = AI_ERROR;
            std::cerr << "[AI ERROR] found ai error!" << std::endl;
        }
    } else if (root["type"].asString() == "box_disappear") {
        Pos pos = Pos(root["pos"][0].asInt(), root["pos"][1].asInt(), root["pos"][2].asInt());
        Node &node = pos2node(pos);
        node.box = false;
    } else {
        std::cerr << "[SDK ERROR] message format error in function _in_turn" << std::endl;
        // throw std::runtime_error("[SDK ERROR] message format error in function _in_turn");
    }
}
void AI_Client::_off_turn() {
    Json::Value info = root["content"];
    if (info[0].asString() == "died") {
        for (int i = 1; i < info.size(); ++i) {
            this->_player(info[i].asInt()).status = DEAD;
        }
    } else if (info[0].asString() == "escaped") {
        this->_player(root["playerid"].asInt()).status = ESCAPED;
    } else if (info[0].asString() == "see") {
        std::string action = info[1].asString();
        if (action == "pos_update") {
            Json::Value pos_arr = info[2];
            int cur_player = root["playerid"].asInt();
            int tmp[3];
            for (int i = 0; i < 3; ++i) {
                tmp[i] = pos_arr[i].asInt();
            }
            this->_player(cur_player).pos = Pos(tmp[0], tmp[1], tmp[2]);
        } else if (action == "regenerate") {
            int cur_player = root["playerid"].asInt();
            this->_player(cur_player).status = ALIVE;
            this->_player(cur_player).pos = get_spawn_pos(cur_player);
        } else if (action == "interprops_status_update") {
            std::string name = info[3].asString();
            if (name == "Box") {
                Json::Value pos_arr = info[2];
                // int cur_player = root["playerid"].asInt();
                int tmp[3];
                for (int i = 0; i < 3; ++i) {
                    tmp[i] = pos_arr[i].asInt();
                }
                Node &node_pos = *map.nodeset[tmp[0]][tmp[1]][tmp[2]];
                if (info[4].asString() == "appear") {
                    node_pos.box = true;
                } else {
                    node_pos.box = false;
                }
            }
        }
    } else if (info[0].asString() == "getkey") {
        auto &p = this->_player(root["playerid"].asInt());
        for (int i = 1; i < info[1].size(); ++i) {
            p.keys.push_back(info[1][i].asInt());
        }
    } else if (info[0].asString() == "trap_triggered") {
        this->_trigger_trap(info[1].asString(), json_to_pos(info[2]));
    } else if (info[0].asString() == "trap_destroyed") {
        this->_destroy_trap(info[1].asString(), json_to_pos(info[2]));
    } else if (info[0].asString() == "hp_update") {
        player.hp = info[1].asInt();
    } else if (info[0].asString() == "player_enter_top_layer") {
        int player_id = root["playerid"].asInt();
        this->_player(player_id).has_entered_escape_layer = true;
    } else if (info[0].asString() == "ai_error") {
        int player_id = root["playerid"].asInt();
        this->_player(player_id).status = AI_ERROR;
    } else {
        std::cerr << "[SDK ERROR] message format error in function _off_turn" << std::endl;
        // throw std::runtime_error("[SDK ERROR] message format error in function _off_turn");
    }
}
void AI_Client::_trigger_trap(std::string trap, Pos pos) {
    return; // nothing to do
}
void AI_Client::_destroy_trap(std::string trap, Pos pos) {
    return; // nothing to do
}
void AI_Client::_send_len(const std::string &s) {
    int len = s.length();
    unsigned char lenb[4];
    lenb[0] = (unsigned char)(len);
    lenb[1] = (unsigned char)(len >> 8);
    lenb[2] = (unsigned char)(len >> 16);
    lenb[3] = (unsigned char)(len >> 24);
    for (int i = 0; i < 4; i++)
        printf("%c", lenb[3 - i]);
}

void AI_Client::_send_msg(const std::string &s) {
    std::string tmp = s;
    tmp.pop_back();
    _send_len(tmp);
    // std::cout << s;
    // std::cout.flush();
    printf("%s", tmp.c_str());
}

void AI_Client::_read_map(const char *map_config_path) {
    // 地图文件中坐标以z, x, y顺序排列，sdk中坐标以x, y, z顺序排列
    std::ifstream map_in(map_config_path);
    std::string buffer;
    if (!map_in) {
        std::cerr << "No map file" << std::endl;
        throw std::runtime_error("Fail when reading map in sdk.");
    }
    while (getline(map_in, buffer)) {
        if (buffer[0] == '[') {
            if (buffer == "[Node]")
                cur_reader = NODE;
            if (buffer == "[Edge]")
                cur_reader = EDGE;
            if (buffer == "[Material]")
                cur_reader = MATERIAL;
            if (buffer == "[Elevator]")
                cur_reader = ELEVATOR;
            if (buffer == "[Block]")
                cur_reader = BLOCK;
            continue;
        }
        if (cur_reader == NODE) {
            int z = int(buffer[0] - '0'), x = int(buffer[2] - '0'), y = int(buffer[4] - '0');
            map.nodeset[x][y][z] = new Node(Pos(x, y, z));
        } else if (cur_reader == EDGE) {
            int z1 = int(buffer[0] - '0'), x1 = int(buffer[2] - '0'), y1 = int(buffer[4] - '0');
            int z2 = int(buffer[8] - '0'), x2 = int(buffer[10] - '0'), y2 = int(buffer[12] - '0');
            Edge new_edge = Edge(Pos(x1, y1, z1), Pos(x2, y2, z2));
            map.edgeset.push_back(new_edge);
            Edge *edge_ptr = &map.edgeset.back();
            map.nodeset[x1][y1][z1]->edges.push_back(edge_ptr);
            map.nodeset[x2][y2][z2]->edges.push_back(edge_ptr);
        } else if (cur_reader == MATERIAL) {
            int z = int(buffer[0] - '0'), x = int(buffer[2] - '0'), y = int(buffer[4] - '0');
            map.nodeset[x][y][z]->interprops.push_back(InterpropsType.Materials);
            map.nodeset[x][y][z]->has_materials = true;
        } else if (cur_reader == ELEVATOR) {
            int x = int(buffer[0] - '0'), y = int(buffer[2] - '0');
            for (int z = 0; z < LAYER; ++z) {
                if (!map.nodeset[x][y][z])
                    continue;
                map.nodeset[x][y][z]->interprops.push_back(InterpropsType.Elevator);
                // 为Elevator添加虚边
                for (int j = z + 1; j < LAYER; ++j) {
                    Edge new_edge = Edge(Pos(x, y, j), Pos(x, y, z));
                    new_edge.elevator_only = true;
                    map.edgeset.push_back(new_edge);
                    Edge *edge_ptr = &map.edgeset.back();
                    Node *n1 = map.nodeset[x][y][j];
                    Node *n2 = map.nodeset[x][y][z];
                    if (n1 && n2) {
                        map.nodeset[x][y][j]->edges.push_back(edge_ptr);
                        map.nodeset[x][y][z]->edges.push_back(edge_ptr);
                    }
                }
            }
        } else if (cur_reader == BLOCK) {
            int turn = (buffer[0] - '0') * 10; //当前回合数
            int z1 = int(buffer[3] - '0'), x1 = int(buffer[5] - '0'), y1 = int(buffer[7] - '0');
            int z2 = int(buffer[11] - '0'), x2 = int(buffer[13] - '0'), y2 = int(buffer[15] - '0');
            if (x1 > x2)
                std::swap(x1, x2);
            if (y1 > y2)
                std::swap(y1, y2);
            for (int sx = x1; sx <= x2; ++sx) {
                for (int sy = y1; sy <= y2; ++sy) {
                    if (!map.nodeset[sx][sy][z1])
                        continue;
                    map.nodeset[sx][sy][z1]->block_turn = turn;
                }
            }
        }
    }
    pos2node(get_escape_pos()).interprops.push_back(InterpropsType.EscapeCapsule);
}
Player &AI_Client::_player(int id) {
    if (id == player.id)
        return player;
    for (auto &p : others) {
        if (p.id == id)
            return p;
    }
    throw std::range_error("invalid player id");
}

Node &AI_Client::pos2node(const Pos &p) {
    int x = std::get<0>(p), y = std::get<1>(p), z = std::get<2>(p);
    if (x < 0 || x >= LENGTH || y < 0 || y >= LENGTH || z < 0 || z >= LAYER)
        throw std::runtime_error("in pos2node: illegal position");
    if (!map.nodeset[x][y][z])
        throw std::runtime_error("in pos2node: no node at such position");
    return *map.nodeset[x][y][z];
} //从位置获得相应节点

Pos AI_Client::node2pos(const Node &n) { return n.pos; }

Json::Value AI_Client::_get_json_with_list(const std::string &type, const vector<std::string> &action) {
    Json::Value result;
    Json::Value arr_obj;
    for (auto str : action) {
        arr_obj.append(str);
    }
    result["type"] = type;
    result["action"] = arr_obj;
    result["state"] = state;
    return result;
}

Json::Value AI_Client::_get_json_with_list_and_pos(const std::string &type, const vector<std::string> &action,
                                                   const Pos &pos) {
    Json::Value position;
    position.append(std::get<0>(pos));
    position.append(std::get<1>(pos));
    position.append(std::get<2>(pos));
    Json::Value result = _get_json_with_list(type, action);
    result["action"].append(position);
    return result;
}

void AI_Client::_write_and_send_json(const Json::Value &json_msg) {
    Json::FastWriter writer;
    _send_msg(writer.write(json_msg));
    root.clear();
}

Pos AI_Client::get_escape_pos() const { return Pos(3, 3, 0); } // 获得撤离点的位置坐标，返回值三元组
Pos AI_Client::get_spawn_pos(int id) const {
    assert(0 <= id && id <= 3);
    if (id == 0)
        return Pos(0, 0, 1);
    else if (id == 1)
        return Pos(6, 0, 1);
    else if (id == 2)
        return Pos(6, 6, 1);
    else if (id == 3)
        return Pos(0, 6, 1);
} // 获得出生点位置坐标 id是玩家的编号
Pos AI_Client::get_player_pos() const { return player.pos; } //获得自身位置
Pos AI_Client::get_others_pos(int id) {
    assert(0 <= id && id <= 3 && id != player.id);
    return this->_player(id).pos;
}
int AI_Client::get_player_hp() const { return player.hp; }
int AI_Client::get_others_hp(int id) {
    assert(0 <= id && id <= 3 && id != player.id);
    return this->_player(id).hp;
}
// 查询某一节点的邻接节点集，要求可达，consider_elevator为true则上下楼节点压在neighbors中
std::vector<Pos> AI_Client::get_neighbors(const Pos &p, bool consider_elevator) {
    Node &cur_node = pos2node(p);
    vector<Pos> neighbors;
    for (auto edge : cur_node.edges) {
        if (consider_elevator || !edge->elevator_only) {
            Node another_node = pos2node((cur_node.pos == edge->stpos) ? edge->edpos : edge->stpos);
            if (another_node.able)
                neighbors.push_back(another_node.pos);
        }
    }
    return neighbors;
}
int AI_Client::get_state() const { return this->state; }

// BFS寻找最短路径，返回路径上各位置组成的vector
std::vector<Pos> AI_Client::find_shortest_path(const Pos &s, const Pos &t) {
    // std::cerr << "Find Path..., from ";
    // std::cerr << std::get<0>(s) << ' ' << std::get<1>(s) << ' ' << std::get<2>(s)
    //           << " to " << std::get<0>(t) << ' ' << std::get<1>(t) << ' '
    //           << std::get<2>(t);
    std::vector<Pos> ans;
    bool vis[7][7][7];
    Pos queue[2000];
    int parent[2000]; //代表“到达queue中相应位置的上一个点”
    memset(vis, 0, sizeof(vis));
    memset(queue, 0, sizeof(queue));
    memset(parent, 0, sizeof(parent));
    int front = 0, rear = 0;
    queue[rear] = s;
    parent[rear++] = -1;
    vis[std::get<0>(s)][std::get<1>(s)][std::get<2>(s)] = true;
    while (front != rear) {
        bool found = false;
        Pos cur = queue[front];
        std::vector<Pos> neighbors = get_neighbors(cur, true);
        // std::cerr << "curnode: " << std::get<0>(cur) << ' ' << std::get<1>(cur) << ' ' << std::get<2>(cur)
        // <<std::endl; std::cerr << "neighbors size=" << neighbors.size() << std::endl;
        for (auto pos : neighbors) {
            // std::cerr << "To find in neighbors, size=" << neighbors.size()
            //           << std::endl;
            // std::cerr << std::get<0>(pos) << ' ' << std::get<1>(pos) << ' '
            //           << std::get<2>(pos) << std::endl;
            if (!vis[std::get<0>(pos)][std::get<1>(pos)][std::get<2>(pos)]) {
                queue[rear] = pos;
                parent[rear] = front;
                vis[std::get<0>(pos)][std::get<1>(pos)][std::get<2>(pos)] = true;
                if (pos == t) {
                    int p = rear;
                    ans.push_back(t);
                    while (parent[p] != -1) {
                        p = parent[p];
                        ans.push_back(queue[p]);
                    }
                    found = true;
                    break;
                }
                rear++;
            }
        }
        if (found)
            break;
        front++;
    }
    std::reverse(ans.begin(), ans.end());
    return ans;
}
int AI_Client::get_check_cd() const {
    // std::cerr << "Get Check cd..." << std::endl;
    return detectCD;
}
Tool AI_Client::get_tools() const {
    // std::cerr << "get tools..." << std::endl;
    return player.toolbag;
}
// Tool 中会提供相关查询接口，具体提供什么可根据测试组的反馈制定
std::vector<int> AI_Client::get_keys() const {
    // std::cerr << "get keys..." << std::endl;
    return player.keys;
} // 返回已经得到的钥匙集合
std::vector<int> AI_Client::get_others_keys(int id) {
    // std::cerr << "get others keys..." << std::endl;
    return this->_player(id).keys;
}
int AI_Client::get_id() const {
    // std::cerr << "get id..." << std::endl;
    return player.id;
}
std::pair<bool, std::vector<std::string>> AI_Client::judge_if_can_pick(const Node &cur_node) {
    // std::cerr << "judge pick..." << std::endl;
    /***auto mat_pos = std::find(cur_node.interprops.begin(),
                            cur_node.interprops.end(), "Materials");
    auto h_mat_pos = std::find(cur_node.interprops.begin(),
                               cur_node.interprops.end(), "HMaterial");**/
    std::vector<std::string> pickable_items;
    bool pickable = false;
    if (cur_node.has_materials) {
        pickable = true;
        pickable_items.emplace_back(InterpropsType.Materials);
    }
    if (cur_node.box) {
        pickable = true;
        pickable_items.emplace_back(InterpropsType.Box);
    }
    return std::make_pair(pickable, pickable_items);
}

// 返回值表示操作是否成功
// 发送移动指令
bool AI_Client::move(const Pos &tar) {
    // std::cerr << "move..." << std::endl;
    Json::Value operation = _get_json_with_list_and_pos("action", vector<std::string>({"move"}), tar);
    _write_and_send_json(operation);
    bool ret = _wait_for_action_response();
    if (ret && root["success"].asBool()) {
        player.hp = root["hp"].asInt();
        player.pos = tar;
        player.status = STATUS(root["status"].asInt());
        if (!root["view"].empty()) {
            Json::Value view = root["view"];
            for (auto info : view) {
                Json::Value _pos = info[0];
                Pos pos = Pos(_pos[0].asInt(), _pos[1].asInt(), _pos[2].asInt());
                Node &node = pos2node(pos);
                bool hasbox = info[1].asBool();
                node.box = hasbox;
                if (info.size() == 3) {
                    for (auto &_player_id : info[2]) {
                        int player_id = _player_id.asInt();
                        this->_player(player_id).pos = pos;
                    }
                }
            }
        }
        // 自己上下楼后，将不可见的玩家的坐标变为-1，-1，-1
        int layer = std::get<2>(player.pos);
        for (int i = 0; i < 4; i++) {
            Player &p = this->_player(i);
            if (std::get<2>(p.pos) != layer) {
                p.pos = Pos(-1, -1, -1);
            }
        }
        return true;
    }
    return false;
}
// 攻击
bool AI_Client::attack(const Pos &pos, int playerid) {
    // std::cerr << "attack..." << std::endl;
    Json::Value operation = _get_json_with_list_and_pos("action", vector<std::string>({"attack"}), pos);
    operation["action"].append(playerid);
    _write_and_send_json(operation);
    bool ret = _wait_for_action_response();
    if (ret && root["type"].asString() == "action") {
        bool result = root["success"].asBool();
        this->_player(playerid).hp -= 70;
        return result;
    }
    return false;
}
// 检视物资点
bool AI_Client::inspect_materials(const std::string &type) {
    // std::cerr << "inspect_materials..." << std::endl;
    Json::Value operation =
        _get_json_with_list("action", vector<std::string>({"interact", InterpropsType.Materials, type}));
    _write_and_send_json(operation);
    bool ret = _wait_for_action_response();
    if (ret) {
        bool result = root["success"].asBool();
        if (type == ToolType.MedKit)
            player.toolbag.kits_num += result;
        else if (type == ToolType.Blink)
            player.toolbag.blinks_num += result;
        else if (player.toolbag.trapbag.count(type))
            player.toolbag.trapbag[type][0][0] += result;
        return result;
    }
    return false;
}
bool AI_Client::place_trap(const std::string &type) {
    // std::cerr << "place_trap..." << std::endl;
    Json::Value operation = _get_json_with_list("action", vector<std::string>({"trap", type}));
    _write_and_send_json(operation);
    bool ret = _wait_for_action_response();
    if (ret) {
        bool success = root["success"].asBool();
        if (success && player.toolbag.trapbag.count(type)) {
            player.toolbag.trapbag[type][0][0] -= 1;
            player.toolbag.trapbag[type][1][0] += 1;
            std::vector<int> pos = {std::get<0>(player.pos), std::get<1>(player.pos), std::get<2>(player.pos)};
            player.toolbag.trapbag[type].push_back(pos);
        }
        return success;
    }
    return false;
}
bool AI_Client::use_kit() {
    // std::cerr << "use kit..." << std::endl;
    Json::Value operation = _get_json_with_list("action", vector<std::string>({"tool", ToolType.MedKit}));
    _write_and_send_json(operation);
    bool ret = _wait_for_action_response();
    if (ret) {
        bool success = root["success"].asBool();
        if (success) {
            player.hp = root["hp"].asInt();
        }
        return success;
    }
    return false;
}
// 探查
bool AI_Client::detect(const Pos &tar) {
    // std::cerr << "detect..." << std::endl;
    Json::Value operation = _get_json_with_list("action", vector<std::string>({"detect"}));
    Json::Value pos;
    pos.append(std::get<0>(tar));
    pos.append(std::get<1>(tar));
    pos.append(std::get<2>(tar));
    operation["action"].append(pos);
    _write_and_send_json(operation);
    bool ret = _wait_for_action_response();
    if (ret && root["success"].asBool()) {
        detectCD = 5;
        return root["has_trap"].asBool();
    }
    // throw std::runtime_error("can't detect! " + std::to_string(player.id));
}
// 与钥匙机互动，下一回合将获得钥匙
bool AI_Client::get_key() {
    // std::cerr << "get_key..." << std::endl;
    Json::Value operation = _get_json_with_list("action", vector<std::string>({"interact", InterpropsType.KeyMachine}));
    _write_and_send_json(operation);
    bool ret = _wait_for_action_response();
    if (ret) {
        bool success = root["success"].asBool();
        if (success)
            player.status = SKIP;
        return success;
    }
    return false;
}
bool AI_Client::escape(bool to_escape) {
    Json::Value operation =
        _get_json_with_list("action", vector<std::string>({"interact", InterpropsType.EscapeCapsule}));
    operation["action"].append(to_escape);
    _write_and_send_json(operation);
    bool ret = _wait_for_action_response();
    if (ret) {
        bool success = root["success"].asBool();
        if (success)
            player.status = WAIT_FOR_ESCAPE;
        return success;
    }
    return false;
}
bool AI_Client::blink(Pos target) {
    //  std::cerr << "blink ... " << std::endl;
    Json::Value operation =
        this->_get_json_with_list_and_pos("action", vector<std::string>({"tool", ToolType.Blink}), target);
    this->_write_and_send_json(std::move(operation));
    bool ret = _wait_for_action_response();
    if (ret && root["success"].asBool()) {
        player.toolbag.blinks_num--;
        player.hp = root["hp"].asInt();
        player.pos = target;
        player.status = STATUS(root["status"].asInt());
        return true;
    }
    return false;
}
void AI_Client::finish() {
    //  std::cerr << "finish..." << std::endl;
    Json::Value operation;
    operation["type"] = "finish";
    operation["state"] = state;
    _write_and_send_json(operation);
}
void AI_Client::run() {
    while (true) {
        _listen();
        if (root["type"].asString() == "id")
            _init_game();
        else if (root["type"].asString() == "roundbegin")
            _start_turn();
        else if (root["type"].asString() == "offround")
            _off_turn();
        else {
            _in_turn();
        }
        root.clear();
    }
}

bool AI_Client::inspect_box() {
    // std::cerr << "inspecting box ... " << std::endl;
    Json::Value operation = _get_json_with_list("action", vector<std::string>({"interact", InterpropsType.Box}));
    this->_write_and_send_json(std::move(operation));
    bool ret = _wait_for_action_response();
    if (ret) {
        bool result = root["success"].asBool();
        Json::Value player_keys = result["keys"];
        player.keys.clear();
        for (int j = 0; j < player_keys.size(); ++j) {
            player.keys.push_back(player_keys[j].asInt());
        }
        return result;
    }
    return false;
}

bool AI_Client::_wait_for_action_response() {
    while (true) {
        _listen();
        std::string type = root["type"].asString();
        if (type == "action")
            return true;
        else if (type == "format error")
            return false;
        else if (type == "offround")
            _off_turn();
        else
            _in_turn();
        root.clear();
    }
}

Player AI_Client::get_player_copy(int id) { return Player(this->_player(id)); }

std::pair<AI_Client *, std::vector<Player *>> AI_Client::get_copy() {
    AI_Client *copy = new AI_Client(*this);
    std::vector<Player *> players(4);
    for (int i = 0; i < 4; i++)
        players.push_back(&copy->_player(i));
    return std::pair<AI_Client *, std::vector<Player *>>(copy, players);
}

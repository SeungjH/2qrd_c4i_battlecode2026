#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <deque>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace battlecode {
// Names stay inside this project namespace, not in the global namespace.
using namespace std;

inline constexpr char BotVersion[]="v14";

namespace policy {
inline constexpr int ExpansionEnd = 250;
inline constexpr int SupportStart = 280;
inline constexpr int SplitCooldown = 3;
inline constexpr int PortalRetryDelay = 12;
inline constexpr int QueenReportAge = 40;
inline constexpr int HuntDuration = 30;
inline constexpr int HuntStall = 8;
}
inline constexpr int DR[4]={-1,0,1,0}, DC[4]={0,1,0,-1};
inline constexpr char DIRS[]="NESW";

struct Pos { int x=-1,y=-1; bool operator==(const Pos&) const=default; };
struct Tile { Pos p; int pearl=0, countdown=-1, id=-1; char team='.'; int dir=0; bool head=false; };
struct Portal { bool horizontal; Pos p; bool operator==(const Portal&) const=default; };
struct Move { int dir=-1,to=-1,area=0,exits=0,portalId=-1; bool portal=false,frontier=false,uncertain=false,sustainable=true; double score=-1e20,risk=0; };
struct Resource { int stamp=-100,score=0,lastSent=-100; bool fountain=false; };

// Radio is self-described, not authenticated by the engine. Critical agreements
// additionally require a currently visible ally and fresh status.
enum RadioType { STATUS, DANGER, ESCAPE, OFFER, ACCEPT, HUNT, PORTAL_ROUTE_PACKET, QUEEN };
struct Packet { int type=0,sender=0,stamp=0; unsigned payload=0; };
struct Ally { int len=0,stamp=-100,role=0; Pos head; };
struct Warning { int stamp=0,expiry=0,lastSent=-100; bool permanent=false; unsigned sequence=0; };
struct Region { array<int,49> dist; int area=0,exits=0; bool frontier=false; };
struct Collection { bool safe=false; int closest=-1,dist=1000,count=0; };

// A turn-local plan is separate from persistent per-dragon memory.
struct ActionPlan {
    vector<Move> moves;
    Move chosen;
    string reason;
    int splitSize=0;
    bool courier=false;
};

} // namespace battlecode

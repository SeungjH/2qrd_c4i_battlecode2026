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
using namespace std;

// Explorer v10. Each process controls exactly one dragon.
// MOVE has one direction. Routine splits release two segments; emergency
// reversal leaves the old head with two and sends the long rear away.
namespace policy {
constexpr int ExpansionEnd = 250;
constexpr int SupportStart = 280;
constexpr int SplitCooldown = 3;
constexpr int PortalRetryDelay = 12;
constexpr int QueenReportAge = 40;
constexpr int HuntDuration = 30;
constexpr int HuntStall = 8;
}
constexpr int DR[4]={-1,0,1,0}, DC[4]={0,1,0,-1};
constexpr char DIRS[]="NESW";
struct Pos { int x=-1,y=-1; bool operator==(const Pos&) const=default; };
struct Tile { Pos p; int pearl=0, countdown=-1, id=-1; char team='.'; int dir=0; bool head=false; };
struct Portal { bool horizontal; Pos p; bool operator==(const Portal&) const=default; };
struct Move { int dir=-1,to=-1,area=0,exits=0,portalId=-1; bool portal=false,frontier=false,uncertain=false,sustainable=true; double score=-1e20,risk=0; };
static array<Tile,49> tiles;
static string he[8][7],ve[7][8];
static map<int,vector<Portal>> portalPairs;
static array<int,4096> visits{}, lastVisit{}, lastSeen{}, localAt{};
static vector<Pos> body; // Head to tail; unknown initial segments have x=-1.
static int width,height,id,unitLimit,roundNo,length,units,facing;
static int bearing=-1,hand=1,turnsAlive=0,lastSplit=-100,portalUses=0;
static char team;
// Only observed terrain is remembered; foreign body occupancy is read fresh each turn.
static array<array<int,4>,4096> terrain;
static array<array<int,4>,4096> portalAt;
static vector<pair<int,int>> knownPortalEdges;
static array<int,4096> pearlSeen;
static map<int,int> portalLastUsed;
static map<int,int> sharedPortalExit,portalRouteLastSent;
static int lastPortalEntry=-1,lastPortalDir=-1,lastPortalId=-1,lastPortalRound=-100;
static deque<int> recentHeads;
static int goal=-1, goalSince=0, navigateUntil=-1, lastNewTurn=0;
static bool navigating=false;
static deque<int> foodRounds;
struct Resource { int stamp=-100,score=0,lastSent=-100; bool fountain=false; };
static map<int,Resource> resources;
static array<int,4096> spawnOneRound{},harvestRound{},harvestCount{};
static int resourceGoal=-1;
static array<int,4096> pearlCooldown{};
static int escapeLockUntil=-1,lastEnemySeen=-100,pearlSearchBudget=256;
bool quietEndgame() { return roundNo>=350 && turnsAlive>=80 && roundNo-lastEnemySeen>=80; }


int wrap(int n,int m) { return (n%m+m)%m; }
int code(Pos p) { return p.y*64+p.x; }
int dirOf(char c) { for(int d=0;d<4;d++) if(DIRS[d]==c) return d; return 0; }
int local(Pos p) { return p.x<0?-1:localAt[code(p)]; }
int distance(Pos a,Pos b) {
    int x=abs(a.x-b.x),y=abs(a.y-b.y);
    return min(x,width-x)+min(y,height-y);
}
bool officialQueen() { return id==0 || id==1; }
bool protectedLong() { return officialQueen() || (roundNo>=400 && length>=8) || (roundNo>=350 && length>=16); }
bool recentIncome(int window=12) { return !foodRounds.empty() && roundNo-foodRounds.back()<=window; }
// Radio packets are self-described, not authenticated by the engine. Critical
// agreements additionally require a currently visible ally and fresh status.
enum RadioType { STATUS, DANGER, ESCAPE, OFFER, ACCEPT, HUNT, PORTAL_ROUTE_PACKET, QUEEN };
struct Packet { int type=0,sender=0,stamp=0; unsigned payload=0; };
struct Ally { int len=0,stamp=-100,role=0; Pos head; };
struct Warning { int stamp=0,expiry=0,lastSent=-100; bool permanent=false; unsigned sequence=0; };
static vector<unsigned long long> inbox;
static vector<Packet> received;
static map<int,Ally> allies;
static map<int,Warning> warnings;
static unsigned warningSequence=0;
static int corridorEntry=-1,corridorDir=0,courierEntry=-1,courierUntil=-1;
static int courierDir=0,courierStamp=0;
static bool courierPermanent=false;
static int reverseChildUntil=-1;
static int donationTarget=-1,donationTargetSince=-1,offerStamp=-1,receiveDonor=-1,receiveUntil=-1;
static vector<int> receivePearls;
static bool receiveConfirmed=false;
static int receiveStarted=-1,receiveMeeting=-1;
static map<int,pair<int,int>> reservedFood; // tile -> receiver, expiry
static map<int,Ally> allyReports;
static Packet retiredQueen;
static int retiredUntil=-1;
static int feedTargetId=-1,feedTargetLength=0,feedTargetStamp=-100,queenLastSent=-100;
static Pos feedTargetHead;
static int officialQueenId=-1,officialQueenDeath=-1;
static int servingDonor=-1,servingUntil=-1;
static int previousEntry=-1,previousDirection=-1;
static map<int,int> queenBadEdges;
static int assemblyCenter=-1;
static int donorTravelOwner=-1,donorBestDistance=1000,donorProgress=-1,donorCooldown=-1;
static int huntEnemy=-1,huntSince=-1,huntProgress=-1,huntDistance=1000,huntExits=4,huntCooldown=-1;
static deque<int> huntHeads;
static int attackChildEnemy=-1,attackChildUntil=-1;
static bool attackSplitRadio=false;
static Packet outgoing;
static bool hasOutgoing=false;
static int role=0;

Pos decodePos(int k) { return {k%64,k/64}; }
bool validKey(int k) { Pos p=decodePos(k); return p.x<width && p.y<height; }
unsigned long long encodePacket(Packet p) {
    return (0xD7ULL<<56) | (static_cast<unsigned long long>(team=='B')<<55) |
        (static_cast<unsigned long long>(p.type&7)<<52) |
        (static_cast<unsigned long long>(p.sender&65535)<<36) |
        (static_cast<unsigned long long>(p.stamp&511)<<27) | (p.payload&0x7ffffffu);
}
int packetPriority(int type) {
    if(type==ESCAPE) return 7;
    if(type==DANGER) return 6;
    if(type==ACCEPT) return 5;
    if(type==OFFER) return 4;
    if(type==QUEEN) return 3;
    if(type==PORTAL_ROUTE_PACKET) return 2;
    return 1;
}
void sendPacket(int type,unsigned payload,int stamp=-1,bool /*urgent*/=false) {
    if(hasOutgoing && packetPriority(outgoing.type)>packetPriority(type)) return;
    outgoing={type,id,stamp<0?roundNo:stamp,payload}; hasOutgoing=true;
}
bool feedTargetFresh() { return feedTargetId>=0 && roundNo-feedTargetStamp<=policy::QueenReportAge; }
void selectFeedTarget() {
    int minimum=roundNo>=350?3:8;
    int incumbent=feedTargetId;
    feedTargetId=-1; feedTargetLength=0;
    for(auto it=allyReports.begin();it!=allyReports.end();) {
        if(roundNo-it->second.stamp>policy::QueenReportAge) { it=allyReports.erase(it); continue; }
        const Ally &a=it->second;
        if(a.len>=minimum && (a.len>feedTargetLength || (a.len==feedTargetLength && it->first<feedTargetId))) {
            feedTargetId=it->first;feedTargetLength=a.len;feedTargetHead=a.head;feedTargetStamp=a.stamp;
        }
        ++it;
    }
    // Finish the active gift before replacing its receiver for a tiny length lead.
    auto old=allyReports.find(incumbent);
    if(roundNo>=350 && receiveDonor>=0 && roundNo<=receiveUntil && incumbent==id &&
       old!=allyReports.end() && old->second.len>=minimum && feedTargetLength<=old->second.len+2) {
        feedTargetId=incumbent;feedTargetLength=old->second.len;feedTargetHead=old->second.head;feedTargetStamp=old->second.stamp;
    }
    // IDs 0 and 1 are fixed queens. A split child never inherits this role.
    if(officialQueenId>=0 && officialQueenDeath<0) {
        auto fixed=allyReports.find(officialQueenId);
        feedTargetId=-1;feedTargetLength=0;
        if(fixed!=allyReports.end()) {
            feedTargetId=fixed->first;feedTargetLength=fixed->second.len;
            feedTargetHead=fixed->second.head;feedTargetStamp=fixed->second.stamp;
        }
    }
}
void rememberAllyReport(int owner,int len,Pos head,int stamp) {
    if(owner<0 || len<2 || head.x<0 || head.x>=width || head.y<0 || head.y>=height ||
       stamp>roundNo || roundNo-stamp>policy::QueenReportAge) return;
    auto it=allyReports.find(owner);
    if(it!=allyReports.end() && stamp<it->second.stamp) return;
    if(it!=allyReports.end() && len<it->second.len &&
       (owner==feedTargetId || (owner==id && it->second.len>=8))) {
        retiredQueen={QUEEN,owner,stamp,unsigned(code(head))|(unsigned(len)<<12)};
        retiredUntil=roundNo+12;
    }
    if(owner<=1) {
        if(officialQueenDeath>=stamp) return;
        officialQueenId=owner;officialQueenDeath=-1;
    }
    allyReports[owner]={len,stamp,0,head};
    selectFeedTarget();
}
bool reservedPearl(int key) {
    auto it=reservedFood.find(key);
    if(it!=reservedFood.end() && it->second.second>=roundNo && it->second.first!=id) return true;
    return roundNo>=280 && feedTargetFresh() && feedTargetId!=id && roundNo-feedTargetStamp<=8 &&
        distance(decodePos(key),feedTargetHead)<=4;
}
void rememberResource(int key,int score,int stamp,bool fountain) {
    if(!validKey(key) || stamp>roundNo || roundNo-stamp>(fountain?60:24)) return;
    auto &r=resources[key];
    if(stamp>=r.stamp) { r.stamp=stamp;r.score=score;r.fountain=fountain; }
    if(resources.size()>32) {
        auto old=min_element(resources.begin(),resources.end(),[](const auto&a,const auto&b){
            return a.second.stamp+a.second.score*2<b.second.stamp+b.second.score*2;
        });
        resources.erase(old);
    }
}
int visibleHead(int owner) {
    for(int j=0;j<49;j++) if(tiles[j].id==owner && tiles[j].head) return j;
    return -1;
}
bool visibleAlly(int owner) {
    for(const auto &t:tiles) if(t.id==owner && t.team==team) return true;
    return false;
}
bool freshAlly(int owner) {
    auto it=allies.find(owner);
    return it!=allies.end() && roundNo-it->second.stamp<=4 && visibleAlly(owner);
}
void addWarning(int entrance,int d,int stamp,bool permanent,bool localProof=false) {
    if(!validKey(entrance)) return;
    int expiry=permanent?501:stamp+20;
    (void)localProof;
    if(expiry<roundNo) return;
    int k=entrance*4+d;
    auto it=warnings.find(k);
    if(it==warnings.end() || expiry>it->second.expiry)
        warnings[k]={stamp,expiry,-100,permanent,++warningSequence};
    if(warnings.size()>64) {
        // Keep permanent warnings until all temporary warnings have been evicted.
        auto oldest=min_element(warnings.begin(),warnings.end(),[](const auto&a,const auto&b){
            if(a.second.permanent!=b.second.permanent) return !a.second.permanent;
            return a.second.sequence<b.second.sequence;
        });
        warnings.erase(oldest);
    }
}
bool avoided(int k,int d) {
    auto it=warnings.find(k*4+d);
    return it!=warnings.end() && it->second.expiry>=roundNo;
}
void receiveRadio() {
    received.clear();
    for(auto raw:inbox) {
        if((raw>>56)!=0xD7 || int((raw>>55)&1)!=(team=='B')) continue;
        Packet p{int((raw>>52)&7),int((raw>>36)&65535),int((raw>>27)&511),unsigned(raw&0x7ffffff)};
        if(p.sender==id || p.stamp>roundNo || p.type>QUEEN) continue;
        int age=roundNo-p.stamp;
        if(p.type==QUEEN) {
            int key=p.payload&4095,len=(p.payload>>12)&4095;
            if(len==0 && p.sender==officialQueenId && p.stamp>=feedTargetStamp) {
                officialQueenDeath=p.stamp;allyReports.erase(p.sender);selectFeedTarget();
                retiredQueen=p;retiredUntil=roundNo+12;
            }
            if(validKey(key) && len>=2) rememberAllyReport(p.sender,len,decodePos(key),p.stamp);
            continue;
        }
        if(p.type==DANGER) {
            addWarning(p.payload&4095,(p.payload>>12)&3,p.stamp,(p.payload>>14)&1);
            continue;
        }
        if(p.type==PORTAL_ROUTE_PACKET) {
            // Bit 26 distinguishes resource reports from immutable portal routes.
            if(p.payload&(1u<<26)) {
                rememberResource(p.payload&4095,(p.payload>>12)&15,p.stamp,(p.payload>>16)&1);
                continue;
            }
            if(age>8) continue;
            int entry=p.payload&4095,d=(p.payload>>12)&3,exit=(p.payload>>14)&4095;
            if(!validKey(entry) || !validKey(exit) || sharedPortalExit.size()>=128) continue;
            int key=entry*4+d;
            if(!sharedPortalExit.count(key)) {
                sharedPortalExit[key]=exit;
                if(terrain[entry][d]<0) terrain[entry][d]=exit;
            }
            continue;
        }
        if(age>4) continue;
        if(p.type==HUNT && !(p.payload&(1u<<26)) && turnsAlive==0 && length==2 &&
           p.sender<id && int(p.payload&4095)==code(tiles[24].p) && visibleAlly(p.sender)) {
            attackChildEnemy=(p.payload>>12)&4095;attackChildUntil=roundNo+2;
            courierUntil=roundNo-1;
            continue;
        }
        if(p.type==ESCAPE) {
            int child=p.payload&4095,entry=(p.payload>>12)&4095;
            // Reserved entry 4095 tells the reversed child to leave; it does not transfer queen identity.
            if(turnsAlive<=1 && p.sender<id && child==code(tiles[24].p) && entry==4095 && length>=6) {
                reverseChildUntil=roundNo+2;bearing=facing;
                continue;
            }
            if(turnsAlive<=1 && p.sender<id && child==code(tiles[24].p) && validKey(entry)) {
                courierDir=(p.payload>>24)&3;
                courierPermanent=(p.payload>>26)&1; courierStamp=p.stamp;
                addWarning(entry,courierDir,p.stamp,courierPermanent);
                if(length<=3) { courierEntry=entry; courierUntil=roundNo+32; }
                else { reverseChildUntil=roundNo+8; bearing=facing; }
            }
            continue;
        }
        if(!visibleAlly(p.sender)) continue;
        if(p.type==STATUS) {
            int key=p.payload&4095,len=(p.payload>>12)&4095;
            if(!validKey(key) || len<2) continue;
            int h=visibleHead(p.sender);
            // A status reports post-action position. Allow the few turns since it was sent.
            if(h>=0 && distance(decodePos(key),tiles[h].p)>age+1) continue;
            allies[p.sender]={len,p.stamp,int((p.payload>>24)&7),decodePos(key)};
            if(p.sender==feedTargetId && len<(roundNo>=350?3:8)) feedTargetId=-1;
            rememberAllyReport(p.sender,len,decodePos(key),p.stamp);
        } else {
            received.push_back(p);
            if(p.type==ACCEPT && p.sender==feedTargetId) {
                int donor=p.payload&65535;servingDonor=donor;servingUntil=roundNo+6;
                for(const Tile &t:tiles) if(t.id==donor && t.team==team)
                    reservedFood[code(t.p)]={p.sender,roundNo+10};
            }
        }
    }
    for(auto it=allies.begin();it!=allies.end();) {
        if(roundNo-it->second.stamp>8) it=allies.erase(it); else ++it;
    }
    for(auto it=warnings.begin();it!=warnings.end();) {
        if(it->second.expiry<roundNo) it=warnings.erase(it); else ++it;
    }
    // Our queens move one tile without portals. Only certify death when every
    // possible next head position fits inside this observation, not on a timeout.
    if(officialQueenId>=0 && officialQueenDeath<0 && feedTargetId==officialQueenId &&
       feedTargetId!=id && distance(tiles[24].p,feedTargetHead)+max(1,roundNo-feedTargetStamp+1)<=3 &&
       !visibleAlly(officialQueenId)) {
        officialQueenDeath=roundNo;allyReports.erase(officialQueenId);
        retiredQueen={QUEEN,officialQueenId,roundNo,unsigned(code(feedTargetHead))};
        retiredUntil=roundNo+12;selectFeedTarget();
    }
    if(!feedTargetFresh()) feedTargetId=-1;
    rememberAllyReport(id,length,tiles[24].p,roundNo);
    for(auto it=reservedFood.begin();it!=reservedFood.end();)
        if(it->second.second<roundNo) it=reservedFood.erase(it); else ++it;
    for(auto it=resources.begin();it!=resources.end();)
        if(roundNo-it->second.stamp>(it->second.fountain?60:24)) it=resources.erase(it); else ++it;
}

const string& edge(int at,int d) {
    int r=at/7,c=at%7;
    if(d==0) return he[r][c];
    if(d==1) return ve[r][c+1];
    if(d==2) return he[r+1][c];
    return ve[r][c];
}
void remember(int portalId,bool horizontal,Pos p) {
    p.x=wrap(p.x,width); p.y=wrap(p.y,height);
    Portal e{horizontal,p}; auto &pair=portalPairs[portalId];
    if(find(pair.begin(),pair.end(),e)==pair.end() && pair.size()<2) pair.push_back(e);
}
int portalDestination(int from,int d,int portalId) {
    auto it=portalPairs.find(portalId);
    if(it==portalPairs.end() || it->second.size()!=2) {
        auto known=sharedPortalExit.find(from*4+d);
        return known==sharedPortalExit.end()?-1:known->second;
    }
    Pos p=decodePos(from);
    Portal here{d%2==0,{wrap(p.x+(d==1),width),wrap(p.y+(d==2),height)}};
    const auto &pair=it->second;
    if(!(here==pair[0]) && !(here==pair[1])) return -1;
    Pos exit=pair[here==pair[0]?1:0].p;
    exit.x=wrap(exit.x-(d==3),width); exit.y=wrap(exit.y-(d==0),height);
    return code(exit);
}
bool knownPortalTrap(int destination) {
    if(destination<0 || !validKey(destination)) return false;
    int open=0,portals=0;
    for(int d=0;d<4;d++) {
        if(terrain[destination][d]==-2) return false;
        if(portalAt[destination][d]>=0) portals++;
        else if(terrain[destination][d]>=0) open++;
    }
    return open==0 && portals<=1;
}
bool portalReady(int portalId) {
    auto it=portalLastUsed.find(portalId);
    return it==portalLastUsed.end() || roundNo-it->second>=policy::PortalRetryDelay;
}
// Physical transition, independent of occupancy. The local destination is
// unavailable when its paired exit lies outside the vision square.
int nextTile(int at,int d,bool respectCooldown=true) {
    const string &e=edge(at,d);
    if(e=="w") return -1;
    Pos p=tiles[at].p;
    if(e==".") return local({wrap(p.x+DC[d],width),wrap(p.y+DR[d],height)});
    int portalId=stoi(e);
    if(respectCooldown && !portalReady(portalId)) return -1;
    int destination=portalDestination(code(p),d,portalId);
    return destination<0?-1:local(decodePos(destination));
}

void syncBody() {
    if(int(body.size())!=length || (body.size() && !(body[0]==tiles[24].p)))
        body.assign(length,Pos{});
    body[0]=tiles[24].p;
    array<bool,49> used{}; used[24]=true;
    int predecessor=24;
    for(int k=1;k<length;k++) {
        int found=-1;
        for(int j=0;j<49;j++) if(!used[j] && tiles[j].id==id && !tiles[j].head &&
            nextTile(j,tiles[j].dir,false)==predecessor) { found=j; break; }
        if(found<0) break;
        body[k]=tiles[found].p; used[found]=true; predecessor=found;
    }
}

// One legal move puts our new head at 'to'; its old tail vacates only if no pearl is eaten.
array<bool,49> afterMove(int to) {
    array<bool,49> blocked{};
    for(int j=0;j<49;j++) blocked[j]=tiles[j].id>=0;
    if(!tiles[to].pearl && !body.empty()) {
        int tail=local(body.back());
        if(tail>=0 && tiles[tail].id==id) blocked[tail]=false;
    }
    blocked[to]=true;
    return blocked;
}
struct Region { array<int,49> dist; int area=0,exits=0; bool frontier=false; };
Region region(int start,const array<bool,49>& blocked,bool portals=true) {
    Region out; out.dist.fill(-1); out.dist[start]=0;
    array<int,49> queue{}; int head=0,tail=0; queue[tail++]=start;
    while(head<tail) {
        int at=queue[head++]; out.area++;
        for(int d=0;d<4;d++) {
            if(!portals && edge(at,d)!=".") continue;
            int to=nextTile(at,d);
            if(to<0) {
                // An ordinary open edge outside the current window is an exploration frontier.
                if(edge(at,d)==".") out.frontier=true;
                continue;
            }
            if(blocked[to]) continue;
            if(at==start) out.exits++;
            if(out.dist[to]>=0) continue;
            out.dist[to]=out.dist[at]+1; queue[tail++]=to;
        }
    }
    return out;
}

double headRisk(int target) {
    double risk=0;
    for(int j=0;j<49;j++) if(tiles[j].head && tiles[j].id!=id) {
        bool one=false,two=false;
        for(int d=0;d<4;d++) {
            int a=nextTile(j,d,false);
            if(a==target) one=true;
            if(a<0 || tiles[a].id>=0) continue;
            for(int e=0;e<4;e++) if(nextTile(a,e,false)==target) two=true;
        }
        if(tiles[j].team!=team) risk+=one?120.0:two?((protectedLong() || (roundNo>=250 && length>=8))?35.0:8.0):0.0;
        else risk+=one?18.0:0.0;
    }
    return risk;
}

double separation(int target) {
    map<int,pair<int,int>> distances;
    Pos here=tiles[24].p,there=tiles[target].p;
    for(const auto &t:tiles) if(t.id>=0 && t.id!=id && t.team==team) {
        if(!distances.count(t.id)) distances[t.id]={100,100};
        auto &p=distances[t.id];
        p.first=min(p.first,distance(here,t.p));
        p.second=min(p.second,distance(there,t.p));
    }
    double score=0;
    double crowd=distances.size()>=3?2.0:1.0;
    for(auto [ally,p]:distances) {
        (void)ally;
        score-=crowd*18.0/(p.second+0.5);
        score+=4.0*clamp(p.second-p.first,-1,1);
    }
    return score;
}

bool obstructsQueen(int target) {
    if(roundNo<280 || feedTargetId<0 || feedTargetId==id || feedTargetLength<4 || target<0) return false;
    int h=visibleHead(feedTargetId);if(h<0) return false;
    int forward=nextTile(h,tiles[h].dir,false),exits=0,only=-1;
    for(int d=0;d<4;d++) {int n=nextTile(h,d,false);if(n>=0 && tiles[n].id<0) {exits++;only=n;}}
    return target==forward || (exits==1 && target==only);
}

// Check future steps with our moving tail, rather than only the next empty cell.
// Foreign bodies stay fixed in this short prediction; unknown terrain ends the check.
bool longContinuation(const vector<int>& trail,const array<bool,4096>& others,
                      array<bool,49> eaten,int depth,int &budget) {
    if(depth==0 || --budget<=0) return true;
    int at=trail.front();
    for(int d=0;d<4;d++) {
        if(portalAt[at][d]>=0 || avoided(at,d)) continue;
        int to=terrain[at][d];
        if(to==-2) return true;
        if(to<0 || others[to] || find(trail.begin(),trail.end(),to)!=trail.end()) continue;
        auto next=trail; next.insert(next.begin(),to);
        int j=local(decodePos(to));bool food=j>=0 && tiles[j].pearl && !eaten[j];
        auto consumed=eaten;
        if(food) consumed[j]=true; else next.pop_back();
        if(longContinuation(next,others,consumed,depth-1,budget)) return true;
    }
    return false;
}
bool sustainableLongMove(int to) {
    if(!protectedLong() || int(body.size())!=length) return true;
    vector<int> trail;
    for(Pos p:body) { if(p.x<0) return true;trail.push_back(code(p)); }
    array<bool,4096> others{};
    for(const Tile &t:tiles) if(t.id>=0 && t.id!=id) others[code(t.p)]=true;
    trail.insert(trail.begin(),code(tiles[to].p));
    array<bool,49> eaten{};
    if(tiles[to].pearl) eaten[to]=true; else trail.pop_back();
    int budget=768;
    return longContinuation(trail,others,eaten,6,budget);
}

Move evaluate(int d,int to) {
    Move m; m.dir=d; m.to=to; m.portal=edge(24,d)!=".";
    if(m.portal) m.portalId=stoi(edge(24,d));
    Region r=region(to,afterMove(to));
    m.area=r.area; m.exits=r.exits; m.frontier=r.frontier;
    int nextLength=length+tiles[to].pearl;
    m.risk=headRisk(to);
    m.sustainable=sustainableLongMove(to);
    double s=0.4*min(r.area,20)-m.risk+separation(to);
    if(!m.sustainable) s-=1000;
    if(obstructsQueen(to)) s-=180;
    if(protectedLong()) {
        s+=2.0*min(r.area,32)+5.0*min(r.exits,3);
        if(assemblyCenter>=0)
            s-=4.0*max(0,distance(tiles[to].p,decodePos(assemblyCenter))-max(4,length/4+2));
        auto bad=queenBadEdges.find(code(tiles[24].p)*4+d);
        if(bad!=queenBadEdges.end() && bad->second>=roundNo) s-=500;
    }
    if(r.exits==0) s-=10000; // Avoid eating a pearl which closes our final exit.
    int needed=protectedLong()?min(nextLength+6,32):
        nextLength>=8?min(nextLength+6,24):min(nextLength+2,14);
    if(!r.frontier && r.area<needed) s-=12*(needed-r.area);
    if(nextLength>=8 && !r.frontier && r.exits<2) s-=protectedLong()?45:20;
    // Cheap scouts can pressure a visibly larger enemy without entering a
    // one-step head collision. The actual trade is handled separately.
    if(length<=3 && units>=5 && roundNo<350 && m.risk<18 &&
       (r.frontier || r.area>=8)) {
        for(int j=0;j<49;j++) if(tiles[j].head && tiles[j].team!=team) {
            int observed=0;
            for(const Tile &t:tiles) if(t.id==tiles[j].id) observed++;
            int before=distance(tiles[24].p,tiles[j].p);
            int after=distance(tiles[to].p,tiles[j].p);
            if(observed>=6 && after>=2 && after<before) s+=12;
        }
    }
    if(d==bearing) s+=8;
    if(d==facing) s+=protectedLong()?12:3;
    if(turnsAlive<3 && length==2 && d==bearing) s+=5;
    int k=code(tiles[to].p);
    // A completed toroidal lap is still a loop even when it took more than 16 turns.
    // Prefer an unvisited adjacent strip over the heading bonus on the first return.
    if(visits[k]>0) s-=(recentIncome()?3.0:14.0)+min(visits[k],12)*(recentIncome()?0.5:4.0);
    if(avoided(code(tiles[24].p),d)) s-=5000;
    if(lastVisit[k]>=0) s-=max(0,40-(roundNo-lastVisit[k]))*(protectedLong()?0.15:0.6);
    int novelty=0;
    for(int dy=-3;dy<=3;dy++) for(int dx=-3;dx<=3;dx++) {
        Pos p{wrap(tiles[to].p.x+dx,width),wrap(tiles[to].p.y+dy,height)};
        if(lastSeen[code(p)]<0) novelty++;
    }
    s+=0.22*novelty;
    s+=tiles[to].pearl*(roundNo<policy::ExpansionEnd?18:24);
    if(tiles[to].pearl && reservedPearl(code(tiles[to].p))) s-=100;
    // Distances respect walls and body occupancy, unlike straight-line pearl attraction.
    double food=0;
    for(int j=0;j<49;j++) if(j!=to && r.dist[j]>0 && tiles[j].pearl)
        food=max(food,12.0/(r.dist[j]+1));
    s+=food;
    if(m.portal) {
        s-=4;
        if(!recentIncome(12) && visits[code(tiles[to].p)]==0) s+=12;
    }
    m.score=s;
    return m;
}

Move distantPortal(int d) {
    Move m; m.dir=d; m.portal=true; m.uncertain=true;
    m.portalId=stoi(edge(24,d));
    int target=portalDestination(code(tiles[24].p),d,m.portalId);
    bool known=target>=0;
    set<int> nearbyPortals;
    for(const auto &row:he) for(const string &e:row)
        if(e!="." && e!="w") nearbyPortals.insert(stoi(e));
    for(const auto &row:ve) for(const string &e:row)
        if(e!="." && e!="w") nearbyPortals.insert(stoi(e));
    auto pair=portalPairs.find(m.portalId);
    bool localPair=pair!=portalPairs.end() && pair->second.size()==2;
    bool wideScout=width*height>=768 && roundNo<300 && length<=3;
    if(protectedLong() || !portalReady(m.portalId) || knownPortalTrap(target) ||
       (!known && (roundNo>=350 || length>3 || turnsAlive<3 ||
           (nearbyPortals.size()<2 && !wideScout))) ||
       (known && (length>6 ||
           (!localPair && length>3)))) return m;
    // An unobserved exit may be occupied. Only scouts take that deliberate risk.
    m.frontier=true; m.exits=1; m.area=14;
    int here=code(tiles[24].p);
    bool hungry=!recentIncome(12);
    m.score=(known?17.0:13.0)+(hungry?17.0:0.0);
    if(wideScout && hungry && !known) m.score+=22;
    if(target>=0) {
        m.score+=visits[target]==0?9.0:-min(visits[target],8)*3.0;
        if(lastSeen[target]>=0 && roundNo-lastSeen[target]<8) m.score-=8;
        if(pearlSeen[target]>=0 && roundNo-pearlSeen[target]<=20) m.score+=8;
    }
    if(length>3) m.score-=8;
    if(roundNo>=300) m.score-=7;
    if(avoided(here,d)) m.score-=5000;
    return m;
}

vector<Move> legalMoves() {
    vector<Move> out;
    // Stable tie order is individual to each dragon, never reseeded every round.
    array<int,4> order{bearing,(bearing+hand+4)%4,(bearing-hand+4)%4,(bearing+2)%4};
    for(int d:order) {
        if(protectedLong() && edge(24,d)!=".") continue;
        if(edge(24,d)=="w") continue;
        int to=nextTile(24,d);
        if(to>=0 && tiles[to].id<0 &&
           !(edge(24,d)!="." && knownPortalTrap(code(tiles[to].p))))
            out.push_back(evaluate(d,to));
        else if(to<0 && edge(24,d)!="." && portalReady(stoi(edge(24,d)))) {
            Move m=distantPortal(d);
            if(m.score>-1e19) out.push_back(m);
        }
    }
    stable_sort(out.begin(),out.end(),[](const Move&a,const Move&b){return a.score>b.score;});
    return out;
}

// Keep a destination across turns, so a corridor detour is not abandoned at each bend.
void rememberTerrain() {
    for(int j=0;j<49;j++) {
        int k=code(tiles[j].p);
        pearlSeen[k]=tiles[j].pearl?roundNo:-1;
        if(tiles[j].countdown==1) {
            if(spawnOneRound[k]==roundNo-1) rememberResource(k,15,roundNo,true);
            spawnOneRound[k]=roundNo;
        }
        for(int d=0;d<4;d++) {
            const string &e=edge(j,d);
            if(e==".") {
                terrain[k][d]=code({wrap(tiles[j].p.x+DC[d],width),
                    wrap(tiles[j].p.y+DR[d],height)});
            } else if(e=="w") terrain[k][d]=-1;
            else {
                if(portalAt[k][d]<0) knownPortalEdges.push_back({k,d});
                portalAt[k][d]=stoi(e);
                terrain[k][d]=-3; // Observed entry, paired exit not known yet.
            }
        }
    }
    for(auto [k,d]:knownPortalEdges) {
        int destination=portalDestination(k,d,portalAt[k][d]);
        terrain[k][d]=destination>=0?destination:-3;
    }
}

bool guideOutOfLoop(vector<Move>& moves) {
    // A safe queen circuit is useful. Do not force the scoring unit to seek
    // unknown territory merely because its head positions repeat.
    if(officialQueen()) {goal=-1;navigating=false;return false;}
    if(width*height>=768 && length<=3 && roundNo<300 && !recentIncome() &&
       any_of(moves.begin(),moves.end(),[](const Move&m){return m.uncertain && m.score>40;}))
        return false;
    int here=code(tiles[24].p);
    if(visits[here]==1) lastNewTurn=turnsAlive;
    recentHeads.push_back(here);
    if(recentHeads.size()>32) recentHeads.pop_front();
    set<int> unique(recentHeads.begin(),recentHeads.end());
    bool shortCycle=false;
    int n=int(recentHeads.size());
    for(int period=4;period<=12 && 2*period<=n;period++) {
        bool same=true;
        for(int j=0;j<period;j++) if(recentHeads[n-1-j]!=recentHeads[n-1-j-period]) same=false;
        if(same) { shortCycle=true; break; }
    }
    bool repeating=shortCycle || (recentHeads.size()>=16 && unique.size()*3<=recentHeads.size()*2);
    if(repeating && !recentIncome() && turnsAlive>=escapeLockUntil)
        escapeLockUntil=turnsAlive+8;
    if(!recentIncome() && (repeating || turnsAlive-lastNewTurn>=28)) navigateUntil=turnsAlive+24;
    if(recentIncome()) navigateUntil=-1;
    // New head positions can still skirt the same outer strip. Scouts seek
    // observation frontiers after a food drought, without waiting for a loop.
    bool frontierScout=length<=3 && (id%3==0 || (width*height>=768 && units<12) || !recentIncome(20)) &&
        roundNo<300 && turnsAlive>=8 && !recentIncome(8);
    navigating=frontierScout || (!recentIncome() && (turnsAlive<navigateUntil || turnsAlive-lastNewTurn>=12));
    if(!navigating || moves.empty()) { goal=-1; return false; }

    array<bool,4096> blocked{};
    for(const auto &t:tiles) if(t.id>=0) blocked[code(t.p)]=true;
    for(Pos p:body) if(p.x>=0) blocked[code(p)]=true;
    blocked[here]=false;
    array<bool,4> safe{};
    for(const auto &m:moves)
        safe[m.dir]=m.sustainable && (!m.uncertain || portalDestination(here,m.dir,m.portalId)>=0) &&
            m.risk<18 && m.exits>0 &&
            (m.frontier || m.area>=
                (protectedLong()?min(length+6,32):
                 length>=8?min(length+6,24):min(length+2,14)));
    array<int,4096> dist,first;
    dist.fill(-1); first.fill(-1);
    array<int,4096> queue{}; int qhead=0,qtail=0;
    queue[qtail++]=here; dist[here]=0;
    while(qhead<qtail) {
        int at=queue[qhead++];
        for(int d=0;d<4;d++) {
            if((at==here && !safe[d]) || avoided(at,d) ||
               (protectedLong() && portalAt[at][d]>=0)) continue;
            int to=terrain[at][d];
            if(to<0 || blocked[to] || dist[to]>=0) continue;
            dist[to]=dist[at]+1;
            first[to]=at==here?d:first[at];
            queue[qtail++]=to;
        }
    }
    if(goal==here || (goal>=0 && (dist[goal]<0 || turnsAlive-goalSince>64))) goal=-1;
    if(goal<0) {
        double best=-1e20;
        for(int n=1;n<qtail;n++) {
            int k=queue[n];
            if(dist[k]<4) continue;
            int exits=0,unknown=0;
            for(int to:terrain[k]) { if(to>=0) exits++; if(to==-2 || to==-3) unknown++; }
            // Reject known blind alleys as exploration destinations.
            if(!unknown && exits<2) continue;
            double score=(visits[k]==0?50:0)+unknown*18.0-
                min(visits[k],20)*2.0-0.8*dist[k];
            if(find(recentHeads.begin(),recentHeads.end(),k)==recentHeads.end()) score+=30;
            if(frontierScout) {
                Pos target=decodePos(k);int unseen=0;
                for(int dy=-3;dy<=3;dy++) for(int dx=-3;dx<=3;dx++)
                    unseen+=lastSeen[code({wrap(target.x+dx,width),wrap(target.y+dy,height)})]<0;
                score+=unseen*2.0;
            }
            if(lastVisit[k]>=0) score+=min(roundNo-lastVisit[k],100)*0.15;
            if(pearlSeen[k]>=0 && roundNo-pearlSeen[k]<=20) score+=65;
            if(score>best) { best=score; goal=k; }
        }
        goalSince=turnsAlive;
    }
    if(goal<0) return false; // Fully enclosed or temporarily blocked: retain safe local motion.
    auto chosen=find_if(moves.begin(),moves.end(),[&](const Move&m){return m.dir==first[goal];});
    if(chosen==moves.end()) return false;
    rotate(moves.begin(),chosen,chosen+1);
    return true;
}

int terrainDegree(int k) {
    int n=0; for(int to:terrain[k]) if(to>=0) n++; return n;
}
bool safeMove(const Move &m) {
    if(protectedLong() && m.portal) return false;
    if(!m.sustainable || obstructsQueen(m.to)) return false;
    if(m.uncertain) return m.portal && m.score>-1e19 &&
        portalDestination(code(tiles[24].p),m.dir,m.portalId)>=0 &&
        portalReady(m.portalId) && !avoided(code(tiles[24].p),m.dir);
    int needed=protectedLong()?min(length+6,32):
        length>=8?min(length+6,24):min(length+2,14);
    return m.risk<18 && m.exits>0 && !avoided(code(tiles[24].p),m.dir) &&
        (m.frontier || m.area>=needed);
}
bool promote(vector<Move>& moves,int d) {
    auto it=find_if(moves.begin(),moves.end(),[&](const Move&m){return m.dir==d && safeMove(m);});
    if(it==moves.end()) return false;
    rotate(moves.begin(),it,it+1); return true;
}
// A two-segment scout can pay for a dead-end visit when three visible pearls
// let a three-segment tail child return with a net gain.
bool profitablePocket(const Move &m) {
    if(officialQueen()) return false;
    if(roundNo>=350 || length!=2 || units>=unitLimit || m.portal || m.to<0 ||
       m.risk>=18) return false;
    array<bool,49> seen{};array<int,49> q{};int begin=0,end=0,food=0;
    q[end++]=m.to;seen[m.to]=true;
    while(begin<end && end<=6) {
        int at=q[begin++];food+=tiles[at].pearl;
        for(int d=0;d<4;d++) {
            if(edge(at,d)!=".") continue;
            int to=nextTile(at,d,false);
            if(to<0 || to==24 || tiles[to].id>=0 || seen[to]) continue;
            seen[to]=true;q[end++]=to;
            if(end>6) return false;
        }
    }
    return food>=3;
}
bool profitablePocketMove(vector<Move>& moves) {
    auto it=find_if(moves.begin(),moves.end(),profitablePocket);
    if(it==moves.end()) return false;
    rotate(moves.begin(),it,it+1);return true;
}
// Known-map route, recomputed with current occupancy, retaining a stable destination.
int routeDirection(const vector<Move>& moves,int destination) {
    if(destination<0 || !validKey(destination)) return -1;
    array<bool,4096> blocked{};
    for(const auto &t:tiles) if(t.id>=0) blocked[code(t.p)]=true;
    for(Pos p:body) if(p.x>=0) blocked[code(p)]=true;
    int here=code(tiles[24].p);
    array<int,4096> first; first.fill(-1);
    array<int,4096> q{}; int begin=0,end=0; q[end++]=here; first[here]=4;
    array<bool,4> safe{}; for(const auto &m:moves) if(safeMove(m)) safe[m.dir]=true;
    while(begin<end) {
        int at=q[begin++];
        for(int d=0;d<4;d++) {
            if((at==here && !safe[d]) || avoided(at,d) ||
               (protectedLong() && portalAt[at][d]>=0)) continue;
            int to=terrain[at][d];
            if(to<0 || blocked[to] || first[to]>=0) continue;
            first[to]=at==here?d:first[at];
            if(to==destination) return first[to];
            q[end++]=to;
        }
    }
    return -1;
}
// Simulate the complete route, including tail retention on every visible meal.
// Foreign bodies are fixed for this short prediction; unknown cells remain uncertain.
bool pearlPathSafe(const vector<int>& path) {
    vector<int> trail;
    // Incomplete initial body memory is uncertainty, not proof of an unsafe meal.
    for(Pos p:body) {if(p.x<0) return true;trail.push_back(code(p));}
    array<bool,4096> others{};
    for(const Tile &t:tiles) if(t.id>=0 && t.id!=id) others[code(t.p)]=true;
    array<bool,49> eaten{};
    for(int next:path) {
        if(others[next] || find(trail.begin(),trail.end(),next)!=trail.end()) return false;
        trail.insert(trail.begin(),next);
        int j=local(decodePos(next));bool food=j>=0 && tiles[j].pearl && !eaten[j];
        if(food) eaten[j]=true;else trail.pop_back();
    }
    int target=local(decodePos(trail.front()));
    if(target<0) return false;
    array<bool,49> blocked{};
    for(int j=0;j<49;j++) blocked[j]=others[code(tiles[j].p)];
    for(int key:trail) {int j=local(decodePos(key));if(j>=0) blocked[j]=true;}
    Region terminal=region(target,blocked,false);
    int beforeMeal=int(trail.size())-1;
    bool protect=officialQueen() || (roundNo>=400 && beforeMeal>=8) || (roundNo>=350 && beforeMeal>=16);
    int needed=protect?min(beforeMeal+6,32):beforeMeal>=8?min(beforeMeal+6,24):min(beforeMeal+2,14);
    if(terminal.exits==0 || (!terminal.frontier && terminal.area<needed)) return false;
    if(!protect) return true; // Scouts retain their existing emergency-split option.
    return longContinuation(trail,others,eaten,6,pearlSearchBudget);
}
// Route to a visible pearl through known open edges, even when a wall makes
// its Manhattan distance misleading. The first step must remain safe.
bool guideToPearl(vector<Move>& moves) {
    pearlSearchBudget=256;
    if(moves.empty()) return false;
    for(const Move &m:moves) if(m.to>=0 && tiles[m.to].pearl && !safeMove(m))
        pearlCooldown[code(tiles[m.to].p)]=roundNo+24;
    static array<bool,4096> blocked;blocked.fill(false);
    for(const Tile &t:tiles) if(t.id>=0) blocked[code(t.p)]=true;
    for(Pos p:body) if(p.x>=0) blocked[code(p)]=true;
    int here=code(tiles[24].p);blocked[here]=false;
    static array<int,4096> first,dist,parent;first.fill(-1);dist.fill(-1);parent.fill(-1);
    static array<int,4096> q;int begin=0,end=0,best=-1,bestDist=11;
    q[end++]=here;dist[here]=0;
    while(begin<end) {
        int at=q[begin++];
        if(dist[at]>=bestDist || dist[at]>=10) continue;
        for(int d=0;d<4;d++) {
            if(avoided(at,d) || (protectedLong() && portalAt[at][d]>=0)) continue;
            if(at==here && none_of(moves.begin(),moves.end(),[&](const Move&m){return m.dir==d && safeMove(m);})) continue;
            int to=terrain[at][d];
            if(to<0 || blocked[to] || dist[to]>=0) continue;
            dist[to]=dist[at]+1;first[to]=at==here?d:first[at];parent[to]=at;q[end++]=to;
            int j=local(decodePos(to));
            if(j<0 || !tiles[j].pearl || dist[to]>=bestDist) continue;
            if(reservedPearl(to) || roundNo<pearlCooldown[to]) continue;
            vector<int> path;
            for(int at=to;at!=here;at=parent[at]) path.push_back(at);
            reverse(path.begin(),path.end());
            if(!pearlPathSafe(path)) {pearlCooldown[to]=roundNo+24;continue;}
            best=to;bestDist=dist[to];
        }
    }
    return best>=0 && promote(moves,first[best]);
}
bool guideToResource(vector<Move>& moves) {
    if(recentIncome() || roundNo>=490 || moves.empty()) return false;
    double best=-1e20;int direction=-1;resourceGoal=-1;
    for(auto [key,r]:resources) {
        int dist=distance(tiles[24].p,decodePos(key));
        if(dist<2 || dist>30 || roundNo+dist+6>=500 || reservedPearl(key)) continue;
        int crowd=0;
        for(const Tile &t:tiles) if(t.head && t.team==team && t.id!=id &&
            distance(t.p,decodePos(key))<=3) crowd++;
        if(crowd>=2 && feedTargetId!=id) continue;
        int d=routeDirection(moves,key);
        if(d<0) continue;
        double value=r.score*4.0-dist-crowd*12.0-(roundNo-r.stamp)*0.2;
        if(value>best) {best=value;direction=d;resourceGoal=key;}
    }
    return direction>=0 && promote(moves,direction);
}
void updateCorridor(const Move &m) {
    if(m.to<0 || m.portal) { corridorEntry=-1; return; }
    int from=code(tiles[24].p),to=code(tiles[m.to].p);
    int a=terrainDegree(from),b=terrainDegree(to);
    if(corridorEntry<0 && a>=3 && b<=2) { corridorEntry=from; corridorDir=m.dir; }
    else if(corridorEntry>=0 && (to==corridorEntry || b>=3)) corridorEntry=-1;
}
bool guideCourier(vector<Move>& moves) {
    if(roundNo<=reverseChildUntil) {
        role=3;
        if(promote(moves,bearing)) return true;
        return !moves.empty();
    }
    if(roundNo>courierUntil) return false;
    role=3;
    if(courierEntry>=0) {
        addWarning(courierEntry,courierDir,courierStamp,courierPermanent);
        sendPacket(DANGER,unsigned(courierEntry)|(unsigned(courierDir)<<12)|
            (unsigned(courierPermanent)<<14),courierStamp);
        if(code(tiles[24].p)==courierEntry) { courierUntil=roundNo-1; return false; }
        int d=routeDirection(moves,courierEntry);
        if(d>=0 && promote(moves,d)) return true;
    }
    // Fresh children have no inherited map. In a narrow passage their initial
    // facing points away from their parent, and each bend gives the only free exit.
    if(terrainDegree(code(tiles[24].p))>=3 && courierEntry<0) { courierUntil=roundNo-1; return false; }
    if(promote(moves,facing)) return true;
    return !moves.empty();
}
void prepareEscapeRadio() {
    if(corridorEntry<0 || body.empty() || body.back().x<0) return;
    bool permanent=terrainDegree(code(tiles[24].p))<=1;
    for(int d=0;d<4;d++) if(edge(24,d)!="." && edge(24,d)!="w") permanent=false;
    addWarning(corridorEntry,corridorDir,roundNo,permanent,true);
    unsigned data=unsigned(code(body.back())) | (unsigned(corridorEntry)<<12) |
        (unsigned(corridorDir)<<24) | (unsigned(permanent)<<26);
    sendPacket(ESCAPE,data,roundNo,true);
}

vector<int> visibleBody(int owner,int expected) {
    vector<int> out; int at=visibleHead(owner); array<bool,49> used{};
    if(at<0 || expected>49) return out;
    out.push_back(at); used[at]=true;
    while(int(out.size())<expected) {
        int found=-1;
        for(int j=0;j<49;j++) if(!used[j] && tiles[j].id==owner &&
            nextTile(j,tiles[j].dir,false)==at) { found=j; break; }
        if(found<0) return out; // A visible prefix is enough to plan a partial gift.
        out.push_back(found); used[found]=true; at=found;
    }
    return out;
}
struct Collection { bool safe=false; int closest=-1,dist=1000,count=0; };
Collection collectionPlan(int donor,int receiver,int receiverLength) {
    Collection plan;
    int receiverHead=visibleHead(receiver);
    int donorLength=donor==id?length:(freshAlly(donor)?allies[donor].len:0);
    if(receiverHead<0 || donorLength<2 || donorLength>12) return plan;
    auto parts=visibleBody(donor,donorLength);
    if(donor==id && int(body.size())==length) {
        parts.clear();
        for(Pos p:body) parts.push_back(local(p));
    }
    if(parts.empty()) return plan;
    array<bool,49> blocked{};
    for(int j=0;j<49;j++) blocked[j]=tiles[j].id>=0 && tiles[j].id!=donor;
    Region r=region(receiverHead,blocked,false);
    for(int k=0;k<int(parts.size());k+=2) {
        int at=parts[k];
        if(at<0) continue;
        if(r.dist[at]<0) continue;
        plan.count++;
        if(r.dist[at]<plan.dist) { plan.dist=r.dist[at]; plan.closest=at; }
    }
    int expected=(donorLength+1)/2;
    int required=roundNo<400 && !quietEndgame()?expected:max(1,(expected+1)/2);
    if(plan.closest<0 || plan.dist>5 || plan.count<required ||
       roundNo+plan.dist+plan.count*2+2>=500) return plan;
    if(!r.frontier && r.area<receiverLength+plan.count+3) return plan;
    int turns=0;for(int d=0;d<4;d++) {int n=nextTile(receiverHead,d,false);if(n>=0 && !blocked[n]) turns++;}
    if(turns<2) return plan;
    // Don't donate food in a contested pocket; this is intentionally conservative.
    for(int j=0;j<49;j++) if(tiles[j].head && tiles[j].team!=team &&
        distance(tiles[j].p,tiles[plan.closest].p)<=plan.dist+2) return plan;
    plan.safe=true; return plan;
}
int deliberateDeathDirection() {
    for(int d=0;d<4;d++) if(edge(24,d)=="w") return d;
    for(int d=0;d<4;d++) {
        int to=nextTile(24,d,false);
        if(to>=0 && tiles[to].id==id && !tiles[to].head) return d;
    }
    for(int d=0;d<4;d++) {
        int to=nextTile(24,d,false);
        if(to>=0 && tiles[to].id>=0 && tiles[to].team==team && !tiles[to].head) return d;
    }
    return -1;
}
bool betterRank(int owner,int len) { return len>length || (len==length && owner<id); }
bool approachAlly(vector<Move>& moves,int owner) {
    int h=visibleHead(owner); if(h<0) return false;
    Pos meeting=tiles[h].p;
    int forward=tiles[h].dir,at=code(meeting);
    for(int step=0;step<2;step++) {
        int next=terrain[at][forward];
        if(next<0 || portalAt[at][forward]>=0) break;
        int j=local(decodePos(next));
        if(j>=0 && tiles[j].id>=0) break;
        meeting=decodePos(next);at=next;
    }
    double best=-1e20; int d=-1;
    for(const auto &m:moves) if(m.to>=0 && safeMove(m)) {
        // Prefer delivering in front of the queen's motion, rather than chasing its tail.
        double score=-distance(tiles[m.to].p,meeting)*12.0 + m.score*0.08;
        if((tiles[m.to].p.x==tiles[h].p.x || tiles[m.to].p.y==tiles[h].p.y) &&
           distance(tiles[m.to].p,tiles[h].p)<=3) score+=8;
        if(score>best) { best=score; d=m.dir; }
    }
    return d>=0 && promote(moves,d);
}
bool eligibleDonor(int donorId,int donorLength) {
    if(donorId<=1 || donorLength<2 || donorLength>12) return false;
    if(feedTargetId<=1) {
        // Fixed queen outranks a longer collector. Grow it progressively,
        // preserving most scouts until the collection phase.
        return roundNo>=policy::SupportStart && feedTargetLength>=2 &&
            donorLength<=(quietEndgame()?12:roundNo<330?4:roundNo<400?6:12);
    }
    if(feedTargetLength<=donorLength) return false;
    if(roundNo<400) return donorLength<=(quietEndgame()?12:6) && feedTargetLength>=8;
    return feedTargetLength-donorLength>=3 || (donorLength<=3 && feedTargetLength>=4);
}
bool donationCandidate() {
    // Keep a collector beside the queen until the final collection window.
    if(quietEndgame() && feedTargetId<=1 && roundNo<470 && units<=2) return false;
    return !officialQueen() && feedTargetFresh() && feedTargetId!=id && eligibleDonor(id,length);
}
bool circleMeeting(vector<Move>& moves,int meeting) {
    if(meeting<0) return false;
    int d=-1;double best=-1e20;
    for(const Move &m:moves) if(m.to>=0 && safeMove(m)) {
        double score=m.score*0.12-distance(tiles[m.to].p,decodePos(meeting))*14.0;
        if(score>best) {best=score;d=m.dir;}
    }
    return d>=0 && promote(moves,d);
}
bool donorHasTurn() {
    if(servingUntil>=roundNo && servingDonor>=0) return servingDonor==id;
    int h=visibleHead(feedTargetId);if(h<0) return true;
    int best=id,near=distance(tiles[24].p,tiles[h].p);
    for(const Tile &t:tiles) if(t.head && t.team==team && t.id!=id && t.id!=feedTargetId) {
        auto a=allies.find(t.id);if(a==allies.end() || roundNo-a->second.stamp>4 || !eligibleDonor(t.id,a->second.len) ||
            (a->second.role!=0 && a->second.role!=1 && a->second.role!=6)) continue;
        int dist=distance(t.p,tiles[h].p);
        if(dist<=4 && (dist<near || (dist==near && t.id<best))) {best=t.id;near=dist;}
    }
    return best==id;
}
bool queenAssembly(vector<Move>& moves) {
    if(roundNo<300 || feedTargetId!=id || length<4 || receiveDonor>=0 || moves.empty()) return false;
    if(assemblyCenter<0) {
        array<bool,49> blocked{};for(int j=0;j<49;j++) blocked[j]=tiles[j].id>=0;
        Region room=region(24,blocked,false);
        if(room.area>=min(length+8,28) && room.exits>=2) {
            assemblyCenter=code(tiles[24].p);role=5;return false;
        }
    }
    array<bool,49> blocked{};for(int j=0;j<49;j++) blocked[j]=tiles[j].id>=0;
    Region here=region(24,blocked,false);
    if(here.area>=min(length+8,28) && here.exits>=2) {role=5;return false;}
    auto best=moves.end();double value=-1e20;
    for(auto it=moves.begin();it!=moves.end();++it) if(it->to>=0 && safeMove(*it)) {
        double score=2.0*min(it->area,32)+5.0*min(it->exits,3)+it->score*0.2;
        if(assemblyCenter>=0) score-=4.0*max(0,distance(tiles[it->to].p,decodePos(assemblyCenter))-max(4,length/4+2));
        if(score>value) {value=score;best=it;}
    }
    if(best==moves.end()) return false;
    if(assemblyCenter<0 && best->area>=min(length+8,28) && best->exits>=2) assemblyCenter=code(tiles[best->to].p);
    rotate(moves.begin(),best,best+1);role=5;return true;
}

// Sonar length selects the role; local recovery checks select the sacrifice turn.
bool coordinateDonation(vector<Move>& moves,Move &chosen,string &reason) {
    if(roundNo<(feedTargetId<=1 && feedTargetId>=0?policy::SupportStart:350) || roundNo>=497 || roundNo<=courierUntil ||
       roundNo<=reverseChildUntil) return false;
    if(!feedTargetFresh()) { donationTarget=-1;receiveDonor=-1;return false; }
    if(receiveDonor>=0 && (roundNo>receiveUntil || feedTargetId!=id)) {
        receiveDonor=-1;receivePearls.clear();receiveConfirmed=false;
    }
    for(const Packet &p:received) if(feedTargetId==id && p.type==OFFER && int(p.payload&65535)==id &&
        freshAlly(p.sender) &&
        eligibleDonor(p.sender,allies[p.sender].len) &&
        (receiveDonor<0 || receiveDonor==p.sender)) {
        Collection plan=collectionPlan(p.sender,id,length);
        if(plan.safe && any_of(moves.begin(),moves.end(),safeMove)) {
            if(receiveDonor<0) {
                receiveDonor=p.sender;receiveStarted=roundNo;receiveUntil=roundNo+6;
                receiveConfirmed=false;
            }
            receiveMeeting=code(tiles[plan.closest].p);
            if(!receiveConfirmed) {
                receivePearls.clear();
                auto parts=visibleBody(p.sender,allies[p.sender].len);
                for(int k=0;k<int(parts.size());k+=2) receivePearls.push_back(code(tiles[parts[k]].p));
            }
            sendPacket(ACCEPT,unsigned(p.sender)|(unsigned(p.stamp)<<16),roundNo,true);
        }
    }
    if(receiveDonor>=0) {
        donationTarget=-1;role=2;
        // Refresh actual donor positions while it lives. An OFFER is not a pearl.
        if(freshAlly(receiveDonor)) {
            auto parts=visibleBody(receiveDonor,allies[receiveDonor].len);
            receivePearls.clear();
            for(int k=0;k<int(parts.size());k+=2) receivePearls.push_back(code(tiles[parts[k]].p));
        }
        int best=-1,bestDist=10000;vector<int> actual;
        for(int key:receivePearls) {
            int j=local(decodePos(key));
            if(j<0 || !tiles[j].pearl) continue;
            actual.push_back(key);
            int d=routeDirection(moves,key),dist=distance(tiles[24].p,decodePos(key));
            if(d>=0 && dist<bestDist) {bestDist=dist;best=d;}
        }
        if(!actual.empty()) receiveConfirmed=true;
        if(receiveConfirmed) {
            receivePearls=actual;
            if(best>=0 && promote(moves,best)) {
                reason="RECEIVE_PEARLS";receiveUntil=roundNo+4;return false;
            }
            receiveDonor=-1;receivePearls.clear();receiveConfirmed=false;
        } else if(roundNo-receiveStarted<=5 && circleMeeting(moves,receiveMeeting)) {
            reason="RECEIVE_WAIT";return false;
        } else {
            receiveDonor=-1;receivePearls.clear();receiveConfirmed=false;
        }
        role=0;
    }
    if(!donationCandidate() || roundNo<donorCooldown) {donationTarget=-1;return false;}
    int target=feedTargetId;
    if(!donorHasTurn()) {
        int best=-1;double score=-1e20;
        for(const Move &m:moves) if(m.to>=0 && safeMove(m)) {
            int dist=distance(tiles[m.to].p,feedTargetHead);
            double value=m.score+min(dist,5)*12.0;
            if(value>score) {score=value;best=m.dir;}
        }
        if(best>=0 && promote(moves,best)) {role=6;reason="DONOR_WAIT_CLEAR";}
        return false;
    }
    int travel=distance(tiles[24].p,feedTargetHead);
    if(donorTravelOwner!=target) {donorTravelOwner=target;donorBestDistance=travel;donorProgress=roundNo;}
    if(travel<donorBestDistance) {donorBestDistance=travel;donorProgress=roundNo;}
    if(travel>3 && roundNo-donorProgress>=12) {
        donationTarget=-1;donorTravelOwner=-1;donorCooldown=roundNo+12;return false;
    }
    if(roundNo+travel+(length+1)/2*2+6>=500) {donationTarget=-1;return false;}
    bool nearby=freshAlly(target) && visibleHead(target)>=0;
    Collection plan;
    if(nearby && (allies[target].role==0 || allies[target].role==2 || allies[target].role==5))
        plan=collectionPlan(id,target,allies[target].len);
    if(!nearby || !plan.safe) {
        donationTarget=-1;
        int d=nearby?-1:routeDirection(moves,code(feedTargetHead));
        if(nearby && approachAlly(moves,target)) {role=1;reason="DONOR_SEEK";}
        else {
            if(d<0) {
                int closest=travel;
                for(const Move &m:moves) if(m.to>=0 && safeMove(m)) {
                    int next=distance(tiles[m.to].p,feedTargetHead);
                    if(next<closest) {closest=next;d=m.dir;}
                }
            }
            if(d>=0 && promote(moves,d)) {role=1;reason="DONOR_SEEK";}
        }
        return false;
    }
    // With a live queen two steps away and every dropped pearl reachable, a
    // cheap donor can act immediately. Sonar length is sufficient role consent.
    if((target<=1 || roundNo>=400) && length<=4 && (target<=1 || allies[target].len>=length+3) &&
       roundNo-allies[target].stamp<=2 && travel<=2 && plan.dist<=2 &&
       plan.count>=(length+1)/2) {
        int d=deliberateDeathDirection();
        if(d>=0) {donationTarget=target;chosen.dir=d;role=1;reason="DONATE_DIRECT";return true;}
    }
    if(donationTarget!=target) {
        donationTarget=target;donationTargetSince=roundNo;offerStamp=roundNo;
    } else if(roundNo-offerStamp>4) offerStamp=roundNo;
    role=1;
    for(const Packet &p:received) if(p.type==ACCEPT && p.sender==target &&
        int(p.payload&65535)==id && int((p.payload>>16)&511)==offerStamp && roundNo-p.stamp<=3) {
        int d=deliberateDeathDirection();
        if(d>=0) {chosen.dir=d;reason="DONATE_CONFIRMED";return true;}
    }
    sendPacket(OFFER,unsigned(target),offerStamp,true);
    // A short gift may proceed after repeated missed replies, with a live visible queen.
    if(((target<=1 && roundNo>=330) || roundNo>=410) && length<=6 && allies[target].len>=4 &&
       roundNo-donationTargetSince>=3 && travel<=3) {
        int d=deliberateDeathDirection();
        if(d>=0) {chosen.dir=d;reason="DONATE_NEAR";return true;}
    }
    if(circleMeeting(moves,code(tiles[plan.closest].p))) reason="DONOR_APPROACH";
    return false;
}

void endHunt() {
    huntEnemy=-1;huntCooldown=roundNo+8;huntHeads.clear();
}
bool coordinateHunt(vector<Move>& moves) {
    if(roundNo>=350 || roundNo<huntCooldown || roundNo<=courierUntil ||
       roundNo<=reverseChildUntil || protectedLong() || role!=0 || length>4 || units<4) return false;
    int claimed=-1,leader=65536;Pos reported;
    for(const Packet &p:received) if(p.type==HUNT && roundNo-p.stamp<=3 && freshAlly(p.sender) &&
        allies[p.sender].len<=4 && p.sender<leader) {
        int key=p.payload&4095,enemy=(p.payload>>12)&4095;
        if(!validKey(key)) continue;
        int h=visibleHead(enemy);
        if(h>=0 && tiles[h].team==team) continue;
        leader=p.sender;claimed=enemy;reported=decodePos(key);
    }
    if(claimed>=0 && (huntEnemy<0 || leader<id)) {
        if(huntEnemy!=claimed) {huntEnemy=claimed;huntSince=roundNo;huntProgress=roundNo;huntDistance=1000;huntExits=4;huntHeads.clear();}
    }
    int enemy=visibleHead(huntEnemy);
    if(enemy<0 && huntEnemy>=0 && claimed==huntEnemy) {
        if(roundNo-huntSince>=policy::HuntDuration || roundNo-huntProgress>=policy::HuntStall) {endHunt();return false;}
        int d=routeDirection(moves,code(reported));
        if(d>=0 && promote(moves,d)) {role=4;return true;}
    }
    if(enemy<0 && huntEnemy>=0) {endHunt();return false;}
    if(enemy<0) {
        double best=-1e20;
        for(int j=0;j<49;j++) if(tiles[j].head && tiles[j].team!=team) {
            int observed=0;for(const Tile &t:tiles) observed+=t.id==tiles[j].id;
            double score=min(observed,12)*2.0-distance(tiles[24].p,tiles[j].p)*3.0;
            if(score>best) {best=score;enemy=j;}
        }
        if(enemy<0) return false;
        huntEnemy=tiles[enemy].id;huntSince=roundNo;huntProgress=roundNo;
        huntDistance=1000;huntExits=4;huntHeads.clear();
    }
    int observed=0;for(const Tile &t:tiles) observed+=t.id==huntEnemy;
    bool guard=feedTargetFresh() && feedTargetLength>=8 && distance(tiles[enemy].p,feedTargetHead)<=4;
    if(observed<max(4,length*2) && !guard) {endHunt();return false;}
    if(!guard && !moves.empty() && moves.front().to>=0 && tiles[moves.front().to].pearl) return false;
    vector<pair<int,int>> members; // owner, local head
    if(distance(tiles[24].p,tiles[enemy].p)<=5) members.push_back({id,24});
    for(int j=0;j<49;j++) if(tiles[j].head && tiles[j].team==team && tiles[j].id!=id &&
        distance(tiles[j].p,tiles[enemy].p)<=5) {
        auto it=allies.find(tiles[j].id);
        if(it!=allies.end() && roundNo-it->second.stamp<=5 &&
            (it->second.len>4 || (it->second.role!=0 && it->second.role!=4))) continue;
        members.push_back({tiles[j].id,j});
    }
    sort(members.begin(),members.end(),[&](auto a,auto b){
        int da=distance(tiles[a.second].p,tiles[enemy].p),db=distance(tiles[b.second].p,tiles[enemy].p);
        return da!=db?da<db:a.first<b.first;
    });
    if(members.size()>3) members.resize(3);
    if(none_of(members.begin(),members.end(),[&](auto a){return a.first==id;})) return false;
    sort(members.begin(),members.end());
    if(members.size()<2) {endHunt();return false;}
    vector<pair<int,int>> blocks;
    int exitCount=0;
    for(int d=0;d<4;d++) {
        if(edge(enemy,d)!=".") continue;
        int a=nextTile(enemy,d,false);
        if(a<0 || tiles[a].id>=0) continue;
        exitCount++;
        if(edge(a,d)!=".") continue;
        int b=nextTile(a,d,false);
        if(b>=0 && tiles[b].id<0) blocks.push_back({d,b});
    }
    if(blocks.empty() || int(members.size())<exitCount) {endHunt();return false;}
    array<bool,49> blocked{};for(int j=0;j<49;j++) blocked[j]=tiles[j].id>=0;
    Region routes=region(24,blocked,false);
    set<int> assigned,claimedSlots;int target=-1,slot=-1;bool blocking=false;
    for(const Packet &p:received) if(p.type==HUNT && roundNo-p.stamp<=2 &&
        int((p.payload>>12)&4095)==huntEnemy && (p.payload&(1u<<26)) && p.sender<id) {
        int direction=(p.payload>>24)&3;
        if(any_of(members.begin(),members.end(),[&](auto member){return member.first==p.sender;}) &&
           any_of(blocks.begin(),blocks.end(),[&](auto block){return block.first==direction;})) {
            assigned.insert(p.sender);claimedSlots.insert(direction);
        }
    }
    // Assign the closest available hunter to each exit, with ID breaking ties.
    for(auto [direction,cell]:blocks) {
        if(claimedSlots.count(direction)) continue;
        int owner=-1,best=10000;
        for(auto [candidate,at]:members) if(!assigned.count(candidate)) {
            int cost=distance(tiles[at].p,tiles[cell].p);
            if(cost<best) {best=cost;owner=candidate;}
        }
        if(owner<0) break;
        assigned.insert(owner);
        if(owner==id) {target=cell;slot=direction;blocking=true;}
    }
    // In a corridor, a second hunter stages beside the target rather than abandoning the hunt.
    if(target<0 && members.size()>=2 && blocks.size()==1 && !assigned.count(id)) {
        int best=10000;
        for(int j=0;j<49;j++) if(routes.dist[j]>=1 && routes.dist[j]<=6 && tiles[j].id<0 &&
            j!=blocks[0].second && distance(tiles[j].p,tiles[enemy].p)>=2 && headRisk(j)<18) {
            int score=distance(tiles[j].p,tiles[enemy].p)*4+routes.dist[j];
            if(score<best) {best=score;target=j;slot=(blocks[0].first+1)%4;}
        }
    }
    if(target<0 || routes.dist[target]<0 || routes.dist[target]>6) return false;
    int dist=routes.dist[target];
    if(dist<huntDistance || exitCount<huntExits) {huntProgress=roundNo;huntDistance=dist;huntExits=exitCount;}
    huntHeads.push_back(code(tiles[24].p));if(huntHeads.size()>8) huntHeads.pop_front();
    set<int> visited(huntHeads.begin(),huntHeads.end());
    if(roundNo-huntSince>=policy::HuntDuration || roundNo-huntProgress>=policy::HuntStall ||
       (huntHeads.size()==8 && visited.size()<=3)) {endHunt();return false;}
    int d=routeDirection(moves,code(tiles[target].p));
    if(d<0 || !promote(moves,d)) return false;
    role=4;
    if(huntEnemy<=4095) sendPacket(HUNT,unsigned(code(tiles[enemy].p))|
        (unsigned(huntEnemy)<<12)|(unsigned(slot)<<24)|(unsigned(blocking)<<26));
    return true;
}

bool newbornAmbush() {
    if(length!=2 || turnsAlive!=0 || body.size()!=2) return false;
    if(roundNo<=attackChildUntil) return true;
    int tail=local(body.back());
    if(tail<0) return false;
    for(int d=0;d<4;d++) {
        int to=nextTile(tail,d,false);
        if(to>=0 && tiles[to].team==team && tiles[to].id>=0 && tiles[to].id<id && !tiles[to].head)
            return true;
    }
    return false;
}
int favorableHeadTrade() {
    if(officialQueen()) return -1;
    bool ambush=newbornAmbush() && roundNo<350;
    bool guarding=roundNo>=350 && feedTargetFresh() && feedTargetId!=id && feedTargetLength>=8 &&
        distance(tiles[24].p,feedTargetHead)<=6;
    if(!ambush && ((roundNo>=350 && !guarding) ||
       (length!=2 && !(guarding && length==3)) || units<5 || roundNo<=courierUntil ||
       roundNo<=reverseChildUntil || (role!=0 && !(guarding && (role==1 || role==6))))) return -1;
    for(int d=0;d<4;d++) {
        if(edge(24,d)!=".") continue;
        int target=nextTile(24,d,false);
        if(target<0 || !tiles[target].head || tiles[target].team==team) continue;
        if(guarding && distance(tiles[target].p,feedTargetHead)>3) continue;
        int enemy=tiles[target].id,observed=0;
        for(const Tile &t:tiles) if(t.id==enemy) observed++;
        // The full enemy may extend outside vision, so this is a lower bound.
        if(observed>=(ambush?4:6) && (attackChildEnemy<0 || attackChildEnemy==enemy || !ambush)) return d;
    }
    return -1;
}
void emitRadio() {
    int postLength=int(body.size());
    if(postLength<2) postLength=length;
    Pos head=body.empty()?tiles[24].p:body.front();
    if(head.x<0) head=tiles[24].p;
    rememberAllyReport(id,postLength,head,roundNo);
    Packet status{STATUS,id,roundNo,unsigned(code(head))|(unsigned(min(postLength,4095))<<12)|(unsigned(role)<<24)};
    if(!hasOutgoing && !warnings.empty() && roundNo%3==0) {
        auto it=min_element(warnings.begin(),warnings.end(),[](const auto&a,const auto&b){return a.second.lastSent<b.second.lastSent;});
        if(roundNo-it->second.lastSent>=4) {
            sendPacket(DANGER,unsigned(it->first/4)|(unsigned(it->first%4)<<12)|
                (unsigned(it->second.permanent)<<14),it->second.stamp);
            it->second.lastSent=roundNo;
        }
    }
    Packet queen;bool shareQueen=false;
    if(roundNo<=retiredUntil && (retiredQueen.stamp==roundNo || roundNo%3==1)) {queen=retiredQueen;shareQueen=true;}
    else if(feedTargetFresh() && (feedTargetId<=1 || feedTargetLength>=(roundNo>=350?3:8))) {
        queen={QUEEN,feedTargetId,feedTargetStamp,unsigned(code(feedTargetHead))|(unsigned(min(feedTargetLength,4095))<<12)};
        shareQueen=true;
    }
    Packet info;bool shareInfo=false;
    int resource=-1,oldest=10000;
    for(auto [key,r]:resources) if(roundNo-r.lastSent>=8 && r.lastSent<oldest) {resource=key;oldest=r.lastSent;}
    if(resource>=0 && (roundNo%2==0 || sharedPortalExit.empty())) {
        auto &r=resources[resource];
        info={PORTAL_ROUTE_PACKET,id,r.stamp,unsigned(resource)|(unsigned(r.score)<<12)|
            (unsigned(r.fountain)<<16)|(1u<<26)};
        r.lastSent=roundNo;shareInfo=true;
    } else {
        int key=-1;oldest=10000;
        for(auto [k,exit]:sharedPortalExit) {
            (void)exit;int sent=portalRouteLastSent.count(k)?portalRouteLastSent[k]:-100;
            if(roundNo-sent>=8 && sent<oldest) {oldest=sent;key=k;}
        }
        if(key>=0) {
            info={PORTAL_ROUTE_PACKET,id,roundNo,unsigned(key/4)|(unsigned(key%4)<<12)|
                (unsigned(sharedPortalExit[key])<<14)};
            portalRouteLastSent[key]=roundNo;shareInfo=true;
        }
    }
    int aligned=-1;
    if(hasOutgoing && (outgoing.type==OFFER || outgoing.type==ACCEPT)) {
        int owner=outgoing.type==OFFER?donationTarget:receiveDonor,at=visibleHead(owner);
        if(at>=0) {
            Pos other=tiles[at].p;
            if(head.x==other.x && head.y!=other.y)
                aligned=wrap(other.y-head.y,height)<=wrap(head.y-other.y,height)?2:0;
            else if(head.y==other.y && head.x!=other.x)
                aligned=wrap(other.x-head.x,width)<=wrap(head.x-other.x,width)?1:3;
        }
    }
    int queenDir=(roundNo+id)%4;
    if(queenDir==aligned) queenDir=(queenDir+1)%4;
    int infoDir=(queenDir+1)%4;
    if(infoDir==aligned) infoDir=(infoDir+1)%4;
    Packet primary=outgoing;bool tactic=hasOutgoing;
    if(!hasOutgoing && shareQueen) {outgoing=queen;hasOutgoing=true;queenLastSent=roundNo;}
    else if(!hasOutgoing && shareInfo) {outgoing=info;hasOutgoing=true;}
    for(int d=0;d<4;d++) {
        Packet packet=status;
        if(tactic && (primary.type==ESCAPE || attackSplitRadio || d==aligned || ((roundNo+d)%2==0))) packet=primary;
        // Give length/election reports a channel even while warnings or offers are active.
        if(shareQueen && d==queenDir && (primary.type!=ESCAPE || roundNo<=retiredUntil)) packet=queen;
        if(shareInfo && d==infoDir && (!tactic || (primary.type!=ESCAPE && d!=aligned))) packet=info;
        cout<<"SONAR "<<DIRS[d]<<' '<<encodePacket(packet)<<'\n';
    }
}

bool trappedAllyAtTail() {
    if(length!=2 || body.size()!=2) return false;
    int tail=local(body[1]);
    if(tail<0) return false;
    for(int j=0;j<49;j++) if(tiles[j].head && tiles[j].team==team && tiles[j].id<id) {
        bool tailExit=false,freeExit=false,unknownExit=false;
        for(int d=0;d<4;d++) {
            int to=nextTile(j,d,false);
            if(to==tail) tailExit=true;
            if(to>=0 && tiles[to].id<0) {
                array<bool,49> blocked{};
                for(int k=0;k<49;k++) blocked[k]=tiles[k].id>=0;
                Region r=region(to,blocked);
                if(r.area>1 || r.frontier) freeExit=true;
            }
            if(to<0 && edge(j,d)!="w") unknownExit=true;
        }
        if(tailExit && !freeExit && !unknownExit) return true;
    }
    return false;
}

bool cutCanReleaseParent() {
    if(length<4 || int(body.size())!=length) return false;
    int released=local(body[length-2]); // New child's tail, NOT its new head.
    if(released<0) return false;
    for(int d=0;d<4;d++) if(nextTile(24,d)==released) return true;
    return false;
}

bool reverseRescue(const vector<Move>& moves) {
    if(length<6 || units>=unitLimit || int(body.size())!=length ||
       body.back().x<0 || cutCanReleaseParent()) return false;
    // A merely narrow but still open route is not a reason to shed two segments.
    for(const Move &m:moves) if(m.sustainable && m.exits>0 && m.risk<18) return false;
    bool ownWall=false;
    for(int d=0;d<4;d++) {
        int to=nextTile(24,d,false);
        if(to>=0 && tiles[to].id==id && !tiles[to].head) ownWall=true;
    }
    if(!ownWall) return false;
    int tail=local(body.back());
    if(tail<0) {
        if(!protectedLong()) return false;
        int key=code(body.back());
        array<bool,4096> occupied{};
        for(Pos p:body) if(p.x>=0) occupied[code(p)]=true;
        for(const Tile &t:tiles) if(t.id>=0) occupied[code(t.p)]=true;
        // A trapped queen may reverse its whole rear using previously observed terrain.
        // Remote occupancy remains uncertain; this is preferable to repeated blind cuts.
        for(int d=0;d<4;d++) {
            int to=terrain[key][d];
            if(to<0 || portalAt[key][d]>=0 || occupied[to] || avoided(key,d)) continue;
            int room=0;array<bool,4096> seen{};vector<int> q{to};seen[to]=true;
            for(size_t n=0;n<q.size() && room<min(length+2,24);n++) {
                int at=q[n];room++;
                for(int e=0;e<4;e++) {
                    int next=terrain[at][e];
                    if(next<0 || portalAt[at][e]>=0 || occupied[next] || seen[next]) continue;
                    seen[next]=true;q.push_back(next);
                }
            }
            if(room>=min(length+2,24)) return true;
        }
        return false;
    }
    // The reversed child's head is the old tail. Verify its first step, including
    // occupancy and an exit beyond that step, before spending a split action.
    for(int d=0;d<4;d++) {
        if(edge(tail,d)!=".") continue;
        int to=nextTile(tail,d,false);
        if(to<0 || tiles[to].id>=0 || headRisk(to)>=18) continue;
        Region r=region(to,afterMove(to),false);
        if(r.frontier || r.area>=min(length+2,20)) return true;
    }
    return false;
}
bool pocketEscapeSplit(const vector<Move>& moves) {
    if(officialQueen()) return false;
    if(length<5 || length>8 || units>=unitLimit || corridorEntry<0 ||
       int(body.size())!=length || body.back().x<0 ||
       distance(body.back(),decodePos(corridorEntry))>1) return false;
    for(const Move &m:moves) if(m.sustainable && m.exits>0 && m.risk<18) return false;
    int tail=local(body.back()),key=code(body.back());
    for(int d=0;d<4;d++) {
        int to=tail>=0?nextTile(tail,d,false):-1;
        if(tail>=0 && edge(tail,d)=="." && to>=0 &&
           tiles[to].id<0 && headRisk(to)<18) return true;
        if(tail>=0 || portalAt[key][d]>=0) continue;
        int next=terrain[key][d];
        if(next<0 || lastSeen[next]<0 || roundNo-lastSeen[next]>6) continue;
        if(find_if(body.begin(),body.end(),[&](Pos p){return p.x>=0 && code(p)==next;})==body.end())
            return true;
    }
    return false;
}

int emergencyPortalDirection() {
    if(protectedLong()) return -1;
    int best=-1,bestRank=-1000;
    for(int d=0;d<4;d++) {
        const string &e=edge(24,d);
        if(e=="." || e=="w") continue;
        int to=nextTile(24,d,false);
        if(to>=0 && tiles[to].id>=0) continue;
        int portalId=stoi(e);
        int destination=portalDestination(code(tiles[24].p),d,portalId);
        int rank=(portalReady(portalId)?20:0)+(destination>=0?10:0);
        if(destination>=0) rank-=min(visits[destination],10)*2;
        if(rank>bestRank) { bestRank=rank; best=d; }
    }
    return best;
}

bool routineSplit(const vector<Move>& moves) {
    if(officialQueen() || length<4 || units>=unitLimit-4 ||
       roundNo-lastSplit<policy::SplitCooldown || moves.empty()) return false;
    // Productive small collectors can keep scouting in the middle game;
    // length-seven and larger contenders preserve their growth.
    if(roundNo>=300 || (roundNo>=policy::ExpansionEnd && length>6)) return false;
    if(roundNo>=400 && (length!=4 || units>=6 || !recentIncome(16))) return false;
    if(roundNo>=policy::ExpansionEnd && !recentIncome(16)) return false;
    int cap=min(unitLimit,max(4,width*height/20));
    if(roundNo>=300 && units>=min(cap,max(12,width*height/30))) return false;
    if(roundNo>=policy::ExpansionEnd && length>4) {
        int income=0;
        for(int at:foodRounds) income+=roundNo-at<=32;
        int visiblePearls=0;
        for(const Tile &t:tiles) visiblePearls+=t.pearl;
        if(income<2 && visiblePearls<2) return false;
    }
    if(length==4) {
        int visiblePearls=0;
        for(const Tile &t:tiles) visiblePearls+=t.pearl;
        bool wideExpansion=width*height>=768 && roundNo<200 && units<12;
        if(!wideExpansion && !recentIncome(12) && visiblePearls<2) return false;
    }
    if(units>=cap || headRisk(24)>=18) return false;
    const Move &m=moves.front();
    if(m.exits==0 || m.risk>=18 || (!m.frontier && m.area<8)) return false;
    set<int> heads;
    for(const auto &t:tiles) if(t.head && t.team==team && t.id!=id && distance(t.p,tiles[24].p)<=3) heads.insert(t.id);
    if(!heads.empty()) return false; // Spread first; split after leaving the cluster.
    if(!body.empty() && body.back().x>=0) {
        if(length>4 && distance(body.back(),tiles[24].p)<=2) return false;
        int tail=local(body.back());
        if(tail>=0) {
            bool childCanLeave=false;
            for(int d=0;d<4;d++) {
                int to=nextTile(tail,d);
                if(to>=0 && tiles[to].id<0) childCanLeave=true;
            }
            if(!childCanLeave) return false;
        }
    }
    return true;
}

int tailAmbushEnemy(const vector<Move>& moves) {
    if(roundNo>=350 || length<4 || units>=unitLimit-4 || protectedLong() ||
       role==1 || role==2 || role==3 || roundNo-lastSplit<policy::SplitCooldown ||
       int(body.size())!=length || body.back().x<0 ||
       !any_of(moves.begin(),moves.end(),safeMove)) return -1;
    Pos tail=body.back(),before=body[length-2];
    if(distance(tail,before)!=1) return -1; // No ambush across a portal body link.
    int target=-1,best=3;
    for(const Tile &t:tiles) if(t.head && t.team!=team && t.id>=0) {
        Pos predicted=t.p;
        if(t.id>id) {
            int key=code(t.p),next=terrain[key][t.dir];
            if(next<0 || portalAt[key][t.dir]>=0) continue;
            int j=local(decodePos(next));
            if(j>=0 && tiles[j].id>=0) continue;
            predicted=decodePos(next);
        }
        if(distance(tail,predicted)!=1) continue;
        int observed=0;for(const Tile &part:tiles) observed+=part.id==t.id;
        if(observed>best) {best=observed;target=t.id;}
    }
    return target;
}

void recordMove(const Move &m) {
    previousEntry=code(tiles[24].p);previousDirection=m.dir;
    updateCorridor(m);
    if(m.portal) {
        portalUses++;
        portalLastUsed[m.portalId]=roundNo;
        lastPortalEntry=code(tiles[24].p);
        lastPortalDir=m.dir;
        lastPortalId=m.portalId;
        lastPortalRound=roundNo;
    }
    if(m.to<0) { bearing=m.dir; body.clear(); return; }
    int oldForward=nextTile(24,bearing);
    // A sustained obstacle, explored strip, or nearby ally gives the next heading.
    if(m.dir!=bearing && (oldForward<0 || tiles[oldForward].id>=0 ||
        visits[code(tiles[oldForward].p)]>0 || separation(m.to)>separation(oldForward)+3)) bearing=m.dir;
    body.insert(body.begin(),tiles[m.to].p);
    if(!tiles[m.to].pearl) body.pop_back();
    else {
        foodRounds.push_back(roundNo);
        int key=code(tiles[m.to].p);
        if(roundNo-harvestRound[key]>24) harvestCount[key]=0;
        harvestRound[key]=roundNo;harvestCount[key]++;
        if(tiles[m.to].countdown>=0 && harvestCount[key]>=2)
            rememberResource(key,min(14,harvestCount[key]*3),roundNo,false);
    }
    while(!foodRounds.empty() && roundNo-foodRounds.front()>32) foodRounds.pop_front();
}

void observePortalArrival() {
    if(lastPortalEntry<0 || roundNo!=lastPortalRound+1) return;
    int ordinary=0,otherPortals=0;
    for(int d=0;d<4;d++) {
        const string &e=edge(24,d);
        ordinary+=e==".";
        otherPortals+=e!="." && e!="w" && stoi(e)!=lastPortalId;
    }
    int key=lastPortalEntry*4+lastPortalDir;
    if(ordinary==0 && otherPortals==0) {
        addWarning(lastPortalEntry,lastPortalDir,roundNo,true,true);
        sendPacket(DANGER,unsigned(lastPortalEntry)|
            (unsigned(lastPortalDir)<<12)|(1u<<14),roundNo,true);
    } else if(sharedPortalExit.size()<128) {
        int exit=code(tiles[24].p);
        sharedPortalExit[key]=exit;
        terrain[lastPortalEntry][lastPortalDir]=exit;
        portalRouteLastSent[key]=roundNo;
        sendPacket(PORTAL_ROUTE_PACKET,unsigned(lastPortalEntry)|
            (unsigned(lastPortalDir)<<12)|(unsigned(exit)<<14),roundNo,true);
    }
    lastPortalEntry=-1;
}

bool readTurn() {
    string tag; if(!(cin>>tag) || tag=="ENDGAME") return false;
    if(tag!="ROUND") return false;
    char f; int messages=0;
    if(!(cin>>roundNo>>tag>>f>>tag>>length>>tag>>units>>tag>>messages)) return false;
    facing=dirOf(f); inbox.clear();
    for(int i=0;i<messages;i++) { unsigned long long raw; if(!(cin>>raw)) return false; inbox.push_back(raw); }
    string first; if(!(cin>>first)) return false;
    if(first=="ECHOES") { int ignored; for(int i=0;i<5;i++) cin>>ignored; cin>>first; }
    localAt.fill(-1);
    for(int j=0;j<49;j++) {
        tiles[j]=Tile{};
        if(j==0) tiles[j].p.x=stoi(first); else cin>>tiles[j].p.x;
        cin>>tiles[j].p.y>>tiles[j].pearl>>tiles[j].countdown;
        localAt[code(tiles[j].p)]=j; lastSeen[code(tiles[j].p)]=roundNo;
    }
    int parts=0; cin>>tag>>parts;
    for(int i=0;i<parts;i++) {
        char t,d; int owner,x,y,head; cin>>t>>owner>>x>>y>>d>>head;
        int j=local({x,y});
        if(j>=0) { tiles[j].team=t; tiles[j].id=owner; tiles[j].head=head; tiles[j].dir=dirOf(d); }
    }
    for(int r=0;r<8;r++) for(int c=0;c<7;c++) {
        cin>>he[r][c];
        if(he[r][c]!="." && he[r][c]!="w") {
            Pos p=tiles[min(r,6)*7+c].p; p.y+=(r==7); remember(stoi(he[r][c]),true,p);
        }
    }
    for(int r=0;r<7;r++) for(int c=0;c<8;c++) {
        cin>>ve[r][c];
        if(ve[r][c]!="." && ve[r][c]!="w") {
            Pos p=tiles[r*7+min(c,6)].p; p.x+=(c==7); remember(stoi(ve[r][c]),false,p);
        }
    }
    if(!cin) return false;
    for(const Tile &t:tiles) if(t.id>=0 && t.team!=team) lastEnemySeen=roundNo;
    syncBody();
    rememberTerrain();
    receiveRadio();
    if(bearing<0) { bearing=facing; hand=((unsigned(id)*2654435761u+unsigned(width*height))&1)?1:-1; }
    return true;
}

int main() {
    // Match the official helper: buffer all output, then flush once per turn.
    static char outputBuffer[8192]; setvbuf(stdout,outputBuffer,_IOFBF,sizeof(outputBuffer));
    cin.tie(nullptr);
    string tag;
    if(!(cin>>tag>>id) || tag!="ID") return 0;
    if(!(cin>>tag>>team>>tag>>width>>height>>tag>>unitLimit)) return 0;
    lastVisit.fill(-10000); lastSeen.fill(-1); localAt.fill(-1); pearlSeen.fill(-1);
    spawnOneRound.fill(-100);harvestRound.fill(-100);
    for(auto &row:terrain) row.fill(-2);
    for(auto &row:portalAt) row.fill(-1);
    while(readTurn()) {
        hasOutgoing=false; role=0;attackSplitRadio=false;
        observePortalArrival();
        if(turnsAlive==0 && length==2 && courierEntry<0 && terrainDegree(code(tiles[24].p))<=2) courierUntil=roundNo+16;
        int headKey=code(tiles[24].p); visits[headKey]++; lastVisit[headKey]=roundNo;
        auto moves=legalMoves();
        bool guided=guideOutOfLoop(moves);
        bool courier=guideCourier(moves);
        bool escapeLocked=guided && turnsAlive<escapeLockUntil;
        bool foodRoute=!courier && !escapeLocked && guideToPearl(moves);
        bool resourceRoute=!courier && !escapeLocked && !foodRoute && guideToResource(moves);
        bool pocket=!courier && !escapeLocked && profitablePocketMove(moves);
        int splitSize=0;
        Move chosen; string reason=courier?"COURIER_ESCAPE":pocket?"PROFITABLE_POCKET":foodRoute?"PEARL_ROUTE":resourceRoute?"RESOURCE_ROUTE":guided?"NAVIGATE":"EXPLORE";
        if(!courier && queenAssembly(moves)) reason="QUEEN_ASSEMBLY";
        bool donated=coordinateDonation(moves,chosen,reason);
        if(!donated && chosen.dir<0) {
            int trade=favorableHeadTrade();
            if(trade>=0) { chosen.dir=trade; reason=newbornAmbush()?"TAIL_AMBUSH_TRADE":
                roundNo>=350?"QUEEN_GUARD_TRADE":"FAVORABLE_HEAD_TRADE"; }
        }
        if(!donated && chosen.dir<0 && coordinateHunt(moves)) reason="HUNT_BLOCK";
        // A two-segment child releases its tail for a trapped parent. If eating would
        // retain the blocking tail, take a non-food move; otherwise sacrifice ONLY the child.
        if(!donated && !officialQueen() && trappedAllyAtTail()) {
            auto it=find_if(moves.begin(),moves.end(),[](const Move&m){return m.to>=0 && !tiles[m.to].pearl;});
            if(it!=moves.end()) { chosen=*it; reason="RELEASE_TAIL"; }
            else {
                int tail=local(body.back());
                for(int d=0;d<4;d++) if(nextTile(24,d,false)==tail) { chosen.dir=d; break; }
                reason="SACRIFICE_TWO";
            }
        }
        if(chosen.dir<0) {
            bool canSplit=length>=4 && units<unitLimit;
            if(canSplit && pocketEscapeSplit(moves)) {
                splitSize=3;reason="POCKET_ESCAPE_3";
            } else if(canSplit && reverseRescue(moves)) {
                splitSize=length-2; reason="REVERSE_RESCUE";
            } else if(moves.empty() && emergencyPortalDirection()>=0) {
                chosen.dir=emergencyPortalDirection(); chosen.portal=true;
                chosen.portalId=stoi(edge(24,chosen.dir)); reason="LAST_RESORT_PORTAL";
            } else if(canSplit && ((moves.empty() && !protectedLong()) ||
                ((moves.empty() || moves.front().exits==0) && cutCanReleaseParent()))) {
                splitSize=2; reason=cutCanReleaseParent()?"RESCUE_SPLIT_2":"LAST_RESORT_SPLIT_2";
            } else if(int enemy=tailAmbushEnemy(moves);!courier && enemy>=0) {
                splitSize=2;reason="TAIL_AMBUSH_SPLIT";
                if(enemy<=4095) {
                    sendPacket(HUNT,unsigned(code(body.back()))|(unsigned(enemy)<<12));
                    attackSplitRadio=true;
                }
            } else if(!courier && role==0 && routineSplit(moves)) {
                splitSize=2; reason="SCOUT_SPLIT_2";
            } else if(!moves.empty()) {
                chosen=moves.front();
                if(chosen.uncertain && reason=="EXPLORE")
                    reason=portalDestination(code(tiles[24].p),chosen.dir,chosen.portalId)>=0?
                        "PORTAL_ROUTE":"PORTAL_SCOUT";
            }
            else {
                // Unknown exits are used only when every verified move is impossible.
                for(int d=0;d<4;d++) if(!protectedLong() && edge(24,d)!="." &&
                    edge(24,d)!="w") {
                    chosen.dir=d; chosen.portal=true; chosen.portalId=stoi(edge(24,d));
                    reason="LAST_RESORT_PORTAL"; break;
                }
                if(chosen.dir<0) {
                    // Never take an ally's head down with us when escape is impossible.
                    int bestLoss=100;
                    for(int d=0;d<4;d++) {
                        int to=nextTile(24,d,false),loss=100;
                        if(to>=0 && tiles[to].id==id) loss=0;
                        else if(edge(24,d)=="w") loss=1;
                        else if(to>=0 && tiles[to].id>=0 && !tiles[to].head) loss=2;
                        else if(to>=0 && tiles[to].head && tiles[to].team!=team) loss=3;
                        if(loss<bestLoss) { bestLoss=loss; chosen.dir=d; }
                    }
                    if(chosen.dir<0) chosen.dir=(facing+2)%4;
                    reason="NO_LEGAL_ESCAPE";
                }
            }
        }
        if(splitSize) {
            if(protectedLong() && previousEntry>=0 && previousDirection>=0)
                queenBadEdges[previousEntry*4+previousDirection]=roundNo+80;
            if(reason=="REVERSE_RESCUE" && feedTargetId==id && length-splitSize==2 && splitSize>=6) {
                unsigned data=unsigned(code(body.back()))|(4095u<<12);
                sendPacket(ESCAPE,data,roundNo,true);
            } else if(reason!="SCOUT_SPLIT_2" && reason!="TAIL_AMBUSH_SPLIT") prepareEscapeRadio();
            cout<<"SPLIT "<<splitSize<<'\n';
            body.resize(length-splitSize); lastSplit=roundNo;
        } else {
            cout<<"MOVE "<<DIRS[chosen.dir]<<'\n';
            if(chosen.to>=0 || chosen.portal) recordMove(chosen);
        }
        emitRadio();
        cout<<"INDICATOR v10 "<<reason<<" bearing="<<DIRS[bearing]<<" goal="<<goal<<" peer="<<donationTarget<<" target="<<feedTargetId<<" queen="<<officialQueenId<<" enemy="<<huntEnemy<<" entry="<<corridorEntry<<" resource="<<resourceGoal<<" official="<<officialQueenId<<" queen_dead="<<officialQueenDeath<<" quiet="<<quietEndgame()<<"\nPROTOCOL 3\nENDTURN\n"<<flush;
        turnsAlive++;
    }
    return 0;
}

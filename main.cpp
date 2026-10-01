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

// Explorer v2. Each process controls exactly one dragon.
// Policy invariants: MOVE has ONE direction; every SPLIT is exactly 2.
namespace policy {
constexpr int SplitSize = 2;
constexpr int SplitAtLength = 5;
constexpr int ExpansionEnd = 300;
constexpr int SplitCooldown = 3;
constexpr int PortalLimit = 2;
}
constexpr int DR[4]={-1,0,1,0}, DC[4]={0,1,0,-1};
constexpr char DIRS[]="NESW";
struct Pos { int x=-1,y=-1; bool operator==(const Pos&) const=default; };
struct Tile { Pos p; int pearl=0, countdown=-1, id=-1; char team='.'; int dir=0; bool head=false; };
struct Portal { bool horizontal; Pos p; bool operator==(const Portal&) const=default; };
struct Move { int dir=-1,to=-1,area=0,exits=0; bool portal=false,frontier=false; double score=-1e20,risk=0; };
static array<Tile,49> tiles;
static string he[8][7],ve[7][8];
static map<int,vector<Portal>> portalPairs;
static array<int,4096> visits{}, lastVisit{}, lastSeen{}, localAt{};
static vector<Pos> body; // Head to tail; unknown initial segments have x=-1.
static int width,height,id,unitLimit,roundNo,length,units,facing;
static int bearing=-1,hand=1,turnsAlive=0,lastSplit=-100,portalUses=0;
static char team;

int wrap(int n,int m) { return (n%m+m)%m; }
int code(Pos p) { return p.y*64+p.x; }
int dirOf(char c) { for(int d=0;d<4;d++) if(DIRS[d]==c) return d; return 0; }
int local(Pos p) { return p.x<0?-1:localAt[code(p)]; }
int distance(Pos a,Pos b) {
    int x=abs(a.x-b.x),y=abs(a.y-b.y);
    return min(x,width-x)+min(y,height-y);
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
// Physical transition, independent of occupancy. Portal exit must be in current vision.
int nextTile(int at,int d,bool respectQuota=true) {
    const string &e=edge(at,d);
    if(e=="w") return -1;
    Pos p=tiles[at].p;
    if(e==".") return local({wrap(p.x+DC[d],width),wrap(p.y+DR[d],height)});
    if(respectQuota && portalUses>=policy::PortalLimit) return -1;
    auto it=portalPairs.find(stoi(e));
    if(it==portalPairs.end() || it->second.size()!=2) return -1;
    Portal here{d%2==0,{wrap(p.x+(d==1),width),wrap(p.y+(d==2),height)}};
    const auto &pair=it->second;
    if(!(here==pair[0]) && !(here==pair[1])) return -1;
    Pos exit=pair[here==pair[0]?1:0].p;
    exit.x=wrap(exit.x-(d==3),width); exit.y=wrap(exit.y-(d==0),height);
    return local(exit);
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
Region region(int start,const array<bool,49>& blocked) {
    Region out; out.dist.fill(-1); out.dist[start]=0;
    array<int,49> queue{}; int head=0,tail=0; queue[tail++]=start;
    while(head<tail) {
        int at=queue[head++]; out.area++;
        for(int d=0;d<4;d++) {
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
        if(tiles[j].team!=team) risk+=one?120.0:two?8.0:0.0;
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
    for(auto [ally,p]:distances) {
        (void)ally;
        score-=18.0/(p.second+0.5);
        score+=4.0*clamp(p.second-p.first,-1,1);
    }
    return score;
}

Move evaluate(int d,int to) {
    Move m; m.dir=d; m.to=to; m.portal=edge(24,d)!=".";
    Region r=region(to,afterMove(to));
    m.area=r.area; m.exits=r.exits; m.frontier=r.frontier;
    int nextLength=length+tiles[to].pearl;
    m.risk=headRisk(to);
    double s=0.4*min(r.area,20)-m.risk+separation(to);
    if(r.exits==0) s-=10000; // Avoid eating a pearl which closes our final exit.
    if(!r.frontier && r.area<min(nextLength+2,14)) s-=12*(min(nextLength+2,14)-r.area);
    if(d==bearing) s+=8;
    if(d==facing) s+=3;
    if(turnsAlive<3 && length==2 && d==bearing) s+=5;
    int k=code(tiles[to].p);
    // A completed toroidal lap is still a loop even when it took more than 16 turns.
    // Prefer an unvisited adjacent strip over the heading bonus on the first return.
    if(visits[k]>0) s-=14+min(visits[k],12)*4.0;
    if(lastVisit[k]>=0) s-=max(0,40-(roundNo-lastVisit[k]))*0.6;
    int novelty=0;
    for(int dy=-3;dy<=3;dy++) for(int dx=-3;dx<=3;dx++) {
        Pos p{wrap(tiles[to].p.x+dx,width),wrap(tiles[to].p.y+dy,height)};
        if(lastSeen[code(p)]<0) novelty++;
    }
    s+=0.22*novelty;
    s+=tiles[to].pearl*(roundNo<policy::ExpansionEnd?18:24);
    // Distances respect walls and body occupancy, unlike straight-line pearl attraction.
    double food=0;
    for(int j=0;j<49;j++) if(j!=to && r.dist[j]>0 && tiles[j].pearl)
        food=max(food,12.0/(r.dist[j]+1));
    s+=food;
    if(m.portal) s-=4;
    m.score=s;
    return m;
}

vector<Move> legalMoves() {
    vector<Move> out;
    // Stable tie order is individual to each dragon, never reseeded every round.
    array<int,4> order{bearing,(bearing+hand+4)%4,(bearing-hand+4)%4,(bearing+2)%4};
    for(int d:order) {
        int to=nextTile(24,d);
        if(to>=0 && tiles[to].id<0) out.push_back(evaluate(d,to));
    }
    stable_sort(out.begin(),out.end(),[](const Move&a,const Move&b){return a.score>b.score;});
    return out;
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

bool routineSplit(const vector<Move>& moves) {
    if(roundNo>=policy::ExpansionEnd || length<policy::SplitAtLength || units>=unitLimit ||
       roundNo-lastSplit<policy::SplitCooldown || moves.empty()) return false;
    int cap=min(unitLimit,max(4,width*height/20));
    if(units>=cap || headRisk(24)>=18) return false;
    const Move &m=moves.front();
    if(m.exits==0 || m.risk>=18 || (!m.frontier && m.area<8)) return false;
    set<int> heads;
    for(const auto &t:tiles) if(t.head && t.team==team && t.id!=id && distance(t.p,tiles[24].p)<=3) heads.insert(t.id);
    if(!heads.empty()) return false; // Spread first; split after leaving the cluster.
    if(!body.empty() && body.back().x>=0) {
        if(distance(body.back(),tiles[24].p)<=2) return false;
        int tail=local(body.back());
        if(tail>=0) {
            bool childCanLeave=false;
            for(int d=0;d<4;d++) { int to=nextTile(tail,d); if(to>=0 && tiles[to].id<0) childCanLeave=true; }
            if(!childCanLeave) return false;
        }
    }
    return true;
}

void recordMove(const Move &m) {
    int oldForward=nextTile(24,bearing);
    // A sustained obstacle, explored strip, or nearby ally gives the next heading.
    if(m.dir!=bearing && (oldForward<0 || tiles[oldForward].id>=0 ||
        visits[code(tiles[oldForward].p)]>0 || separation(m.to)>separation(oldForward)+3)) bearing=m.dir;
    body.insert(body.begin(),tiles[m.to].p);
    if(!tiles[m.to].pearl) body.pop_back();
    if(m.portal) portalUses++;
}

bool readTurn() {
    string tag; if(!(cin>>tag) || tag=="ENDGAME") return false;
    if(tag!="ROUND") return false;
    char f; int messages=0;
    if(!(cin>>roundNo>>tag>>f>>tag>>length>>tag>>units>>tag>>messages)) return false;
    facing=dirOf(f);
    for(int i=0;i<messages;i++) { unsigned long long ignored; if(!(cin>>ignored)) return false; }
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
    syncBody();
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
    lastVisit.fill(-10000); lastSeen.fill(-1); localAt.fill(-1);
    while(readTurn()) {
        int headKey=code(tiles[24].p); visits[headKey]++; lastVisit[headKey]=roundNo;
        auto moves=legalMoves();
        bool split=false;
        Move chosen; string reason="EXPLORE";
        // A two-segment child releases its tail for a trapped parent. If eating would
        // retain the blocking tail, take a non-food move; otherwise sacrifice ONLY the child.
        if(trappedAllyAtTail()) {
            auto it=find_if(moves.begin(),moves.end(),[](const Move&m){return !tiles[m.to].pearl;});
            if(it!=moves.end()) { chosen=*it; reason="RELEASE_TAIL"; }
            else {
                int tail=local(body.back());
                for(int d=0;d<4;d++) if(nextTile(24,d,false)==tail) { chosen.dir=d; break; }
                reason="SACRIFICE_TWO";
            }
        }
        if(chosen.dir<0) {
            bool canSplit=length>=4 && units<unitLimit;
            if(canSplit && (moves.empty() || (moves.front().exits==0 && cutCanReleaseParent()))) {
                split=true; reason=cutCanReleaseParent()?"RESCUE_SPLIT_2":"LAST_RESORT_SPLIT_2";
            } else if(routineSplit(moves)) {
                split=true; reason="SCOUT_SPLIT_2";
            } else if(!moves.empty()) chosen=moves.front();
            else {
                // Unknown exits are used only when every verified move is impossible.
                for(int d=0;d<4;d++) if(edge(24,d)!="." && edge(24,d)!="w" && portalUses<policy::PortalLimit) {
                    chosen.dir=d; portalUses++; reason="LAST_RESORT_PORTAL"; body.clear(); break;
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
        if(split) {
            cout<<"SPLIT 2\n";
            body.resize(length-policy::SplitSize); lastSplit=roundNo;
        } else {
            cout<<"MOVE "<<DIRS[chosen.dir]<<'\n';
            if(chosen.to>=0) recordMove(chosen);
        }
        cout<<"INDICATOR v2 "<<reason<<" bearing="<<DIRS[bearing]<<"\nPROTOCOL 3\nENDTURN\n"<<flush;
        turnsAlive++;
    }
    return 0;
}

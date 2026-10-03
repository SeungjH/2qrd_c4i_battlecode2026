#pragma once
#include "bot.hpp"

namespace battlecode {

// One legal move puts our new head at 'to'; its old tail vacates only if no pearl is eaten.
inline array<bool,49> Bot::afterMove(int to) {
    array<bool,49> blocked{};
    for(int j=0;j<49;j++) blocked[j]=tiles[j].id>=0;
    if(!tiles[to].pearl && !body.empty()) {
        int tail=local(body.back());
        if(tail>=0 && tiles[tail].id==id) blocked[tail]=false;
    }
    blocked[to]=true;
    return blocked;
}

inline Region Bot::region(int start,const array<bool,49>& blocked,bool portals) {
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

inline double Bot::headRisk(int target) {
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

inline double Bot::separation(int target) {
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

inline bool Bot::obstructsQueen(int target) {
    if(officialQueen() || target<0) return false;
    int h=-1;
    for(int j=0;j<49;j++) if(tiles[j].head && tiles[j].team==team && tiles[j].id>=0 && tiles[j].id<=1) h=j;
    if(h<0 && roundNo>=280 && feedTargetId>=0 && feedTargetId!=id && feedTargetLength>=4) h=visibleHead(feedTargetId);
    if(h<0) return false;
    int forward=nextTile(h,tiles[h].dir,false),exits=0,only=-1;
    for(int d=0;d<4;d++) {int n=nextTile(h,d,false);if(n>=0 && tiles[n].id<0) {exits++;only=n;}}
    return target==forward || (exits==1 && target==only);
}

// Check future steps with our moving tail, rather than only the next empty cell.
// Foreign bodies stay fixed in this short prediction; unknown terrain ends the check.
inline bool Bot::longContinuation(const vector<int>& trail,const array<bool,4096>& others,
                      array<bool,49> eaten,int depth,int &budget) {
    if(depth==0) return true;
    if(--budget<=0) return !officialQueen();
    int at=trail.front();
    for(int d=0;d<4;d++) {
        if(portalAt[at][d]>=0 || avoided(at,d)) continue;
        int to=terrain[at][d];
        if(to==-2) {if(!officialQueen()) return true;continue;}
        if(to<0 || others[to] || find(trail.begin(),trail.end(),to)!=trail.end()) continue;
        auto next=trail; next.insert(next.begin(),to);
        int j=local(decodePos(to));bool food=j>=0 && tiles[j].pearl && !eaten[j];
        auto consumed=eaten;
        if(food) consumed[j]=true; else next.pop_back();
        if(longContinuation(next,others,consumed,depth-1,budget)) return true;
    }
    return false;
}

inline bool Bot::sustainableLongMove(int to) {
    if(!protectedLong() || int(body.size())!=length) return true;
    vector<int> trail;
    for(Pos p:body) { if(p.x<0) return true;trail.push_back(code(p)); }
    array<bool,4096> others{};
    for(const Tile &t:tiles) if(t.id>=0 && t.id!=id) others[code(t.p)]=true;
    trail.insert(trail.begin(),code(tiles[to].p));
    array<bool,49> eaten{};
    if(tiles[to].pearl) eaten[to]=true; else trail.pop_back();
    int budget=768;
    return longContinuation(trail,others,eaten,officialQueen()?min(12,max(6,length+2)):6,budget);
}

inline Move Bot::evaluate(int d,int to) {
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
        if(assemblyCenter>=0 && homeDistance(tiles[to].p,assemblyCenter)>max(4,length/4+2))
            s-=80.0;
        s+=2.0*min(r.area,32)+5.0*min(r.exits,3);
        if(assemblyCenter>=0)
            s-=4.0*max(0,homeDistance(tiles[to].p,assemblyCenter)-max(4,length/4+2));
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
    if(officialQueen()) {
        int clearance=3;
        for(int j=0;j<49;j++) for(int e=0;e<4;e++) if(edge(j,e)=="w")
            clearance=min(clearance,distance(tiles[to].p,tiles[j].p));
        s+=clearance*12;
    }
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

inline Move Bot::distantPortal(int d) {
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

inline vector<Move> Bot::legalMoves() {
    vector<Move> out;
    // Stable tie order is individual to each dragon, never reseeded every round.
    array<int,4> order{bearing,(bearing+hand+4)%4,(bearing-hand+4)%4,(bearing+2)%4};
    for(int d:order) {
        if(protectedLong() && edge(24,d)!=".") continue;
        if(edge(24,d)=="w") continue;
        int to=nextTile(24,d);
        if(to>=0 && tiles[to].id<0 && !(roundNo>=280 && tiles[to].pearl && exclusivePearl(code(tiles[to].p))) &&
           !(edge(24,d)!="." && knownPortalTrap(code(tiles[to].p))))
            out.push_back(evaluate(d,to));
        else if(to<0 && edge(24,d)!="." && portalReady(stoi(edge(24,d)))) {
            Move m=distantPortal(d);
            if(m.score>-1e19) out.push_back(m);
        }
    }
    if(officialQueen()) {
        bool viable=any_of(out.begin(),out.end(),[](const Move &m){return m.sustainable;});
        bool wide=any_of(out.begin(),out.end(),[this](const Move &m){return m.sustainable && m.risk<18 && m.exits>0 && terrainDegree(code(tiles[m.to].p))>=3;});
        out.erase(remove_if(out.begin(),out.end(),[&](const Move &m){
            return (viable && !m.sustainable) || (wide && terrainDegree(code(tiles[m.to].p))<3);
        }),out.end());
    }
    stable_sort(out.begin(),out.end(),[](const Move&a,const Move&b){return a.score>b.score;});
    return out;
}

inline bool Bot::safeMove(const Move &m) {
    if(protectedLong() && m.portal) return false;
    if(!m.sustainable || obstructsQueen(m.to)) return false;
    if(roundNo>=280 && m.to>=0 && tiles[m.to].pearl && exclusivePearl(code(tiles[m.to].p))) return false;
    if(protectedLong() && assemblyCenter>=0 && m.to>=0 &&
       homeDistance(tiles[m.to].p,assemblyCenter)>max(4,length/4+2) &&
       homeDistance(tiles[m.to].p,assemblyCenter)>=homeDistance(tiles[24].p,assemblyCenter)) return false;
    if(m.uncertain) return m.portal && m.score>-1e19 &&
        portalDestination(code(tiles[24].p),m.dir,m.portalId)>=0 &&
        portalReady(m.portalId) && !avoided(code(tiles[24].p),m.dir);
    int needed=protectedLong()?min(length+6,32):
        length>=8?min(length+6,24):min(length+2,14);
    return m.risk<18 && m.exits>0 && !avoided(code(tiles[24].p),m.dir) &&
        (m.frontier || m.area>=needed);
}

inline bool Bot::promote(vector<Move>& moves,int d) {
    auto it=find_if(moves.begin(),moves.end(),[&](const Move&m){return m.dir==d && safeMove(m);});
    if(it==moves.end()) return false;
    rotate(moves.begin(),it,it+1); return true;
}

} // namespace battlecode

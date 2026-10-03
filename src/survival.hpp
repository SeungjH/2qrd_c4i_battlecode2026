#pragma once
#include "bot.hpp"

namespace battlecode {

inline bool Bot::quietEndgame() { return roundNo>=350 && turnsAlive>=80 && roundNo-lastEnemySeen>=80; }

inline bool Bot::officialQueen() { return id==0 || id==1; }

inline bool Bot::recentIncome(int window) { return !foodRounds.empty() && roundNo-foodRounds.back()<=window; }

inline bool Bot::protectedLong() { return officialQueen() || (roundNo>=350 && feedTargetId==id && length>=4) ||
    (roundNo>=400 && length>=8) || (roundNo>=350 && length>=16); }

inline bool Bot::trappedAllyAtTail() {
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

inline bool Bot::cutCanReleaseParent() {
    if(length<4 || int(body.size())!=length) return false;
    int released=local(body[length-2]); // New child's tail, NOT its new head.
    if(released<0) return false;
    for(int d=0;d<4;d++) if(nextTile(24,d)==released) return true;
    return false;
}

inline bool Bot::physicallyTrapped() {
    for(int d=0;d<4;d++) {
        if(edge(24,d)=="w") continue;
        int to=nextTile(24,d,false);
        if(to<0) return false; // An unobserved portal is not a proven dead end.
        if(tiles[to].id<0) return false;
        // Collision is checked before the tail advances. An occupied tail is
        // not an escape this turn, even when this move would eat no pearl.
    }
    return true;
}

// A queen cannot hand its scoring identity to a reversed child. Release only
// the two tail segments when the old tail is its otherwise blocked next cell.
inline bool Bot::queenTailReleaseSplit() {
    if(!officialQueen() || length<4 || units>=unitLimit ||
       int(body.size())!=length || body.back().x<0 || !physicallyTrapped()) return false;
    for(int d=0;d<4;d++) {
        if(edge(24,d)!=".") continue;
        int to=nextTile(24,d,false);
        if(to>=0 && tiles[to].id==id && tiles[to].p==body.back()) return true;
    }
    return false;
}

// A newborn two-segment child can free its head cell immediately by dying in
// place. Moving once would retain that cell as its tail and still block the queen.
// Require a visible allied scoring queen, no free/unknown physical exit, and
// our head as its ordinary-edge exit. No untrusted sonar claim is sufficient.
inline bool Bot::newbornQueenTailGift() {
    if(officialQueen() || length<2 || turnsAlive!=0) return false;
    if(queenGiftStamp>=roundNo-1) return true;
    if(length!=2 || int(body.size())!=2) return false;
    for(int j=0;j<49;j++) if(tiles[j].head && tiles[j].team==team &&
                            tiles[j].id>=0 && tiles[j].id<=1 && tiles[j].id<id) {
        bool headExit=false,otherExit=false;
        for(int d=0;d<4;d++) {
            if(edge(j,d)=="w") continue;
            if(edge(j,d)!=".") {otherExit=true;break;}
            int to=nextTile(j,d,false);
            if(to<0 || tiles[to].id<0) {otherExit=true;break;}
            if(to==24) headExit=true;
        }
        if(headExit && !otherExit) return true;
    }
    return false;
}

inline bool Bot::retainedQueenCanEscape() {
    if(!officialQueen()) return false;
    auto available=legalMoves();
    return any_of(available.begin(),available.end(),[this](const Move &m){return safeMove(m);});
}

inline bool Bot::reverseRescue(const vector<Move>& moves) {
    if(officialQueen() || !physicallyTrapped()) return false;
    if(length<4 || units>=unitLimit || int(body.size())!=length ||
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
    auto mappedTailEscape=[&]() {
        int key=code(body.back());
        array<bool,4096> occupied{};
        for(Pos p:body) if(p.x>=0) occupied[code(p)]=true;
        for(const Tile &t:tiles) if(t.id>=0) occupied[code(t.p)]=true;
        // Any trapped dragon may reverse its rear using previously observed terrain.
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
    };
    if(tail<0) return mappedTailEscape();
    // The reversed child's head is the old tail. Verify its first step, including
    // occupancy and an exit beyond that step, before spending a split action.
    for(int d=0;d<4;d++) {
        if(edge(tail,d)!=".") continue;
        int to=nextTile(tail,d,false);
        if(to<0 || tiles[to].id>=0 || headRisk(to)>=18) continue;
        Region r=region(to,afterMove(to),false);
        if(r.frontier || r.area>=min(length+2,20)) return true;
    }
    return mappedTailEscape();
}

inline bool Bot::pocketEscapeSplit(const vector<Move>& moves) {
    if(!physicallyTrapped()) return false;
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

inline int Bot::emergencyPortalDirection() {
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

inline bool Bot::routineSplit(const vector<Move>& moves) {
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

inline int Bot::branchChildDirection(const vector<Move>& moves) {
    if(roundNo>=300 || officialQueen() || length<5 || length>8 || units>=min(unitLimit,width*height/20) ||
       role!=0 || roundNo-lastSplit<policy::SplitCooldown || !any_of(moves.begin(),moves.end(),[this](const Move &m){return safeMove(m);}) ||
       terrainDegree(code(tiles[24].p))<3 || body.back().x<0) return -1;
    int tail=local(body.back());if(tail<0 || terrainDegree(code(body.back()))<3) return -1;
    int choices=0,best=-1;double value=-1e20;
    for(int d=0;d<4;d++) {
        if(edge(tail,d)!=".") continue;
        int to=nextTile(tail,d,false);
        if(to<0 || tiles[to].id>=0 || headRisk(to)>=18) continue;
        array<bool,49> blocked{};for(int j=0;j<49;j++) blocked[j]=tiles[j].id>=0;
        Region room=region(to,blocked,false);
        if(room.exits==0 || (!room.frontier && room.area<6)) continue;
        choices++;
        double score=distance(tiles[to].p,tiles[24].p)*5+room.area+int(visits[code(tiles[to].p)]==0)*4;
        if(score>value) {value=score;best=d;}
    }
    return choices>=2?best:-1;
}

// Keep the fixed head and remove the smallest suffix that gives it a verified escape.
inline int Bot::queenReleaseSplitSize() {
    if(!officialQueen() || length<4 || units>=unitLimit || int(body.size())!=length) return 0;
    if(any_of(body.begin(),body.end(),[](Pos p){return p.x<0;})) return 0;
    if(!physicallyTrapped()) return 0;
    array<bool,4096> others{};
    for(const Tile &t:tiles) if(t.id>=0 && t.id!=id) others[code(t.p)]=true;
    array<bool,49> savedFood{};
    for(int j=0;j<49;j++) savedFood[j]=tiles[j].pearl;
    for(int cut=2;cut<=length-2;cut++) {
        int retained=length-cut;
        // The reversed child drops every second cell, starting at the old tail.
        for(int k=length-1;k>=retained;k-=2) {int j=local(body[k]);if(j>=0) tiles[j].pearl=true;}
        for(int d=0;d<4;d++) {
            if(edge(24,d)!=".") continue;
            int to=nextTile(24,d,false);
            if(to<0 || tiles[to].id!=id || headRisk(to)>=18) continue;
            auto at=find(body.begin()+retained,body.end(),tiles[to].p);
            if(at==body.end()) continue;
            vector<int> trail;for(int k=0;k<retained;k++) trail.push_back(code(body[k]));
            trail.insert(trail.begin(),code(tiles[to].p));
            array<bool,49> eaten{};
            if(tiles[to].pearl) eaten[to]=true;else trail.pop_back();
            int budget=768;
            bool escape=longContinuation(trail,others,eaten,min(12,max(6,retained+2)),budget);
            if(escape) {
                for(int j=0;j<49;j++) tiles[j].pearl=savedFood[j];
                return cut;
            }
        }
        for(int j=0;j<49;j++) tiles[j].pearl=savedFood[j];
    }
    return 0;
}

// Only a visible fixed queen with no usable exit can request emergency clearance.
inline bool Bot::relieveQueenLane(ActionPlan &plan) {
    if(officialQueen()) return false;
    int q=-1;
    for(int j=0;j<49;j++) if(tiles[j].head && tiles[j].team==team && tiles[j].id>=0 && tiles[j].id<=1) q=j;
    if(q<0) return false;
    array<bool,49> blocked{};for(int j=0;j<49;j++) blocked[j]=tiles[j].id>=0;
    bool ourExit=false;
    for(int d=0;d<4;d++) {
        if(edge(q,d)!=".") continue;
        int to=nextTile(q,d,false);if(to<0) return false;
        if(tiles[to].id<0) {
            Region r=region(to,blocked,false);
            if(r.frontier || r.area>=4) return false;
        }
        if(tiles[to].id==id) ourExit=true;
    }
    if(!ourExit || length>12) return false;
    // Advancing an uneaten tail costs nothing and is preferred to sacrifice.
    int tail=body.empty()?-1:local(body.back());
    bool tailExit=false;
    for(int d=0;d<4;d++) if(edge(q,d)=="." && nextTile(q,d,false)==tail) tailExit=true;
    if(tailExit) for(const Move &m:plan.moves) if(m.to>=0 && !tiles[m.to].pearl && safeMove(m)) {
        plan.chosen=m;plan.reason="QUEEN_CLEAR_TAIL";return true;
    }
    for(int j=0;j<49;j++) if(tiles[j].id==id) blocked[j]=false;
    for(int d=0;d<4;d++) {
        int to=nextTile(q,d,false);
        if(edge(q,d)!="." || to<0 || tiles[to].id!=id) continue;
        bool contested=false;
        for(const Tile &t:tiles) if(t.head && t.id>=0 && t.team!=team && distance(t.p,tiles[to].p)<=2) contested=true;
        if(contested) continue;
        Region r=region(to,blocked,false);
        if(r.frontier || r.area>=6) {
            plan.chosen=Move{};plan.splitSize=0;plan.sacrifice=true;
            plan.reason="QUEEN_CLEAR_LANE";return true;
        }
    }
    return false;
}

} // namespace battlecode

#pragma once
#include "bot.hpp"

namespace battlecode {

inline bool Bot::guideOutOfLoop(vector<Move>& moves) {
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
    for(const auto &t:tiles) if(t.id>=0 || (roundNo>=280 && t.pearl && exclusivePearl(code(t.p)))) blocked[code(t.p)]=true;
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

// A two-segment scout can pay for a dead-end visit when three visible pearls
// let a three-segment tail child return with a net gain.
inline bool Bot::profitablePocket(const Move &m) {
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

inline bool Bot::profitablePocketMove(vector<Move>& moves) {
    auto it=find_if(moves.begin(),moves.end(),[this](const Move &m){return profitablePocket(m);});
    if(it==moves.end()) return false;
    rotate(moves.begin(),it,it+1);return true;
}

// Known-map route, recomputed with current occupancy, retaining a stable destination.
inline int Bot::routeDirection(const vector<Move>& moves,int destination) {
    if(destination<0 || !validKey(destination)) return -1;
    array<bool,4096> blocked{};
    for(const auto &t:tiles) if(t.id>=0 || (roundNo>=280 && t.pearl && exclusivePearl(code(t.p)))) blocked[code(t.p)]=true;
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

inline int Bot::routeToMeeting(const vector<Move>& moves,int destination) {
    if(destination<0 || !validKey(destination)) return -1;
    static array<bool,4096> blocked;blocked.fill(false);
    static array<int,4096> first,dist,q;first.fill(-1);dist.fill(-1);
    for(const Tile &t:tiles) if(t.id>=0 || (roundNo>=280 && t.pearl && exclusivePearl(code(t.p)))) blocked[code(t.p)]=true;
    for(Pos p:body) if(p.x>=0) blocked[code(p)]=true;
    int here=code(tiles[24].p),begin=0,end=0,best=-1;double value=-1e20;
    array<bool,4> safe{};for(const Move &m:moves) if(safeMove(m)) safe[m.dir]=true;
    q[end++]=here;dist[here]=0;
    while(begin<end) {
        int at=q[begin++];
        for(int d=0;d<4;d++) {
            if((at==here && !safe[d]) || avoided(at,d)) continue;
            int to=terrain[at][d];
            if(to<0 || blocked[to] || dist[to]>=0) continue;
            dist[to]=dist[at]+1;first[to]=at==here?d:first[at];q[end++]=to;
            int gap=distance(decodePos(to),decodePos(destination));
            bool frontier=any_of(terrain[to].begin(),terrain[to].end(),[](int e){return e<-1;});
            double score=-12.0*gap-dist[to]+(frontier?3:0);
            if(score>value) {value=score;best=first[to];}
        }
    }
    return best;
}

// Simulate the complete route, including tail retention on every visible meal.
// Foreign bodies are fixed for this short prediction; unknown cells remain uncertain.
inline bool Bot::pearlPathSafe(const vector<int>& path) {
    vector<int> trail;
    // Incomplete initial body memory is uncertainty, not proof of an unsafe meal.
    for(Pos p:body) {if(p.x<0) return true;trail.push_back(code(p));}
    array<bool,4096> others{};
    for(const Tile &t:tiles) if(t.id>=0 && t.id!=id) others[code(t.p)]=true;
    array<bool,49> eaten{};bool finalMeal=false;
    for(int next:path) {
        if(protectedLong() && assemblyCenter>=0 &&
           homeDistance(decodePos(next),assemblyCenter)>max(4,length/4+2)) return false;
        if(others[next] || find(trail.begin(),trail.end(),next)!=trail.end()) return false;
        trail.insert(trail.begin(),next);
        int j=local(decodePos(next));bool food=j>=0 && tiles[j].pearl && !eaten[j];
        finalMeal=food;
        if(food) eaten[j]=true;else trail.pop_back();
    }
    int target=local(decodePos(trail.front()));
    if(target<0) return false;
    array<bool,49> blocked{};
    for(int j=0;j<49;j++) blocked[j]=others[code(tiles[j].p)];
    for(int key:trail) {int j=local(decodePos(key));if(j>=0) blocked[j]=true;}
    Region terminal=region(target,blocked,false);
    int beforeMeal=int(trail.size())-int(finalMeal);
    bool protect=officialQueen() || (roundNo>=400 && beforeMeal>=8) || (roundNo>=350 && beforeMeal>=16);
    int needed=protect?min(beforeMeal+6,32):beforeMeal>=8?min(beforeMeal+6,24):min(beforeMeal+2,14);
    if(terminal.exits==0 || (!terminal.frontier && terminal.area<needed)) return false;
    if(!protect) return true; // Scouts retain their existing emergency-split option.
    return longContinuation(trail,others,eaten,6,pearlSearchBudget);
}

// Route to a visible pearl through known open edges, even when a wall makes
// its Manhattan distance misleading. The first step must remain safe.
inline bool Bot::guideToPearl(vector<Move>& moves) {
    pearlSearchBudget=256;
    if(moves.empty()) return false;
    for(const Move &m:moves) if(m.to>=0 && tiles[m.to].pearl && !safeMove(m))
        pearlCooldown[code(tiles[m.to].p)]=roundNo+24;
    static array<bool,4096> blocked;blocked.fill(false);
    for(const Tile &t:tiles) if(t.id>=0 || (roundNo>=280 && t.pearl && exclusivePearl(code(t.p)))) blocked[code(t.p)]=true;
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

inline bool Bot::guideToResource(vector<Move>& moves) {
    if(recentIncome() || roundNo>=490 || moves.empty()) return false;
    double best=-1e20;int direction=-1;resourceGoal=-1;
    for(auto [key,r]:resources) {
        if(protectedLong() && assemblyCenter>=0 && homeDistance(decodePos(key),assemblyCenter)>max(4,length/4+2)) continue;
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

inline void Bot::updateCorridor(const Move &m) {
    if(m.to<0 || m.portal) { corridorEntry=-1; return; }
    int from=code(tiles[24].p),to=code(tiles[m.to].p);
    int a=terrainDegree(from),b=terrainDegree(to);
    if(corridorEntry<0 && a>=3 && b<=2) { corridorEntry=from; corridorDir=m.dir; }
    else if(corridorEntry>=0 && (to==corridorEntry || b>=3)) corridorEntry=-1;
}

inline bool Bot::guideCourier(vector<Move>& moves) {
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
    vector<Move> choices;
    for(const Move &m:moves) if(m.to>=0 && safeMove(m)) choices.push_back(m);
    if(choices.size()>=2) {
        auto best=max_element(choices.begin(),choices.end(),[](const Move&a,const Move&b){
            return a.area*2+a.exits*5+a.score*0.1 < b.area*2+b.exits*5+b.score*0.1;
        });
        if(promote(moves,best->dir)) return true;
    }
    if(promote(moves,facing)) return true;
    return !moves.empty();
}

inline void Bot::prepareEscapeRadio() {
    if(corridorEntry<0 || body.empty() || body.back().x<0) return;
    bool permanent=terrainDegree(code(tiles[24].p))<=1;
    for(int d=0;d<4;d++) if(edge(24,d)!="." && edge(24,d)!="w") permanent=false;
    addWarning(corridorEntry,corridorDir,roundNo,permanent,true);
    unsigned data=unsigned(code(body.back())) | (unsigned(corridorEntry)<<12) |
        (unsigned(corridorDir)<<24) | (unsigned(permanent)<<26);
    sendPacket(ESCAPE,data,roundNo,true);
}

} // namespace battlecode

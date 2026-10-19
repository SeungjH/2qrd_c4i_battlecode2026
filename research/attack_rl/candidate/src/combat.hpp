#pragma once
#include "bot.hpp"

namespace battlecode {

inline void Bot::endHunt() {
    huntEnemy=-1;huntCooldown=roundNo+8;huntHeads.clear();
}

inline bool Bot::coordinateRLHunt(vector<Move>& moves) {
    auto stop=[&](const char* why){
        if(huntEnemy>=0) cout<<"LOG HUNT_END "<<id<<' '<<roundNo<<' '<<huntEnemy<<' '<<why<<'\n';
        endHunt();rlAttackRoster.clear();rlAttackPeers.clear();rlPeerSeen.clear();return false;
    };
    if(officialQueen() || protectedLong() || length<2 || length>4 || roundNo>=350 ||
       roundNo<=courierUntil || roundNo<=reverseChildUntil || role!=0 || units<4) {
        if(huntEnemy>=0)return stop("ineligible");return false;
    }
    if(roundNo<huntCooldown)return false;
    if(huntEnemy>=0 && roundNo-huntSince>=policy::HuntDuration)return stop("duration");
    if(none_of(moves.begin(),moves.end(),[&](const Move&m){return !m.portal && safeMove(m);}))
        return huntEnemy>=0?stop("danger"):false;

    int enemy=visibleHead(huntEnemy);
    if(enemy<0 && huntEnemy>=0) {
        if(roundNo-rlAttackSeen>3)return stop("target_lost");
        int d=routeDirection(moves,code(rlAttackEnemy));
        if(d>=0 && promote(moves,d)) {role=4;return true;}
        return false;
    }
    if(enemy<0) {
        // Adopt a nearby leader's target only when we have no ongoing target.
        int leader=65536;
        for(const Packet&p:received) if(p.type==HUNT && roundNo-p.stamp<=3 && p.sender>1 &&
            freshAlly(p.sender) && allies[p.sender].len>=2 && allies[p.sender].len<=4 && p.sender<leader) {
            int at=visibleHead((p.payload>>12)&4095);
            if(at>=0 && tiles[at].team!=team) {enemy=at;leader=p.sender;}
        }
        if(enemy<0) {
            double best=-1e20;
            for(int j=0;j<49;j++) if(tiles[j].head && tiles[j].team!=team) {
                int observed=0;for(const Tile&t:tiles)observed+=t.id==tiles[j].id;
                double score=min(observed,12)*2.0-distance(tiles[24].p,tiles[j].p)*3.0;
                if(score>best){best=score;enemy=j;}
            }
        }
        if(enemy<0 || distance(tiles[24].p,tiles[enemy].p)>5)return false;
    }
    if(distance(tiles[24].p,tiles[enemy].p)>6)return huntEnemy>=0?stop("target_far"):false;

    vector<int> roster;
    if(huntEnemy>=0 && !rlAttackRoster.empty()) {
        // Keep the agreed roster; brief missed observations do not dissolve it.
        for(int owner:rlAttackRoster) {
            if(owner==id){roster.push_back(owner);continue;}
            auto it=allies.find(owner);int at=visibleHead(owner);
            if(it!=allies.end() && roundNo-it->second.stamp<=4 &&
               (it->second.len<2 || it->second.len>4 || (it->second.role!=0 && it->second.role!=4)))continue;
            if(at>=0 && distance(tiles[at].p,tiles[enemy].p)<=6) {
                rlPeerSeen[owner]=roundNo;roster.push_back(owner);
            } else if(rlPeerSeen.count(owner) && roundNo-rlPeerSeen[owner]<=3)roster.push_back(owner);
        }
    } else {
        vector<pair<int,int>> nearby{{distance(tiles[24].p,tiles[enemy].p),id}};
        for(int j=0;j<49;j++) if(tiles[j].head && tiles[j].team==team && tiles[j].id>1 && tiles[j].id!=id &&
            distance(tiles[j].p,tiles[enemy].p)<=5) {
            auto it=allies.find(tiles[j].id);
            // Sonar length is authoritative; do not recruit an unknown long dragon.
            if(it==allies.end() || roundNo-it->second.stamp>4 || it->second.len<2 || it->second.len>4 ||
               (it->second.role!=0 && it->second.role!=4))continue;
            nearby.push_back({distance(tiles[j].p,tiles[enemy].p),tiles[j].id});
        }
        sort(nearby.begin(),nearby.end());if(nearby.size()>3)nearby.resize(3);
        for(auto [dist,owner]:nearby){roster.push_back(owner);rlPeerSeen[owner]=roundNo;}
    }
    if(roster.size()<2 || find(roster.begin(),roster.end(),id)==roster.end())
        return huntEnemy>=0?stop("team_lost"):false;
    sort(roster.begin(),roster.end());
    array<bool,49> blocked{};for(int j=0;j<49;j++)blocked[j]=tiles[j].id>=0;
    Region routes=region(24,blocked,false);int nearest=1000,goalCell=-1;
    for(int d=0;d<4;d++)if(edge(enemy,d)==".") {
        int at=nextTile(enemy,d,false);
        if(at>=0 && tiles[at].id<0 && routes.dist[at]>=0 && routes.dist[at]<nearest){nearest=routes.dist[at];goalCell=at;}
    }
    // The model need not prove a complete surround, but the target must be reachable.
    if(goalCell<0 || nearest>8)return huntEnemy>=0?stop("unreachable"):false;
    if(huntEnemy<0) {
        huntEnemy=tiles[enemy].id;huntSince=roundNo;huntProgress=roundNo;huntDistance=1000;
        rlAttackStarted=roundNo;huntHeads.clear();
        cout<<"LOG HUNT_START "<<id<<' '<<roundNo<<' '<<huntEnemy<<'\n';
    }
    if(nearest<huntDistance){huntDistance=nearest;huntProgress=roundNo;}
    if(roundNo-huntProgress>=policy::HuntStall)return stop("stalled");
    int d=routeDirection(moves,code(tiles[goalCell].p));
    if(d>=0)promote(moves,d); // Safe fallback only; RL still chooses its own direction.
    role=4;rlAttackTarget=huntEnemy;rlAttackEnemy=tiles[enemy].p;rlAttackSeen=roundNo;rlAttackFresh=true;
    rlAttackRoster=roster;rlAttackPeers.clear();
    for(int owner:roster)if(owner!=id)rlAttackPeers.push_back(owner);
    rlAttackSlot=int(find(roster.begin(),roster.end(),id)-roster.begin());
    if(huntEnemy<=4095)sendPacket(HUNT,unsigned(code(rlAttackEnemy))|(unsigned(huntEnemy)<<12)|(unsigned(rlAttackSlot)<<24));
    return true;
}

inline bool Bot::coordinateHunt(vector<Move>& moves) {
    if(rlAttackEnabled) return coordinateRLHunt(moves);
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
    if(rlAttackTarget!=huntEnemy)rlAttackStarted=roundNo;
    rlAttackTarget=huntEnemy;rlAttackEnemy=tiles[enemy].p;rlAttackSeen=roundNo;rlAttackFresh=true;
    rlAttackPeers.clear();rlAttackRoster.clear();
    for(auto member:members)rlAttackRoster.push_back(member.first);
    rlAttackSlot=int(find(rlAttackRoster.begin(),rlAttackRoster.end(),id)-rlAttackRoster.begin());
    for(int owner:rlAttackRoster)if(owner!=id)rlAttackPeers.push_back(owner);
    if(huntEnemy<=4095) sendPacket(HUNT,unsigned(code(tiles[enemy].p))|
        (unsigned(huntEnemy)<<12)|(unsigned(slot)<<24)|(unsigned(blocking)<<26));
    return true;
}

inline bool Bot::newbornAmbush() {
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

inline int Bot::favorableHeadTrade() {
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

inline int Bot::tailAmbushEnemy(const vector<Move>& moves) {
    if(roundNo>=350 || length<4 || units>=unitLimit-4 || protectedLong() ||
       role==1 || role==2 || role==3 || roundNo-lastSplit<policy::SplitCooldown ||
       int(body.size())!=length || body.back().x<0 ||
       !any_of(moves.begin(),moves.end(),[this](const Move &m){return safeMove(m);})) return -1;
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

} // namespace battlecode
